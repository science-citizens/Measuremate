#pip install Ultralytics
#pip install picamera2
#pip install libcamera

#Als er een foutmelding optreedt die aangeeft dat het niet geinstalleerd kan worden in
#deze omgeving, voer het script dan uit in een terminal buiten deze omgeving zoals een virtuele omgeving.

#Met de volgende stappen kun je een virtuele omgeving aanmaken en activeren:
#1. Installeer de benodigde pakketten voor virtuele omgevingen (indien nog niet gedaan)
# sudo apt install python3-venv -y

# 2. Maak een virtuele omgeving aan (mapnaam 'env' of wat je maar wilt)
# python3 -m venv env

# 3. activeer de virtuele omgeving
# source env/bin/activate


from ultralytics import YOLO
from picamera2 import Picamera2
from libcamera import Transform

from pathlib import Path
import cv2
import time
import datetime
import numpy as np

DETECT_MODEL_PATH = "runs/Yolo8n2/weights/best_ncnn_model"       # YOLO detect model 
CLS_MODEL_PATH    = "runs/yolov8n-cls2/weights/best_ncnn_model"  # YOLO classificeer model

# Camera resoluties
HIGH_RES = (1920, 1080)     # hoofdstream (breedte, hoogte)
LOW_RES  = (640, 640)       # lores stream voor detectie (liefst zelfde als imgsz detectiemodel)

# Detectie instellingen
DETECT_CONF    = 0.6        # minimale confidence voor detecties
MIN_BOX_AREA   = 500        # minimale oppervlakte van de bounding box (in pixels^2)

# Classificatie instellingen
IMGSZ_DETECT   = 640        # input size voor detectiemodel (moet matchen met ONNX/PT setup)
IMGSZ_CLS      = 224        # input size voor classificatiemodel (moet matchen met training)
MIN_CLS_CONF   = 0.70       # classificatie moet minimaal 70% confidence hebben

# Opslag mappen
BASE_OUT_DIR   = Path("captures")
OUT_IMG_DIR    = BASE_OUT_DIR / "images"   # geannoteerde full-res images
OUT_CROP_DIR   = BASE_OUT_DIR / "crops"    # losse crops van vogels
SAVE_CROPS     = False      # op True laten als je crops wilt bewaren

# Clips opslaan
OUT_CLIP_DIR            = BASE_OUT_DIR / "videos"
SAVE_CLIPS              = False          # zet op False als je (tijdelijk) geen clips wilt
CLIP_DURATION_SECONDS   = 3              # lengte van de clip per event
CLIP_FPS                = 15             # frames per seconde in de clip

# Algemene instellingen
TITLE_PREFIX   = "MeasureMate2"
DETECT_INTERVAL_SECONDS = 1   # tijd tussen detectie-runs

# Cooldown
COOLDOWN_AFTER_CAPTURE = 3.0  # seconden wachten na een foto voordat nieuwe detectie mogelijk is

FPS_PRINT_INTERVAL = 5       


def ensure_dirs() -> None:
    # Zorgt dat de outputmappen bestaan
    OUT_IMG_DIR.mkdir(parents=True, exist_ok=True)
    if SAVE_CROPS:
        OUT_CROP_DIR.mkdir(parents=True, exist_ok=True)
    if SAVE_CLIPS:
        OUT_CLIP_DIR.mkdir(parents=True, exist_ok=True)


def timestamp_str() -> str:
    # Geeft een tijdstempel als string terug: YYYYMMDD_HHMMSS
    return datetime.datetime.now().strftime("%Y%m%d_%H%M%S")


def sanitize_label(label: str) -> str:
    # Maakt een string voor bestandsnamen:
    # - spaties vervangen door underscores
    # - alles in lowercase
    return label.strip().replace(" ", "_").lower()


def scale_box(box, scale_x: float, scale_y: float, img_w: int, img_h: int, margin: int = 0):
    x1, y1, x2, y2 = box

    # Schalen
    x1 = int(x1 * scale_x) - margin
    y1 = int(y1 * scale_y) - margin
    x2 = int(x2 * scale_x) + margin
    y2 = int(y2 * scale_y) + margin

    # Afkappen op afbeelding
    x1 = max(0, min(img_w - 1, x1))
    y1 = max(0, min(img_h - 1, y1))
    x2 = max(0, min(img_w - 1, x2))
    y2 = max(0, min(img_h - 1, y2))

    return x1, y1, x2, y2


