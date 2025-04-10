#include "esp_camera.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <HTTPClient.h>

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
String triggerPath;

// Server URL for face recognition
String serverUrl = "https://atm-automation.onrender.com/face_recognition";

// GPIO pins
#define LED_PIN 2 // Flashlight LED pin

// Function declarations
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);

void setup()
{
  Serial.begin(115200);

  // Configure button and LED pins
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

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

  // Initialize Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;  
  config.xclk_freq_hz = 16000000;   // Optimized for stability
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // Optimized Quality and Performance
  config.frame_size = FRAMESIZE_FHD;  // 1920x1080
  config.jpeg_quality = 12;           // Balances size & quality
  config.fb_count = 2;

  if (esp_camera_init(&config) != ESP_OK)
  {
    Serial.println("Camera initialization failed!");
    return;
  }
  Serial.println("Camera initialized successfully!");

  // Apply Advanced Camera Settings
  sensor_t *s = esp_camera_sensor_get();
  if (s)
  {
    s->set_brightness(s, 1);       // Slight brightness increase
    s->set_contrast(s, 2);         // Enhanced contrast
    s->set_saturation(s, 2);       // Color saturation boost
    s->set_gainceiling(s, (gainceiling_t)4); // Better exposure balance
    s->set_whitebal(s, 1);         // Enable white balance
    s->set_awb_gain(s, 1);         // Enable AWB gain
    s->set_exposure_ctrl(s, 1);    // Enable auto exposure
    s->set_gain_ctrl(s, 1);        // Enable auto gain control
    s->set_lenc(s, 1);             // Enable lens correction
    s->set_hmirror(s, 1);          // Enable horizontal mirroring (if needed)
    s->set_vflip(s, 1);            // Enable vertical flip (if mounted upside down)
  }
  Serial.println("Camera settings applied!");

  // Start Firebase streaming
  if (!Firebase.RTDB.beginStream(&fbdoStream, triggerPath))
  {
    Serial.printf("Stream failed: %s\n", fbdoStream.errorReason().c_str());
    {
      // Reset the Firebase action trigger
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

void captureForFaceRec(){
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    digitalWrite(LED_PIN, LOW);
    return;
  }
  esp_camera_fb_return(fb);
  delay(200);
  fb = esp_camera_fb_get();
  if (!fb)
  {
      Serial.println("Failed to capture fresh image");
      return;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "image/jpeg");
    int httpResponseCode = http.POST(fb->buf, fb->len);

    if (httpResponseCode > 0)
    {
      Serial.printf("Image uploaded: %s\n", http.getString().c_str());
    }
    else
    {
      Serial.printf("Image upload failed: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi not connected");
  }
  esp_camera_fb_return(fb);
}

void streamCallback(FirebaseStream data)
{
  Serial.println("Stream event received!");
  if (data.dataType() == "string")
  {
    String action = data.stringData();
    Serial.printf("Action received: %s\n", action.c_str());
    if (action == "face_rec"){
      Serial.println("Face recognition request received from Firebase");
      captureForFaceRec();
    }
    {
      // Reset the Firebase action trigger
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

void loop()
{
  // No polling required, stream handles everything
}