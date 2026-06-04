#pragma once
#include <Arduino.h>

namespace Motion {

    // Exposed for Occupancy module
    extern bool pirState;
    extern bool radarGpioState;

    // Full upstream interface (all methods main.cpp expects)
    void ConnectToWifi(bool updating);
    void Setup();
    void SerialReport();
    void Loop();
    bool SendDiscovery();
    bool SendOnline();
    bool Command(String& command, String& payload);

}
