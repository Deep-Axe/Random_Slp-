#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define BUTTON_PIN 21
#define LED_GREEN  25
#define LED_RED    26
#define LED_BLUE   27

uint8_t vcu_mac[] = {0x90, 0x15, 0x06, 0x94, 0xBA, 0xFC};

typedef struct {
    bool estop;
    uint8_t counter;
} Message;

Message msg;
esp_now_peer_info_t peerInfo;
bool lastSendSuccess = false;
bool estopLatched = false;
bool lastStableButtonState = false;
bool lastRawButtonState = false;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressTime = 0;
#define DEBOUNCE_MS 150      
#define MIN_PRESS_MS 100       // button must be held for at least 100ms

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    lastSendSuccess = (status == ESP_NOW_SEND_SUCCESS);
    if (lastSendSuccess) {
        digitalWrite(LED_BLUE, HIGH);
    } else {
        digitalWrite(LED_BLUE, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    digitalWrite(LED_RED,   HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE,  LOW);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);

    if (esp_now_init() != ESP_OK) {
        while (1) {
            digitalWrite(LED_RED,   HIGH);
            digitalWrite(LED_GREEN, HIGH);
            digitalWrite(LED_BLUE,  HIGH);
            delay(200);
            digitalWrite(LED_RED,   LOW);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(LED_BLUE,  LOW);
            delay(200);
        }
    }

    esp_now_register_send_cb(onSent);

    memcpy(peerInfo.peer_addr, vcu_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        return;
    }

    msg.counter = 0;
    msg.estop = false;
}

void loop() {
    bool rawButton = digitalRead(BUTTON_PIN);

    // Restart debounce timer on any raw change
    if (rawButton != lastRawButtonState) {
        lastDebounceTime = millis();
        lastRawButtonState = rawButton;
    }

    // Only process after debounce window settles
    if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {

        // Rising edge — button just pressed
        if (rawButton == HIGH && lastStableButtonState == LOW) {
            buttonPressTime = millis(); // record when press started
        }

        // Falling edge — button just released
        if (rawButton == LOW && lastStableButtonState == HIGH) {
            // Only count as valid press if held long enough
            if (millis() - buttonPressTime >= MIN_PRESS_MS) {
                estopLatched = !estopLatched;
            }
        }

        lastStableButtonState = rawButton;
    }

    // Update message
    msg.estop = estopLatched;
    msg.counter++;

    // Update LEDs
    if (estopLatched) {
        digitalWrite(LED_RED,   LOW);
        digitalWrite(LED_GREEN, HIGH);
    } else {
        digitalWrite(LED_RED,   HIGH);
        digitalWrite(LED_GREEN, LOW);
    }

    // Send packet
    esp_err_t result = esp_now_send(vcu_mac, (uint8_t*)&msg, sizeof(msg));
    if (result != ESP_OK) {
        digitalWrite(LED_BLUE, !digitalRead(LED_BLUE));
    }

    delay(80);
    digitalWrite(LED_BLUE, LOW);
    delay(20);
}
