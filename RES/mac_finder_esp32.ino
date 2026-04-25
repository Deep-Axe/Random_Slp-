#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(1000); 
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100); 
    
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
}

void loop() {}
