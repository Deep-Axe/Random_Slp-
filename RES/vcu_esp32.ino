#include <esp_now.h>
#include <WiFi.h>

#define SDC_PIN 26
#define TIMEOUT_MS 300 // open shutdown if no packet for 300ms

typedef struct {
    bool estop;
    uint8_t counter;
} Message;

unsigned long lastPacketTime = 0;
bool shutdownOpen = false;

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    Message msg;
    memcpy(&msg, data, sizeof(msg));
    
    lastPacketTime = millis();
    
    if (msg.estop) {
        // E-Stop pressed - open shutdown
        digitalWrite(SDC_PIN, LOW);
        shutdownOpen = true;
    } else {
        // Normal operation - keep shutdown closed
        digitalWrite(SDC_PIN, HIGH);
        shutdownOpen = false;
    }
}

void setup() {
    pinMode(SDC_PIN, OUTPUT);
    digitalWrite(SDC_PIN, LOW); // safe state on boot
    
    WiFi.mode(WIFI_STA);
    esp_now_init();
    esp_now_register_recv_cb(onReceive);
    
    // Wait for first packet before enabling shutdown
    while(millis() - lastPacketTime > TIMEOUT_MS) {
        delay(10);
    }
    digitalWrite(SDC_PIN, HIGH); // arm shutdown line
}

void loop() {
    // Watchdog - if no packet received open shutdown
    if (millis() - lastPacketTime > TIMEOUT_MS) {
        digitalWrite(SDC_PIN, LOW);
        shutdownOpen = true;
    }
    delay(10);
}
