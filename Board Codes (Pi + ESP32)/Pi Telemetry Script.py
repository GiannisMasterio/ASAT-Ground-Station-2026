import time
import pigpio
import struct
import os
import serial

# --- 1. ΡΥΘΜΙΣΕΙΣ LORA (UART) ---
try:
    # Ανοίγουμε τη σειριακή θύρα του Pi Zero για το LoRa
    lora = serial.Serial(
        port='/dev/ttyS0', 
        baudrate=9600, # Βεβαιώσου ότι είναι το σωστό baud rate για το module σου
        timeout=1
    )
    print("✅ Το LoRa Module (UART) αρχικοποιήθηκε επιτυχώς!")
except Exception as error:
    print(f"❌ Σφάλμα LoRa: {error}")
    print("Ελέγξτε τις συνδέσεις TX/RX και αν είναι ενεργό το Serial Port.")
    exit(1)

RX_PIN = 17
BAUD = 57600
publish_time_interval = 1.0

# --- 3. PAYLOAD  ---
telemetry_packet = {
    "MainVoltage": 0.0,
    "MainCurrent": 0.0,
    "SVoltage": 0.0,
    "SCurrent": 0.0,
    "GSpeed": 0.0,
    "SatCount": 0,
    "Latitude": 0.0,
    "Longitude": 0.0,
    "Altitude": 0.0,
    "HomeDist": 0.0,
    "PiTemp": 0.0
}

def get_cpu_temperature():
    try:
        with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
            temp_raw = f.read()
        return round(float(temp_raw) / 1000.0, 1)
    except Exception:
        return 0.0

def process_telemetry(hex_data):
    """ Ενημερώνει το Dictionary με τα νέα δεδομένα από S.Port """
    global telemetry_packet
    hex_data = hex_data.replace("\n", "").replace(" ", "").strip()
    try:
        raw_bytes = bytearray.fromhex(hex_data)
    except ValueError:
        return

    destuffed = bytearray()
    i = 0
    while i < len(raw_bytes):
        if raw_bytes[i] == 0x7D and i + 1 < len(raw_bytes):
            destuffed.append(raw_bytes[i+1] ^ 0x20)
            i += 2
        else:
            destuffed.append(raw_bytes[i])
            i += 1

    i = 0
    while i <= len(destuffed) - 9:
        if destuffed[i] == 0x7E and destuffed[i+2] == 0x10:
            phys_id = destuffed[i+1] & 0x1F
            app_id = destuffed[i+3] | (destuffed[i+4] << 8)
            value_bytes = destuffed[i+5 : i+9]
            value = struct.unpack('<i', value_bytes)[0]
            
            if app_id == 0x0210:
                readable_val = value / 100.0
                if phys_id == 13: telemetry_packet["MainVoltage"] = readable_val
                elif phys_id == 2: telemetry_packet["SVoltage"] = readable_val
            elif app_id == 0x0200:
                readable_val = value / 100.0
                if phys_id == 13: telemetry_packet["MainCurrent"] = readable_val
                elif phys_id == 2: telemetry_packet["SCurrent"] = readable_val
            elif app_id == 0x0830:
                telemetry_packet["GSpeed"] = round((value / 1000.0) * 1.852, 2)
            elif app_id == 0x0820:
                telemetry_packet["Altitude"] = round(value / 100.0, 2)
            elif app_id == 0x0800:
                val_u32 = struct.unpack('<I', value_bytes)[0]
                is_lon = (val_u32 & 0x80000000) != 0
                is_neg = (val_u32 & 0x40000000) != 0
                total_minutes = (val_u32 & 0x3FFFFFFF) / 10000.0
                decimal_deg = total_minutes / 60.0
                if is_neg:
                    decimal_deg = -decimal_deg
                if is_lon:
                    telemetry_packet["Longitude"] = round(decimal_deg, 6)
                else:
                    telemetry_packet["Latitude"] = round(decimal_deg, 6)
            i += 8
        else:
            i += 1

pi = pigpio.pi()
while not pi.connected:
    print("⏳ Το pigpio δεν είναι έτοιμο ακόμα. Αναμονή 2 δευτερόλεπτα...")
    time.sleep(2)
    pi = pigpio.pi() 

try: pi.bb_serial_read_close(RX_PIN)
except: pass

pi.set_mode(RX_PIN, pigpio.INPUT)
pi.bb_serial_read_open(RX_PIN, BAUD)
pi.bb_serial_invert(RX_PIN, 1)

print(f"🚀 Σύστημα ενεργό: Συλλέγει S.Port και εκπέμπει μέσω LoRa (UART)...")

last_publish_time = time.time()

try:
    while True:
        # Διαβάζουμε S.Port
        count, data = pi.bb_serial_read(RX_PIN)
        if count > 0:
            hex_data = data.hex().upper()
            process_telemetry(hex_data)

        current_time = time.time()
        
        if current_time - last_publish_time >= publish_time_interval:
            telemetry_packet["PiTemp"] = get_cpu_temperature()
            
            # ΣΥΜΠΙΕΣΗ ΔΕΔΟΜΕΝΩΝ (Binary Struct Packing)
            binary_payload = struct.pack('<fffffBfffff',
                float(telemetry_packet['MainVoltage']),
                float(telemetry_packet['MainCurrent']),
                float(telemetry_packet['SVoltage']),
                float(telemetry_packet['SCurrent']),
                float(telemetry_packet['GSpeed']),
                int(telemetry_packet['SatCount']),
                float(telemetry_packet['Latitude']),
                float(telemetry_packet['Longitude']),
                float(telemetry_packet['Altitude']),
                float(telemetry_packet['HomeDist']),
                float(telemetry_packet['PiTemp'])
            )

            # Αποστολή των 41 bytes μέσω LoRa UART
            lora.write(binary_payload)
            lora.flush() # Εξασφαλίζει ότι τα δεδομένα έφυγαν από τον buffer
            
            print(f"📤 Εκπέμφθηκαν {len(binary_payload)} bytes")
            
            last_publish_time = current_time

        time.sleep(0.005)

except KeyboardInterrupt:
    print("\n🛑 Τερματισμός...")
finally:
    if 'lora' in locals() and lora.is_open:
        lora.close()
    pi.bb_serial_read_close(RX_PIN)
    pi.stop()