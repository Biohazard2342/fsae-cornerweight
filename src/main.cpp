// FSAE Corner Weight - BLE Scanner (Serial JSON output for RPi bridge)
//
// Hardware: Arduino Nano ESP32 (ESP32-S3), Daiso ZH-S1402-V02 BLE scales.
// Output format (one JSON object per line @ 115200 baud):
//   {"corner":"FL","weight_kg":0.0,"mac":"...","rssi":-58,"raw":"<hex>","ts_ms":1234}
//
// weight_kg stays 0.0 until we map the weight bytes from the captured raw payload.
// The serial_bridge.py on the Pi reads these lines and forwards them to
// ws://127.0.0.1:8000/ws/ingest unchanged.
//
// Toggle DUMP_ALL=1 to print every advertiser in human-readable form instead —
// use this to discover the other 3 scale MACs (the bridge will skip non-JSON lines).

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

#define DUMP_ALL 0

struct KnownScale {
    const char* mac;
    const char* corner;
};

static const KnownScale kScales[] = {
    {"98:f6:7a:51:5b:00", "FL"},  // first real scale — corner is a placeholder, reassign later
};
static constexpr size_t kScaleCount = sizeof(kScales) / sizeof(kScales[0]);

static const KnownScale* matchScale(const std::string& mac) {
    for (size_t i = 0; i < kScaleCount; ++i) {
        if (mac == kScales[i].mac) return &kScales[i];
    }
    return nullptr;
}

static void appendHex(String& out, const uint8_t* data, size_t len) {
    // Note: can't name this HEX — Arduino Print.h has `#define HEX 16`.
    static const char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out += kHexDigits[data[i] >> 4];
        out += kHexDigits[data[i] & 0x0F];
    }
}

#if DUMP_ALL
static void humanDump(const NimBLEAdvertisedDevice* dev) {
    auto payload = dev->getPayload();
    String hex; hex.reserve(payload.size() * 2);
    appendHex(hex, payload.data(), payload.size());
    Serial.print(millis()); Serial.print(" | ");
    Serial.print(dev->getAddress().toString().c_str());
    Serial.print(" | rssi="); Serial.print(dev->getRSSI());
    Serial.print(" | raw["); Serial.print(payload.size()); Serial.print("]=");
    Serial.println(hex);
}
#endif

class ScanCB : public NimBLEScanCallbacks {
    // Per-corner throttle: scales advertise every 4ms, but we only need ~10Hz
    // for the UI and reverse-engineering, and 115200 baud can't keep up otherwise.
    uint32_t lastByCornerMs_[kScaleCount] = {0};
    static constexpr uint32_t kThrottleMs = 100;

    void onResult(const NimBLEAdvertisedDevice* dev) override {
#if DUMP_ALL
        humanDump(dev);
        return;
#else
        std::string mac = dev->getAddress().toString();
        const KnownScale* known = matchScale(mac);
        if (!known) return;

        size_t idx = (size_t)(known - kScales);
        uint32_t now = millis();
        if (now - lastByCornerMs_[idx] < kThrottleMs) return;
        lastByCornerMs_[idx] = now;

        auto payload = dev->getPayload();
        String raw; raw.reserve(payload.size() * 2);
        appendHex(raw, payload.data(), payload.size());

        JsonDocument doc;
        doc["corner"] = known->corner;
        // bytes 4-5 = weight × 100 (BE 16-bit, units of 0.01 kg).
        // Confirmed 2026-05-21 with a 3kg dumbbell: 0x0136 = 310 → 3.10 kg.
        if (payload.size() >= 6) {
            uint16_t w_raw = (uint16_t(payload[4]) << 8) | uint16_t(payload[5]);
            doc["weight_kg"] = w_raw / 100.0f;
        }
        doc["mac"]    = mac.c_str();
        doc["rssi"]   = dev->getRSSI();
        doc["raw"]    = raw;
        doc["ts_ms"]  = now;

        serializeJson(doc, Serial);
        Serial.println();
#endif
    }
};

static ScanCB g_scanCB;

void setup() {
    Serial.begin(115200);
    delay(500);
#if DUMP_ALL
    Serial.println("# fsae-scanner DUMP_ALL mode (human-readable, all devices)");
#endif

    NimBLEDevice::init("fsae-scanner");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&g_scanCB, /*wantDuplicates=*/true);
    scan->setActiveScan(true);
    scan->setInterval(45);   // 0.625ms units → ~28ms
    scan->setWindow(45);
    scan->setMaxResults(0);
    scan->start(0, false);
}

void loop() {
    // BLE scan runs in its own NimBLE task; nothing to do here.
    delay(1000);
}
