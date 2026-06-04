// =============================================================================
// Motion.cpp
// ESPresense fork — GPIO-based PIR and radar motion
//
// CHANGES FROM UPSTREAM:
//   • pirState and radarGpioState are namespace-scope (not static) so
//     Occupancy.cpp can read them via Motion::pirState
//   • Upstream GPIO radar logic unchanged; LD2410Uart UART radar is separate
//   • motion topic remains: OR of pirState | radarGpioState (GPIO-only)
//     Occupancy.cpp handles the fused occupancy using LD2410Uart too
// =============================================================================

#include "Motion.h"
#include "main.h"
#include "mqtt.h"
#include <Arduino.h>

namespace Motion {

// ---------------------------------------------------------------------------
// Public state (extern in Motion.h)
// ---------------------------------------------------------------------------
bool pirState        = false;
bool radarGpioState  = false;

// ---------------------------------------------------------------------------
// Private settings
// ---------------------------------------------------------------------------
static int   pirPin          = -1;
static int   radarPin        = -1;
static int   pirPinMode      = INPUT;
static int   radarPinMode    = INPUT;
static float pirTimeout      = 0.5f;   // seconds
static float radarTimeout    = 0.5f;   // seconds

// Timestamps for timeout hold-off
static unsigned long pirLastOn   = 0;
static unsigned long radarLastOn = 0;

// Previous published values (change-detect)
static bool lastPirState   = false;
static bool lastRadarState = false;
static bool lastMotion     = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int resolvePinMode(const String& modeStr) {
    if (modeStr == "Pullup")            return INPUT_PULLUP;
    if (modeStr == "Pulldown")          return INPUT_PULLDOWN;
    if (modeStr == "Pullup Inverted")   return INPUT_PULLUP;
    if (modeStr == "Pulldown Inverted") return INPUT_PULLDOWN;
    return INPUT;
}

static bool isPinInverted(const String& modeStr) {
    return (modeStr == "Pullup Inverted" || modeStr == "Floating Inverted" ||
            modeStr == "Pulldown Inverted");
}

// ---------------------------------------------------------------------------
// ConnectToWifi
// ---------------------------------------------------------------------------
void ConnectToWifi() {
    pirPin     = HeadlessWiFiSettings.integer("pir_pin",   -1,  "PIR motion pin (-1=disable)");
    radarPin   = HeadlessWiFiSettings.integer("radar_pin", -1,  "Radar GPIO pin (-1=disable)");

    String pirModeStr   = HeadlessWiFiSettings.string("pir_pin_mode",   "Pulldown", "PIR pin type");
    String radarModeStr = HeadlessWiFiSettings.string("radar_pin_mode", "Pulldown", "Radar pin type");

    pirPinMode   = resolvePinMode(pirModeStr);
    radarPinMode = resolvePinMode(radarModeStr);

    pirTimeout   = HeadlessWiFiSettings.floating("pir_timeout",   0.5f, "PIR timeout (s)");
    radarTimeout = HeadlessWiFiSettings.floating("radar_timeout", 0.5f, "Radar GPIO timeout (s)");
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void Setup() {
    if (pirPin   >= 0) pinMode(pirPin,   pirPinMode);
    if (radarPin >= 0) pinMode(radarPin, radarPinMode);

    pirState       = false;
    radarGpioState = false;
    lastPirState   = false;
    lastRadarState = false;
    lastMotion     = false;
}

// ---------------------------------------------------------------------------
// SerialReport
// ---------------------------------------------------------------------------
void SerialReport() {
    Serial.printf("Motion: PIR pin=%d mode=%d  Radar GPIO pin=%d mode=%d\n",
        pirPin, pirPinMode, radarPin, radarPinMode);
}

// ---------------------------------------------------------------------------
// Loop — read GPIO pins, apply hold-off timeout, publish changes
// ---------------------------------------------------------------------------
void Loop() {
    unsigned long now = millis();
    String base = roomsTopic + "/";

    // --- PIR ---
    if (pirPin >= 0) {
        bool raw = digitalRead(pirPin);
        // Active-HIGH after pinMode; Pullup inverted modes flip it
        if (raw) pirLastOn = now;
        bool newPir = (now - pirLastOn) < (unsigned long)(pirTimeout * 1000.0f);
        if (newPir != pirState) {
            pirState = newPir;
        }
    }

    // --- Radar GPIO (separate from LD2410 UART radar) ---
    if (radarPin >= 0) {
        bool raw = digitalRead(radarPin);
        if (raw) radarLastOn = now;
        bool newRadar = (now - radarLastOn) < (unsigned long)(radarTimeout * 1000.0f);
        if (newRadar != radarGpioState) {
            radarGpioState = newRadar;
        }
    }

    // --- Publish pir if changed ---
    if (pirState != lastPirState) {
        lastPirState = pirState;
        pub((base + "pir").c_str(), 0, 1, pirState ? "ON" : "OFF");
    }

    // --- Publish radar (GPIO only) if changed ---
    // Note: LD2410Uart.cpp publishes "radar" for the UART radar.
    // If you only have UART radar (no GPIO radar pin), this stays silent.
    if (radarPin >= 0 && radarGpioState != lastRadarState) {
        lastRadarState = radarGpioState;
        pub((base + "radar_gpio").c_str(), 0, 1, radarGpioState ? "ON" : "OFF");
    }

    // --- Publish combined motion (PIR OR GPIO radar) ---
    bool motion = pirState || radarGpioState;
    if (motion != lastMotion) {
        lastMotion = motion;
        pub((base + "motion").c_str(), 0, 1, motion ? "ON" : "OFF");
    }
}

// ---------------------------------------------------------------------------
// SendDiscovery
// ---------------------------------------------------------------------------
bool SendDiscovery() {
    bool ok = true;

    if (pirPin >= 0)
        ok = ok && sendBinarySensorDiscovery("PIR", "pir", "motion");

    if (radarPin >= 0)
        ok = ok && sendBinarySensorDiscovery("Radar GPIO", "radar_gpio", "motion");

    // Combined motion always present if either sensor is wired
    if (pirPin >= 0 || radarPin >= 0)
        ok = ok && sendBinarySensorDiscovery("Motion", "motion", "motion");

    return ok;
}

} // namespace Motion
