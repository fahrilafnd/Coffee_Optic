

#include <ESP32Servo.h>
#include "HX711.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ============ WIFI & MQTT ===============
const char* ssid = "Redmi 12";
const char* password = "12345678";
const char* mqttServer = "103.127.97.36";
const int mqttPort = 1883;
const char* topicStatus = "coffeoptic/alat/status";
const char* topicData = "coffeoptic/alat/data";
const char* mqtt_user = "CoffeOptic";
const char* mqtt_pass = "010805";

WiFiClient espClient;
PubSubClient client(espClient);

// ============ Sensor & Motor Pin ==========
#define S0 23
#define S1 25
#define S2 4
#define S3 2
#define sensorOut 34

int motorConveyor = 26;
int motorGetar = 27;

#define SERVO_PIN 13
Servo myServo;

#define DT 15
#define SCK 5
HX711 scale;

// ============== Data & Variabel ===============
int red = 0, green = 0, blue = 0;
int jumlahTersortir = 0;
float beratBagus = 0;
bool alatAktif = true;
bool statusSebelumnya = true;
bool dataFinalSudahDikirim = false;

String lastPayload = "";

// ============== SETUP ===================
void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(0));

  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  pinMode(S0, OUTPUT); pinMode(S1, OUTPUT);
  pinMode(S2, INPUT); pinMode(S3, INPUT);
  pinMode(sensorOut, INPUT);
  digitalWrite(S0, HIGH); digitalWrite(S1, LOW);

  pinMode(motorConveyor, OUTPUT);
  pinMode(motorGetar, OUTPUT);
  digitalWrite(motorConveyor, HIGH);
  digitalWrite(motorGetar, HIGH);

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  Serial.println("Inisialisasi HX711...");
  scale.begin(DT, SCK);

  unsigned long startTime = millis();
  while (!scale.is_ready()) {
    if (millis() - startTime > 10000) {
      Serial.println("ERROR: HX711 tidak terdeteksi!");
      Serial.println("Melanjutkan tanpa load cell...");
      break;
    }
    delay(100);
    Serial.print(".");
  }

  if (scale.is_ready()) {
    Serial.println("\nHX711 siap!");
    Serial.println("Kalibrasi HX711...");
    scale.set_scale(456.0); // Ubah sesuai hasil kalibrasi aktual
    scale.tare();
    Serial.println("Kalibrasi selesai!");
  } else {
    Serial.println("HX711 tidak aktif - menggunakan data random untuk berat");
  }

  Serial.println("Sistem siap mendeteksi...");
}

// ============== LOOP ====================
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (statusSebelumnya == true && alatAktif == false && !dataFinalSudahDikirim) {
    kirimDataFinal();
    dataFinalSudahDikirim = true;
    Serial.println("Data final telah dikirim setelah alat dimatikan!");
  }

  if (alatAktif == true && statusSebelumnya == false) {
    dataFinalSudahDikirim = false;
    Serial.println("Alat dinyalakan kembali, sistem siap untuk sesi baru!");
  }

  statusSebelumnya = alatAktif;

  if (!alatAktif) {
    stopMotor();
    delay(1000);
    return;
  }

  digitalWrite(motorConveyor, LOW);
  digitalWrite(motorGetar, LOW);
  delay(1000);

  float berat = 0;
  if (scale.is_ready()) {
    berat = scale.get_units(5);
    if (berat < 0.01 || berat > 1000.0) berat = 0.0;
  } else {
    Serial.println("WARNING: HX711 tidak siap saat membaca berat.");
    berat = random(0, 501) / 100.0;
  }
  beratBagus = berat;

  digitalWrite(S2, LOW); digitalWrite(S3, LOW);
  red = pulseIn(sensorOut, LOW);
  delay(100);
  digitalWrite(S2, HIGH); digitalWrite(S3, HIGH);
  green = pulseIn(sensorOut, LOW);
  delay(100);
  digitalWrite(S2, LOW); digitalWrite(S3, HIGH);
  blue = pulseIn(sensorOut, LOW);
  delay(100);

  Serial.print("Red: "); Serial.print(red);
  Serial.print(" | Green: "); Serial.print(green);
  Serial.print(" | Blue: "); Serial.println(blue);

  bool bijiBuruk = false;
  String nama = "biji_bagus";

  if ((red > 50 && red < 105) && (green > 70 && green < 130) && (blue > 75 && blue < 145)) {
    Serial.println(" >> COKLAT (biji buruk)");
    bijiBuruk = true;
    nama = "biji_coklat";
  } else if ((red > 200 && red < 255) && (green > 200 && green < 255) && (blue > 200 && blue < 255)) {
    Serial.println(" >> HITAM (biji buruk)");
    bijiBuruk = true;
    nama = "biji_hitam";
  }

  if (bijiBuruk) {
    gerakServo();
    jumlahTersortir++;
  }

  Serial.print("Jumlah tersortir sementara: "); Serial.println(jumlahTersortir);
  Serial.print("Berat biji bagus sementara: "); Serial.print(beratBagus); Serial.println(" gram");
  Serial.println("------------------------------------");

  delay(1500);
}

// ============== FUNGSI TAMBAHAN ==================
void kirimDataFinal() {
  String payload = "{";
  payload += "\"jumlah\":" + String(jumlahTersortir) + ",";
  payload += "\"berat\":" + String(beratBagus, 2) + ",";
  payload += "\"status\":\"final\",";
  payload += "\"timestamp\":\"" + String(millis()) + "\"";
  payload += "}";

  if (client.publish(topicData, payload.c_str())) {
    Serial.println("=== DATA FINAL BERHASIL DIKIRIM ===");
    Serial.println("Payload: " + payload);
  } else {
    Serial.println("=== GAGAL MENGIRIM DATA FINAL ===");
  }
}

void gerakServo() {
  myServo.write(70);
  delay(2500);
  myServo.write(0);
  delay(500);
}

void stopMotor() {
  digitalWrite(motorConveyor, HIGH);
  digitalWrite(motorGetar, HIGH);
}

void setup_wifi() {
  delay(10);
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terkoneksi. IP:");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("terhubung.");
      client.subscribe(topicStatus);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik.");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Pesan dari topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  if (String(topic) == topicStatus) {
    if (message == "ON") {
      alatAktif = true;
      Serial.println("Alat dinyalakan");
    } else if (message == "OFF") {
      alatAktif = false;
      Serial.println("Alat dimatikan");
    }
  }
}

