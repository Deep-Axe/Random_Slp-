#include <Servo.h>

Servo steerServo;
Servo brakeServo;

int steerPos = 135;
int brakePos = 0;

void setup() {
    Serial.begin(115200);
    steerServo.attach(9);   // steer on pin 9
    brakeServo.attach(10);  // brake on pin 10
    steerServo.write(135);  // center
    brakeServo.write(0);    // released
}

void loop() {
    if (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        line.trim();

        // Expect "S:135,B:0"
        int sIdx = line.indexOf("S:");
        int bIdx = line.indexOf(",B:");

        if (sIdx >= 0 && bIdx > sIdx) {
            String sVal = line.substring(sIdx + 2, bIdx);
            String bVal = line.substring(bIdx + 3);

            steerPos = constrain(sVal.toInt(), 0, 270);
            brakePos = constrain(bVal.toInt(), 0, 180);

            steerServo.write(steerPos);
            brakeServo.write(brakePos);

            Serial.print("S:"); Serial.print(steerPos);
            Serial.print(" B:"); Serial.println(brakePos);
        }
        // flush buffer
        while (Serial.available()) Serial.read();
    }
    steerServo.write(steerPos);
    brakeServo.write(brakePos);
}
