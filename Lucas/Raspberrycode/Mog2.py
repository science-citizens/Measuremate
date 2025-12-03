#!/usr/bin/env python3
"""
Picamera2 motion-triggered stills met MOG2 + upload naar web-API.
- Analyseert lores stream voor beweging (snel)
- Maakt alleen een full-res JPEG bij "echte" beweging
- Slaat lokaal op én uploadt naar je Next.js endpoint
"""

import os, time
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np
import requests
from picamera2 import Picamera2
from libcamera import Transform

# Opslag
SAVE_DIR = str(Path.home() / "projects" / "measuremate" / "captures")
TITLE_PREFIX = "PiCam v3"

# Full-res still settings
PHOTO_WIDTH, PHOTO_HEIGHT = 1920, 1080

# Lores analysis stream (snel)
LORES_SIZE = (640, 360)   # 16:9; klein houden voor snelheid

# Background subtractor settings
BG_HISTORY = 800
BG_VAR_THRESHOLD = 50
BG_DETECT_SHADOWS = True

# Motion gating
MIN_BLOB_AREA = 1900
SUSTAINED_FRAMES = 5
COOLDOWN_SEC = 3
MAX_GLOBAL_MOTION_RATIO = 0.40  # >40% bewegende pixels = wind -> skip
ROI = None  

#Upload-instellingen
API_URL = os.getenv("MEASUREMATE_UPLOAD_URL", "https://measurematelucasstage.vercel.app/api/upload")
UPLOAD_SECRET = os.getenv("MEASUREMATE_UPLOAD_SECRET", "cosmoiseenredelijkdikkehond")
REQUEST_TIMEOUT = 60
UPLOAD_RETRIES = 2  # naast de eerste poging



def ensure_dir(p): os.makedirs(p, exist_ok=True)
def ts_name(): return datetime.now().strftime("%Y-%m-%d_%H-%M-%S.jpg")


def upload_file(filepath: str, url: str, secret: str, title: str) -> bool:

    if not os.path.exists(filepath):
        print("Upload: bestand niet gevonden:", filepath)
        return False

    for attempt in range(UPLOAD_RETRIES + 1):
        try:
            with open(filepath, "rb") as f:
                files = {"file": (os.path.basename(filepath), f, "image/jpeg")}
                data = {"type": "image", "title": title}
                headers = {"Authorization": f"Bearer {secret}"}

                r = requests.post(
                    url, headers=headers, data=data, files=files, timeout=REQUEST_TIMEOUT
                )

            if 200 <= r.status_code < 300:
                print(f"Upload OK ({r.status_code}) -> {url}")
                return True

        except requests.RequestException as e:
            print(f"Upload fout poging {attempt+1}/{UPLOAD_RETRIES+1}: {e}")

        time.sleep(1.5)  # kleine backoff

    return False



def main():
    ensure_dir(SAVE_DIR)

    picam2 = Picamera2()
    config = picam2.create_still_configuration(
        main={"size": (PHOTO_WIDTH, PHOTO_HEIGHT), "format": "RGB888"},
        lores={"size": LORES_SIZE, "format": "YUV420"},
        transform=Transform(hflip=0, vflip=0),
        buffer_count=4,
        display=None
    )
    picam2.configure(config)
    picam2.start()
    time.sleep(0.5)

    # Background subtractor (MOG2)
    bg = cv2.createBackgroundSubtractorMOG2(
        history=BG_HISTORY,
        varThreshold=BG_VAR_THRESHOLD,
        detectShadows=BG_DETECT_SHADOWS
    )

    sustained = 0
    last_capture = 0.0

    print("Motion detector + uploader")
    try:
        while True:
            # Lores frame (YUV420) -> BGR
            yuv = picam2.capture_array("lores")
            frame = cv2.cvtColor(yuv, cv2.COLOR_YUV420p2BGR)

            # ROI
            work = frame
            if ROI is not None:
                x, y, w, h = ROI
                work = frame[y:y+h, x:x+w]

            # Blur -> background subtractie
            work_blur = cv2.GaussianBlur(work, (5, 5), 0)
            fgmask = bg.apply(work_blur)

            # Schaduwen verwijderen
            if BG_DETECT_SHADOWS:
                _, fgmask = cv2.threshold(fgmask, 200, 255, cv2.THRESH_BINARY)

            # Ruis wegwerken
            fgmask = cv2.medianBlur(fgmask, 5)
            kernel = np.ones((3, 3), np.uint8)
            fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_OPEN, kernel, iterations=1)
            fgmask = cv2.dilate(fgmask, kernel, iterations=1)

            # Wind-guard
            motion_pixels = int(cv2.countNonZero(fgmask))
            total_pixels = fgmask.shape[0] * fgmask.shape[1]
            motion_ratio = motion_pixels / max(1, total_pixels)

            # Contours zoeken + filters
            contours, _ = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            significant = False
            for c in contours:
                area = cv2.contourArea(c)
                if area < MIN_BLOB_AREA:
                    continue
                hull = cv2.convexHull(c)
                hull_area = cv2.contourArea(hull)
                solidity = area / hull_area if hull_area > 0 else 0
                if solidity < 0.5:
                    continue
                significant = True
                break

            # Trigger logica
            trigger = False
            if significant and motion_ratio < MAX_GLOBAL_MOTION_RATIO:
                sustained += 1
                if sustained >= SUSTAINED_FRAMES and (time.time() - last_capture) > COOLDOWN_SEC:
                    trigger = True
            else:
                sustained = 0

            if trigger:
                filename = ts_name()
                out_path = os.path.join(SAVE_DIR, filename)
                picam2.capture_file(out_path)  # full-res still
                stamp = datetime.now().strftime('%H:%M:%S')
                print(f"[{stamp}] Beweging! Opgeslagen: {out_path}")

                title = f"{TITLE_PREFIX} {filename}"
                ok = upload_file(out_path, API_URL, UPLOAD_SECRET, title)
                if not ok:
                    print("Upload niet gelukt; bestand blijft lokaal bewaard.")
                last_capture = time.time()
                sustained = 0

            time.sleep(0.02)
    except KeyboardInterrupt:
        pass
    finally:
        picam2.stop()


if __name__ == "__main__":
    main()
