#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define INPUT_COUNT 3
#define MAX_JSON 65536
#define MAX_LINES 320
#define LINE_SIZE 256

static const char *input_files[INPUT_COUNT] = {
    "data/results/sample_strip1_results.json",
    "data/results/sample_strip2_results.json",
    "data/results/sample_strip3_results.json"
};

static void add_line(char lines[MAX_LINES][LINE_SIZE], int *count,
                     const char *format, ...)
{
    va_list args;
    if (*count >= MAX_LINES) return;
    va_start(args, format);
    vsnprintf(lines[*count], LINE_SIZE, format, args);
    va_end(args);
    ++*count;
}

/* Find a JSON string value for a key and decode the common escaped characters. */
static int json_string(const char *json, const char *key, char *out, size_t cap)
{
    char needle[128];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p || !(p = strchr(p + strlen(needle), ':'))) return 0;
    while (isspace((unsigned char)*++p)) {}
    if (*p++ != '"') return 0;
    size_t n = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            c = *p++;
            if (c == 'n') c = ' ';
            else if (c == 't') c = ' ';
        }
        if (n + 1 < cap) out[n++] = c;
    }
    out[n] = '\0';
    return 1;
}

static int json_number(const char *json, const char *key, double *value)
{
    char needle[128];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p || !(p = strchr(p + strlen(needle), ':'))) return 0;
    *value = strtod(p + 1, NULL);
    return 1;
}

static int read_json(const char *path, char *data, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 0; }
    size_t n = fread(data, 1, cap - 1, f);
    if (ferror(f)) { perror("Reading JSON"); fclose(f); return 0; }
    data[n] = '\0';
    fclose(f);
    return 1;
}

/* Run one OpenCV job while the caller holds the shared-resource lock. */
static int run_opencv(const char *image_path)
{
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return 0; }
    if (pid == 0) {
        char *args[] = {"python3", "src/image_processor.py", (char *)image_path, NULL};
        execvp(args[0], args);
        perror("execvp python3");
        _exit(127);
    }
    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) { perror("waitpid"); return 0; }
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* Emit a small, self-contained PDF with Helvetica text and one report page. */
static int write_pdf(const char *path, char lines[MAX_LINES][LINE_SIZE], int count)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 0; }
    char stream[32768];
    size_t used = 0;
    used += (size_t)snprintf(stream + used, sizeof stream - used,
        "BT /F1 8 Tf 42 756 Td 10 TL\n");
    for (int i = 0; i < count && used + LINE_SIZE + 64 < sizeof stream; ++i) {
        char escaped[LINE_SIZE * 2];
        size_t j = 0;
        for (size_t k = 0; lines[i][k] && j + 2 < sizeof escaped; ++k) {
            unsigned char c = (unsigned char)lines[i][k];
            if (c == '(' || c == ')' || c == '\\') escaped[j++] = '\\';
            escaped[j++] = (c >= 32 && c <= 126) ? (char)c : '?';
        }
        escaped[j] = '\0';
        used += (size_t)snprintf(stream + used, sizeof stream - used,
                                 "(%s) Tj T*\n", escaped);
    }
    snprintf(stream + used, sizeof stream - used, "ET\n");
    size_t stream_len = strlen(stream);

    long offsets[6];
    fputs("%PDF-1.4\n", f);
    offsets[1] = ftell(f); fputs("1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n", f);
    offsets[2] = ftell(f); fputs("2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n", f);
    offsets[3] = ftell(f); fputs("3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >> endobj\n", f);
    offsets[4] = ftell(f); fputs("4 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n", f);
    offsets[5] = ftell(f);
    fprintf(f, "5 0 obj << /Length %zu >> stream\n", stream_len);
    fwrite(stream, 1, stream_len, f);
    fputs("endstream\nendobj\n", f);
    long xref = ftell(f);
    fputs("xref\n0 6\n0000000000 65535 f \n", f);
    for (int i = 1; i <= 5; ++i) fprintf(f, "%010ld 00000 n \n", offsets[i]);
    fprintf(f, "trailer << /Size 6 /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", xref);
    int ok = !ferror(f) && fclose(f) == 0;
    if (!ok) perror("Writing PDF");
    return ok;
}

