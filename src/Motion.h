#pragma once
// =============================================================================
// Motion.h
// ESPresense fork — PIR + GPIO radar
//
// CHANGES FROM UPSTREAM:
//   • pirState and radarGpioState declared extern so Occupancy.cpp can read them
//   • All other interface unchanged
// =============================================================================

namespace Motion {

    // -------------------------------------------------------------------------
    // Live state — exposed for Occupancy module
    // -------------------------------------------------------------------------
    extern bool pirState;         // true while PIR reports motion
    extern bool radarGpioState;   // true while GPIO radar pin reports presence
                                  // (if GPIO radar is also wired; independent of
                                  //  LD2410Uart UART radar)

    // -------------------------------------------------------------------------
    // Standard ESPresense interface
    // -------------------------------------------------------------------------
    void ConnectToWifi();
    void Setup();
    void SerialReport();
    void Loop();
    bool SendDiscovery();

} // namespace Motion
