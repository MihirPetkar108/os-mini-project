import cv2
import numpy as np
import os
import sys
import json
import time
from datetime import datetime

# ============================================================
# URINE TEST STRIP IMAGE ANALYSIS
# OS MINI PROJECT
# ============================================================

# These paths are relative to the project root.
PROCESSED_DIR = "data/processed"
RESULTS_DIR = "data/results"

# Expected reagent-pad order from top to bottom. Confirm this order against
# the manufacturer's layout for the specific strip before interpreting results.
PAD_PARAMETERS = [
    "LEUKOCYTES",
    "NITRITE",
    "UROBILINOGEN",
    "PROTEIN",
    "PH",
    "BLOOD",
    "SPECIFIC_GRAVITY",
    "KETONES",
    "BILIRUBIN",
    "GLUCOSE",
]


# ============================================================
# STEP 1: DETECT COLORED PADS
# ============================================================

def detect_pads(image):
    """
    Detect colored reagent pads using HSV saturation,
    morphological operations, and contour detection.
    """

    print("\n[STEP 1] Converting image to HSV color space...")

    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)

    # Saturation helps distinguish colored regions from
    # a relatively gray or white background.
    saturation = hsv[:, :, 1]

    print("[STEP 2] Separating colored regions from background...")

    _, mask = cv2.threshold(
        saturation,
        20,
        255,
        cv2.THRESH_BINARY
    )

    print("[STEP 3] Removing gaps and image noise...")

    kernel = cv2.getStructuringElement(
        cv2.MORPH_RECT,
        (5, 5)
    )

    mask = cv2.morphologyEx(
        mask,
        cv2.MORPH_CLOSE,
        kernel
    )

    mask = cv2.morphologyEx(
        mask,
        cv2.MORPH_OPEN,
        np.ones((3, 3), dtype=np.uint8)
    )

    print("[STEP 4] Finding connected colored regions...")

    contours, _ = cv2.findContours(
        mask,
        cv2.RETR_EXTERNAL,
        cv2.CHAIN_APPROX_SIMPLE
    )

    print(f"Total contours found: {len(contours)}")

    pads = []

    for contour in contours:

        x, y, w, h = cv2.boundingRect(contour)
        area = cv2.contourArea(contour)

        # Reject small objects and thin lines.
        if area < 300:
            continue

        if w < 20 or h < 10:
            continue

        # Reagent pads are expected to be approximately
        # rectangular. Adjust this range for other images.
        aspect_ratio = w / float(h)

        if not 1.0 <= aspect_ratio <= 5.0:
            continue

        pads.append({
            "x": int(x),
            "y": int(y),
            "width": int(w),
            "height": int(h),
            "area": float(area)
        })

    # Sort detected pads from top to bottom.
    pads.sort(key=lambda pad: pad["y"])

    print(f"Candidate colored pads detected: {len(pads)}")

    return pads, mask


# ============================================================
# STEP 2: EXTRACT PAD COLOR
# ============================================================

def extract_pad_color(image, pad):
    """
    Extract the median RGB color from the central portion
    of a detected pad.
    """

    x = pad["x"]
    y = pad["y"]
    w = pad["width"]
    h = pad["height"]

    crop = image[y:y + h, x:x + w]

    if crop.size == 0:
        return [0, 0, 0]

    # Ignore the border to reduce contamination from edges.
    margin_x = max(1, int(w * 0.15))
    margin_y = max(1, int(h * 0.15))

    if w > 2 * margin_x and h > 2 * margin_y:
        crop = crop[
            margin_y:h - margin_y,
            margin_x:w - margin_x
        ]

    # OpenCV stores colors as BGR.
    crop_rgb = cv2.cvtColor(
        crop,
        cv2.COLOR_BGR2RGB
    )

    # Median color is less sensitive to noisy pixels.
    rgb = np.median(
        crop_rgb.reshape(-1, 3),
        axis=0
    )

    return [
        int(round(value))
        for value in rgb
    ]


# ============================================================
# STEP 3: DESCRIBE PAD COLOR
# ============================================================

