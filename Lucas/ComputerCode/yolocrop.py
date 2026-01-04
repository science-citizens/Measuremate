#Dit script cropt objecten uit afbeeldingen op basis van YOLO-annotaties.
#De gecropte objecten worden opgeslagen in een enkele outputmap.
#Om goed te gebruiken eerst een dataset voorbereiden met YOLO-annotaties.


from pathlib import Path
from PIL import Image
import math

# YOLO dataset folders
SETS = ["train", "valid", "test"]
BASE_DIR = Path("dataset4")                # Map waar train/valid/test in zitten

OUT_DIR = Path("dataset4_cropped")    # Één output folder
OUT_DIR.mkdir(parents=True, exist_ok=True)

PADDING = 0.15
MIN_SIDE = 48

for split in SETS:
    IMG_DIR = BASE_DIR / split / "images"
    LBL_DIR = BASE_DIR / split / "labels"

    print(f"\n--- Verwerk {split} ---")

    for img_path in IMG_DIR.glob("*.jpg"):
        lab_path = LBL_DIR / (img_path.stem + ".txt")
        if not lab_path.exists():
            print(f"Geen label voor {img_path.name} → overslaan")
            continue

        im = Image.open(img_path).convert("RGB")
        W, H = im.size

        with open(lab_path) as f:
            for i, line in enumerate(f):
                parts = line.strip().split()
                if len(parts) != 5:
                    continue

                # yolo format: class, xc, yc, w, h (normalized)
                cls, xc, yc, w, h = map(float, parts)

                # pixel coords
                x1 = (xc - w / 2) * W
                y1 = (yc - h / 2) * H
                x2 = (xc + w / 2) * W
                y2 = (yc + h / 2) * H

                # padding
                pw = (x2 - x1) * PADDING
                ph = (y2 - y1) * PADDING
                x1p = max(0, math.floor(x1 - pw))
                y1p = max(0, math.floor(y1 - ph))
                x2p = min(W, math.ceil(x2 + pw))
                y2p = min(H, math.ceil(y2 + ph))

                # skip te kleine crops
                if (x2p - x1p) < MIN_SIDE or (y2p - y1p) < MIN_SIDE:
                    continue

                # crop
                crop = im.crop((x1p, y1p, x2p, y2p))

                # naam: originele naam + box index
                crop_name = f"{img_path.stem}_{i}.jpg"
                crop.save(OUT_DIR / crop_name, quality=95)

        print(f"Cropped: {img_path.name}")

print("\nAlle crops opgeslagen in", OUT_DIR.resolve())
