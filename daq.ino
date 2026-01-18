#include <FlexCAN_T4.h>
#include <SD.h>
#include <TimeLib.h>
#include "CanFrames.h"
#include <USBHost_t36.h>

// Define SD card CS pin
const int sdChipSelect = BUILTIN_SDCARD;

// Create a FlexCAN object for CAN2 on Teensy 4.1
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can2;
CanFrames::Parser parser;

// USB Host for VN-200
USBHost myusb;
USBSerial vn200Serial(myusb);

File dataFile;

// ======================= DEBUG PRINT =======================
void printSnapshot(const CanFrames::Snapshot &s)
{
    Serial.printf(
        "T1:%u | A:%.1f B:%.1f C:%.1f Gate:%.1f | ",
        s.temperatures1Timestamp,
        s.moduleATemp,
        s.moduleBTemp,
        s.moduleCTemp,
        s.gateDriverTemp);

    Serial.printf(
        "Ctrl:%.1f RTD1:%.1f RTD2:%.1f RTD3:%.1f | ",
        s.controlBoardTemp,
        s.rtd1Temp,
        s.rtd2Temp,
        s.rtd3StallBurstTemp);

    Serial.printf(
        "MotorAngle:%.1f Speed:%.1f TorqueCmd:%.1f TorqueFB:%.1f\n",
        s.motorAngle,
        s.motorSpeed,
        s.commandedTorque,
        s.torqueFeedback);
}

// ======================= DATA STRUCT =======================
struct __attribute__((packed)) CANData
{
    uint32_t timestamp;

    float moduleATemp, moduleBTemp, moduleCTemp, gateDriverTemp;
    uint32_t temperatures1Timestamp;

    float controlBoardTemp, rtd1Temp, rtd2Temp, stallBurstTemp;
    uint32_t temperatures2Timestamp;

    float motorAngle, motorSpeed, electricalFrequency, deltaResolverFiltered;
    uint32_t motorPositionTimestamp;

    float phaseACurrent, phaseBCurrent, phaseCCurrent, dcBusCurrent;
    uint32_t currentInfoTimestamp;

    float dcBusVoltage, outputVoltage, vAB_VdVoltage, vBC_VqVoltage;
    uint32_t voltageInfoTimestamp;

    float commandedTorque, torqueFeedback, powerOnTimer;
    uint32_t torqueTimerTimestamp;

    bool isComplete;
};

struct __attribute__((packed)) VN200Data
{
    uint32_t timestamp;
    float yaw, pitch, roll;
    float pos_ex, pos_ey, pos_ez;
    float vel_ex, vel_ey, vel_ez;
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    bool hasData;
};

CANData currentData;
VN200Data vn200Data;
volatile bool newDataAvailable = false;

String baseFileName = "can_data";
String fileName;
int fileIndex = 0;

// Buffer for SD writes to reduce contention
#define BUFFER_SIZE 1024
char sdBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// ======================= SETUP =======================
void setup()
{
    Serial.begin(115200);
    pinMode(13, OUTPUT);

    can2.begin();
    can2.setBaudRate(250000);
    can2.setMaxMB(16);
    can2.enableFIFO();
    can2.enableFIFOInterrupt();
    can2.onReceive(canSniff);

    Serial.println("CAN Initialized - Listening for CAN messages");

    // Initialize USB Host for VN-200
    myusb.begin();
    Serial.println("Waiting for VN-200 connection...");
    // Don't block - will check in loop

    resetVN200Data();

    delay(100);

    if (!SD.begin(sdChipSelect))
    {
        Serial.println("SD card initialization failed!");
        return;
    }

    findNextFileName();

    dataFile = SD.open(fileName.c_str(), FILE_WRITE);
    if (!dataFile)
    {
        Serial.println("Error opening " + fileName + "!");
        return;
    }

    writeHeader();
    resetCurrentData();
}

// ======================= LOOP =======================
void loop()
{
    can2.events();
    myusb.Task();

    // Read VN-200 data if available
    if (vn200Serial.available())
    {
        String data = vn200Serial.readStringUntil('\n');
        data.trim();
        processVN200Line(data);
    }

    if (newDataAvailable && currentData.isComplete)
    {
        writeDataToSD();
        resetCurrentData();
        newDataAvailable = false;
        delayMicroseconds(100);
    }

    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000)
    {
        digitalWrite(13, !digitalRead(13));
        lastBlink = millis();
        delay(1);
    }
}

