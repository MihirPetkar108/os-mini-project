import cv2
import numpy as np
import os
import json
from datetime import datetime

INPUT_IMAGE = "data/raw_images/sample_strip.png"
OUTPUT_DIR = "data/processed"
REPORT_DIR = "data/reports"

os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(REPORT_DIR, exist_ok=True)


def analyze_strip(image_path):
    """
    Load a urine strip image and extract the average RGB
    color from manually specified reagent-pad regions.

    The coordinates below are placeholders and must be
    calibrated for the actual input image.
    """

    image = cv2.imread(image_path)

    if image is None:
        raise FileNotFoundError(
            f"Could not load image: {image_path}"
        )

    height, width = image.shape[:2]

    print("\nURINE TEST STRIP IMAGE ANALYSIS")
    print("=" * 45)
    print(f"Image dimensions: {width} x {height}")

    # Placeholder regions.
    # Each region is (x1, y1, x2, y2).
    # These coordinates must be adjusted to the actual image.
    pad_height = max(1, height // 12)
    center_x = width // 2

    results = []

    parameter_names = [
        "LEUKOCYTES",
        "NITRITE",
        "UROBILINOGEN",
        "PROTEIN",
        "PH",
        "BLOOD",
        "SPECIFIC_GRAVITY",
        "KETONES",
        "BILIRUBIN",
        "GLUCOSE"
    ]

    for i, parameter in enumerate(parameter_names):
        y1 = int(height * (0.04 + i * 0.085))
        y2 = min(height, y1 + pad_height)

        x1 = max(0, center_x - width // 8)
        x2 = min(width, center_x + width // 8)

        if y1 >= height or y2 <= y1 or x2 <= x1:
            continue

        roi = image[y1:y2, x1:x2]

        # OpenCV stores image channels as BGR.
        mean_bgr = np.mean(roi, axis=(0, 1))
        mean_rgb = mean_bgr[::-1]

        rgb = [int(round(v)) for v in mean_rgb]

        results.append({
            "parameter": parameter,
            "average_rgb": rgb,
            "region": [x1, y1, x2, y2],
            "interpretation": (
                "Not calibrated: compare with the "
                "manufacturer's reference chart"
            )
        })

    return image, results


def main():
    image, results = analyze_strip(INPUT_IMAGE)

    # Save a copy of the input image.
    processed_path = os.path.join(
        OUTPUT_DIR, "analyzed_strip.png"
    )

    cv2.imwrite(processed_path, image)

    report = {
        "project": "Urine Test Strip Reader",
        "timestamp": datetime.now().isoformat(),
        "image": INPUT_IMAGE,
        "results": results,
        "note": (
            "RGB extraction is experimental. "
            "Clinical interpretation is not calibrated."
        )
    }

    report_path = os.path.join(
        REPORT_DIR, "analysis_report.json"
    )

    with open(report_path, "w") as file:
        json.dump(report, file, indent=4)

    print("\nExtracted Color Information")
    print("-" * 45)

    for result in results:
        print(
            f"{result['parameter']:<20} "
            f"RGB = {result['average_rgb']}"
        )

    print("\nFiles saved:")
    print(f"Processed image: {processed_path}")
    print(f"Analysis report: {report_path}")


if __name__ == "__main__":
    main()
