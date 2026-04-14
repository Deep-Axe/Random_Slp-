import cv2
from http.server import BaseHTTPRequestHandler, HTTPServer
import threading

STREAM_PORT = 8080
cap = cv2.VideoCapture(0)   # ZED2i shows as /dev/video0, try 0 or 2 for right RGB
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

frame_lock = threading.Lock()
latest_frame = None

def capture_loop():
    global latest_frame
    while True:
        ret, frame = cap.read()
        if ret:
            _, buf = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 70])
            with frame_lock:
                latest_frame = buf.tobytes()

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args): pass  # suppress per-frame logs

    def do_GET(self):
        if self.path == '/stream':
            self.send_response(200)
            self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=frame')
            self.end_headers()
            try:
                while True:
                    with frame_lock:
                        frame = latest_frame
                    if frame:
                        self.wfile.write(b'--frame\r\n')
                        self.wfile.write(b'Content-Type: image/jpeg\r\n\r\n')
                        self.wfile.write(frame)
                        self.wfile.write(b'\r\n')
            except BrokenPipeError:
                pass
        else:
            self.send_response(404); self.end_headers()

t = threading.Thread(target=capture_loop, daemon=True)
t.start()

print(f"Stream at http://10.52.155.212:{STREAM_PORT}/stream")
print("Open in VLC: Media → Open Network Stream → paste that URL")
HTTPServer(("0.0.0.0", STREAM_PORT), Handler).serve_forever()