int main(int argc, char *argv[])
{
    int from_json_only = 0;
    if (argc == 2 && strcmp(argv[1], "--from-json") == 0) {
        from_json_only = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--from-json]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char lines[MAX_LINES][LINE_SIZE];
    int line_count = 0;

    if (mkdir("reports", 0755) == -1 && errno != EEXIST) {
        perror("Creating reports directory");
        return EXIT_FAILURE;
    }

    /* Share the same lock file used by exp4. Hold it for the entire
       OpenCV -> JSON read -> PDF write workflow. */
    const char *lock_path = "reports/.processing.lock";
    int lock_fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (lock_fd == -1) { perror("Opening shared lock file"); return EXIT_FAILURE; }
    printf("[MUTEX] Requesting exclusive lock on %s...\n", lock_path);
    fflush(stdout);
    if (flock(lock_fd, LOCK_EX) == -1) {
        perror("flock LOCK_EX"); close(lock_fd); return EXIT_FAILURE;
    }
    printf("[MUTEX] Lock acquired. JSON reading and PDF generation are protected.\n");
    fflush(stdout);

    static const char *images[INPUT_COUNT] = {
        "data/raw_images/sample_strip1.png",
        "data/raw_images/sample_strip2.png",
        "data/raw_images/sample_strip3.png"
    };
    if (!from_json_only) {
        for (int i = 0; i < INPUT_COUNT; ++i) {
            if (!run_opencv(images[i])) {
                fprintf(stderr, "OpenCV processing failed for %s\n", images[i]);
                flock(lock_fd, LOCK_UN); close(lock_fd); return EXIT_FAILURE;
            }
        }
    } else {
        printf("Generating reports from existing JSON files; OpenCV will not be rerun.\n");
    }

    for (int i = 0; i < INPUT_COUNT; ++i) {
        line_count = 0;
        char json[MAX_JSON], image[160] = "(not specified)", timestamp[80] = "(not specified)";
        char method[160] = "(not specified)";
        double pad_count = 0, seconds = 0;
        if (!read_json(input_files[i], json, sizeof json)) {
            flock(lock_fd, LOCK_UN); close(lock_fd); return EXIT_FAILURE;
        }
        json_string(json, "input_image", image, sizeof image);
        json_string(json, "timestamp", timestamp, sizeof timestamp);
        json_string(json, "detection_method", method, sizeof method);
        json_number(json, "number_of_pads_detected", &pad_count);
        json_number(json, "processing_time_seconds", &seconds);
        add_line(lines, &line_count, "URINE STRIP IMAGE ANALYSIS REPORT");
        add_line(lines, &line_count, "Generated from %s", input_files[i]);
        add_line(lines, &line_count, "Clinical interpretation is not performed.");
        add_line(lines, &line_count, "");
        add_line(lines, &line_count, "Sample %d: %s", i + 1, image);
        add_line(lines, &line_count, "Timestamp: %s | Pads detected: %.0f | Processing: %.4f s",
                 timestamp, pad_count, seconds);
        add_line(lines, &line_count, "Method: %s", method);

        const char *pads = strstr(json, "\"pads\"");
        const char *end = strstr(json, "\"processing_time_seconds\"");
        if (pads && end && end > pads) {
            const char *p = pads;
            int found = 0;
            while ((p = strstr(p, "\"pad_number\"")) && p < end) {
                double number = 0, red = 0, green = 0, blue = 0;
                double x = 0, y = 0, width = 0, height = 0, area = 0;
                char color[80] = "(unspecified)";
                char parameter[80] = "Not identified in JSON";
                char status[120] = "Not provided";
                char medical_value[80] = "Not provided (JSON value is null or absent)";
                json_number(p, "pad_number", &number);
                json_number(p, "red", &red);
                json_number(p, "green", &green);
                json_number(p, "blue", &blue);
                json_number(p, "x", &x);
                json_number(p, "y", &y);
                json_number(p, "width", &width);
                json_number(p, "height", &height);
                json_number(p, "area_pixels", &area);
                json_string(p, "visual_color_description", color, sizeof color);
                if (!json_string(p, "parameter_name", parameter, sizeof parameter) &&
                    !json_string(p, "parameter", parameter, sizeof parameter)) {
                    json_string(p, "analyte", parameter, sizeof parameter);
                }
                json_string(p, "status", status, sizeof status);
                json_string(p, "medical_value", medical_value, sizeof medical_value);
                add_line(lines, &line_count, "  Pad %.0f | Parameter: %s | Colour: %s | RGB: %.0f, %.0f, %.0f",
                         number, parameter, color, red, green, blue);
                add_line(lines, &line_count,
                         "    Value: %s | Status: %s | Area: %.0f px | Bounds: x=%.0f y=%.0f w=%.0f h=%.0f",
                         medical_value, status, area, x, y, width, height);
                ++found;
                p += strlen("\"pad_number\"");
            }
            if (!found) add_line(lines, &line_count, "  No pad details found.");
        } else {
            add_line(lines, &line_count, "  Pad details unavailable.");
        }
        add_line(lines, &line_count, "");

        add_line(lines, &line_count, "Limitations: Results depend on lighting, camera, and calibration.");
        add_line(lines, &line_count, "Parameter labels follow the configured top-to-bottom order; confirm this mapping for the exact strip model.");
        add_line(lines, &line_count, "Clinical values remain unassigned until calibrated reference data is supplied.");
        add_line(lines, &line_count, "Automated detection has not been validated on real strip photographs.");

        char output[256];
        const char *base = strrchr(image, '/');
        base = base ? base + 1 : image;
        char stem[160];
        snprintf(stem, sizeof stem, "%s", base);
        char *extension = strrchr(stem, '.');
        if (extension) *extension = '\0';
        snprintf(output, sizeof output, "reports/%s_report.pdf", stem);
        if (!write_pdf(output, lines, line_count)) {
            flock(lock_fd, LOCK_UN); close(lock_fd); return EXIT_FAILURE;
        }
        printf("PDF report created: %s\n", output);
    }
    printf("[MUTEX] Releasing lock after report generation.\n");
    if (flock(lock_fd, LOCK_UN) == -1) { perror("flock LOCK_UN"); close(lock_fd); return EXIT_FAILURE; }
    close(lock_fd);
    return EXIT_SUCCESS;
}
