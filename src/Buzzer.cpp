#ifdef SENSORS
#include "Buzzer.h"
#include "globals.h"
#include "defaults.h"
#include <HeadlessWiFiSettings.h>
#include "mqtt.h"
#include <Arduino.h>
#include <ArduinoJson.h>

namespace Buzzer {

bool playing = false;
static int buzzerPin = -1;

static const uint16_t NOTE_FREQ_O4[] = {
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

// Built-in RTTTL melodies
static const char* MELODY_CHIRP     = "chirp:d=16,o=6,b=200:c,p,c";
static const char* MELODY_DESCEND   = "descend:d=8,o=5,b=180:g,e,c";
static const char* MELODY_WELCOME   = "welcome:d=8,o=5,b=160:c,e,g,2c6";
static const char* MELODY_GOODBYE   = "goodbye:d=8,o=5,b=160:g,e,c,2c4";
static const char* MELODY_ALERT     = "alert:d=4,o=6,b=200:e,p,e,p,e,p,e";
static const char* MELODY_SUCCESS   = "success:d=8,o=5,b=180:c,e,g,c6,g,2c6";
static const char* MELODY_ERROR     = "error:d=4,o=4,b=120:b,p,b,2p,b";
static const char* MELODY_DOORBELL  = "doorbell:d=4,o=5,b=100:e,c";
static const char* MELODY_BEEP      = "beep:d=16,o=6,b=200:c";

static char rtttlBuf[256];
static const char* parsePtr;
static int defaultDuration;
static int defaultOctave;
static int bpm;
static unsigned long wholeNoteMs;
static unsigned long noteEndMs = 0;
static bool inGap = false;
static const unsigned long GAP_MS = 20;

static int parseNumber(const char*& p) {
    int num = 0;
    while (*p >= '0' && *p <= '9') {
        num = num * 10 + (*p - '0');
        p++;
    }
    return num;
}

static void skipSep(const char*& p) {
    while (*p == ' ' || *p == ',') p++;
}

static uint16_t noteFrequency(int noteIdx, int octave) {
    uint16_t freq = NOTE_FREQ_O4[noteIdx];
    if (octave > 4) freq <<= (octave - 4);
    else if (octave < 4) freq >>= (4 - octave);
    return freq;
}

static bool parseHeader() {
    const char* p = rtttlBuf;
    while (*p && *p != ':') p++;
    if (!*p) return false;
    p++;
    defaultDuration = 4;
    defaultOctave = 5;
    bpm = 120;
    while (*p && *p != ':') {
        while (*p == ' ' || *p == ',') p++;
        if (*p == 'd' && *(p+1) == '=') { p += 2; defaultDuration = parseNumber(p); if (defaultDuration == 0) defaultDuration = 4; }
        else if (*p == 'o' && *(p+1) == '=') { p += 2; defaultOctave = parseNumber(p); if (defaultOctave < 4 || defaultOctave > 7) defaultOctave = 5; }
        else if (*p == 'b' && *(p+1) == '=') { p += 2; bpm = parseNumber(p); if (bpm == 0) bpm = 120; }
        else p++;
    }
    if (*p == ':') p++;
    parsePtr = p;
    wholeNoteMs = (unsigned long)(60000UL * 4 / bpm);
    return true;
}

static bool playNextNote() {
    if (!parsePtr || !*parsePtr) return false;
    skipSep(parsePtr);
    if (!*parsePtr) return false;
    const char* p = parsePtr;
    int duration = parseNumber(p);
    if (duration == 0) duration = defaultDuration;
    int noteIdx = -1;
    switch (tolower(*p)) {
        case 'c': noteIdx = 0; break;
        case 'd': noteIdx = 2; break;
        case 'e': noteIdx = 4; break;
        case 'f': noteIdx = 5; break;
        case 'g': noteIdx = 7; break;
        case 'a': noteIdx = 9; break;
        case 'b': noteIdx = 11; break;
        case 'p': noteIdx = -1; break;
        default: parsePtr = nullptr; return false;
    }
    p++;
    if (*p == '#') { if (noteIdx >= 0) noteIdx++; p++; }
    bool dotted = false;
    if (*p == '.') { dotted = true; p++; }
    int octave = defaultOctave;
    if (*p >= '4' && *p <= '7') { octave = *p - '0'; p++; }
    if (*p == '.') { dotted = true; p++; }
    skipSep(p);
    parsePtr = p;
    unsigned long durationMs = wholeNoteMs / duration;
    if (dotted) durationMs = durationMs * 3 / 2;
    if (noteIdx >= 0 && buzzerPin >= 0) {
        tone(buzzerPin, noteFrequency(noteIdx, octave), durationMs);
    } else {
        noTone(buzzerPin);
    }
    noteEndMs = millis() + durationMs;
    inGap = false;
    return true;
}

static const char* findBuiltIn(const char* name) {
    if (strcasecmp(name, "chirp") == 0)    return MELODY_CHIRP;
    if (strcasecmp(name, "descend") == 0)  return MELODY_DESCEND;
    if (strcasecmp(name, "welcome") == 0)  return MELODY_WELCOME;
    if (strcasecmp(name, "goodbye") == 0)  return MELODY_GOODBYE;
    if (strcasecmp(name, "alert") == 0)    return MELODY_ALERT;
    if (strcasecmp(name, "success") == 0)  return MELODY_SUCCESS;
    if (strcasecmp(name, "error") == 0)    return MELODY_ERROR;
    if (strcasecmp(name, "doorbell") == 0) return MELODY_DOORBELL;
    if (strcasecmp(name, "beep") == 0)     return MELODY_BEEP;
    return nullptr;
}

void Play(const char* rtttl) {
    if (buzzerPin < 0 || !rtttl || !*rtttl) return;
    if (strchr(rtttl, ':') == nullptr) {
        const char* builtIn = findBuiltIn(rtttl);
        if (builtIn) rtttl = builtIn;
        else return;
    }
    strncpy(rtttlBuf, rtttl, sizeof(rtttlBuf) - 1);
    rtttlBuf[sizeof(rtttlBuf) - 1] = '\0';
    if (parseHeader()) {
        playing = true;
        playNextNote();
    }
}

void Stop() {
    if (buzzerPin >= 0) noTone(buzzerPin);
    playing = false;
    parsePtr = nullptr;
}

void ConnectToWifi() {
    buzzerPin = HeadlessWiFiSettings.integer("buzzer_pin", 13, "Buzzer pin (-1 to disable)");
}

void Setup() {
    if (buzzerPin < 0) return;
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
    Serial.printf("Buzzer: pin=%d ready\n", buzzerPin);
}

void SerialReport() {
    if (buzzerPin < 0) { Serial.println("Buzzer: disabled"); return; }
    Serial.printf("Buzzer: pin=%d playing=%s\n", buzzerPin, playing ? "yes" : "no");
}

void Loop() {
    if (!playing || buzzerPin < 0) return;
    if (millis() < noteEndMs) return;
    if (!inGap) {
        noTone(buzzerPin);
        noteEndMs = millis() + GAP_MS;
        inGap = true;
        return;
    }
    if (!playNextNote()) Stop();
}

bool SendDiscovery() {
    if (buzzerPin < 0) return true;
    return true;
}

bool Command(String& command, String& pay) {
    if (command != "buzzer") return false;
    if (buzzerPin < 0) return true;
    DynamicJsonDocument doc(pay.length() + 100);
    auto err = deserializeJson(doc, pay);
    if (err) {
        Play(pay.c_str());
        return true;
    }
    if (doc.containsKey("stop")) Stop();
    else if (doc.containsKey("melody")) Play(doc["melody"].as<const char*>());
    else if (doc.containsKey("play")) Play(doc["play"].as<const char*>());
    return true;
}

} // namespace Buzzer
#endif
