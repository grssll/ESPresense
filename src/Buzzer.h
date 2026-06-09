#pragma once
#ifdef SENSORS
#include <Arduino.h>

namespace Buzzer {

    extern bool playing;

    void ConnectToWifi();
    void Setup();
    void SerialReport();
    void Loop();
    bool SendDiscovery();
    bool Command(String& command, String& payload);

    // Call from other modules (e.g. Occupancy)
    void Play(const char* rtttl);
    void Stop();

} // namespace Buzzer

#endif // SENSORS
