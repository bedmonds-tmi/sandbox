import csv
import re
import time
import serial

# --- CONFIGURATION ---
# Replace with your serial port (e.g., 'COM3' on Windows, '/dev/ttyACM0' on Linux, or '/dev/cu.usbmodem...' on Mac)
SERIAL_PORT = "COM12"
BAUD_RATE = 921600  # Match your board's console baud rate
DURATION_SECONDS = 20
CSV_FILENAME = "lsm6dsv16x_data.csv"
# ---------------------


def main():
  print(
      f"Attempting to connect to {SERIAL_PORT} at {BAUD_RATE} baud..."
  )

  # Regex to parse: X:val Y:val Z:val Xg:val Yg:val Zg:val
  pattern = re.compile(
      r"X:([-\d.]+)\s+Y:([-\d.]+)\s+Z:([-\d.]+)\s+Xg:([-\d.]+)\s+Yg:([-\d.]+)\s+Zg:([-\d.]+)"
  )

  try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
  except Exception as e:
    print(f"Error: Could not open serial port {SERIAL_PORT}: {e}")
    return

  print(
      f"Connected! Recording sensor data for {DURATION_SECONDS} seconds into"
      f" '{CSV_FILENAME}'..."
  )

  start_time = time.time()
  sample_count = 0

  with open(CSV_FILENAME, mode="w", newline="") as csv_file:
    writer = csv.writer(csv_file)
    # Write header columns
    writer.writerow(
        [
            "Relative_Time_Sec",
            "Accel_X",
            "Accel_Y",
            "Accel_Z",
            "Gyro_X",
            "Gyro_Y",
            "Gyro_Z",
        ]
    )

    while (time.time() - start_time) < DURATION_SECONDS:
      line = ser.readline().decode("utf-8", errors="ignore").strip()
      if not line:
        continue

      match = pattern.search(line)
      if match:
        current_time = time.time() - start_time
        # Extract the 6 float values captured by regex
        values = [float(v) for v in match.groups()]
        
        # Write timestamp and the 6 axes values to CSV
        writer.writerow([f"{current_time:.4f}"] + values)
        sample_count += 1

  ser.close()
  print(f"\nSampling complete!")
  print(f"Successfully saved {sample_count} samples to '{CSV_FILENAME}'.")


if __name__ == "__main__":
  main()