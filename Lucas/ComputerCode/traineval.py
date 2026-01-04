from ultralytics import YOLO
from pathlib import Path

MODEL = "yolov8n.pt"          # verander naar welk model je wilt trainen
DATA_YAML = "data2.yaml"       # yaml file met de paths naar de dataset test train en val

def main():
    model = YOLO(MODEL)

    # training wordt gedaan met de beste parameters voor elk model. om de beste resultaten te krijgen
    # Zo kan er het beste gekeken worden welk model het best presteert in real-time.
    model.train(
        data=DATA_YAML,
        epochs=100,
        imgsz=640,
        batch=16,
        patience=30, # eerder stoppen als er plateau bereikt is
        device="0",         
        project="runs",
        name="Yolo8n2",
        exist_ok=True,
    )
    # evaluatie op test set
    metrics = model.val(data=DATA_YAML, split="test", imgsz=640)

    print("mAP50-95:", float(metrics.box.map))
    print("mAP50:", float(metrics.box.map50))
    print("Precision:", float(metrics.box.mp))
    print("Recall:", float(metrics.box.mr))
    precision = float(metrics.box.mp)
    recall = float(metrics.box.mr)
    f1 = 2 * (precision * recall) / (precision + recall + 1e-6)
    print(f"F1-score: {f1:.4f}")
    
    # Export naar onnx
    best_ckpt = Path("runs/Yolov8n2/weights/best.pt")
    exported = YOLO(str(best_ckpt)).export(format="ncnn")
    print("Exported:", exported)

if __name__ == "__main__":
    main()
