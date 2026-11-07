#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- ΡΥΘΜΙΣΕΙΣ PINS & ΣΕΙΡΙΑΚΩΝ ---
#define LORA_RX_PIN 16
#define LORA_TX_PIN 17

#define SBUS_RX_PIN 27 // Σύνδεσε το καλώδιο Signal του δέκτη (SBUS) στο pin 27
#define SBUS_TX_PIN 26 // Δεν χρησιμοποιείται αλλά χρειάζεται στη δήλωση

// --- ΡΥΘΜΙΣΕΙΣ WIFI & MQTT ---
const char* ssid = "Masterio";
const char* password = "masterakhs95";
const char* mqtt_server = "192.168.1.100";
const int   mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// --- ΜΕΤΑΒΛΗΤΕΣ LORA ---
struct __attribute__((packed)) TelemetryPacket {
  float MainVoltage;
  float MainCurrent;
  float SVoltage;
  float SCurrent;
  float GSpeed;
  uint8_t SatCount;
  float Latitude;
  float Longitude;
  float Altitude;
  float HomeDist;
  float PiTemp;
};

TelemetryPacket telemetry;

// --- ΜΕΤΑΒΛΗΤΕΣ SBUS ---
uint8_t sbusData[25];
uint16_t channels[16]; 
int pwm[16];
int byteIndex = 0;

void setup_wifi() {
  delay(10);
  Serial.printf("\nΣύνδεση στο WiFi: %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Συνδέθηκε!");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Ground_Station")) {
      Serial.println("✅ Συνδέθηκε στο MQTT Broker!");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  // 1. USB Serial Monitor
  Serial.begin(115200);
  
  // 2. LoRa UART (Serial2)
  Serial2.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  
  // 3. SBUS UART (Serial1) - Με Hardware Inversion (true)
  Serial1.begin(100000, SERIAL_8E2, SBUS_RX_PIN, SBUS_TX_PIN, true);
  
  delay(1000);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("🚀 ESP32 Ground Station Έτοιμο (LoRa + SBUS)!");
}

void loop() {
  // --- 1. ΔΙΑΤΗΡΗΣΗ MQTT ---
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // --- 2. ΛΗΨΗ & PUBLISH LORA ---
  if (Serial2.available() >= sizeof(TelemetryPacket)) {
    Serial2.readBytes((uint8_t*)&telemetry, sizeof(TelemetryPacket));

    // Καθαρισμός buffer 
    while(Serial2.available()) Serial2.read();

    char jsonPayload[512];
    snprintf(jsonPayload, sizeof(jsonPayload),
             "{\"MainVoltage\":%.2f,\"MainCurrent\":%.2f,\"SVoltage\":%.2f,\"SCurrent\":%.2f,"
             "\"GSpeed\":%.1f,\"SatCount\":%d,\"Latitude\":%.6f,\"Longitude\":%.6f,"
             "\"Altitude\":%.1f,\"HomeDist\":%.1f,\"PiTemp\":%.1f}",
             telemetry.MainVoltage, telemetry.MainCurrent, telemetry.SVoltage, telemetry.SCurrent,
             telemetry.GSpeed, telemetry.SatCount, telemetry.Latitude, telemetry.Longitude,
             telemetry.Altitude, telemetry.HomeDist, telemetry.PiTemp);

    client.publish("daidalus/telemetry/state", jsonPayload);
    
    // Εμφάνιση μόνο μιας μικρής επιβεβαίωσης στο Serial για το LoRa
    Serial.printf("📡 [LoRa RX] Volts: %.2fV | Alt: %.1fm -> MQTT OK\n", telemetry.MainVoltage, telemetry.Altitude);
  }

  // --- 3. ΛΗΨΗ & ΑΠΟΚΩΔΙΚΟΠΟΙΗΣΗ SBUS ---
  if (Serial1.available() > 0) {
    uint8_t incomingByte = Serial1.read();
    
    if (byteIndex == 0 && incomingByte != 0x0F) {
      return;
    }

    sbusData[byteIndex] = incomingByte;
    byteIndex++;

    if (byteIndex == 25) {
      byteIndex = 0; 
      uint8_t endByte = sbusData[24];
      
      if (endByte == 0x00 || endByte == 0x04 || endByte == 0x14 || endByte == 0x24 || endByte == 0x34) {
        
        channels[0]  = ((sbusData[1]       | sbusData[2] << 8)                   & 0x07FF);
        channels[1]  = ((sbusData[2] >> 3  | sbusData[3] << 5)                   & 0x07FF);
        channels[2]  = ((sbusData[3] >> 6  | sbusData[4] << 2  | sbusData[5] << 10) & 0x07FF);
        channels[3]  = ((sbusData[5] >> 1  | sbusData[6] << 7)                   & 0x07FF);
        channels[4]  = ((sbusData[6] >> 4  | sbusData[7] << 4)                   & 0x07FF);
        channels[5]  = ((sbusData[7] >> 7  | sbusData[8] << 1  | sbusData[9] << 9)  & 0x07FF);
        channels[6]  = ((sbusData[9] >> 2  | sbusData[10] << 6)                  & 0x07FF);
        channels[7]  = ((sbusData[10] >> 5 | sbusData[11] << 3)                  & 0x07FF);
        channels[8]  = ((sbusData[12]      | sbusData[13] << 8)                  & 0x07FF);
        channels[9]  = ((sbusData[13] >> 3 | sbusData[14] << 5)                  & 0x07FF);
        channels[10] = ((sbusData[14] >> 6 | sbusData[15] << 2 | sbusData[16] << 10) & 0x07FF);
        channels[11] = ((sbusData[16] >> 1 | sbusData[17] << 7)                  & 0x07FF);
        channels[12] = ((sbusData[17] >> 4 | sbusData[18] << 4)                  & 0x07FF);
        channels[13] = ((sbusData[18] >> 7 | sbusData[19] << 1 | sbusData[20] << 9)  & 0x07FF);
        channels[14] = ((sbusData[20] >> 2 | sbusData[21] << 6)                  & 0x07FF);
        channels[15] = ((sbusData[21] >> 5 | sbusData[22] << 3)                  & 0x07FF);

        for (int i = 0; i < 16; i++) {
          pwm[i] = map(channels[i], 172, 1811, 1000, 2000);
          pwm[i] = constrain(pwm[i], 800, 2200); 
        }

        // Εκτύπωση των PWM τιμών κάθε 1 δευτερόλεπτο για να μην πλημμυρίσει το Serial
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 1000) {
          lastPrint = millis();
          Serial.printf("🎮 SBUS CH1-4: %4d | %4d | %4d | %4d\n", pwm[0], pwm[1], pwm[2], pwm[3]);
        }
      }
    }
  }
}