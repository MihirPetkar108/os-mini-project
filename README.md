# Urine Test Strip Image Analysis

This OS mini-project uses OpenCV to detect colored reagent pads in urine-strip images. It saves pad colors and image measurements as JSON, generates one PDF per sample, and demonstrates process scheduling and mutual exclusion with POSIX system calls.

Run commands from the project root because the programs use project-relative paths.

## Project structure

```text
.
├── README.md
├── src/
│   ├── image_processor.py
│   ├── analyze_strip.py
│   └── reference_table.py
├── data/
│   ├── raw_images/
│   ├── processed/
│   └── results/
├── experiments/
│   ├── exp2/
│   │   ├── file_operations.c
│   │   └── file_operations        # compiled executable
│   └── exp4/
│       ├── process_management.c
│       ├── process_management    # compiled executable
│       └── output.txt
├── reports/
│   ├── .processing.lock
│   └── *_report.pdf
└── .venv/                         # local Python environment
```

Generated image, JSON, PDF, executable, and lock files may be absent until the corresponding program is run.

## Folders and files

### `src/`

- **`image_processor.py`** — Loads one image, detects colored pads using OpenCV, extracts RGB color and geometry, saves an annotated image and mask under `data/processed/`, and writes results to `data/results/`. It labels detected pads by the configured top-to-bottom order (Leukocytes, Nitrite, Urobilinogen, Protein, then further parameters if detected).
- **`analyze_strip.py`** — An earlier analysis prototype using manually specified pad regions. It writes an analysis JSON report under `data/reports/`; those regions are placeholders and need calibration.
- **`reference_table.py`** — Contains example urine-strip reference levels. RGB reference values are placeholders, so this table is not used to make clinical interpretations.

### `data/`

- **`raw_images/`** — Input sample images (`sample_strip.png`, `sample_strip2.png`, and `sample_strip3.png`).
- **`processed/`** — OpenCV output images: detected-pad annotations and segmentation masks.
- **`results/`** — JSON analysis results, one file per input image. These include pad labels, visual color descriptions, RGB values, dimensions, coordinates, and processing metadata. Medical values remain unassigned.
- **`reports/`** — Created by `analyze_strip.py` for its separate prototype output.

### `experiments/exp2/`

- **`file_operations.c`** — Runs the OpenCV processor for the three sample images (unless called with `--from-json`), reads each JSON result, and writes a separate PDF to the root `reports/` folder. Each PDF includes the configured parameter label, visual color, RGB values, status, area, and coordinates.
- **`file_operations`** — Compiled executable used by exp4 to generate PDFs after scheduling finishes.

### `experiments/exp4/`

- **`process_management.c`** — Creates child processes for the image inputs and schedules them with preemptive Shortest Remaining Time First (SRTF). It asks for each process's arrival and burst times, enforces the configured burst duration, protects scheduled slices with `flock()`, prints a process table and Gantt chart, and runs the exp2 PDF generator at the end.
- **`process_management`** — Compiled exp4 executable.
- **`output.txt`** — Example output from a process-management run.

### `reports/`

- **`*_report.pdf`** — One report per sample image, created by exp2 or automatically after exp4 completes.
- **`.processing.lock`** — Shared advisory lock file used by exp2 and exp4. It can remain empty; the operating system tracks the lock state.

### `.venv/`

Local Python virtual environment. It is not required if OpenCV (`cv2`) and NumPy are already installed in the Python environment used to run the scripts.

## What the C programs do

### `file_operations.c` (exp2)

This program handles image processing and report files:

1. Acquires an exclusive lock on `reports/.processing.lock`.
2. Runs `src/image_processor.py` for the three sample images, unless `--from-json` is supplied.
3. Reads each image's JSON results from `data/results/`.
4. Writes a separate PDF report for each image to `reports/`.
5. Releases the lock when report generation finishes.

The reports include the configured parameter label, visual color, RGB values, pad status, area, and coordinates. `--from-json` skips step 2 and uses existing result files.

### `process_management.c` (exp4)

This program demonstrates process creation, scheduling, and mutual exclusion:

1. Creates a child process for each image; each child runs the OpenCV processor.
2. Prompts for each process's arrival time and burst time, then schedules jobs using preemptive Shortest Remaining Time First (SRTF).
3. Holds an exclusive `flock()` on `reports/.processing.lock` during each scheduled time slice. If image processing exits before the configured burst time, the scheduler continues accounting for the remaining burst.
4. Prints a process table and Gantt chart after scheduling.
5. Runs `experiments/exp2/file_operations --from-json` to create one PDF per JSON result after the processes finish.

Both programs use the same lock file, so their protected sections wait for one another. Compile both executables before running exp4; it launches the exp2 executable at the end.

## Build and run

Install OpenCV and NumPy if needed:

```sh
python3 -m pip install opencv-python numpy
```

Compile both C programs from the project root:

```sh
cc -Wall -Wextra -o experiments/exp2/file_operations experiments/exp2/file_operations.c
cc -Wall -Wextra -o experiments/exp4/process_management experiments/exp4/process_management.c
```

Run exp2 to process the three sample images and generate their PDFs:

```sh
./experiments/exp2/file_operations
```

To generate PDFs from existing JSON without rerunning OpenCV:

```sh
./experiments/exp2/file_operations --from-json
```

Run exp4 with one or more image paths. It prompts for arrival and burst times for each process, then prints scheduling results and generates the PDFs after all jobs finish:

```sh
./experiments/exp4/process_management \
  data/raw_images/sample_strip.png \
  data/raw_images/sample_strip2.png \
  data/raw_images/sample_strip3.png
```

Exp4 invokes the compiled exp2 executable, so compile both programs before running it. Arrival and burst values are in seconds.

## Interpretation limits

Parameter labels follow a configured top-to-bottom strip order. Confirm that order against the exact strip model before relying on the labels. The current image-processing flow extracts color and geometry; it does not compare colors to calibrated manufacturer charts, assign medical values, or provide a diagnosis. Lighting, camera settings, and strip calibration affect measured colors.
