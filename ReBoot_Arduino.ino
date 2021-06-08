#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"
#include "HX711.h"

#define BT_TX (uint8_t) 3
#define BT_RX (uint8_t) 2
#define TEMP_PIN (uint8_t) 8
#define LED (uint8_t) 4
#define BUZZER (uint8_t) 9
#define BAUD 9600
#define NUM_CHARS (byte) 32
#define CHIP_SELECT (uint8_t) 10
#define SENSOR1_DATA (uint8_t) A0
#define SENSOR2_DATA (uint8_t) A2
#define SENSOR3_DATA (uint8_t) A5
#define SENSOR4_DATA (uint8_t) 7
#define SENSOR1_SCK (uint8_t) A1
#define SENSOR2_SCK (uint8_t) A3
#define SENSOR3_SCK (uint8_t) 5
#define SENSOR4_SCK (uint8_t) 6
#define TEST_DURATION 3000
#define NOT_CONNECTED 0
#define LIVE_VIEW 1
#define SETTINGS 2

// Constants
const int CHAR_SIZE = sizeof(char);

// Hardware interface settings
SoftwareSerial btSerial(BT_RX, BT_TX);
RTC_DS1307 rtc;

OneWire oneWire(TEMP_PIN);
DallasTemperature TEMP_SENSOR(&oneWire);

HX711 SENSOR1;
HX711 SENSOR2;
HX711 SENSOR3;
HX711 SENSOR4;

// Hardware status & settings
boolean ledEnabled = true;
boolean isLedOn = false;
boolean buzzerEnabled = true;
boolean isBuzzerOn = false;
boolean error = false;

float threshold1 = 0.0f;
float threshold2 = 0.0f;
float threshold3 = 0.0f;
float threshold4 = 0.0f;

// Hardware testing
boolean ledTestinProgress = false;
unsigned long ledTestStart = 0;
boolean buzzerTestinProgress = false;
unsigned long buzzerTestStart = 0;

// Modes
int8_t mode = 0;

// Bluetooth
char data[NUM_CHARS];
boolean newData = false;

void receive();
void processMessage();
void testLED();
void testBuzzer();
void loadSettings();
void saveSettings();

