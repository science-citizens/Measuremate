#Om ultralytics te gebruiken moet je het eerst installeren met: pip install ultralytics
#Zorg er ook voor dat je in een virtuale omgeving werkt
#Met de volgende stappen kun je een virtuele omgeving aanmaken en activeren:
#1. Installeer de benodigde pakketten voor virtuele omgevingen (indien nog niet gedaan)
# sudo apt install python3-venv -y

# 2. Maak een virtuele omgeving aan (mapnaam 'env' of wat je maar wilt)
# python3 -m venv env

# 3. activeer de virtuele omgeving
# source env/bin/activate

from ultralytics import YOLO
import time, glob, os, cv2, psutil

#Instellingen
MODEL_PATH = "runs/Yolo8n2/weights/best.pt"  # .onnx OR .pt
DATA_YAML  = "data.yaml"
TEST_DIR   = "dataset3/test/images"
IMGSZ      = 640
WARMUP     = 3

# Model laden
model = YOLO(MODEL_PATH)
is_onnx = MODEL_PATH.lower().endswith(".onnx")

# Process handle voorbereiden voor RAM meting
process = psutil.Process()

#model evaluatie parameters, alleen voor .pt modellen
if not is_onnx:
    print("\n[1/3] Evaluatie op test set:")
    metrics = model.val(data=DATA_YAML, split="test", imgsz=IMGSZ)
    print(f"mAP50-95: {metrics.box.map:.4f}")
    print(f"mAP50: {metrics.box.map50:.4f}")
    print(f"Precision: {metrics.box.mp:.4f}")
    print(f"Recall: {metrics.box.mr:.4f}")
    
    precision = metrics.box.mp
    recall = metrics.box.mr
    f1 = 2 * (precision * recall) / (precision + recall)
    print(f"f1: {f1:.4f}")
else:
    print("\n[1/3] Accuracy on test set:")
    print("Skipped for ONNX (model.val requires .pt).")

# Inferentie test door alle test afbeeldingen te gaan en de tijd te meten die het kost
print("\n[2/3] Inferentiesnelheid op test afbeeldingen:")
exts = ("*.jpg", "*.jpeg", "*.png")
jpgs = []
for e in exts:
    jpgs.extend(glob.glob(os.path.join(TEST_DIR, e)))
jpgs = sorted(jpgs)
if not jpgs:
    raise SystemExit("Geen afbeeldingen gevonden in TEST_DIR.")

# Warmup
for p in jpgs[:WARMUP]:
    img = cv2.imread(p)
    _ = model.predict(img, imgsz=IMGSZ, verbose=False)

# Reset CPU meter
psutil.cpu_percent(interval=None)

cpu_readings = []
ram_readings = []

start = time.perf_counter()

for p in jpgs:
    img = cv2.imread(p)
    _ = model.predict(img, imgsz=IMGSZ, verbose=False)

    # CPU percentage sinds laatste call
    cpu_now = psutil.cpu_percent(interval=None)

    # RAM gebruikt door dit proces (MB)
    ram_now = process.memory_info().rss / (1024 * 1024)

    cpu_readings.append(cpu_now)
    ram_readings.append(ram_now)

total_time = time.perf_counter() - start

avg_time_ms = (total_time / len(jpgs)) * 1000.0
fps = 1000.0 / avg_time_ms if avg_time_ms > 0 else 0.0

print(f"Images tested: {len(jpgs)}")
print(f"Avg inference: {avg_time_ms:.2f} ms → {fps:.2f} FPS")

# CPU en RAM gebruik printen
print("\n[3/3] CPU en RAM:")
print(f"Gemiddeld CPU gebruik: {sum(cpu_readings)/len(cpu_readings):.1f}%")
print(f"Maximaal CPU gebruik: {max(cpu_readings):.1f}%")

print(f"Gemiddeld RAM (proces): {sum(ram_readings)/len(ram_readings):.1f} MB")
print(f"Maximaal RAM (proces): {max(ram_readings):.1f} MB")
