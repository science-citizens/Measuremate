from ultralytics import YOLO
from pathlib import Path

MODEL = "yolov8n-cls.pt"     # Verander naar welk model je wilt trainen
DATA_DIR = "Dataset4"         # moet subfolders 'train', 'val', en 'test' bevatten
EPOCHS = 20
IMG_SIZE = 224
BATCH = 32  # goed voor pc training, gebruik 4 of 1 op de Raspberry Pi

def main():
    model = YOLO(MODEL)

    model.train(
        task="classify",
        data=DATA_DIR,       
        epochs=EPOCHS,
        imgsz=IMG_SIZE,
        batch=BATCH,
        project="runs",
        name="yolov8n-cls2", # naam van het opgeslagen model
        exist_ok=True,
    )

    best_ckpt = Path(model.trainer.save_dir) / "weights" / "best.pt"

    model = YOLO(str(best_ckpt))
    metrics = model.val(
        task="classify",
        data=DATA_DIR,
        split="test",
        imgsz=IMG_SIZE
    )

    print(f"\nModel: {MODEL}")
    print(f"Top-1 Accuracy: {float(metrics.top1):.4f}")
    print(f"Top-5 Accuracy: {float(metrics.top5):.4f}")

    #onnx export, kan veranderd worden naar andere formaten zoals ncnn
    export_path = model.export(format="ncnn", imgsz=IMG_SIZE)
    print("Exported model:", export_path)

if __name__ == "__main__":
    main()
