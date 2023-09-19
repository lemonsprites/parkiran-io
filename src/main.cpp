#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Servo.h>

/**
 * RTDB Header add
 */
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

/**
 * =========================================
 * Environtment Variabel
 * =========================================
 */
#define WIFI_SSID "Galura_Digital"
#define WIFI_PASSWORD "Riot_Room"
#define API_KEY "AIzaSyD7o60j3E0p8nMSf_RLJ1dBiCaDfONhHxI"
#define DATABASE_URL "https://finalprojectparking-default-rtdb.asia-southeast1.firebasedatabase.app/"

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
Servo myservo;

byte customChar[8] = {
    0b00000,
    0b01010,
    0b11111,
    0b11111,
    0b01110,
    0b00100,
    0b00000,
    0b00000};

// Define Firebase Data object
FirebaseData fbdo, otp1Subs, otp2Subs, otpEnterSubs, booked1Subs, booked2Subs, frontGate, backGate, status1Subs, status2Subs, end1Subs, end2Subs;

FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

// Ultrasonic 1
const int echoPin1 = D4; // Echo Pin of Ultrasonic Sensor1
const int pingPin1 = D3; // Trigger Pin of Ultrasonic Sensor1

// Ultrasonic 2
const int echoPin2 = D6; // Echo Pin of Ultrasonic Sensor2
const int pingPin2 = D5; // Trigger Pin of Ultrasonic Sensor

// duration & distance ultasonic 1 and 2
float duration_us1, duration_us2, distance_cm1, distance_cm2;

// Enter Gate
//(IR Sensor)
const int ir1 = D0;
//(Servo)
const int servo = D7;

// Exit Gate
//(IR Sensor)
const int ir2 = D8;
//(Servo)
// const int exitServoPin = A0; // Connect servo signal pin to S3
// Servo exitServo;

////Distance for parking detected
const int STD_DISTANCE = 10;

// Parking Slot

/**
 * Global Variabel
 */
int irVal_1; // Sensor IR 1
int irVal_2; // Sensor IR 2

/**
 * =========================================
 * Dinamik Variabel
 * =========================================
 */
int slot;
String otp_1;
String otp_2;
String entered_otp;
int booked_1;
int booked_2;
bool openFrontGate;
bool openBackGate;
String status_1;
String status_2;
int end_1;
int end_2;

unsigned long dataTxInterval = 5000;
unsigned long timer;
unsigned long elapsedMillis = 0;

unsigned long elapsedUltrasonic1 = 0;
unsigned long elapsedUltrasonic2 = 0;

