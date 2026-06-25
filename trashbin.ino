#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <WiFi.h>

// =========================================================================
// 1. PENGATURAN WI-FI & MQTT
// =========================================================================
const char *ssid = "REDMI NOT 9";
const char *password = "11111111";
const char *mqtt_server = "192.168.43.208"; // IP Laptop Kamu
const char *mqtt_topic = "trashbin/status";

// =========================================================================
// 2. PENGATURAN PIN HARDWARE
// =========================================================================
// Sensor Dalam (Kapasitas Sampah)
const int trigPinDalam = 18;
const int echoPinDalam = 5;

// Sensor Luar (Deteksi Tangan)
const int trigPinLuar = 16;
const int echoPinLuar = 17;

// Motor Servo 1 & 2 - BARU
const int servoPin1 = 23;
const int servoPin2 = 22;
Servo tutupBin1;
Servo tutupBin2;

// =========================================================================
// 3. VARIABEL KALIBRASI & LOGIKA TIMER
// =========================================================================
const int JARAK_KOSONG = 50;
const int JARAK_PENUH = 5;

unsigned long lastReconnectAttempt = 0;
unsigned long lastPublishTime = 0;
unsigned long waktuBukaTutup = 0;
bool statusTongTerbuka = false;

WiFiClient espClient;
PubSubClient client(espClient);

int bacaJarak(int pinTrig, int pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  long duration = pulseIn(pinEcho, HIGH, 30000);
  int distance = duration * 0.034 / 2;
  return (distance == 0) ? 999 : distance;
}

int filterMovingAverage() {
  static int sampel[5] = {0};
  static int indeks = 0;

  int jarakMentah = bacaJarak(trigPinDalam, echoPinDalam);
  if (jarakMentah == 999)
    jarakMentah = JARAK_KOSONG;

  sampel[indeks] = jarakMentah;
  indeks = (indeks + 1) % 5;

  long total = 0;
  for (int i = 0; i < 5; i++)
    total += sampel[i];
  return total / 5;
}

int hitungKapasitasPersen(int jarakTerfilter) {
  if (jarakTerfilter > JARAK_KOSONG)
    jarakTerfilter = JARAK_KOSONG;
  if (jarakTerfilter < JARAK_PENUH)
    jarakTerfilter = JARAK_PENUH;
  return ((JARAK_KOSONG - jarakTerfilter) * 100) / (JARAK_KOSONG - JARAK_PENUH);
}

void setup_wifi() {
  Serial.print("\nMenghubungkan ke ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
}

boolean reconnectMQTT() {
  if (client.connect("ESP32_SmartTrashBin")) {
    Serial.println("TERHUBUNG KEMBALI KE BROKER!");
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPinDalam, OUTPUT);
  pinMode(echoPinDalam, INPUT);
  pinMode(trigPinLuar, OUTPUT);
  pinMode(echoPinLuar, INPUT);

  // Inisialisasi Dua Servo
  tutupBin1.setPeriodHertz(50);
  tutupBin2.setPeriodHertz(50);

  tutupBin1.attach(servoPin1, 500, 2400);
  tutupBin2.attach(servoPin2, 500, 2400);

  // Tutup saat pertama kali nyala (0 derajat)
  tutupBin1.write(0);
  tutupBin2.write(0);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  unsigned long currentMillis = millis();

  if (!client.connected()) {
    if (currentMillis - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = currentMillis;
      if (reconnectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }

  // LOGIKA BUKA TUTUP SERVO GANDA
  int jarakTangan = bacaJarak(trigPinLuar, echoPinLuar);

  if (jarakTangan < 15 && !statusTongTerbuka) {
    // Buka kedua servo
    tutupBin1.write(90);
    tutupBin2.write(90);

    statusTongTerbuka = true;
    waktuBukaTutup = currentMillis;
    Serial.println("Tangan terdeteksi! Membuka tong...");
  }

  if (statusTongTerbuka && (currentMillis - waktuBukaTutup >= 3000)) {
    // Tutup kembali kedua servo
    tutupBin1.write(0);
    tutupBin2.write(0);

    statusTongTerbuka = false;
    Serial.println("Menutup tong kembali...");
  }

  // PENGIRIMAN DATA MQTT
  if (client.connected() && (currentMillis - lastPublishTime >= 2000)) {
    lastPublishTime = currentMillis;

    int jarakMatang = filterMovingAverage();
    int kapasitasPersen = hitungKapasitasPersen(jarakMatang);

    StaticJsonDocument<128> doc;
    doc["jarak_cm"] = jarakMatang;
    doc["kapasitas_persen"] = kapasitasPersen;
    doc["status"] = (kapasitasPersen >= 90) ? "PENUH" : "AMAN";

    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    client.publish(mqtt_topic, jsonBuffer);
  }
}