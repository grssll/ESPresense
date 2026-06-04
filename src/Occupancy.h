#pragma once
#ifdef SENSORS

namespace Occupancy {

    // Current occupancy state — true when PIR or LD2410 has detected presence
    extern bool occupied;

    // Standard ESPresense sensor module interface
    void ConnectToWifi();
    void Setup();
    void SerialReport();
    void Loop();
    bool SendDiscovery();

} // namespace Occupancy

#endif // SENSORS
