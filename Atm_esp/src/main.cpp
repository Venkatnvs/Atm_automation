#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <ESP32Servo.h>

// Replace with your Wi-Fi credentials
const char *ssid = "Project";
const char *password = "12345678";

// Firebase configuration
#define DATABASE_URL "https://atm-54854-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define API_KEY "AIzaSyBOMfnk1RfGoeBmXXViTfdwakJgN75Uq0w"

// Define Firebase objects
FirebaseData fbdoStream, fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Firebase real-time database paths
#define RTDB_PATH "/esp/"
String mainPath = RTDB_PATH;
String currentID;
String triggerPath;

#define SS_PIN 5     // Chip Select pin for SPI communication
#define SERVO_PIN 15  // Servo connected to pin 15
#define RED 12
#define GREEN 14
#define YELLOW 13
#define BUZZER 27

MFRC522DriverPinSimple ss_pin(SS_PIN);
MFRC522DriverSPI driver{ss_pin};    // Create SPI driver
MFRC522 mfrc522{driver};            // Create MFRC522 instance
Servo myServo;                      // Servo object 

// Function declarations
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RED, HIGH);
  digitalWrite(GREEN, HIGH);
  digitalWrite(YELLOW, HIGH);
  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(YELLOW, LOW);
  digitalWrite(BUZZER, LOW);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 3;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  auth.user.email = "venkatnvs2005@gmail.com";
  auth.user.password = "venkat123";
  Firebase.begin(&config, &auth);

  // Wait for Firebase to be ready
  int retryCount = 0;
  while (!Firebase.ready() && retryCount < 5)
  {
    delay(500);
    retryCount++;
    Serial.print(".");
  }
  Serial.println("\nFirebase ready!");

  triggerPath = mainPath;
  triggerPath += "/triggers/action";

  currentID = mainPath;
  currentID += "/current_id";

  mfrc522.PCD_Init();    // Initialize MFRC522 module
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);  // Display version info

  Serial.println(F("Scan PICC to see UID"));

  // Attach servo to pin 15
  myServo.attach(SERVO_PIN);

  // Start Firebase streaming
  if (!Firebase.RTDB.beginStream(&fbdoStream, triggerPath))
  {
    Serial.printf("Stream failed: %s\n", fbdoStream.errorReason().c_str());
    {
      if (!Firebase.RTDB.setString(&fbdo, triggerPath, "none"))
      {
        Serial.printf("Failed to reset trigger: %s\n", fbdo.errorReason().c_str());
      }
    }
  }
  else
  {
    Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  }
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(100);
    return;
  }
  Serial.print("Card UID: ");
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uidString += "0"; 
    }
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  
  Serial.println(uidString);
  Serial.println();
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  if (uidString.length() > 0){
    digitalWrite(YELLOW, HIGH);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);
    if (!Firebase.RTDB.setString(&fbdo, currentID, uidString)){
      Serial.printf("Failed to set current ID: %s\n", fbdo.errorReason().c_str());
    }
    if (!Firebase.RTDB.setString(&fbdo, triggerPath, "face_rec")){
      Serial.printf("Failed to set trigger: %s\n", fbdo.errorReason().c_str());
    }
  }

  delay(500);
}

void streamCallback(FirebaseStream data)
{
  Serial.println("Stream event received!");
  if (data.dataType() == "string")
  {
    String action = data.stringData();
    Serial.printf("Action received: %s\n", action.c_str());
    if (action == "open_door")
    {
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, LOW);
      digitalWrite(YELLOW, LOW);
      Serial.println("Open door request received from Firebase");
      myServo.write(0);
      delay(5000);
      myServo.write(90);
    }
    if (action == "failed_rec")
    {
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, HIGH);
      digitalWrite(YELLOW, LOW);
      Serial.println("Face recognition failed");
      digitalWrite(BUZZER, HIGH);
      delay(2000);
      digitalWrite(BUZZER, LOW);
    }
    if (action == "success_rec")
    {
      digitalWrite(GREEN, HIGH);
      digitalWrite(RED, LOW);
      digitalWrite(YELLOW, LOW);
      Serial.println("Face recognition successful");
    }
    if (action != "face_rec")
    {
      if (!Firebase.RTDB.setString(&fbdo, triggerPath, "none"))
      {
        Serial.printf("Failed to reset trigger: %s\n", fbdo.errorReason().c_str());
      }
    }
  }
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
  {
    Serial.println("Stream timeout occurred, reconnecting...");
  }
  else
  {
    Serial.println("Stream disconnected, trying to reconnect...");
  }
  Firebase.RTDB.beginStream(&fbdoStream, triggerPath);
}