// ======================= CAN CALLBACK =======================
void canSniff(const CAN_message_t &msg)
{
    if (
        msg.id == 0x0A0 || msg.id == 0x0A1 ||
        msg.id == 0x0A5 || msg.id == 0x0A6 ||
        msg.id == 0x0A7 || msg.id == 0x0AC)
    {
        parser.feed(msg.id, msg.buf);

        if (parser.hasComplete())
        {
            CanFrames::Snapshot snap;
            parser.read(snap);
            printSnapshot(snap);
            parser.clearWindow();
        }
    }

    noInterrupts();
    processCANMessage(msg);
    interrupts();
}

// ======================= MESSAGE ROUTER =======================
void processCANMessage(const CAN_message_t &msg)
{
    switch (msg.id)
    {
    case 0x0A0:
        parseTemperatures1(msg);
        break;
    case 0x0A1:
        parseTemperatures2(msg);
        break;
    case 0x0A5:
        parseMotorPosition(msg);
        break;
    case 0x0A6:
        parseCurrentInformation(msg);
        break;
    case 0x0A7:
        parseVoltageInformation(msg);
        break;
    case 0x0AC:
        parseTorqueTimerInformation(msg);
        break;
    default:
        return;
    }

    newDataAvailable = true;
    checkDataCompleteness();
}

// ======================= FILE FUNCTIONS =======================
void writeHeader()
{
    dataFile.println(
        "Timestamp,Module A Temp,Module B Temp,Module C Temp,Gate Driver Temp,"
        "Timestamp,Control Board Temp,RTD #1 Temp,RTD #2 Temp,Stall Burst Model Temp,"
        "Timestamp,Motor Angle,Motor Speed,Electrical Frequency,Delta Resolver Filtered,"
        "Timestamp,Phase A Current,Phase B Current,Phase C Current,DC Bus Current,"
        "Timestamp,DC Bus Voltage,Output Voltage,VAB/Vd Voltage,VBC/Vq Voltage,"
        "Timestamp,Commanded Torque,Torque Feedback,Power On Timer,"
        "VN_Timestamp,Yaw,Pitch,Roll,Pos_Ex,Pos_Ey,Pos_Ez,Vel_Ex,Vel_Ey,Vel_Ez,Accel_X,Accel_Y,Accel_Z,Gyro_X,Gyro_Y,Gyro_Z");
    dataFile.flush();
}

void writeDataToSD()
{
    dataFile.printf(
        "%u,%.1f,%.1f,%.1f,%.1f,"
        "%u,%.1f,%.1f,%.1f,%.1f,"
        "%u,%.1f,%.1f,%.1f,%.1f,"
        "%u,%.1f,%.1f,%.1f,%.1f,"
        "%u,%.1f,%.1f,%.1f,%.1f,"
        "%u,%.1f,%.1f,%.3f,"
        "%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",

        currentData.temperatures1Timestamp,
        currentData.moduleATemp,
        currentData.moduleBTemp,
        currentData.moduleCTemp,
        currentData.gateDriverTemp,

        currentData.temperatures2Timestamp,
        currentData.controlBoardTemp,
        currentData.rtd1Temp,
        currentData.rtd2Temp,
        currentData.stallBurstTemp,

        currentData.motorPositionTimestamp,
        currentData.motorAngle,
        currentData.motorSpeed,
        currentData.electricalFrequency,
        currentData.deltaResolverFiltered,

        currentData.currentInfoTimestamp,
        currentData.phaseACurrent,
        currentData.phaseBCurrent,
        currentData.phaseCCurrent,
        currentData.dcBusCurrent,

        currentData.voltageInfoTimestamp,
        currentData.dcBusVoltage,
        currentData.outputVoltage,
        currentData.vAB_VdVoltage,
        currentData.vBC_VqVoltage,

        currentData.torqueTimerTimestamp,
        currentData.commandedTorque,
        currentData.torqueFeedback,
        currentData.powerOnTimer,

        vn200Data.timestamp,
        vn200Data.yaw,
        vn200Data.pitch,
        vn200Data.roll,
        vn200Data.pos_ex,
        vn200Data.pos_ey,
        vn200Data.pos_ez,
        vn200Data.vel_ex,
        vn200Data.vel_ey,
        vn200Data.vel_ez,
        vn200Data.accel_x,
        vn200Data.accel_y,
        vn200Data.accel_z,
        vn200Data.gyro_x,
        vn200Data.gyro_y,
        vn200Data.gyro_z);

    dataFile.flush();

    digitalWrite(13, HIGH);
    delay(200);
    digitalWrite(13, LOW);
    delay(200);
}

