#ifdef SENSORS
// =============================================================================
// LD2410Uart.cpp
// ESPresense fork — LD2410 24GHz mmWave radar via UART
//
// Ported from ESPHome esphome/components/ld2410/ld2410.cpp (Apache-2.0).
// Replaces the single-GPIO radar pin with full UART frame parsing, exposing:
//   • has_target / has_moving / has_still binary sensors
//   • moving_distance, still_distance, detection_distance (cm)
//   • moving_energy, still_energy (0-100)
//
// WIRING (ESP32dev):
//   LD2410 TX  →  GPIO 16  (ESP32 RX2)
//   LD2410 RX  →  GPIO 17  (ESP32 TX2)
//   LD2410 VCC →  5V
//   LD2410 GND →  GND
//   LD2410 OUT →  not required (UART replaces it entirely)
//
// All pins are configurable via the ESPresense web UI hardware settings page.
// =============================================================================

#include "LD2410Uart.h"
#include "globals.h"
#include "defaults.h"
#include <HeadlessWiFiSettings.h>
#include "mqtt.h"   // pub(), sendSensorDiscovery(), sendBinarySensorDiscovery()
#include <Arduino.h>

namespace LD2410Uart {

// ---------------------------------------------------------------------------
// State variables (extern-declared in header)
// ---------------------------------------------------------------------------
bool     initialized     = false;
bool     hasTarget        = false;
bool     hasMovingTarget  = false;
bool     hasStillTarget   = false;
uint16_t movingDistance   = 0;
uint8_t  movingEnergy     = 0;
uint16_t stillDistance    = 0;
uint8_t  stillEnergy      = 0;
uint16_t detectionDistance= 0;

// ---------------------------------------------------------------------------
// Settings (loaded from NVS via HeadlessWiFiSettings)
// ---------------------------------------------------------------------------
static int  rxPin       = -1;  // -1 = disabled
static int  txPin       = -1;
#include "driver/uart.h"
#include "driver/gpio.h"
static const uart_port_t LD2410_UART = UART_NUM_1;
static const int UART_BUF_SIZE = 512;  // UART1 — works on all ESP32 variants

// ---------------------------------------------------------------------------
// Frame parsing constants (from ESPHome ld2410.cpp)
// ---------------------------------------------------------------------------
static constexpr uint8_t DATA_FRAME_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t DATA_FRAME_FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
static constexpr uint8_t CMD_FRAME_HEADER[4]  = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[4]  = {0x04, 0x03, 0x02, 0x01};

// Inner data frame bytes
static constexpr uint8_t INNER_HEADER = 0xAA;  // buffer[7]
static constexpr uint8_t INNER_FOOTER = 0x55;  // buffer[buf_pos - 6]
static constexpr uint8_t INNER_CRC    = 0x00;  // buffer[buf_pos - 5]

// Byte offsets within the received buffer (0-indexed from frame start)
static constexpr uint8_t OFF_DATA_TYPE       =  6; // 0x01=engineering, 0x02=normal
static constexpr uint8_t OFF_TARGET_STATES   =  8; // 0x00-0x03
static constexpr uint8_t OFF_MOVE_DIST_LOW   =  9;
static constexpr uint8_t OFF_MOVE_DIST_HIGH  = 10;
static constexpr uint8_t OFF_MOVE_ENERGY     = 11;
static constexpr uint8_t OFF_STILL_DIST_LOW  = 12;
static constexpr uint8_t OFF_STILL_DIST_HIGH = 13;
static constexpr uint8_t OFF_STILL_ENERGY    = 14;
static constexpr uint8_t OFF_DET_DIST_LOW    = 15;
static constexpr uint8_t OFF_DET_DIST_HIGH   = 16;

// Target state bitmasks
static constexpr uint8_t MOVE_BITMASK  = 0x01;
static constexpr uint8_t STILL_BITMASK = 0x02;

// Command bytes (for engineering mode toggle)
static constexpr uint8_t CMD_ENABLE_CONF  = 0xFF;
static constexpr uint8_t CMD_DISABLE_CONF = 0xFE;
static constexpr uint8_t CMD_ENABLE_ENG   = 0x62;
static constexpr uint8_t CMD_DISABLE_ENG  = 0x63;

// Buffer
static constexpr size_t MAX_FRAME_LEN = 64;
static uint8_t  buf[MAX_FRAME_LEN];
static size_t   bufPos = 0;

// Publish-rate limiting: only push MQTT when values change
static bool     lastHasTarget       = false;
static bool     lastHasMoving       = false;
static bool     lastHasStill        = false;
static uint16_t lastMovingDist      = 0xFFFF;
static uint8_t  lastMovingEnergy    = 0xFF;
static uint16_t lastStillDist       = 0xFFFF;
static uint8_t  lastStillEnergy     = 0xFF;
static uint16_t lastDetectionDist   = 0xFFFF;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool matchesHeader(const uint8_t* pattern, const uint8_t* data) {
    return memcmp(pattern, data, 4) == 0;
}

static uint16_t le16(uint8_t lo, uint8_t hi) {
    return (uint16_t)hi << 8 | lo;
}

// ---------------------------------------------------------------------------
// Send a config-mode command frame over UART (from ESPHome send_command_)
// ---------------------------------------------------------------------------
static void sendCommand(uint8_t cmd, const uint8_t* val = nullptr, uint8_t valLen = 0) {
    if (!initialized) return;
    uart_write_bytes(LD2410_UART, CMD_FRAME_HEADER, 4);
    uint8_t lenBuf[4] = {(uint8_t)(2 + valLen), 0x00, cmd, 0x00};
    uart_write_bytes(LD2410_UART, lenBuf, 4);
    if (val && valLen) uart_write_bytes(LD2410_UART, val, valLen);
    uart_write_bytes(LD2410_UART, CMD_FRAME_FOOTER, 4);
    if (cmd != CMD_ENABLE_CONF && cmd != CMD_DISABLE_CONF) delay(50);
}

static void setConfigMode(bool enable) {
    const uint8_t val[2] = {0x01, 0x00};
    sendCommand(enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF,
                enable ? val : nullptr,
                enable ? 2 : 0);
}

// ---------------------------------------------------------------------------
// Periodic data frame handler (ported from ESPHome handle_periodic_data_)
// ---------------------------------------------------------------------------
static void handlePeriodicData() {
    // Minimum sane frame: 4 header + 2 len + 1 type + 1 inner_hdr +
    //                     1 state + 6 distance/energy + ... + 1 inner_ftr +
    //                     1 crc + 4 outer_ftr = at least 12 bytes
    if (bufPos < 12) return;

    // Validate outer frame header/footer
    if (!matchesHeader(DATA_FRAME_HEADER, buf))            return;
    if (!matchesHeader(DATA_FRAME_FOOTER, buf + bufPos - 4)) return;

    // Validate inner header/footer/CRC
    if (buf[7]            != INNER_HEADER) return;
    if (buf[bufPos - 6]   != INNER_FOOTER) return;
    if (buf[bufPos - 5]   != INNER_CRC)    return;

    // Parse target state byte
    uint8_t targetState = buf[OFF_TARGET_STATES];
    bool newHasMoving   = (targetState & MOVE_BITMASK)  != 0;
    bool newHasStill    = (targetState & STILL_BITMASK) != 0;
    bool newHasTarget   = (targetState != 0x00);

    // Parse distances and energies
    uint16_t newMovingDist   = le16(buf[OFF_MOVE_DIST_LOW],  buf[OFF_MOVE_DIST_HIGH]);
    uint8_t  newMovingEnergy = buf[OFF_MOVE_ENERGY];
    uint16_t newStillDist    = le16(buf[OFF_STILL_DIST_LOW], buf[OFF_STILL_DIST_HIGH]);
    uint8_t  newStillEnergy  = buf[OFF_STILL_ENERGY];
    uint16_t newDetDist      = le16(buf[OFF_DET_DIST_LOW],   buf[OFF_DET_DIST_HIGH]);

    // Update shared state
    hasTarget        = newHasTarget;
    hasMovingTarget  = newHasMoving;
    hasStillTarget   = newHasStill;
    movingDistance   = newMovingDist;
    movingEnergy     = newMovingEnergy;
    stillDistance    = newStillDist;
    stillEnergy      = newStillEnergy;
    detectionDistance = newDetDist;

    // --- Publish only on change ---
    String base = roomsTopic + "/";

    if (newHasTarget != lastHasTarget) {
        lastHasTarget = newHasTarget;
        pub((base + "radar").c_str(), 0, 1, newHasTarget ? "ON" : "OFF");
    }
    if (newHasMoving != lastHasMoving) {
        lastHasMoving = newHasMoving;
        pub((base + "radar_moving").c_str(), 0, 1, newHasMoving ? "ON" : "OFF");
    }
    if (newHasStill != lastHasStill) {
        lastHasStill = newHasStill;
        pub((base + "radar_still").c_str(), 0, 1, newHasStill ? "ON" : "OFF");
    }
    if (newMovingDist != lastMovingDist) {
        lastMovingDist = newMovingDist;
        pub((base + "radar_moving_distance").c_str(), 0, 1,
            String(newMovingDist).c_str());
    }
    if (newMovingEnergy != lastMovingEnergy) {
        lastMovingEnergy = newMovingEnergy;
        pub((base + "radar_moving_energy").c_str(), 0, 1,
            String(newMovingEnergy).c_str());
    }
    if (newStillDist != lastStillDist) {
        lastStillDist = newStillDist;
        pub((base + "radar_still_distance").c_str(), 0, 1,
            String(newStillDist).c_str());
    }
    if (newStillEnergy != lastStillEnergy) {
        lastStillEnergy = newStillEnergy;
        pub((base + "radar_still_energy").c_str(), 0, 1,
            String(newStillEnergy).c_str());
    }
    if (newDetDist != lastDetectionDist) {
        lastDetectionDist = newDetDist;
        pub((base + "radar_distance").c_str(), 0, 1,
            String(newDetDist).c_str());
    }
}

// ---------------------------------------------------------------------------
// Byte-by-byte frame accumulator (ported from ESPHome readline_)
// ---------------------------------------------------------------------------
static void readline(uint8_t b) {
    if (bufPos >= MAX_FRAME_LEN - 1) {
        bufPos = 0; // overflow guard
        return;
    }
    buf[bufPos++] = b;

    // Need at least 4 bytes to check a footer
    if (bufPos < 4) return;

    // Check for data frame footer at current tail
    if (matchesHeader(DATA_FRAME_FOOTER, buf + bufPos - 4)) {
        handlePeriodicData();
        bufPos = 0;
        return;
    }
    // Check for command ACK footer (we don't process ACKs in v1, just flush)
    if (matchesHeader(CMD_FRAME_FOOTER, buf + bufPos - 4)) {
        bufPos = 0;
    }
}

// ---------------------------------------------------------------------------
// Standard ESPresense sensor module interface
// ---------------------------------------------------------------------------

void ConnectToWifi() {
    // Register settings — appear on Hardware page in web UI
    rxPin = HeadlessWiFiSettings.integer("LD2410_RX_pin", 44,
        "LD2410 UART RX pin (-1 to disable)");
    txPin = HeadlessWiFiSettings.integer("LD2410_TX_pin", 43,
        "LD2410 UART TX pin (-1 to disable)");
}

void Setup() {
    if (rxPin < 0 || txPin < 0) {
        initialized = false;
        return;
    }

    // Use ESP-IDF UART driver directly — bypasses Arduino HardwareSerial
    // which has issues with GPIO43/44 on ESP32-S3 CDC builds.
    // First detach UART0 from these pins if it's claiming them
    uart_driver_delete(UART_NUM_0);
    gpio_reset_pin((gpio_num_t)rxPin);
    gpio_reset_pin((gpio_num_t)txPin);

    uart_config_t uart_config = {
        .baud_rate = 256000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(LD2410_UART, &uart_config);
    uart_set_pin(LD2410_UART, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(LD2410_UART, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    delay(100); // let the sensor settle after power-up

    // Ensure normal (non-engineering) mode so the basic data frame streams
    setConfigMode(true);
    sendCommand(CMD_DISABLE_ENG);
    setConfigMode(false);

    initialized = true;
    Serial.printf("LD2410Uart: ESP-IDF UART1 RX=%d TX=%d @ 256000 baud\n", rxPin, txPin);
}

void SerialReport() {
    if (!initialized) {
        Serial.println("LD2410Uart: disabled (no UART pins configured)");
        return;
    }
    Serial.printf("LD2410Uart: RX=%d TX=%d — target=%s moving=%s still=%s\n",
        rxPin, txPin,
        hasTarget       ? "ON" : "OFF",
        hasMovingTarget ? "ON" : "OFF",
        hasStillTarget  ? "ON" : "OFF");
}

void Loop() {
    if (!initialized) return;

    // Read from ESP-IDF UART driver
    uint8_t rxBuf[128];
    int len = uart_read_bytes(LD2410_UART, rxBuf, sizeof(rxBuf), 0);

    // Debug: log byte count every 5 seconds
    static unsigned long lastDebug = 0;
    static unsigned long totalBytes = 0;
    if (len > 0) totalBytes += len;
    if (millis() - lastDebug > 5000) {
        Serial.printf("LD2410Uart: %lu bytes received in last 5s\n", totalBytes);
        totalBytes = 0;
        lastDebug = millis();
    }

    for (int i = 0; i < len; i++) {
        readline(rxBuf[i]);
    }
}

bool SendDiscovery() {
    if (!initialized) return true;

    // Names must slugify to match our MQTT topics
    // e.g. "Radar" -> slug "radar" -> state topic "~/radar"
    bool ok = sendBinarySensorDiscovery("Radar", EC_NONE, "motion")
           && sendBinarySensorDiscovery("Radar Moving", EC_NONE, "motion")
           && sendBinarySensorDiscovery("Radar Still", EC_NONE, "motion");

    ok = ok
      && sendSensorDiscovery("Radar Moving Distance", EC_NONE, "distance", "cm")
      && sendSensorDiscovery("Radar Moving Energy", EC_NONE)
      && sendSensorDiscovery("Radar Still Distance", EC_NONE, "distance", "cm")
      && sendSensorDiscovery("Radar Still Energy", EC_NONE)
      && sendSensorDiscovery("Radar Distance", EC_NONE, "distance", "cm");

    return ok;
}

} // namespace LD2410Uart

#endif // SENSORS
