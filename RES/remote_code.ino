#include <esp_now.h>
#include <WiFi.h>

#define BUTTON_PIN 21
#define LED_GREEN  25
#define LED_RED    26
#define LED_BLUE   27

// TODO: VCU ESP32 MAC address , use MAC finder code from the folder to get MAC address of ESP32 on VCU
uint8_t vcu_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
    bool estop;
    uint8_t counter; // rolling counter to detect packet loss
} Message;

Message msg;
esp_now_peer_info_t peerInfo;

void onSent(const uint8_t *mac, esp_now_send_status_t status) {
    lastSendSuccess = (status == ESP_NOW_SEND_SUCCESS);
    
    if (lastSendSuccess) {
        // Blue blink = packet delivered successfully
        digitalWrite(LED_BLUE, HIGH);
    } else {
        // Blue off = delivery failed, VCU not receiving
        digitalWrite(LED_BLUE, LOW);
    }
}

void setup() {
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    
    // Boot state - system armed
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    
    WiFi.mode(WIFI_STA);
    esp_now_init();
    esp_now_register_send_cb(onSent);
    
    memcpy(peerInfo.peer_addr, vcu_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    
    msg.counter = 0;
    msg.estop = false;
}

void loop() {
    msg.estop = digitalRead(BUTTON_PIN);
    msg.counter++;
    
    // Update LEDs
    if (msg.estop) {
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, HIGH); // green = estop active
    } else {
        digitalWrite(LED_RED, HIGH);  // red = armed/running
        digitalWrite(LED_GREEN, LOW);
    }
    
    esp_now_send(vcu_mac, (uint8_t*)&msg, sizeof(msg));
    delay(100); // send every 100ms
}