// ======================= HELPERS =======================
void resetCurrentData()
{
    memset(&currentData, 0, sizeof(CANData));
    currentData.isComplete = false;
}

void checkDataCompleteness()
{
    currentData.isComplete =
        currentData.temperatures1Timestamp != 0 &&
        currentData.temperatures2Timestamp != 0 &&
        currentData.motorPositionTimestamp != 0 &&
        currentData.currentInfoTimestamp != 0 &&
        currentData.voltageInfoTimestamp != 0 &&
        currentData.torqueTimerTimestamp != 0;
}

void findNextFileName()
{
    do
    {
        fileName = baseFileName + String(fileIndex) + ".csv";
        fileIndex++;
    } while (SD.exists(fileName.c_str()));

    Serial.print("Logging to file: ");
    Serial.println(fileName);
}

// ======================= PARSE FUNCTIONS =======================
void parseTemperatures1(const CAN_message_t &msg)
{
    currentData.temperatures1Timestamp = millis();

    int16_t moduleATemp = (int16_t)((msg.buf[1] << 8) | msg.buf[0]);
    int16_t moduleBTemp = (int16_t)((msg.buf[3] << 8) | msg.buf[2]);
    int16_t moduleCTemp = (int16_t)((msg.buf[5] << 8) | msg.buf[4]);
    int16_t gateDriverTemp = (int16_t)((msg.buf[7] << 8) | msg.buf[6]);

    currentData.moduleATemp = moduleATemp / 10.0f;
    currentData.moduleBTemp = moduleBTemp / 10.0f;
    currentData.moduleCTemp = moduleCTemp / 10.0f;
    currentData.gateDriverTemp = gateDriverTemp / 10.0f;
}

void parseTemperatures2(const CAN_message_t &msg)
{
    currentData.temperatures2Timestamp = millis();

    int16_t controlBoardTemp = (int16_t)((msg.buf[1] << 8) | msg.buf[0]);
    int16_t rtd1Temp = (int16_t)((msg.buf[3] << 8) | msg.buf[2]);
    int16_t rtd2Temp = (int16_t)((msg.buf[5] << 8) | msg.buf[4]);
    int16_t stallBurstTemp = (int16_t)((msg.buf[7] << 8) | msg.buf[6]);

    currentData.controlBoardTemp = controlBoardTemp / 10.0f;
    currentData.rtd1Temp = rtd1Temp / 10.0f;
    currentData.rtd2Temp = rtd2Temp / 10.0f;
    currentData.stallBurstTemp = stallBurstTemp / 10.0f;
}

void parseMotorPosition(const CAN_message_t &msg)
{
    currentData.motorPositionTimestamp = millis();

    int16_t motorAngle = (int16_t)((msg.buf[1] << 8) | msg.buf[0]);
    int16_t motorSpeed = abs((int16_t)((msg.buf[3] << 8) | msg.buf[2]));
    int16_t electricalFrequency = (int16_t)((msg.buf[5] << 8) | msg.buf[4]);
    int16_t deltaResolverFiltered = (int16_t)((msg.buf[7] << 8) | msg.buf[6]);

    currentData.motorAngle = motorAngle / 10.0f;
    currentData.motorSpeed = motorSpeed;
    currentData.electricalFrequency = electricalFrequency / 10.0f;
    currentData.deltaResolverFiltered = deltaResolverFiltered / 10.0f;
}

void parseCurrentInformation(const CAN_message_t &msg)
{
    currentData.currentInfoTimestamp = millis();

    int16_t phaseACurrent = (int16_t)((msg.buf[1] << 8) | msg.buf[0]);
    int16_t phaseBCurrent = (int16_t)((msg.buf[3] << 8) | msg.buf[2]);
    int16_t phaseCCurrent = (int16_t)((msg.buf[5] << 8) | msg.buf[4]);
    int16_t dcBusCurrent = (int16_t)((msg.buf[7] << 8) | msg.buf[6]);

    currentData.phaseACurrent = phaseACurrent / 10.0f;
    currentData.phaseBCurrent = phaseBCurrent / 10.0f;
    currentData.phaseCCurrent = phaseCCurrent / 10.0f;
    currentData.dcBusCurrent = dcBusCurrent / 10.0f;
}

