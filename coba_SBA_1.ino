#include <WiFi.h>
#include <PubSubClient.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <RTClib.h>

// add
#include <ArduinoJson.h> // Add ArduinoJson library

// Pin konfigurasi
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define CS_PIN 4
#define RELAY_PIN 25

// Konfigurasi WiFi
const char* ssid = "CrusherLimestone";
const char* password = "Crusher123";

// Konfigurasi broker MQTT
const char* mqtt_broker = "34.101.175.32";
const char* topic_status = "QUARRY/RFID/STATUS";
const char* topic_control = "QUARRY/RFID/CONTROL";
const char* mqtt_username = "";
const char* mqtt_password = "";
const int mqtt_port = 1883;

// Objek global
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 3000;  // Interval pengiriman data dalam milidetik (3 detik)

unsigned long lastRelayPublishTime = 0;  // Untuk relay status
const unsigned long relayPublishInterval = 3000;  // Interval pengiriman status relay

WiFiClient espClient;
PubSubClient client(espClient);
RTC_DS3231 rtc;
bool relayState = false;  // Status relay, default is OFF
String rfidNumber = "RFID-LOC-A";  // Nilai tetap untuk rfid_number
String vehicleData = "";  // Menyimpan data sementara dalam satu baris
unsigned long lastReceiveTime = 0;  // Waktu terakhir data diterima
const int timeout = 50;  // Waktu tunggu (ms)

// Fungsi koneksi WiFi
void setupWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

// Fungsi reconnect MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(topic_control);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Fungsi callback MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("Message arrived [" + String(topic) + "]: " + message);
  if (String(topic) == topic_control) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
      Serial.println("Failed to parse JSON");
      return;
    }
    // cek kondisi ketika pesan key "Relay" ON/OFF
    const char* relayCommand = doc["Relay"];
    if (strcmp(relayCommand, "ON") == 0) {
      controlRelay(LOW, "Relay ON");
    } else if (strcmp(relayCommand, "OFF") == 0) {
      controlRelay(HIGH, "Relay OFF");
    }
  }
}

// Fungsi kontrol relay
void controlRelay(int state, const char* message) {
  digitalWrite(RELAY_PIN, state);
  relayState = (state == LOW);  // Update relay state
  Serial.println(message);
}

// Fungsi untuk menyimpan data ke SD card
void storeDataToSD(String data) {
  File file = SD.open("/datalog.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  if (file.print(data)) {
    Serial.println("Data stored to SD card");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Fungsi mendapatkan waktu saat ini
String CurrentDateTime() {
  DateTime now = rtc.now();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buffer);
}

// Fungsi inisialisasi RTC
void initRTC() {
  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (true) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// Fungsi mendapatkan status SD card
String getSDCardStatus() {
  return SD.cardType() == CARD_NONE ? "No SD card" : "SD card present";
}

void setup() {
  Serial.begin(9600);
  setupWiFi();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(mqttCallback);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  if (!SD.begin(CS_PIN)) {
    Serial.println("Card Mount Failed");
    return;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.println("SD Card initialized");
  initRTC();
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Handle serial input untuk vehicleData
  if (Serial.available() > 0) {
    lastReceiveTime = millis();  // Reset waktu ketika data mulai diterima
    
    while (Serial.available() > 0) {
      int data = Serial.read();
      if (data < 0x10) {
        vehicleData += "0";
      }
      vehicleData += String(data, HEX);
      vehicleData += " ";
    }
  }

  // Jika tidak ada data baru dalam batas waktu tertentu, cetak string dan kirim data ke broker MQTT
  if ((millis() - lastReceiveTime) > timeout) {
    // Ubah vehicleData menjadi huruf kapital jika ada datanya
    if (vehicleData.length() > 0) {
      vehicleData.toUpperCase();
      Serial.println("vehicle_number: " + vehicleData);
    } else {
      vehicleData = "NO_DATA";  // Jika tidak ada vehicle data, kirim NO_DATA
    }

    // Periksa apakah sudah waktunya untuk mengirim data ke broker MQTT
    if (millis() - lastPublishTime >= publishInterval) {
      // Buat payload JSON
      String jsonPayload = "{\"vehicle_number\": \"" + vehicleData + "\",\"rfid_number\":\"" + rfidNumber + "\",\"sd_card_status\":\"" + getSDCardStatus() + "\",\"rtc\":\"" + CurrentDateTime() + "\"}";
      
      // Kirim data JSON ke broker MQTT
      if (client.publish(topic_status, jsonPayload.c_str())) {
        Serial.println("Message published successfully: " + jsonPayload);
      } else {
        Serial.println("Failed to publish, storing data to SD card");
        storeDataToSD(jsonPayload);
      }

      // Perbarui waktu terakhir pengiriman data
      lastPublishTime = millis();
    }

    vehicleData = "";  // Reset data
  }

  // Kirim status relay secara periodik (interval 3 detik)
  if (millis() - lastRelayPublishTime >= relayPublishInterval) {
    String controlJson = "{\"rfid_number\":\"" + rfidNumber + "\",\"active\":" + (relayState ? "true" : "false") + "}";
    client.publish(topic_status, controlJson.c_str());
    
    // Perbarui waktu terakhir pengiriman status relay
    lastRelayPublishTime = millis();
  }

  delay(2000);  // Delay untuk mengurangi beban loop
}