def describe_color(rgb):
    """
    Return a basic visual color description.

    These descriptions are not medical interpretations.
    """

    r, g, b = rgb

    # Grayscale or nearly grayscale colors.
    if max(rgb) - min(rgb) < 25:

        brightness = sum(rgb) / 3

        if brightness > 200:
            return "Light gray / near white"

        elif brightness < 80:
            return "Dark gray"

        else:
            return "Gray"

    if r > g * 1.25 and r > b * 1.15:
        return "Reddish"

    if g > r * 1.15 and g > b * 1.05:
        return "Greenish"

    if b > r * 1.15 and b > g * 1.05:
        return "Bluish"

    if r > 100 and g > 80 and b < 140:
        return "Yellowish / brownish"

    if r > 100 and b > 90 and g < 150:
        return "Pinkish / purplish"

    return "Mixed color"


# ============================================================
# STEP 4: PROCESS IMAGE
# ============================================================

def process_image(image_path):
    """
    Load the input image, detect pads, extract colors,
    save annotated images, and write JSON results.

    Returns True if processing and output saving succeed.
    Returns False if an error occurs.
    """

    start_time = time.time()

    print("\n" + "=" * 60)
    print("URINE TEST STRIP IMAGE ANALYSIS")
    print("=" * 60)

    # --------------------------------------------------------
    # STEP 0: VALIDATE INPUT
    # --------------------------------------------------------

    print("\n[STEP 0] Checking input image...")

    if not os.path.isfile(image_path):
        print(f"ERROR: Input file not found: {image_path}")
        return False

    image = cv2.imread(image_path)

    if image is None:
        print("ERROR: OpenCV could not read the image.")
        print("Check whether the file is a valid image.")
        return False

    height, width = image.shape[:2]

    if height == 0 or width == 0:
        print("ERROR: The input image has invalid dimensions.")
        return False

    print(f"Image loaded successfully: {width} x {height} pixels")

    # --------------------------------------------------------
    # STEP 1: DETECT PADS
    # --------------------------------------------------------

    pads, mask = detect_pads(image)

    # --------------------------------------------------------
    # STEP 2: CREATE OUTPUT DIRECTORIES
    # --------------------------------------------------------

    print("\n[STEP 5] Creating output directories...")

    try:
        os.makedirs(PROCESSED_DIR, exist_ok=True)
        os.makedirs(RESULTS_DIR, exist_ok=True)

    except OSError as error:
        print(f"ERROR: Could not create output directories: {error}")
        return False

    filename = os.path.basename(image_path)
    name = os.path.splitext(filename)[0]

    processed_path = os.path.join(
        PROCESSED_DIR,
        name + "_detected.png"
    )

    mask_path = os.path.join(
        PROCESSED_DIR,
        name + "_mask.png"
    )

    results_path = os.path.join(
        RESULTS_DIR,
        name + "_results.json"
    )

    # --------------------------------------------------------
    # STEP 3: EXTRACT COLORS AND ANNOTATE IMAGE
    # --------------------------------------------------------

    print("\n[STEP 6] Extracting individual pad colors...")

    annotated = image.copy()
    pad_results = []

    for index, pad in enumerate(pads, start=1):

        rgb = extract_pad_color(image, pad)
        color_label = describe_color(rgb)

        x = pad["x"]
        y = pad["y"]
        w = pad["width"]
        h = pad["height"]

        pad_result = {
            "pad_number": index,

            "parameter": (
                PAD_PARAMETERS[index - 1]
                if index <= len(PAD_PARAMETERS)
                else "UNMAPPED"
            ),

            "coordinates": {
                "x": x,
                "y": y,
                "width": w,
                "height": h
            },

            "area_pixels": pad["area"],

            "rgb": {
                "red": rgb[0],
                "green": rgb[1],
                "blue": rgb[2]
            },

            "visual_color_description": color_label,

            # Clinical interpretation is intentionally omitted.
            "medical_value": None,

            "status": "Color extracted; parameter label follows configured strip order; clinical value not assigned"
        }

        pad_results.append(pad_result)

        # Draw a green rectangle around the detected pad.
        cv2.rectangle(
            annotated,
            (x, y),
            (x + w, y + h),
            (0, 255, 0),
            2
        )

        # Add a label above the rectangle.
        cv2.putText(
            annotated,
            f"Pad {index}",
            (x, max(20, y - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            (0, 0, 255),
            2
        )

        print(f"\nPad {index}")
        print(f"  Position: x={x}, y={y}")
        print(f"  Size: {w} x {h}")
        print(f"  Area: {pad['area']:.2f} pixels")
        print(f"  Median RGB: {rgb}")
        print(f"  Visual color: {color_label}")

    # --------------------------------------------------------
    # STEP 4: SAVE OUTPUT IMAGES
    # --------------------------------------------------------

    print("\n[STEP 7] Saving processed images...")

    try:
        annotated_saved = cv2.imwrite(
            processed_path,
            annotated
        )

        mask_saved = cv2.imwrite(
            mask_path,
            mask
        )

        if not annotated_saved:
            print("ERROR: Could not save annotated image.")
            return False

        if not mask_saved:
            print("ERROR: Could not save segmentation mask.")
            return False

    except cv2.error as error:
        print(f"ERROR: Image saving failed: {error}")
        return False

    print(f"Annotated image saved: {processed_path}")
    print(f"Segmentation mask saved: {mask_path}")

    # --------------------------------------------------------
    # STEP 5: PREPARE STRUCTURED RESULTS
    # --------------------------------------------------------

    elapsed = round(time.time() - start_time, 4)

    results = {
        "input_image": image_path,

        "image_dimensions": {
            "width": width,
            "height": height
        },

        "detection_method": (
            "HSV saturation threshold, "
            "morphological operations, and contours"
        ),

        "number_of_pads_detected": len(pad_results),

        "pads": pad_results,

        "processing_time_seconds": elapsed,

        "timestamp": datetime.now().isoformat(),

        "output_files": {
            "annotated_image": processed_path,
            "segmentation_mask": mask_path
        },

        "limitations": [
            "Pad detection has not been validated on real strip photographs.",
            "Parameter labels follow the configured top-to-bottom order and must be confirmed for the exact strip model.",
            "No clinical interpretation is performed.",
            "Color results depend on lighting, camera, and calibration.",
            "HSV thresholding may detect unwanted colored regions."
        ]
    }

    # --------------------------------------------------------
    # STEP 6: SAVE JSON RESULTS
    # --------------------------------------------------------

    print("\n[STEP 8] Writing JSON results...")

    try:
        with open(
            results_path,
            "w",
            encoding="utf-8"
        ) as file:

            json.dump(
                results,
                file,
                indent=4
            )

    except (OSError, TypeError, ValueError) as error:
        print(f"ERROR: Could not save JSON results: {error}")
        return False

    print(f"JSON results saved: {results_path}")

    # --------------------------------------------------------
    # STEP 7: DISPLAY SUMMARY
    # --------------------------------------------------------

    print("\n" + "=" * 60)
    print("PROCESSING SUMMARY")
    print("=" * 60)

    print(f"Input image: {image_path}")
    print(f"Image dimensions: {width} x {height}")
    print(f"Pads detected: {len(pad_results)}")
    print(f"Processing time: {elapsed} seconds")

    if len(pad_results) == 4:
        print("SUCCESS: Four candidate colored pads detected.")

    elif len(pad_results) == 0:
        print("WARNING: No colored pads detected.")
        print("The image or HSV threshold may need adjustment.")

    else:
        print(
            f"WARNING: Expected 4 pads, "
            f"but detected {len(pad_results)}."
        )

    print("\nImage analysis completed.")
    print("Results are visual estimates, not medical diagnoses.")

    return True


# ============================================================
# MAIN ENTRY POINT
# ============================================================

if __name__ == "__main__":

    if len(sys.argv) < 2:

        print(
            "Usage: python3 src/image_processor.py "
            "data/raw_images/sample_strip.png"
        )

        sys.exit(1)

    if len(sys.argv) > 2:

        print("ERROR: Please provide only one image path.")
        print(
            "Usage: python3 src/image_processor.py "
            "data/raw_images/sample_strip.png"
        )

        sys.exit(1)

    image_path = sys.argv[1]

    success = process_image(image_path)

    sys.exit(0 if success else 1)
