#include <Arduino.h>
#include <LoRa.h>

#define SBUS_RX_PIN 3
#define SBUS_TX_PIN 4 // Δεν το χρειαζόμαστε για λήψη, αλλά πρέπει να δηλωθεί

#define LORA_SS    18  // CS/NSS pin
#define LORA_RST   14  // Reset pin
#define LORA_DIO0  26  // DIO0 pin (Interrupt)

uint8_t sbusData[25]; // Ένα πλήρες πακέτο SBUS αποτελείται από 25 bytes
uint16_t channels[16];    
int pwm[16];          
int LoraSendInterval = 50;

void decode_sbus(size_t byteIndex){
   if (byteIndex == 25) {
      byteIndex = 0; // Μηδενισμός για το επόμενο πακέτο

      // Έλεγχος του End Byte
      uint8_t endByte = sbusData[24];
      if (endByte == 0x00 || endByte == 0x04 || endByte == 0x14 || endByte == 0x24 || endByte == 0x34) { // to endbyte prepei na einai kt apayta
        // --- Decoding ΚΑΝΑΛΙΩΝ 1 έως 16 ---
        channels[0]  = ((sbusData[1]       | sbusData[2] << 8)                  & 0x07FF); // to mask grafetai 0000 0111 1111 1111
        channels[1]  = ((sbusData[2] >> 3  | sbusData[3] << 5)                  & 0x07FF);
        channels[2]  = ((sbusData[3] >> 6  | sbusData[4] << 2  | sbusData[5] << 10) & 0x07FF);
        channels[3]  = ((sbusData[5] >> 1  | sbusData[6] << 7)                  & 0x07FF);
        channels[4]  = ((sbusData[6] >> 4  | sbusData[7] << 4)                  & 0x07FF);
        channels[5]  = ((sbusData[7] >> 7  | sbusData[8] << 1  | sbusData[9] << 9)  & 0x07FF);
        channels[6]  = ((sbusData[9] >> 2  | sbusData[10] << 6)                 & 0x07FF);
        channels[7]  = ((sbusData[10] >> 5 | sbusData[11] << 3)                 & 0x07FF);
        channels[8]  = ((sbusData[12]      | sbusData[13] << 8)                 & 0x07FF);
        channels[9]  = ((sbusData[13] >> 3 | sbusData[14] << 5)                 & 0x07FF);
        channels[10] = ((sbusData[14] >> 6 | sbusData[15] << 2 | sbusData[16] << 10)& 0x07FF);
        channels[11] = ((sbusData[16] >> 1 | sbusData[17] << 7)                 & 0x07FF);
        channels[12] = ((sbusData[17] >> 4 | sbusData[18] << 4)                 & 0x07FF);
        channels[13] = ((sbusData[18] >> 7 | sbusData[19] << 1 | sbusData[20] << 9) & 0x07FF);
        channels[14] = ((sbusData[20] >> 2 | sbusData[21] << 6)                 & 0x07FF);
        channels[15] = ((sbusData[21] >> 5 | sbusData[22] << 3)                 & 0x07FF);

        // to sbus data[23]

        // Μετατροπή των τιμών SBUS σε κλασικά PWM (1000 - 2000 ms)
        for (size_t i = 0; i < 16; i++) {
          pwm[i] = map(channels[i], 172, 1811, 988, 2012); // βάζω τα ακριβή, "ανοιχτά" όρια (extended limits) που χρησιμοποιούν λογισμικά όπως το OpenTX/EdgeTX για να αντιστοιχούν στο 100% της κίνησης του στικ.
        } 
      }
    }
}

void setup() {
  Serial.begin(115200);
  delay(3000); // Μικρή καθυστέρηση για να προλάβει να ανοίξει το Serial Monitor
  
  // Στο ESP32-C6 χρησιμοποιούμε το Serial1 αντί για Serial2
  // Το πρωτόκολλο SBUS τρέχει στα 100.000 baud, 8 data bits, Even parity, 2 stop bits.
  Serial1.begin(100000, SERIAL_8E2, SBUS_RX_PIN, SBUS_TX_PIN, true); // Το "true" στο τέλος ενεργοποιεί το Hardware Inversion!

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  LoRa.setTxPower(20); // max 20dBm 

  Serial.println("ESP32-C6 Ready. Waiting for 16-Channel FrSky SBUS data...");
}

// Η πληροφορία είναι 11-bit: Το πρωτόκολλο SBUS στέλνει έναν αριθμό (θέση του στικ) που μπορεί να είναι από το 0 έως το 2047.
// Για να γραφτεί αυτός ο αριθμός στο δυαδικό σύστημα, χρειάζεται το πολύ 11 bits.

void loop() {

  // Χρησιμοποιούμε το Serial1 (Hardware UART) -- apo edw diabazoume
  if (Serial1.available() > 0) {
    uint8_t incomingByte = Serial1.read(); // διαβαζουμε απτο serial1 - hardware uart
    static int byteIndex = 0;
    
    // Το SBUS πακέτο ξεκινάει ΠΑΝΤΑ με το byte 0x0F
    if (byteIndex == 0 && incomingByte != 0x0F) return;

    sbusData[byteIndex] = incomingByte;
    byteIndex++;
    
    decode_sbus(byteIndex);
    static unsigned long lastLoraSend = 0;
    
    if (millis() - lastLoraSend > LoraSendInterval) { 
      lastLoraSend = millis();

    // Αποστολή των 32 bytes (16 κανάλια x 2 bytes) ως καθαρό binary
      LoRa.beginPacket();
      LoRa.write((uint8_t*)pwm, sizeof(pwm)); 
      LoRa.endPacket();

      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 500) { // print για serial monitor καθε 0.5sec
      lastPrint = millis();
      Serial.printf("Sent via LoRa -> CH1: %d, CH2: %d, CH3: %d, CH4: %d\n", pwm[0], pwm[1], pwm[2], pwm[3]);
      }
    }
  }
}
