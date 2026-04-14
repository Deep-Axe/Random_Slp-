import socket, serial, time, threading

UDP_PORT     = 4210
STM32_PORT   = "/dev/ttyUSB0"   # adjust: ls /dev/ttyUSB* or /dev/ttyACM*
ARDUINO_PORT = "/dev/ttyUSB1"   # adjust: the other USB serial device
BAUD         = 115200
WATCHDOG_MS  = 300              # zero everything if no packet for this long

stm32   = serial.Serial(STM32_PORT,   BAUD, timeout=1)
arduino = serial.Serial(ARDUINO_PORT, BAUD, timeout=1)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", UDP_PORT))
sock.settimeout(0.1)

last_rx = time.time()

def send_stm32(dac_val):
    # STM32 expects "DAC:4095\n" or whatever your firmware reads
    stm32.write(f"DAC:{dac_val}\n".encode())

def send_arduino(steer, brake):
    # Arduino expects "S:135,B:0\n"
    arduino.write(f"S:{steer},B:{brake}\n".encode())

def watchdog():
    while True:
        if time.time() - last_rx > WATCHDOG_MS / 1000.0:
            send_stm32(0)
            send_arduino(135, 0)
        time.sleep(0.05)

t = threading.Thread(target=watchdog, daemon=True)
t.start()

print(f"Listening on UDP {UDP_PORT} | STM32={STM32_PORT} | Arduino={ARDUINO_PORT}")

try:
    while True:
        try:
            data, addr = sock.recvfrom(1024)
        except socket.timeout:
            continue

        last_rx = time.time()
        msg = data.decode().strip()

        if msg == "STOP":
            send_stm32(0)
            send_arduino(135, 0)
            print("\rSTOP                    ", end="", flush=True)
            continue

        try:
            parts = dict(p.split(":") for p in msg.split(","))
            dac   = int(parts["T"])
            steer = int(parts["S"])
            brake = int(parts["B"])
            dac   = max(0, min(4096, dac))
            steer = max(0, min(270, steer))
            brake = max(0, min(180, brake))

            send_stm32(dac)
            send_arduino(steer, brake)
            print(f"\rDAC={dac:4d}  Steer={steer:3d}  Brake={brake:3d}", end="", flush=True)

        except (KeyError, ValueError) as e:
            print(f"\nParse error: {msg} — {e}")

except KeyboardInterrupt:
    send_stm32(0)
    send_arduino(135, 0)
    print("\nClean exit.")
finally:
    sock.close()
    stm32.close()
    arduino.close()