def draw_box_with_label(img, box, label: str, color=(0, 255, 0), thickness: int = 2) -> None:
    # Tekent een bounding box met een gekleurde achtergrond + tekstlabel.
    x1, y1, x2, y2 = box

    # Teken de box
    cv2.rectangle(img, (x1, y1), (x2, y2), color, thickness)

    # Tekstinstellingen
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 0.5
    text_thickness = 1

    (tw, th), _ = cv2.getTextSize(label, font, font_scale, text_thickness)

    # Achtergrond voor tekst
    cv2.rectangle(img, (x1, y1 - th - 4), (x1 + tw, y1), color, -1)
    # Tekst
    cv2.putText(img, label, (x1, y1 - 2), font, font_scale, (0, 0, 0), text_thickness, cv2.LINE_AA)


def load_models():
    # Laadt de detectie- en classificatiemodellen.
    detect_model = YOLO(DETECT_MODEL_PATH, task="detect")
    cls_model = YOLO(CLS_MODEL_PATH, task="classify")
    print("[INFO] Modellen geladen.")
    return detect_model, cls_model


def init_camera():
    # Initialiseert de Picamera2 met een main (high-res) en lores stream in RGB.
    print("pi cam starten")
    picam2 = Picamera2()

    config = picam2.create_video_configuration(
        main={"size": HIGH_RES, "format": "RGB888"},
        lores={"size": LOW_RES, "format": "RGB888"},
        transform=Transform(vflip=0, hflip=0),
        buffer_count=4
    )

    picam2.configure(config)
    picam2.start()

    print("Camera gestart.")
    return picam2


def record_clip(picam2: Picamera2, out_path: Path, duration_sec: int = CLIP_DURATION_SECONDS, fps: int = CLIP_FPS) -> None:
    # Neemt een korte video-clip op van de main stream en slaat die op als MP4.
    frame_time = 1.0 / fps
    total_frames = int(duration_sec * fps)

    # HIGH_RES = (breedte, hoogte)
    frame_width, frame_height = HIGH_RES
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(
        str(out_path),
        fourcc,
        fps,
        (frame_width, frame_height)
    )

    print(f"Clip opnemen> {out_path}")

    for _ in range(total_frames):
        # Gebruik capture_buffer voor snelheid en converteer naar numpy array
        buf = picam2.capture_buffer("main")
        frame = np.frombuffer(buf, dtype=np.uint8).reshape((frame_height, frame_width, 3))
        if frame is not None:
            writer.write(frame)
        time.sleep(frame_time)

    writer.release()
    print(f"Clip opgeslagen: {out_path}")


