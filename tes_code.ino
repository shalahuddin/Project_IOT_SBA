/*******************************************************************************
 * core 1 : digunakan untuk membaca alat dan mematikan RFID
 * core 2 : digunakan untuk mengirim data
 *
 *******************************************************************************/

#include <WiFi.h>
#include <PubSubClient.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <RTClib.h>
// loRa_serialization
// #include "LoraMessage.h"
#include "LoraEncoder.h"  // encode pesan


// Pin konfigurasi
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define CS_PIN 4
#define RELAY_PIN 25

// Konfigurasi WiFi
const char* ssid = "CrusherLimestone";
const char* password = "Crusher123";


#define NUM_LEDS 1  //
#define DATA_PIN 5  //
CRGB leds[NUM_LEDS]; //

// Deklarasi tugas
TaskHandle_t Task1;
TaskHandle_t Task2;


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
  // Inisialisasi komunikasi serial
  Serial.begin(115200);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  // Inisialisasi FastLED
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  // Membuat Task1 yang berjalan pada core 1 untuk mengambil waktu
  xTaskCreatePinnedToCore(
    Task1code,   /* Fungsi task */
    "Task1",     /* Nama task */
    10000,       /* Ukuran stack */
    NULL,        /* Parameter task */
    1,           /* Prioritas task */
    &Task1,      /* Handle task */
    1);          /* Core di mana task berjalan (core 1) */
    
  delay(500); 

  // Membuat Task2 yang berjalan pada core 0 untuk menyalakan lampu LED
  xTaskCreatePinnedToCore(
    Task2code,   /* Fungsi task */
    "Task2",     /* Nama task */
    10000,       /* Ukuran stack */
    NULL,        /* Parameter task */
    1,           /* Prioritas task */
    &Task2,      /* Handle task */
    0);          /* Core di mana task berjalan (core 0) */
    
  delay(500); 
}

// Fungsi untuk Task1: Membaca sensor
void Task1code( void * pvParameters ){
  Serial.print("Task1 berjalan di core: ");
  Serial.println(xPortGetCoreID());
  //
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

  // Loop
  for(;;){
    // Handle serial input untuk vehicleData
    if (Serial.available() > 0) {
      lastReceiveTime = millis();  // Reset waktu ketika data mulai diterima
      
      while (Serial.available() > 0) {
        int data = Serial.read();
        if (data < 0x10) {
          vehicleData += "0";
        }
        vehicleData += String(data, HEX);  // data mobil
        vehicleData += " ";
      }
    }
    timeClient.update();
    Serial.print("Current time: ");
    Serial.println(timeClient.getFormattedTime());
    delay(1000);  // Update setiap 1 detik
  } 
}

// Fungsi untuk Task2: Menyalakan LED warna hijau di core 0
void Task2code( void * pvParameters ){
  Serial.print("Task2 berjalan di core: ");
  Serial.println(xPortGetCoreID());

  for(;;){
    // Loop through rainbow colors
    Serial.print("core: ");
    Serial.println(xPortGetCoreID());
  for (int i=0; i<=255;i++) {
    leds[0] = CHSV( i, 255, 100);
    FastLED.show();
    delay(16);
  }
    delay(500);  // Update setiap 500 ms
  }
}

void loop() {
  // Tidak ada kode dalam loop utama
}