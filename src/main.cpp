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

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Galura_Digital"
#define WIFI_PASSWORD "Riot_Room"

// Insert Firebase project API Key
#define API_KEY "AIzaSyD7o60j3E0p8nMSf_RLJ1dBiCaDfONhHxI"

// Insert RTDB URLefine the RTDB URL */
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
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;
FirebaseData slotsRef;

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

int slot;
int irVal_1;
int irVal_2;
String otp_1;
String otp_2;
String entered_otp = "";

const long setInterval = 1000;
const long getInterval = 1000;

void setup()
{
    Serial.begin(9600);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }

    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    /**
     * RTDB Config
     */
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    Firebase.reconnectWiFi(true);

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("ok");
        signupOK = true;
    }
    else
        Serial.printf("%s\n", config.signer.signupError.message.c_str());

    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);

    //  Ultrasonic PinMode
    pinMode(pingPin1, OUTPUT);
    pinMode(pingPin2, OUTPUT);
    pinMode(echoPin1, INPUT);
    pinMode(echoPin2, INPUT);

    //  IR Sensor PinMode
    pinMode(ir1, INPUT);
    pinMode(ir2, INPUT);

    // Servo
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

void ultrasonic_1()
{
    digitalWrite(pingPin1, HIGH);
    delayMicroseconds(10);
    digitalWrite(pingPin1, LOW);

    duration_us1 = pulseIn(echoPin1, HIGH);

    distance_cm1 = 0.017 * duration_us1;

    String status1;
    int booked_1;

    if (Firebase.ready() && signupOK)
    {
        unsigned long setFlow = millis();
        unsigned long getFlow = millis();
        unsigned long setFlowPrev = 0;
        unsigned long getFlowPrev = 0;

        if (setFlow - setFlowPrev <= setInterval || setFlowPrev == 0)
        {
            if (Firebase.RTDB.setFloat(&fbdo, "Ultrasonic/Slot_A1/distance", distance_cm1))
            {
                //      Serial.println("PASSED");
                Serial.print("Slot A1");
                Serial.println();
                Serial.print("Distance: ");
                Serial.println(distance_cm1);
            }

            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            setFlowPrev = setFlow
        }

        if (getFlow - getFlowPrev >= getInterval)
        {
            if (Firebase.RTDB.getString(&slotsRef, "Ultrasonic/Slot_A1/otp_1"))
            {
                //      Serial.println("PASSED");
                otp_1 = slotsRef.stringData();

                Serial.println();
                Serial.print("otp1: ");
                Serial.println(otp_1);
            }

            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (getFlow - getFlowPrev >= getInterval)
        {
            if (Firebase.RTDB.getInt(&slotsRef, "Ultrasonic/Slot_A1/booked_1"))
            {
                //      Serial.println("PASSED");
                booked_1 = slotsRef.intData();

                Serial.println();
                Serial.print("booked_status_1: " + String(booked_1));
            }

            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (booked_1 == 1)
        {
            if (distance_cm1 >= STD_DISTANCE)
                status1 = "Fill";
            else
                status1 = "Booked";
        }
        else
            status1 = "Empty";

        if (setFlow - setFlowPrev <= setInterval)
        {
            if (Firebase.RTDB.setString(&fbdo, "Ultrasonic/Slot_A1/status", status1))
            {
                //      Serial.println("PASSED");
                Serial.println();
                Serial.print("Updated Status: ");
                Serial.println(status1);
            }

            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            setFlowPrev = setFlow
        }

        // Update LCD
        lcd.setCursor(3, 1);
        lcd.print("A1 :");
        lcd.createChar(0, customChar);
        lcd.setCursor(2, 2);
        lcd.write((byte)0);
        lcd.setCursor(3, 2);
        lcd.print(status1);
    }
}

void ultrasonic_2()
{
    digitalWrite(pingPin2, HIGH);
    delayMicroseconds(10);
    digitalWrite(pingPin2, LOW);

    duration_us2 = pulseIn(echoPin2, HIGH);

    distance_cm2 = 0.017 * duration_us2;

    String status2;
    int booked_2;

    if (Firebase.ready() && signupOK)
    {
        unsigned long setFlow = millis();
        unsigned long getFlow = millis();
        unsigned long setFlowPrev = 0;
        unsigned long getFlowPrev = 0;

        if (setFlow - setFlowPrev <= setInterval || setFlowPrev == 0)
        {
            if (Firebase.RTDB.setFloat(&fbdo, "Ultrasonic/Slot_A2/distance", distance_cm2))
            {
                //      Serial.println("PASSED");
                Serial.print("Slot A2");
                Serial.println();
                Serial.print("Distance: ");
                Serial.println(distance_cm2);
            }
            setFlowPrev = setFlow
        }

        if (getFlow - getFlowPrev >= getInterval || getInterval == 0)
        {
            if (Firebase.RTDB.getString(&slotsRef, "Ultrasonic/Slot_A2/otp_2"))
            {
                //      Serial.println("PASSED");
                otp_2 = slotsRef.stringData();

                Serial.println();
                Serial.print("otp2: ");
                Serial.println(otp_2);
            }
            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (getFlow - getFlowPrev >= getInterval)
        {
            if (Firebase.RTDB.getInt(&slotsRef, "Ultrasonic/Slot_A2/booked_2"))
            {
                //      Serial.println("PASSED");
                booked_2 = slotsRef.intData();

                Serial.println();
                Serial.print("booked_status_2: " + String(booked_2));
            }

            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (booked_2 == 1)
        {
            if (distance_cm2 <= STD_DISTANCE)
                status2 = "Fill";
            else
                status2 = "Booked";
        }
        else
            status2 = "Empty";

        if (setFlow - setFlowPrev <= setInterval)
        {
            if (Firebase.RTDB.setString(&fbdo, "Ultrasonic/Slot_A2/status", status2))
            {
                //      Serial.println("PASSED");
                Serial.println();
                Serial.print("Updated Status: ");
                Serial.println(status2);
            }
            else
            {
                Serial.println("FAILED to update status");
                Serial.println("REASON: " + fbdo.errorReason());
            }
            setFlowPrev = setFlow
        }

        //  Update LCD
        lcd.setCursor(13, 1);
        lcd.print("A2 :");
        lcd.createChar(0, customChar);
        lcd.setCursor(12, 2);
        lcd.write((byte)0);
        lcd.setCursor(13, 2);
        lcd.print(status2);
    }
}

void entering_gate()
{

    // Read deklarasi sensor IR
    irVal_1 = digitalRead(ir1);

    if (Firebase.ready() && signupOK)
    {
        unsigned long setFlow = millis();
        unsigned long getFlow = millis();
        unsigned long setFlowPrev = 0;
        unsigned long getFlowPrev = 0;

        if (getFlow - getFlowPrev >= getInterval || getInterval == 0)
        {
            if (Firebase.RTDB.getInt(&slotsRef, "Entering_Gates/Ir_Enter"))
            {
                Serial.print("Entered_Ir : ");
                Serial.println(irVal_1);
            }
            else
            {
                Serial.println("Failed to update available slots.");
                Serial.println("Reason: " + slotsRef.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (setFlow - setFlowPrev >= setInterval || setInterval == 0)
        {
            if (Firebase.RTDB.setInt(&slotsRef, "Entering_Gates/Ir_Realtime", irVal_1))
            {
                Serial.print("Ir_Realtime : ");
                Serial.println(irVal_1);
            }
            else
            {
                Serial.println("Failed to update available slots.");
                Serial.println("Reason: " + slotsRef.errorReason());
            }
            setFlowPrev = setFlow
        }

        if (slot == 0)
        {
            // Parking is full, do not open the gates
            lcd.setCursor(0, 3);
            lcd.print("Parking Full!");
            delay(100);
        }
        else
        {
            if (getFlow - getFlowPrev >= getInterval)
            {
                Firebase.RTDB.getInt(&slotsRef, "Entering_Gates/Ir_Enter");
                if (slotsRef.intData() == 0 && (otp_1 == entered_otp || otp_2 == entered_otp))
                    myservo.write(0);
                else
                    myservo.write(180);

                getFlowPrev = getFlow
            }

            lcd.clear();
        }
    }
}

void exit_gate()
{
    // Servo and Infrared Configuration
    irVal_2 = digitalRead(ir2);
    int end_1;
    int end_2;

    if (Firebase.ready() && signupOK)
    {
        unsigned long setFlow = millis();
        unsigned long getFlow = millis();
        unsigned long setFlowPrev = 0;
        unsigned long getFlowPrev = 0;

        if (getFlow - getFlowPrev >= getInterval || getFlowPrev == 0)
        {

            if (Firebase.RTDB.getInt(&slotsRef, "Ultrasonic/Slot_A1/end_1"))
                end_1 = slotsRef.intData();
            else
            {
                Serial.println("Failed to update end_1.");
                Serial.println("Reason: " + fbdo.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (setFlow - setFlowPrev >= setInterval || setInterval == 0)
        {
            if (Firebase.RTDB.setInt(&slotsRef, "Exit_Gates/Ir_Realtime", irVal_2))
            {
                Serial.print("Ir_Realtime : ");
                Serial.println(irVal_2);
            }
            else
            {
                Serial.println("Failed to update available slots.");
                Serial.println("Reason: " + slotsRef.errorReason());
            }
            setFlowPrev = setFlow
        }

        if (getFlow - getFlowPrev >= getInterval)
        {
            if (Firebase.RTDB.getInt(&slotsRef, "Ultrasonic/Slot_A2/end_2"))
                end_2 = slotsRef.intData();
            else
            {
                Serial.println("Failed to update end_2.");
                Serial.println("Reason: " + fbdo.errorReason());
            }
            getFlowPrev = getFlow
        }

        if (getFlow - getFlowPrev >= getInterval)
        {

            Firebase.RTDB.getInt(&slotsRef, "Exit_Gates/Ir_Exit");
            if (slotsRef.intData() == 0 && (end_1 == 1 || end_2 == 1))
            {
                // Open the exit gate
                myservo.write(0); // Open the exit gate
                Serial.println("Exit gate opened.");
            }
            else
                myservo.write(180);

            getFlowPrev = getFlow
        }

        if (end_1 == 1)
        {
            if (setFlow - setFlowPrev >= setInterval)
            {
                // If end_1 is 1, set booked_1 to 0
                if (Firebase.RTDB.setInt(&slotsRef, "Ultrasonic/Slot_A1/booked_1", 0))
                    Serial.println("Slot A1 booked_1 set to 0.");
                else
                {
                    Serial.println("Failed to update booked_1.");
                    Serial.println("Reason: " + fbdo.errorReason());
                }
                setFlowPrev = setFlow
            }
        }

        if (end_2 == 1)
        {
            if (setFlow - setFlowPrev >= setInterval)
            {

                if (Firebase.RTDB.setInt(&slotsRef, "Ultrasonic/Slot_A2/booked_2", 0))
                    Serial.println("Slot A2 booked_2 set to 0.");
                else
                {
                    Serial.println("Failed to update booked_2.");
                    Serial.println("Reason: " + fbdo.errorReason());
                }
                setFlowPrev = setFlow
            }
            // If end_2 is 1, set booked_2 to 0
        }
    }
}

void updateSlotInfo(int availableSlots)
{
    if (Firebase.ready() && signupOK)
    {
        if (Firebase.RTDB.setInt(&slotsRef, "Parking_Slots/Remaining", availableSlots))
        {
            Serial.print("Available Slots: ");
            Serial.println(availableSlots);
        }
        else
        {
            Serial.println("Failed to update available slots.");
            Serial.println("Reason: " + slotsRef.errorReason());
        }
    }
}

void validate_otp()
{
    if (Firebase.ready() && signupOK)
    {
        if (Firebase.RTDB.getString(&slotsRef, "entered_otp"))
        {
            entered_otp = slotsRef.stringData();
            Serial.print("Entered_Otp: ");
            Serial.println(entered_otp);
        }
        else
        {
            Serial.println("Failed to update available slots.");
            Serial.println("Reason: " + slotsRef.errorReason());
        }
    }
}

void loop()
{

    if (distance_cm1 <= STD_DISTANCE && distance_cm2 <= STD_DISTANCE)
    {
        // Both slots are filled
        slot = 0; // Update slot count to indicate all slots occupied
    }
    else if (distance_cm1 <= STD_DISTANCE || distance_cm2 <= STD_DISTANCE)
    {
        // One of the slots is filled, the other is empty
        slot = 1; // Update slot count to indicate 1 slot available
    }
    else
    {
        // Both slots are empty
        slot = 2; // Update slot count to indicate 2 slots available
    }

    updateSlotInfo(slot); // Update initial slot information
    entering_gate();
    ultrasonic_1();
    ultrasonic_2();
    exit_gate();
    lcd.setCursor(3, 0);
    lcd.print("Remaining : ");
    lcd.setCursor(16, 0);
    lcd.print(slot);
    lcd.setCursor(9, 1);
    lcd.print("|");
    lcd.setCursor(9, 2);
    lcd.print("|");
    Serial.println("______________________________");
    validate_otp();
    Serial.println("______________________________");
}
