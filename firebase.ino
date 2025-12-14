
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include <addons/TokenHelper.h>

#include <addons/RTDBHelper.h>

#define WIFI_SSID "WIFI SSID"
#define WIFI_PASSWORD "WIFI PASS"

#define API_KEY "" //add web app on project settings to get api key
#define DATABASE_URL "" //get in on realtime db section
#define USER_EMAIL "" //add authentication feature in firebase
#define USER_PASSWORD ""

// Define Firebase Data object
FirebaseData db; //manual db access, non stream / subscribe
FirebaseData statusStream; //subscribe to /status
FirebaseData minutesStream; //subscribe to /minutes
FirebaseData secondsStream; //subscribe to /seconds
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;

// Timer variables
int minutes = 0;
int seconds = 0;
bool isOn = false;
unsigned long lastTickMillis = 0;

#define RELAY_PIN 5
#define BUTTON_PIN 4
void setup()
{

  Serial.begin(115200);
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

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  Firebase.reconnectNetwork(true);
  db.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  db.setResponseSize(2048);
  statusStream.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  statusStream.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Firebase.RTDB.beginStream(&statusStream, "/status");
  Firebase.RTDB.setStreamCallback(&statusStream, statusCallback, timeoutcallback);
  
  // Setup minutes stream
  minutesStream.setBSSLBufferSize(4096, 1024);
  minutesStream.setResponseSize(2048);
  Firebase.RTDB.beginStream(&minutesStream, "/minutes");
  Firebase.RTDB.setStreamCallback(&minutesStream, minutesCallback, timeoutcallback);
  
  // Setup seconds stream
  secondsStream.setBSSLBufferSize(4096, 1024);
  secondsStream.setResponseSize(2048);
  Firebase.RTDB.beginStream(&secondsStream, "/seconds");
  Firebase.RTDB.setStreamCallback(&secondsStream, secondsCallback, timeoutcallback);
  
  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
}

void statusCallback(FirebaseStream data)
{
    Serial.print("Status Path: ");
    Serial.println(data.streamPath());
    Serial.print("Type: ");
    Serial.println(data.dataType());
    Serial.print("Data: ");
    Serial.println(data.stringData());
    // Parse status as bool and set RELAY_PIN accordingly
    if (data.dataType() == "boolean") {
      isOn = data.boolData();
      digitalWrite(RELAY_PIN, isOn ? HIGH : LOW);
      Serial.print("Relay set to: ");
      Serial.println(isOn ? "ON" : "OFF");
    }
}

void minutesCallback(FirebaseStream data)
{
    Serial.print("Minutes Path: ");
    Serial.println(data.streamPath());
    Serial.print("Type: ");
    Serial.println(data.dataType());
    
    if (data.dataType() == "int") {
      minutes = data.intData();
      Serial.print("Minutes set to: ");
      Serial.println(minutes);
    }
}

void secondsCallback(FirebaseStream data)
{
    Serial.print("Seconds Path: ");
    Serial.println(data.streamPath());
    Serial.print("Type: ");
    Serial.println(data.dataType());
    
    if (data.dataType() == "int") {
      seconds = data.intData();
      Serial.print("Seconds set to: ");
      Serial.println(seconds);
    }
}

void timeoutcallback(bool timeout)
{
    if (timeout)
        Serial.println("Stream timeout, reconnecting...");
}
bool doorState;
void loop()
{  
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready, waiting...");
    delay(1000);
    return;
  }
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState != doorState) {
      if (Firebase.RTDB.setBool(&db, "/door", buttonState)) {
        Serial.printf("Updated /door to %s\n", buttonState ? "true" : "false");
      } else {
        Serial.printf("Failed to update /door: %s\n", db.errorReason().c_str());
      }
  }

  doorState = buttonState;

  // Timer countdown logic
  if (isOn && (minutes > 0 || seconds > 0)) {
    unsigned long now = millis();
    if (now - lastTickMillis >= 1000) {
      lastTickMillis = now;
      
      if (seconds == 0) {
        if (minutes > 0) {
          minutes--;
          seconds = 59;
        }
      } else {
        seconds--;
      }
      
      // Update Firebase with new timer values
      Firebase.RTDB.setInt(&db, "/minutes", minutes);
      Firebase.RTDB.setInt(&db, "/seconds", seconds);
      
      Serial.printf("Timer: %02d:%02d\n", minutes, seconds);
      
      // Timer reached 0, turn off relay
      if (minutes == 0 && seconds == 0) {
        isOn = false;
        digitalWrite(RELAY_PIN, LOW);
        Firebase.RTDB.setBool(&db, "/status", false);
        Serial.println("Timer finished! Relay OFF");
      }
    }
  } else {
    lastTickMillis = millis(); // Reset tick when not counting
  }

  delay(50);
}
