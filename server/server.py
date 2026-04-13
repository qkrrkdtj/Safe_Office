from flask import Flask
import cv2
import os
import time
from datetime import datetime
from ultralytics import YOLO

app = Flask(__name__)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SAVE_DIR = os.path.join(SCRIPT_DIR, "captures")
RESULT_DIR = os.path.join(SCRIPT_DIR, "results")
MODEL_PATH = os.path.join(SCRIPT_DIR, "best.pt")

os.makedirs(SAVE_DIR, exist_ok=True)
os.makedirs(RESULT_DIR, exist_ok=True)

yolo_model = YOLO(MODEL_PATH)

current_count = 0   

@app.route('/capture')
def capture():
    global current_count

    cap = cv2.VideoCapture(0)
    time.sleep(0.3)

    if not cap.isOpened():
        print("[ERROR] camera open failed")
        return "-1"

    for _ in range(5):
        cap.read()

    ret, frame = cap.read()
    cap.release()

    if not ret:
        print("[ERROR] frame capture failed")
        return "-1"

    now = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    filename = os.path.join(SAVE_DIR, f"capture_{now}.jpg")
    cv2.imwrite(filename, frame)

    print(f"[CAPTURED] {filename}")

    results = yolo_model.predict(source=filename, conf=0.7, verbose=False)
    r = results[0]

    box_count = 0 if r.boxes is None else len(r.boxes)
    current_count = box_count

    print(f"[DETECT] box_count = {box_count}")
    print(f"[COUNT] current_count = {current_count}")

    plotted = r.plot()
    result_path = os.path.join(RESULT_DIR, f"result_{now}.jpg")
    cv2.imwrite(result_path, plotted)
    print(f"[RESULT] {result_path}")

    return str(current_count)

@app.route('/count')
def get_count():
    global current_count
    return str(current_count)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
