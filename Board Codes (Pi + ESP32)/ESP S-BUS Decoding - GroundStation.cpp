#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

const char* ssid = "YOUR_LOCAL_WIFI_SSID";
const char* password = "YOUR_LOCAL_WIFI_PASSWORD";
const char* mqtt_server = "192.168.1.100"; // Η IP του τοπικού σου MQTT Broker (π.χ. Mosquitto)
const int   mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// Το __attribute__((packed)) απαγορεύει στον compiler να βάλει "άδεια" bytes
struct __attribute__((packed)) TelemetryPacket {
  float MainVoltage;  // 4 bytes
  float MainCurrent;  // 4 bytes
  float SVoltage;     // 4 bytes
  float SCurrent;     // 4 bytes
  float GSpeed;       // 4 bytes
  uint8_t SatCount;   // 1 byte  (To 'B' της Python)
  float Latitude;     // 4 bytes
  float Longitude;    // 4 bytes
  float Altitude;     // 4 bytes
  float HomeDist;     // 4 bytes
  float PiTemp;       // 4 bytes
};

TelemetryPacket telemetry;

void setup_wifi() {
  delay(10);
  Serial.printf("\nΣύνδεση στο WiFi: %s", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Συνδέθηκε! IP: " + WiFi.localIP().toString());
}

void reconnect() {
  // Προσπάθεια επανασύνδεσης στο MQTT αν χαθεί η σύνδεση
  while (!client.connected()) {
    Serial.print("🔄 Σύνδεση στο MQTT Broker...");
    // Βάλε mqtt_user, mqtt_pass μέσα στην connect αν χρειάζεται
    if (client.connect("ESP32_LoRa_Gateway")) {
      Serial.println(" Επιτυχία!");
    } else {
      Serial.print(" Απέτυχε, rc=");
      Serial.print(client.state());
      Serial.println(" Νέα δοκιμή σε 5 δευτερόλεπτα...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // 1. Εκκίνηση WiFi & MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // 2. Εκκίνηση LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(868E6)) {
    Serial.println("❌ Αποτυχία εκκίνησης LoRa Module!");
    while (true);
  }
  Serial.println("✅ Το LoRa Gateway είναι έτοιμο και περιμένει τηλεμετρία!");
}

void loop() {
  // Διατήρηση σύνδεσης MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Έλεγχος για εισερχόμενα πακέτα LoRa
  int packetSize = LoRa.parsePacket();
  
  if (packetSize == sizeof(TelemetryPacket)) { // Περιμένουμε ακριβώς 41 bytes
    
    // Ρουφάμε τα 41 bytes κατευθείαν μέσα στο C++ struct
    LoRa.readBytes((uint8_t*)&telemetry, sizeof(TelemetryPacket));

    // Φτιάχνουμε το JSON String για το MQTT
    char jsonPayload[512];
    snprintf(jsonPayload, sizeof(jsonPayload),
             "{\"MainVoltage\":%.2f,\"MainCurrent\":%.2f,\"SVoltage\":%.2f,\"SCurrent\":%.2f,"
             "\"GSpeed\":%.1f,\"SatCount\":%d,\"Latitude\":%.6f,\"Longitude\":%.6f,"
             "\"Altitude\":%.1f,\"HomeDist\":%.1f,\"PiTemp\":%.1f}",
             telemetry.MainVoltage, telemetry.MainCurrent, telemetry.SVoltage, telemetry.SCurrent,
             telemetry.GSpeed, telemetry.SatCount, telemetry.Latitude, telemetry.Longitude,
             telemetry.Altitude, telemetry.HomeDist, telemetry.PiTemp);

    // Δημοσίευση (Publish) στο Τοπικό Δίκτυο
    client.publish("daidalus/telemetry/state", jsonPayload);

    // Εκτύπωση στη Σειριακή Θύρα (USB Laptop) για επιβεβαίωση
    Serial.printf("📡 [LoRa RX] Alt: %.1fm | Sats: %d | PiTemp: %.1fC | Published to MQTT!\n", 
                  telemetry.Altitude, telemetry.SatCount, telemetry.PiTemp);
  }
  else if (packetSize > 0) {
    Serial.printf("⚠️ Ήρθε άγνωστο πακέτο: %d bytes\n", packetSize);
  }
}
