import socket, time, os
import pygame

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

JETSON_IP = "10.52.155.212"
PORT = 4210
DAC_MAX = 4096
DEADZONE = 0.08

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

pygame.init()
pygame.joystick.init()
if pygame.joystick.get_count() == 0:
    print("No controller found."); exit(1)

joy = pygame.joystick.Joystick(0)
joy.init()
print(f"Controller: {joy.get_name()}")
print("Left stick: up/down = throttle, left/right = steer |  button = full brake | START = quit")

def apply_deadzone(val, dz=DEADZONE):
    if abs(val) < dz:
        return 0.0
    return (val - dz * (1 if val > 0 else -1)) / (1.0 - dz)

try:
    while True:
        pygame.event.pump()

        # START button (button 7 on Xbox, varies) = quit
        if joy.get_button(7):
            sock.sendto("STOP".encode(), (JETSON_IP, PORT))
            print("Quit."); break

        # Left stick axes: 0 = left/right (steer), 1 = up/down (throttle, inverted)
        raw_throttle = -joy.get_axis(1)   # push up = positive
        raw_steer    =  joy.get_axis(0)   # push right = positive

        throttle = apply_deadzone(raw_throttle)
        steer     = apply_deadzone(raw_steer)

        # Map throttle: up half = 0→4096, down half = 0 (no reverse)
        # If you want reverse, change to allow negative DAC range
        dac_val = int(max(0.0, throttle) * DAC_MAX)

        # Steer maps -1.0→+1.0 to servo 0→270 (135 = center)
        steer_angle = int(135 + steer * 135)   # 0=full left, 135=center, 270=full right
        steer_angle = max(0, min(270, steer_angle))

        # X button (button 0 on Xbox/PS) = full brake servo, else 0
        x_pressed = joy.get_button(0)
        brake_angle = 180 if x_pressed else 0

        msg = f"T:{dac_val},S:{steer_angle},B:{brake_angle}"
        sock.sendto(msg.encode(), (JETSON_IP, PORT))
        print(f"\r{msg:<50}", end="", flush=True)

        time.sleep(0.05)  # 20 Hz

finally:
    sock.close()
    pygame.quit()
    print("\nDone.")
