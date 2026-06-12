#ifdef SENSORS
// =============================================================================
// Occupancy.cpp
// ESPresense fork — AIOsense-style fused room occupancy
//
// Combines:
//   • PIR motion      (Motion::pirState     — fast trigger, binary, ~2m range)
//   • LD2410 radar    (LD2410Uart::hasTarget — still presence, 0-6m range)
//
// Logic (mirrors AIOsense occupancy.yaml):
//   ON  immediately when PIR fires OR radar detects any target
//   OFF only after `occupancy_timeout` seconds of no detection from either
//
// This single `occupancy` binary sensor is what drives your automations.
// PIR fires instantly; radar keeps the latch while you're sitting still.
// =============================================================================

#include "Occupancy.h"
#include "LD2410Uart.h"
#include "Motion.h"     // Motion::pirState
#include "LEDs.h"
#include "Buzzer.h"
#include "string_utils.h"
#include "globals.h"
#include "defaults.h"
#include <HeadlessWiFiSettings.h>
#include "mqtt.h"
#include <Arduino.h>

namespace Occupancy {

// Configurable behavior (set in ConnectToWifi)
static uint8_t onR = 0,   onG = 0,   onB = 255;   // occupied color (blue)
static uint8_t offR = 255, offG = 0, offB = 0;     // clear color (red)
static int     ledBrightness = 128;
static int     onMelodyIdx = 0;   // 0 = None
static int     offMelodyIdx = 0;
static bool    ledEnabled = false;
static bool    buzzerEnabled = false;

// Melody preset names — index 0 is "None"
static const char* MELODY_NAMES[] = {
    "", "chirp", "descend", "welcome", "goodbye",
    "alert", "success", "error", "doorbell", "beep"
};
static const int MELODY_COUNT = 10;

// Parse "RRGGBB" hex string into r,g,b
static void parseHexColor(const String& hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    String h = hex;
    if (h.startsWith("#")) h = h.substring(1);
    if (h.length() != 6) return;
    long val = strtol(h.c_str(), nullptr, 16);
    r = (val >> 16) & 0xFF;
    g = (val >> 8) & 0xFF;
    b = val & 0xFF;
}

// ---------------------------------------------------------------------------
// Public state
// ---------------------------------------------------------------------------
bool occupied = false;

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------
static unsigned long delayedOffMs    = 15000; // configurable
static unsigned long lastDetectMs    = 0;
static bool          lastPublished   = false;  // tracks last sent value
static bool          discoveryReady  = false;

// ---------------------------------------------------------------------------
// ConnectToWifi — register settings (called during WiFi-settings phase)
// ---------------------------------------------------------------------------
void ConnectToWifi() {
    int timeoutSec = HeadlessWiFiSettings.integer(
        "Occupancy_timeout", 15,
        "Occupancy delayed-off (seconds) after last detection");
    delayedOffMs = (unsigned long)timeoutSec * 1000UL;

    // --- LED behavior ---
    ledEnabled = HeadlessWiFiSettings.checkbox(
        "occ_led_enable", false, "Drive LED 1 from occupancy state");

    String onHex = HeadlessWiFiSettings.string(
        "occ_on_color", "0000FF", "Occupied color (hex RRGGBB)");
    String offHex = HeadlessWiFiSettings.string(
        "occ_off_color", "FF0000", "Clear color (hex RRGGBB)");
    parseHexColor(onHex, onR, onG, onB);
    parseHexColor(offHex, offR, offG, offB);

    ledBrightness = HeadlessWiFiSettings.integer(
        "occ_led_brightness", 0, 255, 128, "Occupancy LED brightness (0-255)");

    // --- Buzzer behavior ---
    buzzerEnabled = HeadlessWiFiSettings.checkbox(
        "occ_buzzer_enable", false, "Play melody on occupancy change");

    std::vector<String> melodies = {
        "None", "chirp", "descend", "welcome", "goodbye",
        "alert", "success", "error", "doorbell", "beep"
    };
    onMelodyIdx = HeadlessWiFiSettings.dropdown(
        "occ_on_melody", melodies, 0, "Melody when occupied");
    offMelodyIdx = HeadlessWiFiSettings.dropdown(
        "occ_off_melody", melodies, 0, "Melody when clear");
}

// ---------------------------------------------------------------------------
// Setup — nothing hardware-specific; depends only on Motion and LD2410Uart
// ---------------------------------------------------------------------------
void Setup() {
    occupied       = false;
    lastDetectMs   = 0;
    lastPublished  = false;
    discoveryReady = false;
    Serial.printf("Occupancy: delayed-off = %lus\n", delayedOffMs / 1000UL);
}

void SerialReport() {
    Serial.printf("Occupancy: %s  (PIR=%s  radar=%s  timeout=%lus)\n",
        occupied ? "ON" : "OFF",
        Motion::pirState           ? "ON" : "OFF",
        LD2410Uart::hasTarget      ? "ON" : "OFF",
        delayedOffMs / 1000UL);
}

// ---------------------------------------------------------------------------
// Loop — AIOsense fusion logic, runs every main-loop tick
// ---------------------------------------------------------------------------
void Loop() {
    bool detection = Motion::pirState || LD2410Uart::hasTarget;

    if (detection) {
        // Keep the latch alive
        lastDetectMs = millis();

        if (!occupied) {
            occupied = true;
            pub((roomsTopic + "/occupancy").c_str(), 0, 1, "ON");
            lastPublished = true;

            if (ledEnabled) {
                String cmd = "led_1";
                String pay = Sprintf(
                    "{\"state\":\"ON\",\"brightness\":%d,\"color\":{\"r\":%d,\"g\":%d,\"b\":%d}}",
                    ledBrightness, onR, onG, onB);
                LEDs::Command(cmd, pay);
            }
            if (buzzerEnabled && onMelodyIdx > 0)
                Buzzer::Play(MELODY_NAMES[onMelodyIdx]);
        }
    } else {
        // If no detection: wait for delayed_off to expire before clearing
        if (occupied && (millis() - lastDetectMs >= delayedOffMs)) {
            occupied = false;
            pub((roomsTopic + "/occupancy").c_str(), 0, 1, "OFF");
            lastPublished = false;

            if (ledEnabled) {
                String cmd = "led_1";
                String pay = Sprintf(
                    "{\"state\":\"ON\",\"brightness\":%d,\"color\":{\"r\":%d,\"g\":%d,\"b\":%d}}",
                    ledBrightness, offR, offG, offB);
                LEDs::Command(cmd, pay);
            }
            if (buzzerEnabled && offMelodyIdx > 0)
                Buzzer::Play(MELODY_NAMES[offMelodyIdx]);
        }
    }
}

// ---------------------------------------------------------------------------
// SendDiscovery — HA MQTT discovery for the occupancy binary sensor
// ---------------------------------------------------------------------------
bool SendDiscovery() {
    bool ok = sendBinarySensorDiscovery("Occupancy", EC_NONE, "occupancy");
    if (ok) discoveryReady = true;
    return ok;
}

} // namespace Occupancy

#endif // SENSORS
