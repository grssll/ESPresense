#pragma once
#include <cstdint>
#ifdef SENSORS

namespace LD2410Uart {

    // -------------------------------------------------------------------------
    // Live state — readable by other modules (e.g. Occupancy)
    // -------------------------------------------------------------------------
    extern bool initialized;
    extern bool hasTarget;           // any presence (moving OR still)
    extern bool hasMovingTarget;     // moving target only
    extern bool hasStillTarget;      // still / stationary target only
    extern uint16_t movingDistance;  // cm
    extern uint8_t  movingEnergy;    // 0-100
    extern uint16_t stillDistance;   // cm
    extern uint8_t  stillEnergy;     // 0-100
    extern uint16_t detectionDistance; // cm (nearest of moving/still)

    // -------------------------------------------------------------------------
    // Standard ESPresense sensor module interface
    // -------------------------------------------------------------------------
    void ConnectToWifi();
    void Setup();
    void SerialReport();
    void Loop();
    bool SendDiscovery();

} // namespace LD2410Uart

#endif // SENSORS