void setup()
{
    Serial.begin(9600);

    /**
     * =========================================
     * Wifi Setup
     * =========================================
     */
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[System] Connecting to Wi-Fi.\n");
    unsigned long timer = millis();
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED)
        if (millis() - elapsedMillis > dataTxInterval)
        {
            ++attempt;
            elapsedMillis = timer;
        }

    Serial.printf("[System] Connected with IP (%s) with %s attempt.\n", WiFi.localIP(), attempt);

    /**
     * =========================================
     * RTDB Setup
     * =========================================
     */
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    Serial.println("[System] Connecting to Firebase RTDB.\n");
    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.printf("[System] Connected to Firebase RTDB.\n");
        signupOK = true;
    }
    else
        Serial.printf("[System] Failed to Connect with Firebase RTDB: %s\n", config.signer.signupError.message);

    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    /**
     * =========================================
     * Data Subscriber Init
     * =========================================
     */
    if (!Firebase.RTDB.beginStream(&otp1Subs, "Ultrasonic/Slot_A1/otp_1"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", otp1Subs.dataPath(), otp1Subs.errorReason());

    if (!Firebase.RTDB.beginStream(&otp2Subs, "Ultrasonic/Slot_A2/otp_2"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", otp2Subs.dataPath(), otp2Subs.errorReason());

    if (!Firebase.RTDB.beginStream(&otpEnterSubs, "entered_otp"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", otpEnterSubs.dataPath(), otpEnterSubs.errorReason());

    if (!Firebase.RTDB.beginStream(&booked1Subs, "Ultrasonic/Slot_A1/booked_1"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", booked1Subs.dataPath(), booked1Subs.errorReason());

    if (!Firebase.RTDB.beginStream(&booked2Subs, "Ultrasonic/Slot_A2/booked_2"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", booked2Subs.dataPath(), booked2Subs.errorReason());

    if (!Firebase.RTDB.beginStream(&frontGate, "Entering_Gates/Ir_Enter"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", frontGate.dataPath(), frontGate.errorReason());

    if (!Firebase.RTDB.beginStream(&backGate, "Exit_Gates/Ir_Exit"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", backGate.dataPath(), backGate.errorReason());

    if (!Firebase.RTDB.beginStream(&end1Subs, "Ultrasonic/Slot_A1/end_1"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", end1Subs.dataPath(), end1Subs.errorReason());

    if (!Firebase.RTDB.beginStream(&end2Subs, "Ultrasonic/Slot_A1/end_2"))
        Serial.printf("[Error] Failed Subscribing to (' %s '): %s\n", end2Subs.dataPath(), end2Subs.errorReason());

    /**
     * =========================================
     * Pin & Hardware Config
     * =========================================
     */
    pinMode(pingPin1, OUTPUT);
    pinMode(pingPin2, OUTPUT);
    pinMode(echoPin1, INPUT);
    pinMode(echoPin2, INPUT);

    // IR PIN
    pinMode(ir1, INPUT);
    pinMode(ir2, INPUT);

    // Servo Config
    myservo.attach(servo);
    //  exitServo.attach(exitServoPin);

    //  LCD Config When the Device ON
    Wire.begin(D2, D1);
    lcd.begin(20, 4);
    lcd.backlight();
    lcd.setCursor(2, 0);
    lcd.print("Car Parking Lot");
    lcd.setCursor(6, 1);
    lcd.print("Detector");
    lcd.setCursor(9, 2);
    lcd.print("By");
    lcd.setCursor(7, 3);
    lcd.print("Yosep");
    delay(700);
    lcd.clear();
}

void loop()
{
    timer = millis();
    irVal_1 = digitalRead(ir1);
    irVal_2 = digitalRead(ir2);

    if (distance_cm1 <= STD_DISTANCE && distance_cm2 <= STD_DISTANCE)
        slot = 0; // Update slot count to indicate all slots occupied
    else if (distance_cm1 <= STD_DISTANCE || distance_cm2 <= STD_DISTANCE)
        slot = 1; // Update slot count to indicate 1 slot available
    else
        slot = 2; // Update slot count to indicate 2 slots available

    if (Firebase.ready() && signupOK)
    {
        if (!Firebase.RTDB.readStream(&otp1Subs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", otp1Subs.dataPath(), otp1Subs.errorReason());

        if (!Firebase.RTDB.readStream(&otp2Subs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", otp2Subs.dataPath(), otp2Subs.errorReason());

        if (!Firebase.RTDB.readStream(&otpEnterSubs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", otpEnterSubs.dataPath(), otpEnterSubs.errorReason());

        if (otp1Subs.streamAvailable() || otp2Subs.streamAvailable() || otpEnterSubs.streamAvailable())
        {
            if (otp1Subs.dataType() == "string")
                otp_1 = otp1Subs.stringData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'otp_1': %s (%s)\n", otp1Subs.dataPath(), otp1Subs.dataType(), otp1Subs.errorReason());

            if (otp2Subs.dataType() == "string")
                otp_2 = otp2Subs.stringData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'otp_2': %s (%s)\n", otp2Subs.dataPath(), otp2Subs.dataType(), otp2Subs.errorReason());

            if (otpEnterSubs.dataType() == "string")
                entered_otp = otpEnterSubs.stringData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'entered_otp': %s (%s)\n", otpEnterSubs.dataPath(), otpEnterSubs.dataType(), otpEnterSubs.errorReason());
        }

        if (!Firebase.RTDB.readStream(&booked1Subs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", booked1Subs.dataPath(), booked1Subs.errorReason());

        if (!Firebase.RTDB.readStream(&booked2Subs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", booked2Subs.dataPath(), booked2Subs.errorReason());

        if (booked1Subs.streamAvailable() || booked2Subs.streamAvailable())
        {
            if (booked1Subs.dataType() == "int")
                booked_1 = booked1Subs.intData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'booked_1': %s (%s)\n", booked1Subs.dataPath(), booked1Subs.dataType(), booked1Subs.errorReason());

            if (booked2Subs.dataType() == "int")
                booked_2 = booked2Subs.intData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'booked_2': %s (%s)\n", booked2Subs.dataPath(), booked2Subs.dataType(), booked2Subs.errorReason());
        }

        if (!Firebase.RTDB.readStream(&frontGate))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", frontGate.dataPath(), frontGate.errorReason());

        if (!Firebase.RTDB.beginStream(&backGate, "Exit_Gates/Ir_Sensor"))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", backGate.dataPath(), backGate.errorReason());

        if (frontGate.streamAvailable() || backGate.streamAvailable())
        {
            if (booked1Subs.dataType() == "int")
                openFrontGate = frontGate.intData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'openFrontGate': %s (%s)\n", frontGate.dataPath(), frontGate.dataType(), frontGate.errorReason());

            if (booked2Subs.dataType() == "int")
                openBackGate = backGate.intData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'openBackGate': %s (%s)\n", backGate.dataPath(), backGate.dataType(), backGate.errorReason());
        }

        if (!Firebase.RTDB.readStream(&end1Subs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", end1Subs.dataPath(), end1Subs.errorReason());

        if (!Firebase.RTDB.readStream(&end2Subs))
            Serial.printf("[Error] Subcriber to (' %s ') has error: %s.\n", end2Subs.dataPath(), end2Subs.errorReason());

        if (end1Subs.streamAvailable() || end2Subs.streamAvailable())
        {
            if (end1Subs.dataType() == "int")
                booked_1 = end1Subs.intData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'booked_1': %s (%s)\n", end1Subs.dataPath(), end1Subs.dataType(), end1Subs.errorReason());

            if (end2Subs.dataType() == "int")
                booked_2 = end2Subs.intData();
            else
                Serial.printf("[Info] Data has been retrieve from (%s) and saved to 'booked_2': %s (%s)\n", end2Subs.dataPath(), end2Subs.dataType(), end2Subs.errorReason());
        }

        updateSlotInfo(slot); // Update initial slot information

        if (timer - elapsedMillis > dataTxInterval)
        {
            if (Firebase.RTDB.setString(&fbdo, "Entering_Gates/Ir_Realtime", irVal_1))
                Serial.printf("[Info] Data has been set to (%s) with value: %s (%s)\n", fbdo.dataPath(), irVal_1, fbdo.errorReason());
            else
                Serial.printf("[Error] Data set has error occured on (%s) with value: %s\n", fbdo.dataPath(), fbdo.errorReason());
        }

        if (timer - elapsedMillis > dataTxInterval)
        {
            if (Firebase.RTDB.setString(&fbdo, "Entering_Gates/Ir_Realtime", irVal_2))
                Serial.printf("[Info] Data has been set to (%s) with value: %s (%s)\n", fbdo.dataPath(), irVal_2, fbdo.errorReason());
            else
                Serial.printf("[Error] Data set has error occured on (%s) with value: %s\n", fbdo.dataPath(), fbdo.errorReason());
        }

        entering_gate();

        if (timer - elapsedMillis > dataTxInterval)
            ultrasonic_1();

        if (timer - elapsedMillis > dataTxInterval)
            ultrasonic_2();

        exit_gate();
    }

    lcd.setCursor(3, 0);
    lcd.print("Remaining : ");
    lcd.setCursor(16, 0);
    lcd.print(slot);
    lcd.setCursor(9, 1);
    lcd.print("|");
    lcd.setCursor(9, 2);
    lcd.print("|");

    elapsedMillis = timer;
}

void ultrasonic_1()
{
    /**
     * Ultrasonic Prosedur
     * 1. Konek ke PIN
     * 2. Kalkulasi Distance dan Logicnya
     * 3. Assign Value
     * 4. Patch ke Database (hanya 'Book' => 'Fill' && 'Fill' -> 'Book)
     * 5. Update LCD
     */

    unsigned long localTimer = millis();

    if (localTimer >= 100)
        digitalWrite(pingPin1, HIGH);

    elapsedUltrasonic1 = localTimer;
    digitalWrite(pingPin1, LOW);

    duration_us1 = pulseIn(echoPin1, HIGH);
    distance_cm1 = 0.017 * duration_us1;

    if (booked_1 == 1 && distance_cm1 <= STD_DISTANCE)
        status_1 = "Fill";
    else
        status_1 = "Booked";

    if (Firebase.RTDB.setFloat(&fbdo, "Ultrasonic/Slot_A1/distance", distance_cm1))
        Serial.printf("[Info] Data has been set to (%s) with value: %s (%s)\n", fbdo.dataPath(), distance_cm1, fbdo.errorReason());
    else
        Serial.printf("[Error] Data set has error occured on (%s) with value: %s\n", fbdo.dataPath(), fbdo.errorReason());

    if (Firebase.RTDB.setString(&fbdo, "Ultrasonic/Slot_A1/status", status_1))
        Serial.printf("[Info] Data has been set to (%s) with value: %s (%s)\n", fbdo.dataPath(), status_1, fbdo.errorReason());
    else
        Serial.printf("[Error] Data set has error occured on (%s) with value: %s\n", fbdo.dataPath(), fbdo.errorReason());

    // Update LCD
    lcd.setCursor(3, 1);
    lcd.print("A1 :");
    lcd.createChar(0, customChar);
    lcd.setCursor(2, 2);
    lcd.write((byte)0);
    lcd.setCursor(3, 2);
    lcd.print(status_1);
}

void ultrasonic_2()
{
    /**
     * Ultrasonic Prosedur
     * 1. Konek ke PIN
     * 2. Kalkulasi Distance dan Logicnya
     * 3. Assign Value
     * 4. Patch ke Database (hanya 'Book' => 'Fill' && 'Fill' -> 'Book)
     * 5. Update LCD
     */
    unsigned long localTimer = millis();

    if (localTimer >= 100)
        digitalWrite(pingPin1, HIGH);

    elapsedUltrasonic2 = localTimer;
    digitalWrite(pingPin2, LOW);

    duration_us2 = pulseIn(echoPin2, HIGH);
    distance_cm2 = 0.017 * duration_us2;

    if (booked_2 == 1 && distance_cm2 <= STD_DISTANCE)
        status_2 = "Fill";
    else
        status_2 = "Booked";

    if (Firebase.RTDB.setFloat(&fbdo, "Ultrasonic/Slot_A2/distance", distance_cm2))
        Serial.printf("[Info] Data has been set to (%s) with value: %s (%s)\n", fbdo.dataPath(), distance_cm2, fbdo.errorReason());
    else
        Serial.printf("[Error] Data set has error occured on (%s) with value: %s\n", fbdo.dataPath(), fbdo.errorReason());

    if (Firebase.RTDB.setString(&fbdo, "Ultrasonic/Slot_A2/status", status_2))
        Serial.printf("[Info] Data has been set to (%s) with value: %s (%s)\n", fbdo.dataPath(), status_2, fbdo.errorReason());
    else
        Serial.printf("[Error] Data set has error occured on (%s) with value: %s\n", fbdo.dataPath(), fbdo.errorReason());

    //  Update LCD
    lcd.setCursor(13, 1);
    lcd.print("A2 :");
    lcd.createChar(0, customChar);
    lcd.setCursor(12, 2);
    lcd.write((byte)0);
    lcd.setCursor(13, 2);
    lcd.print(status_2);
}

void entering_gate()
{
    if (openFrontGate == 0 && (otp_1 == entered_otp || otp_2 == entered_otp))
        myservo.write(0);
    else
        myservo.write(180);

    lcd.clear();
}

void exit_gate()
{
    if ((end_1 == 1 && openBackGate == 0) || openFrontGate == 0)
    {

        myservo.write(0);
        Serial.printf("[System] Exit Gate opened.\n");
    }
    else
    {
        myservo.write(180);
        Serial.printf("[System] Exit Gate closed.\n");
    }

    if ((end_2 == 1 && openBackGate == 0) || openBackGate == 0)
    {
        myservo.write(0);
        Serial.printf("[System] Exit Gate opened.\n");
    }
    else
    {
        myservo.write(180);
        Serial.printf("[System] Exit Gate closed.\n");
    }
}

void updateSlotInfo(int availableSlots)
{

    if (Firebase.RTDB.setIntAsync(&fbdo, "Parking_Slots/Remaining", availableSlots))
        Serial.printf("[System] Available Slots: %s\n", availableSlots);
    else
        Serial.printf("[Error] Error occured: %s", fbdo.errorReason());

    if (slot == 0)
    {
        lcd.setCursor(0, 3);
        lcd.print("[System] Parking Full!");
    }
}
