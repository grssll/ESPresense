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
#include "globals.h"
#include "defaults.h"
#include <HeadlessWiFiSettings.h>
#include "mqtt.h"
#include <Arduino.h>

namespace Occupancy {

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
        }
    } else {
        // If no detection: wait for delayed_off to expire before clearing
        if (occupied && (millis() - lastDetectMs >= delayedOffMs)) {
            occupied = false;
            pub((roomsTopic + "/occupancy").c_str(), 0, 1, "OFF");
            lastPublished = false;
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
