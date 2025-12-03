#Om ultralytics te gebruiken moet je het eerst installeren met: pip install ultralytics
#Zorg er ook voor dat je in een virtuale omgeving werkt
#Met de volgende stappen kun je een virtuele omgeving aanmaken en activeren:
#1. Installeer de benodigde pakketten voor virtuele omgevingen (indien nog niet gedaan)
# sudo apt install python3-venv -y

# 2. Maak een virtuele omgeving aan (mapnaam 'env' of wat u maar wilt)
# python3 -m venv env

# 3. activeer de virtuele omgeving
# source env/bin/activate

from ultralytics import YOLO
from pathlib import Path
import time, cv2, psutil

MODEL_PATH = "runs/yolov11n-cls2/weights/best.pt"
TEST_DIR   = "dataset4/test"
IMGSZ      = 224
WARMUP     = 3

model = YOLO(MODEL_PATH)

jpgs = list(Path(TEST_DIR).rglob("*.jpg"))
classes = sorted([d.name for d in Path(TEST_DIR).iterdir() if d.is_dir() ])
class_to_idx = {c: i for i, c in enumerate(classes)}

# psutil proces-object (alleen 1x maken)
proc = psutil.Process()

# Warmup
for p in jpgs[:WARMUP]:
    img = cv2.imread(str(p))
    _ = model.predict(img, imgsz=IMGSZ, verbose=False)

# CPU meter resetten
psutil.cpu_percent(interval=None)

top1_correct = 0
top5_correct = 0
cpu_readings = []
ram_readings = []   # in MB

start = time.perf_counter()

for p in jpgs:
    img = cv2.imread(str(p))
    result = model.predict(img, imgsz=IMGSZ, verbose=False)[0]

    # CPU & RAM meten
    cpu_now = psutil.cpu_percent(interval=None)
    mem_now = proc.memory_info().rss / (1024 * 1024)  # RSS in MB

    cpu_readings.append(cpu_now)
    ram_readings.append(mem_now)

    true_idx = class_to_idx[p.parent.name]
    pred_top1 = int(result.probs.top1)
    pred_top5 = list(map(int, result.probs.top5))

    if pred_top1 == true_idx:
        top1_correct += 1
    if true_idx in pred_top5:
        top5_correct += 1

elapsed = time.perf_counter() - start

n = len(jpgs)
top1 = top1_correct / n
top5 = top5_correct / n
avg_ms = (elapsed / n) * 1000
fps = 1000 / avg_ms if avg_ms > 0 else 0
avg_cpu = sum(cpu_readings) / len(cpu_readings)
avg_ram = sum(ram_readings) / len(ram_readings)
max_ram = max(ram_readings)

print(f"Images tested: {n}")
print(f"Top-1 accuracy: {top1:.4f}")
print(f"Top-5 accuracy: {top5:.4f}")
print(f"Avg inference: {avg_ms:.2f} ms ({fps:.2f} FPS)")
print(f"Average CPU load: {avg_cpu:.1f}%")
print(f"Average RAM (this process): {avg_ram:.1f} MB")
print(f"Max RAM (this process): {max_ram:.1f} MB")