void parseTorqueTimerInformation(const CAN_message_t &msg)
{
    currentData.torqueTimerTimestamp = millis();

    int16_t commandedTorque = (int16_t)((msg.buf[1] << 8) | msg.buf[0]);
    int16_t torqueFeedback = (int16_t)((msg.buf[3] << 8) | msg.buf[2]);

    uint32_t powerOnTimer =
        (msg.buf[7] << 24) |
        (msg.buf[6] << 16) |
        (msg.buf[5] << 8) |
        msg.buf[4];

    currentData.commandedTorque = commandedTorque / 10.0f;
    currentData.torqueFeedback = torqueFeedback / 10.0f;
    currentData.powerOnTimer = powerOnTimer * 0.003f;
}

void resetVN200Data()
{
    memset(&vn200Data, 0, sizeof(VN200Data));
    vn200Data.hasData = false;
}

void processVN200Line(String line)
{
    // Check for $VNISE
    if (!line.startsWith("$VNISE"))
    {
        Serial.print("ERROR: Not receiving $VNISE. Send Read Reg Commands");
        Serial.println(line);
        return;
    }

    // Parse VNISE sentence
    int parts[16]; // Store indices of commas
    int partCount = 0;

    // Find all comma positions
    for (int i = 0; i < line.length() && partCount < 16; i++)
    {
        if (line.charAt(i) == ',')
        {
            parts[partCount++] = i;
        }
    }

    if (partCount < 15)
    {
        Serial.println("VN Parse error: not enough fields");
        return;
    }

    // Extract checksum if present
    int starPos = line.indexOf('*');

    // Parse float values (fields 1-15)
    vn200Data.timestamp = millis();
    vn200Data.yaw = line.substring(parts[0] + 1, parts[1]).toFloat();
    vn200Data.pitch = line.substring(parts[1] + 1, parts[2]).toFloat();
    vn200Data.roll = line.substring(parts[2] + 1, parts[3]).toFloat();
    vn200Data.pos_ex = line.substring(parts[3] + 1, parts[4]).toFloat();
    vn200Data.pos_ey = line.substring(parts[4] + 1, parts[5]).toFloat();
    vn200Data.pos_ez = line.substring(parts[5] + 1, parts[6]).toFloat();
    vn200Data.vel_ex = line.substring(parts[6] + 1, parts[7]).toFloat();
    vn200Data.vel_ey = line.substring(parts[7] + 1, parts[8]).toFloat();
    vn200Data.vel_ez = line.substring(parts[8] + 1, parts[9]).toFloat();
    vn200Data.accel_x = line.substring(parts[9] + 1, parts[10]).toFloat();
    vn200Data.accel_y = line.substring(parts[10] + 1, parts[11]).toFloat();
    vn200Data.accel_z = line.substring(parts[11] + 1, parts[12]).toFloat();
    vn200Data.gyro_x = line.substring(parts[12] + 1, parts[13]).toFloat();
    vn200Data.gyro_y = line.substring(parts[13] + 1, parts[14]).toFloat();

    // Last field (gyro_z) - handle checksum
    String lastField = line.substring(parts[14] + 1);
    if (starPos != -1)
    {
        lastField = lastField.substring(0, lastField.indexOf('*'));
    }
    vn200Data.gyro_z = lastField.toFloat();
    vn200Data.hasData = true;
}

void parseVoltageInformation(const CAN_message_t &msg)
{
    currentData.voltageInfoTimestamp = millis();

    int16_t dcBusVoltage = (int16_t)((msg.buf[1] << 8) | msg.buf[0]);
    int16_t outputVoltage = (int16_t)((msg.buf[3] << 8) | msg.buf[2]);
    int16_t vAB_VdVoltage = (int16_t)((msg.buf[5] << 8) | msg.buf[4]);
    int16_t vBC_VqVoltage = (int16_t)((msg.buf[7] << 8) | msg.buf[6]);

    currentData.dcBusVoltage = dcBusVoltage / 10.0f;
    currentData.outputVoltage = outputVoltage / 10.0f;
    currentData.vAB_VdVoltage = vAB_VdVoltage / 10.0f;
    currentData.vBC_VqVoltage = vBC_VqVoltage / 10.0f;
}
