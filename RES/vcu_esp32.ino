#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define SDC_PIN 26
#define TIMEOUT_MS 300

typedef struct {
    bool estop;
    uint8_t counter;
} Message;

unsigned long lastPacketTime = 0;
bool shutdownOpen = false;
bool firstPacketReceived = false;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    Message msg;
    memcpy(&msg, data, sizeof(msg));

    lastPacketTime = millis();
    firstPacketReceived = true;

    if (msg.estop) {
        digitalWrite(SDC_PIN, LOW);
        shutdownOpen = true;
        Serial.println("E-STOP received! Shutdown opened.");
    } else {
        digitalWrite(SDC_PIN, HIGH);
        shutdownOpen = false;
        Serial.println("System ARMED. Shutdown closed.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(SDC_PIN, OUTPUT);
    digitalWrite(SDC_PIN, LOW); // safe state on boot

    WiFi.mode(WIFI_STA);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        while (1) {
            digitalWrite(SDC_PIN, LOW);
            delay(1000);
        }
    }

    esp_now_register_recv_cb(onReceive);

    Serial.println("VCU ready, waiting for remote...");
    lastPacketTime = millis();

    Serial.println("Waiting for fi
