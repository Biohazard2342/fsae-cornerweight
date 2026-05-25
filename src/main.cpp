// FSAE Corner Weight - HX711 reader (per-corner ESP32)
//
// One ESP32 + one HX711 + 4 strain-gauge load cells (Wheatstone bridge from
// the salvaged Daiso scale chassis) = one corner module.
//
// Output: one JSON line per sample @ ~10 Hz, e.g.
//   {"corner":"FL","weight_kg":12.34,"raw":3210000,"ts_ms":54321}
// — exact same shape the existing RPi serial_bridge / FastAPI server expects,
// so no changes on the Pi side.
//
// Serial commands (115200 baud, newline-terminated):
//   tare           — zero the scale at current load, persist offset
//   cal <kg>       — with <kg> known weight on the scale, set scale factor
//   corner <FL|FR|RL|RR>  — set this board's corner ID, persist
//   info           — print calibration / raw value
//
// Calibration + tare + corner are stored in NVS (Preferences) so they survive
// reboots. Until the first `cal` is run, weight_kg is omitted from output and
// the UI shows "--".

#include <Arduino.h>
#include <HX711.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>

// HX711 wiring (ESP32-S3 GPIO; change here if you route differently)
static constexpr int kDoutPin = 4;   // HX711 DOUT
static constexpr int kSckPin  = 5;   // HX711 PD_SCK

static constexpr uint32_t kSamplePeriodMs = 100;   // 10 Hz output
static constexpr int      kSampleAvg      = 3;     // running average per sample
static constexpr int      kTareAvg        = 20;    // averaging on tare
static constexpr int      kCalAvg         = 20;    // averaging on calibration read

HX711 scale;
Preferences prefs;

static float    g_calFactor   = NAN;     // raw counts per kg; NAN = uncalibrated
static long     g_tareOffset  = 0;
static String   g_corner      = "FL";
static uint32_t g_lastSampleMs = 0;
static String   g_cmdBuf;


static void persist() {
    prefs.begin("scale", false);
    if (!isnan(g_calFactor)) prefs.putFloat("cal", g_calFactor);
    prefs.putLong("tare", g_tareOffset);
    prefs.putString("corner", g_corner);
    prefs.end();
}

static void loadFromNvs() {
    prefs.begin("scale", true);
    g_calFactor  = prefs.getFloat("cal", NAN);
    g_tareOffset = prefs.getLong("tare", 0);
    g_corner     = prefs.getString("corner", "FL");
    prefs.end();

    scale.set_offset(g_tareOffset);
    if (!isnan(g_calFactor)) scale.set_scale(g_calFactor);

    Serial.printf("# loaded corner=%s tare=%ld cal=%.4f\n",
                  g_corner.c_str(), g_tareOffset,
                  isnan(g_calFactor) ? 0.0f : g_calFactor);
}

static void cmdTare() {
    if (!scale.wait_ready_timeout(1000)) {
        Serial.println("# tare failed: HX711 not ready");
        return;
    }
    scale.tare(kTareAvg);
    g_tareOffset = scale.get_offset();
    persist();
    Serial.printf("# tare ok, offset=%ld\n", g_tareOffset);
}

static void cmdCalibrate(float knownKg) {
    if (knownKg <= 0.001f) {
        Serial.println("# cal failed: need positive weight, e.g. `cal 3.10`");
        return;
    }
    if (!scale.wait_ready_timeout(1000)) {
        Serial.println("# cal failed: HX711 not ready");
        return;
    }
    long avgRaw = scale.get_value(kCalAvg);   // tare-corrected raw
    g_calFactor = avgRaw / knownKg;           // counts per kg
    scale.set_scale(g_calFactor);
    persist();
    Serial.printf("# cal ok, factor=%.4f (avgRaw=%ld for %.2fkg)\n",
                  g_calFactor, avgRaw, knownKg);
}

static void cmdSetCorner(String c) {
    c.trim();
    c.toUpperCase();
    if (c != "FL" && c != "FR" && c != "RL" && c != "RR") {
        Serial.println("# corner must be one of: FL FR RL RR");
        return;
    }
    g_corner = c;
    persist();
    Serial.printf("# corner set: %s\n", g_corner.c_str());
}

static void cmdInfo() {
    long raw = scale.is_ready() ? scale.read() : 0;
    Serial.printf("# corner=%s tare=%ld cal=%.4f raw=%ld%s\n",
                  g_corner.c_str(), g_tareOffset,
                  isnan(g_calFactor) ? 0.0f : g_calFactor,
                  raw, scale.is_ready() ? "" : " (HX711 NOT READY)");
}

static void processCommand(const String& line) {
    if (line == "tare") {
        cmdTare();
    } else if (line.startsWith("cal ")) {
        cmdCalibrate(line.substring(4).toFloat());
    } else if (line.startsWith("corner ")) {
        cmdSetCorner(line.substring(7));
    } else if (line == "info") {
        cmdInfo();
    } else if (line == "help" || line == "?") {
        Serial.println("# commands: tare | cal <kg> | corner <FL|FR|RL|RR> | info");
    } else {
        Serial.printf("# unknown: '%s' (try `help`)\n", line.c_str());
    }
}

static void readSerialCommands() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (g_cmdBuf.length() > 0) {
                processCommand(g_cmdBuf);
                g_cmdBuf = "";
            }
        } else if (g_cmdBuf.length() < 128) {
            g_cmdBuf += c;
        }
    }
}

static void emitSample() {
    if (millis() - g_lastSampleMs < kSamplePeriodMs) return;
    if (!scale.is_ready()) return;
    g_lastSampleMs = millis();

    long raw = scale.read_average(kSampleAvg);

    JsonDocument doc;
    doc["corner"] = g_corner;
    if (!isnan(g_calFactor) && g_calFactor != 0) {
        float w = (raw - g_tareOffset) / g_calFactor;
        doc["weight_kg"] = w;
    }
    doc["raw"]   = raw;
    doc["ts_ms"] = (uint32_t)millis();

    serializeJson(doc, Serial);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("# FSAE corner-weight HX711 firmware");
    Serial.println("# commands: tare | cal <kg> | corner <FL|FR|RL|RR> | info");

    scale.begin(kDoutPin, kSckPin);
    delay(100);  // settle
    loadFromNvs();
}

void loop() {
    readSerialCommands();
    emitSample();
}
