import socket
import serial
import time

PORT = 4210
SERIAL_PORT = '/dev/ttyACM0'
BAUDRATE = 115200

# Velocity in m/s — STM maps 3.0–5.0 → 0.5–3.3V internally
CMD_MAP = {
    "FWD":   4.0,   # tune this — mid-range for initial test
    "BRAKE": 0.0,
    "STOP":  0.0,
    "BWD":   0.0,
}

def main():
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.1)
    time.sleep(1.5)
    while ser.in_waiting:
        print("STM32:", ser.readline().decode(errors='ignore').strip())

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))
    sock.settimeout(0.2)

    last_cmd = None
    print(f"Listening on UDP:{PORT}, forwarding to {SERIAL_PORT}")

    try:
        while True:
            try:
                data, addr = sock.recvfrom(1024)
            except socket.timeout:
                cmd = "STOP"
            else:
                cmd = data.decode().strip()
                print(f"[{addr[0]}] → {cmd}")

            velocity = CMD_MAP.get(cmd, 0.0)

            if velocity != last_cmd:
                msg = f"{velocity:.3f}\n"
                ser.write(msg.encode())
                if ser.in_waiting:
                    echo = ser.readline().decode(errors='ignore').strip()
                    if echo:
                        print(f"  STM32: {echo}")
                last_cmd = velocity

    except KeyboardInterrupt:
        print("\nSending stop and exiting.")
        ser.write(b"0.000\n")
    finally:
        ser.close()
        sock.close()

if __name__ == "__main__":
    main()