def main():
    # Zorg dat mappen bestaan
    ensure_dirs()

    # Modellen en camera laden
    detect_model, cls_model = load_models()
    picam2 = init_camera()

    print("detectie pipeline gestart.")
    print("Ctrl+C om te stoppen.")

    last_detect_time = 0.0

    # Variabelen voor FPS van het detectiemodel
    last_fps_print = time.time()
    last_det_fps = 0.0

    # Cooldown
    last_capture_time = 0.0  # timestamp van laatste foto

    try:
        while True:
            now = time.time()

            # Beperk hoe vaak we een detectie-run doen (CPU besparen)
            if now - last_detect_time < DETECT_INTERVAL_SECONDS:
                continue
            last_detect_time = now

            # 1) Pak een lores frame voor detectie - gebruik capture_buffer voor snelheid
            lo_h, lo_w = LOW_RES[1], LOW_RES[0]
            buf_lores = picam2.capture_buffer("lores")
            lores = np.frombuffer(buf_lores, dtype=np.uint8).reshape((lo_h, lo_w, 3))
            #print("lores shape:", lores.shape)

            if lores is None:
                continue

            # 2) Run het detectiemodel op de lores afbeelding (RGB) + meet tijd
            t0 = time.perf_counter()
            detect_results = detect_model(
                lores,
                imgsz=IMGSZ_DETECT,
                conf=DETECT_CONF,
                verbose=False
            )[0]
            t1 = time.perf_counter()

            # detect-FPS (alleen model, geen capture)
            dt = t1 - t0
            if dt > 0:
                last_det_fps = 1.0 / dt

            # Elke FPS_PRINT_INTERVAL seconden: print de laatst gemeten detect-FPS
            if time.time() - last_fps_print >= FPS_PRINT_INTERVAL:
                print(f"yolo {last_det_fps:.2f} FPS (model: {DETECT_MODEL_PATH})")
                last_fps_print = time.time()

            # Als er geen boxes zijn, ga verder met de volgende iteratie
            if detect_results.boxes is None or len(detect_results.boxes) == 0:
                continue

            # 3) Filter alleen de vogel-detecties en verwijder heel kleine boxes
            bird_boxes = []
            for box in detect_results.boxes:
                det_conf = float(box.conf[0])


                # Box-coördinaten
                x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()
                box_area = (x2 - x1) * (y2 - y1)

                if box_area < MIN_BOX_AREA:
                    continue

                bird_boxes.append((x1, y1, x2, y2, det_conf))

            # Als we geen bruikbare vogel-boxen hebben, ga verder
            if not bird_boxes:
                continue

            # Check of we in cooldown periode zitten (net een foto gemaakt)
            if time.time() - last_capture_time < COOLDOWN_AFTER_CAPTURE:
                continue  # Skip detectie, wacht tot cooldown voorbij is

            # 4) High-res frame ophalen - gebruik capture_buffer voor snelheid
            hi_h, hi_w = HIGH_RES[1], HIGH_RES[0]
            buf_hi = picam2.capture_buffer("main")
            hi = np.frombuffer(buf_hi, dtype=np.uint8).reshape((hi_h, hi_w, 3))
            # print("main shape:", hi.shape)

            if hi is None:
                continue

            lo_h, lo_w = lores.shape[:2]
            scale_x = hi_w / lo_w
            scale_y = hi_h / lo_h

            # 5) High-res frame voorbereiden voor annotatie
            annotated = hi.copy()
            ts = timestamp_str()
            detections_for_filename = []

            # 6) Voor iedere gedetecteerde vogel: crop + classificatie + tekenen
            for i, (x1_lo, y1_lo, x2_lo, y2_lo, det_conf) in enumerate(bird_boxes, start=1):
                # Box schalen naar high-res
                x1, y1, x2, y2 = scale_box(
                    [x1_lo, y1_lo, x2_lo, y2_lo],
                    scale_x,
                    scale_y,
                    hi_w,
                    hi_h,
                    margin=30  # kleine marge rond de vogel
                )

                crop_img = hi[y1:y2, x1:x2]
                if crop_img.size == 0:
                    continue

                # Classificatie van de crop
                cls_results = cls_model(
                    crop_img,
                    imgsz=IMGSZ_CLS,
                    verbose=False
                )[0]

                probs = cls_results.probs
                top1_idx = int(probs.top1)
                top1_conf = float(probs.top1conf)
                species_name = cls_results.names[top1_idx]
                species_pct = int(top1_conf * 100)

                # Confidence-drempel
                if top1_conf < MIN_CLS_CONF:
                    print(f"Classificatie te laag voor crop {i}: {species_name} {species_pct}% -> genegeerd")
                    continue  # deze vogel slaan we helemaal over

                # Label-tekst en tekenen op de geannoteerde high-res
                label_text = f"{species_name} {species_pct:d}% (det {det_conf:.2f})"
                draw_box_with_label(annotated, (x1, y1, x2, y2), label_text)

                # Voor bestandsnaam
                safe_species = sanitize_label(species_name)
                detections_for_filename.append(f"{safe_species}-{species_pct:d}")

                # Eventueel crop wegschrijven
                if SAVE_CROPS:
                    crop_filename = f"{ts}_crop{i}_{safe_species}_{species_pct:d}pct.jpg"
                    crop_path = OUT_CROP_DIR / crop_filename
                    cv2.imwrite(str(crop_path), crop_img)
                    print(f"Crop opgeslagen: {crop_path}")

            # 7) Alleen iets opslaan als er ten minste één vogel boven de drempel zit
            if detections_for_filename:
                summary_part = "_".join(detections_for_filename)
                img_filename = f"{TITLE_PREFIX}_{ts}_{summary_part}.jpg"
                img_path = OUT_IMG_DIR / img_filename
                cv2.imwrite(str(img_path), annotated)
                print(f"Geannoteerde afbeelding opgeslagen: {img_path}")

                if SAVE_CLIPS:
                    clip_filename = f"{TITLE_PREFIX}_{ts}_{summary_part}.mp4"
                    clip_path = OUT_CLIP_DIR / clip_filename
                    record_clip(picam2, clip_path)

                # Cooldown starten zodat je niet blijft spammen
                last_capture_time = time.time()
                print(
                    f"Cooldown gestart ({COOLDOWN_AFTER_CAPTURE}s) - negeer vogels tot "
                    f"{datetime.datetime.fromtimestamp(last_capture_time + COOLDOWN_AFTER_CAPTURE).strftime('%H:%M:%S')}"
                )
            else:
                # Er waren wel vogel-boxen, maar geen enkele classificatie boven de drempel
                print("Alle classificaties onder MIN_CLS_CONF, niets opgeslagen.")

    except KeyboardInterrupt:
        print("\nstopped")

    finally:
        # Camera netjes stoppen
        picam2.stop()
        print("cam stopped")


if __name__ == "__main__":
    main()