void setup() {
    pinMode(LED, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    Serial.begin(BAUD);
    rtc.begin();
    if (!SD.begin(CHIP_SELECT)) {
        Serial.println(F("ERROR: SD init failed."));
        error = true;   
    } else {
        loadSettings();
        
        Serial.println(F("SD initialized."));
        Serial.println(F("Settings: "));
        Serial.print(F("LED: "));
        Serial.println(ledEnabled);
        Serial.print(F("Buzzer: "));
        Serial.println(buzzerEnabled);
        Serial.print(F("Threshold 1: "));
        Serial.println(threshold1);
        Serial.print(F("Threshold 2: "));
        Serial.println(threshold2);
        Serial.print(F("Threshold 3: "));
        Serial.println(threshold3);
        Serial.print(F("Threshold 4: "));
        Serial.println(threshold4);
        
    }
        
    TEMP_SENSOR.begin();

    SENSOR1.begin(SENSOR1_DATA, SENSOR1_SCK);
    SENSOR1.set_scale(-63.f);
    SENSOR1.tare();
    Serial.println(F("Sensor 1 initialized.")); 

    SENSOR2.begin(SENSOR2_DATA, SENSOR2_SCK);
    SENSOR2.set_scale(-58.f);
    SENSOR2.tare();
    Serial.println(F("Sensor 2 initialized."));

    SENSOR3.begin(SENSOR3_DATA, SENSOR3_SCK);
    SENSOR3.set_scale(-50.f);
    SENSOR3.tare();
    Serial.println(F("Sensor 3 initialized."));

    SENSOR4.begin(SENSOR4_DATA, SENSOR4_SCK);
    SENSOR4.set_scale(-46.f);
    SENSOR4.tare();
    Serial.println(F("Sensor 4 initialized."));         

    btSerial.begin(BAUD);
}

void loop() {      
    if (!error) {
        receive();
        processMessage();

        DateTime now = rtc.now();
        float sensor1 = SENSOR1.get_units() / 1000;
        float sensor2 = SENSOR2.get_units() / 1000;
        float sensor3 = SENSOR3.get_units() / 1000;
        float sensor4 = SENSOR4.get_units() / 1000;
        TEMP_SENSOR.requestTemperatures();
        float temperature = TEMP_SENSOR.getTempCByIndex(0);

        Serial.print(F("S1: "));
        Serial.print(sensor1);
        Serial.print(F(" S2: "));
        Serial.print(sensor2);
        Serial.print(F(" S3: "));
        Serial.print(sensor3);
        Serial.print(F(" S4: "));
        Serial.print(sensor4);
        Serial.print(F(" T: "));
        Serial.println(temperature);

        if (isnan(sensor1) || isnan(sensor2) || isnan(sensor3) || isnan(sensor4)) return;

        if (!buzzerTestinProgress && !ledTestinProgress) {
            if (threshold1 > 0 && sensor1 > threshold1 ||
                threshold2 > 0 && sensor2 > threshold2 ||
                threshold3 > 0 && sensor3 > threshold3 ||
                threshold4 > 0 && sensor4 > threshold4) {
                if (buzzerEnabled) 
                {
                    tone(BUZZER, 1000);
                    isBuzzerOn = true;
                }
                if (ledEnabled) {
                    digitalWrite(LED, HIGH);
                    isLedOn = true;
                }
            } else {
                if (isBuzzerOn) {
                    noTone(BUZZER);
                    isBuzzerOn = false;
                }

                if (isLedOn) {
                    digitalWrite(LED, LOW);
                    isLedOn = false;
                }
            }
        }

        if (mode == LIVE_VIEW) {
            btSerial.print(F("<"));
            btSerial.print(F("M;"));
            btSerial.print(now.unixtime());
            btSerial.print(F(";"));
            if (threshold1 > -1.0f) btSerial.print(sensor1);
            else btSerial.print(-1.0f);
            btSerial.print(F(";"));
            if (threshold2 > -1.0f) btSerial.print(sensor2);
            else btSerial.print(-1.0f);
            btSerial.print(F(";"));
            if (threshold3 > -1.0f) btSerial.print(sensor3);
            else btSerial.print(-1.0f);
            btSerial.print(F(";"));
            if (threshold4 > -1.0f) btSerial.print(sensor4);
            else btSerial.print(-1.0f);
            btSerial.print(F(";"));
            btSerial.print(temperature);
            btSerial.println(F(">"));
        }

        if (ledTestinProgress && ((millis() - ledTestStart >= TEST_DURATION))) {
            ledTestStart = 0;
            ledTestinProgress = false;
            digitalWrite(LED, LOW);
        }

        if (buzzerTestinProgress && ((millis() - buzzerTestStart >= TEST_DURATION))) {
            buzzerTestStart = 0;
            buzzerTestinProgress = false;
            noTone(BUZZER);
        }

        delay(250);
    } else {
        tone(BUZZER, 1000);
        digitalWrite(LED, HIGH);
        delay(1000);
        noTone(BUZZER);
        digitalWrite(LED, LOW);
        delay(1000);
    }
}

// Receive a Bluetooth message from the companion app
// Valid messages are encapsulated within '<' and '>' to ensure that the whole message is received.
void receive() {
    static boolean receiving = false;
    static byte index = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (btSerial.available() > 0 && !newData) {
        rc = btSerial.read();
        delay(10);

        if (receiving) {
            if (rc != endMarker) {
                data[index] = rc;
                index++;
                
                if (index >= NUM_CHARS) {
                    index = NUM_CHARS - 1;
                }
            } else {
                data[index] = '\0';
                receiving = false;
                index = 0;
                newData = true;
            }
        } else if (rc == startMarker) {
            receiving = true;
        }
    }
}

// Processes a received message based on length and content
void processMessage() {
    if (newData) {
        int dataLength = strlen(data);
        
        Serial.println(data);

        if (dataLength == 1) {
            switch (data[0]) {
                case 'l': mode = LIVE_VIEW;
                break;
                case 's': {
                    mode = SETTINGS;
                    btSerial.print(F("<S;"));
                    btSerial.print(ledEnabled);
                    btSerial.print(F(";"));
                    btSerial.print(buzzerEnabled);
                    btSerial.print(F(";"));
                    btSerial.print(threshold1);
                    btSerial.print(F(";"));
                    btSerial.print(threshold2);
                    btSerial.print(F(";"));
                    btSerial.print(threshold3);
                    btSerial.print(F(";"));
                    btSerial.print(threshold4);
                    btSerial.println(F(">"));
                }
                break;
                case 'H': btSerial.println(F("<W>"));
            }
        } else if (dataLength == 2) {
            if (strcmp(data, "LT") == 0) testLED();
            if (strcmp(data, "BT") == 0) testBuzzer();
        } else {
            char* token = strtok(data, ";");
            int count = 0;

            if (strcmp(token, "S") == 0) {
                while (token != NULL) {
                    token = strtok(NULL, ";");
                    if (count == 0) ledEnabled = atoi(token);
                    if (count == 1) buzzerEnabled = atoi(token);
                    if (count == 2) threshold1 = atof(token);
                    if (count == 3) threshold2 = atof(token);
                    if (count == 4) threshold3 = atof(token);
                    if (count == 5) threshold4 = atof(token);
                    count++;
                }

                saveSettings();
            }
        }

        newData = false;
    }
}

// Initiate an LED test
void testLED() {
    ledTestStart = millis();
    ledTestinProgress = true;
    digitalWrite(LED, HIGH);
}

// Initiate a buzzer test
void testBuzzer() {
    buzzerTestStart = millis();
    buzzerTestinProgress = true;
    tone(BUZZER, 1000);
}

// Load settings from SD card
void loadSettings() {
    File settingsFile = SD.open("settings.txt", FILE_READ);

    if (settingsFile) {

        char settings[10];
        int index = 0;
        
        while (settingsFile.available()) {
            settings[index] = settingsFile.read();
            index++;
        }

        settingsFile.close();
        settings[index] = '\0';

        int count = 0;
        
        char* token = strtok(settings, ";");
        ledEnabled = atoi(token);

        while (token != NULL) {
            token = strtok(NULL, ";");
            if (count == 0) buzzerEnabled = atoi(token);
            if (count == 1) threshold1 = atof(token);
            if (count == 2) threshold2 = atof(token);
            if (count == 3) threshold3 = atof(token);
            if (count == 4) threshold4 = atof(token);
            count++;
        }
    } else {
        Serial.println(F("ERROR: Reading settings failed."));
        error = true;
    }
}

// Save settings to SD card
void saveSettings() {
    SD.remove("settings.txt");
    
    File settingsFile = SD.open("settings.txt", FILE_WRITE);
    if (settingsFile) {
        settingsFile.print(ledEnabled);
        settingsFile.print(";");
        settingsFile.print(buzzerEnabled);
        settingsFile.print(";");
        settingsFile.print(threshold1);
        settingsFile.print(";");
        settingsFile.print(threshold2);
        settingsFile.print(";");
        settingsFile.print(threshold3);
        settingsFile.print(";");
        settingsFile.print(threshold4);
        settingsFile.println(";");

        settingsFile.close();
    } else {
        Serial.println(F("ERROR: Saving settings failed."));
        error = true;
    }
}
