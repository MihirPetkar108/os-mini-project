/*
 * EXPERIMENT 4: PROCESS MANAGEMENT
 * EXPERIMENT 6: PREEMPTIVE SJF (SRTF)
 * EXPERIMENT 7: MUTUAL EXCLUSION
 *
 * Project: Urine Test Strip Image Analysis
 *
 * System calls:
 *   fork()     - Creates child processes
 *   execvp()   - Executes Python image processor
 *   waitpid()  - Waits for child processes
 *   getpid()   - Returns process ID
 *   getppid()  - Returns parent process ID
 *   kill()     - Sends SIGSTOP and SIGCONT signals
 *
 * Synchronization:
 *   flock() - Locks the shared processing resource.
 *
 * Run from the project root.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#define DEFAULT_IMAGE "data/raw_images/sample_strip.png"
#define PYTHON_SCRIPT "src/image_processor.py"
#define RESOURCE_FILE "reports/.processing.lock"

typedef struct {
    int id;
    char *image;

    int arrival;
    int burst;
    int remaining;

    int start;
    int completion;
    int turnaround;
    int waiting;

    pid_t pid;

    int started;
    int finished;
    int success;
    int actual_done;
} Job;

typedef struct {
    int job_id; /* -1 represents CPU idle */
    int start;
    int end;
} GanttSegment;

static void record_gantt_segment(GanttSegment **segments, int *count,
                                 int *capacity, int job_id,
                                 int start, int end)
{
    if (*count > 0 && (*segments)[*count - 1].job_id == job_id &&
        (*segments)[*count - 1].end == start) {
        (*segments)[*count - 1].end = end;
        return;
    }

    if (*count == *capacity) {
        int new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        GanttSegment *grown = realloc(*segments,
            (size_t)new_capacity * sizeof **segments);
        if (!grown) {
            perror("realloc Gantt chart");
            exit(EXIT_FAILURE);
        }
        *segments = grown;
        *capacity = new_capacity;
    }

    (*segments)[*count] = (GanttSegment){job_id, start, end};
    ++*count;
}

static void log_message(const char *category, const char *message)
{
    printf("[%s PID=%ld] %s\n",
           category, (long)getpid(), message);
    fflush(stdout);
}

static void log_job(int time, int id, const char *message)
{
    printf("[TIME %d] [P%d] %s\n", time, id, message);
    fflush(stdout);
}

/* Generate one PDF per JSON result after all scheduled jobs have finished. */
static int generate_pdf_reports(void)
{
    const char *generator = "experiments/exp2/file_operations";
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork for PDF report generator");
        return 0;
    }
    if (pid == 0) {
        execl(generator, generator, "--from-json", (char *)NULL);
        fprintf(stderr, "Cannot run PDF generator '%s': %s\n",
                generator, strerror(errno));
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid for PDF report generator");
            return 0;
        }
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/*
 * Open the shared processing lock file and acquire an exclusive lock.
 *
 * flock() is advisory: other processes must also cooperate by
 * locking the same file before accessing it.
 */
static int acquire_resource_lock(int process_id, int time)
{
    int fd = open(RESOURCE_FILE, O_CREAT | O_RDWR, 0666);

    if (fd == -1) {
        fprintf(stderr,
                "[TIME %d] [P%d] ERROR: Cannot open shared resource '%s': %s\n",
                time, process_id, RESOURCE_FILE, strerror(errno));
        return -1;
    }

    printf("[TIME %d] [P%d] [MUTEX] Requesting exclusive lock on %s.\n",
           time, process_id, RESOURCE_FILE);
    fflush(stdout);

    if (flock(fd, LOCK_EX) == -1) {
        fprintf(stderr,
                "[TIME %d] [P%d] [MUTEX] Lock acquisition failed: %s\n",
                time, process_id, strerror(errno));
        close(fd);
        return -1;
    }

    printf("[TIME %d] [P%d] [MUTEX] LOCK ACQUIRED. "
           "Other cooperating processes must wait.\n",
           time, process_id);
    fflush(stdout);

    return fd;
}

static void release_resource_lock(int fd, int process_id, int time)
{
    if (fd < 0)
        return;

    printf("[TIME %d] [P%d] [MUTEX] Releasing exclusive lock.\n",
           time, process_id);
    fflush(stdout);

    if (flock(fd, LOCK_UN) == -1) {
        fprintf(stderr,
                "[TIME %d] [P%d] [MUTEX] Unlock failed: %s\n",
                time, process_id, strerror(errno));
    } else {
        printf("[TIME %d] [P%d] [MUTEX] LOCK RELEASED. "
               "Another process may acquire the resource.\n",
               time, process_id);
        fflush(stdout);
    }

    close(fd);
}

/*
 * Create a child for each image.
 *
 * Each child initially stops itself. The parent resumes and suspends
 * children according to the SRTF scheduling simulation.
 */
static void create_processes(Job jobs[], int n)
{
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            printf("[CHILD PID=%ld] Created for P%d.\n",
                   (long)getpid(), jobs[i].id);

            printf("[CHILD PID=%ld] Parent PID=%ld\n",
                   (long)getpid(), (long)getppid());
            fflush(stdout);

            raise(SIGSTOP);

            char *python_args[] = {
                "python3",
                PYTHON_SCRIPT,
                jobs[i].image,
                NULL
            };

            execvp("python3", python_args);

            fprintf(stderr,
                    "[CHILD PID=%ld] execvp failed: %s\n",
                    (long)getpid(), strerror(errno));
            _exit(127);
        }

        jobs[i].pid = pid;

        printf("[PARENT PID=%ld] Created child PID=%ld for P%d.\n",
               (long)getpid(), (long)pid, jobs[i].id);
        fflush(stdout);
    }

    for (int i = 0; i < n; i++) {
        int status;
        pid_t result;

        do {
            result = waitpid(jobs[i].pid, &status, WUNTRACED);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (!WIFSTOPPED(status)) {
            fprintf(stderr,
                    "P%d failed to enter its initial stopped state.\n",
                    jobs[i].id);
            exit(EXIT_FAILURE);
        }

        printf("[PARENT] P%d (PID=%ld) is ready and waiting for scheduling.\n",
               jobs[i].id, (long)jobs[i].pid);
    }
}

/*
 * Select the process with the shortest remaining time among
 * arrived, unfinished processes.
 */
static int select_srtf_job(Job jobs[], int n, int current_time)
{
    int selected = -1;

    for (int i = 0; i < n; i++) {
        if (jobs[i].finished || jobs[i].arrival > current_time)
            continue;

        if (selected == -1 ||
            jobs[i].remaining < jobs[selected].remaining ||
            (jobs[i].remaining == jobs[selected].remaining &&
             jobs[i].arrival < jobs[selected].arrival)) {
            selected = i;
        }
    }

    return selected;
}

/*
 * Execute the SRTF scheduling simulation.
 *
 * Burst times are scheduling estimates, not measurements of the
 * Python process's actual CPU consumption.
 *
 * The parent holds the shared-resource lock for a selected process's
 * time slice. The lock is released before another process is selected.
 */
static void schedule_srtf(Job jobs[], int n, GanttSegment **timeline,
                          int *timeline_count, int *timeline_capacity)
{
    int current_time = 0;
    int completed = 0;
    int previous_selected = -1;

    printf("\n===== PREEMPTIVE SJF (SRTF) =====\n");
    printf("Scheduling policy: Shortest Remaining Time First\n");
    printf("Each scheduling time unit represents one second.\n");
    printf("Shared resource: %s\n", RESOURCE_FILE);
    printf("Synchronization: exclusive flock() around each time slice.\n\n");

    while (completed < n) {
        int selected = select_srtf_job(jobs, n, current_time);

        if (selected == -1) {
            printf("[TIME %d] CPU IDLE: no process has arrived.\n",
                   current_time);
            fflush(stdout);
            record_gantt_segment(timeline, timeline_count, timeline_capacity,
                                 -1, current_time, current_time + 1);
            sleep(1);
            current_time++;
            continue;
        }

        Job *job = &jobs[selected];

        if (previous_selected != -1 &&
            previous_selected != selected &&
            !jobs[previous_selected].finished) {
            printf("[TIME %d] [SCHEDULER] Context switch: P%d -> P%d.\n",
                   current_time,
                   jobs[previous_selected].id,
                   job->id);
        }

        if (!job->started) {
            job->started = 1;
            job->start = current_time;

            log_job(current_time, job->id,
                    "First scheduled for execution.");
        }

        printf("[TIME %d] [SCHEDULER] Selected P%d; remaining burst = %d.\n",
               current_time, job->id, job->remaining);
        fflush(stdout);

        /*
         * Acquire the shared resource before running this time slice.
         * If lock acquisition fails, mark this job as failed.
         */
        int resource_fd = acquire_resource_lock(job->id, current_time);

        if (resource_fd == -1) {
            job->finished = 1;
            job->success = 0;
            job->completion = current_time;
            completed++;
            previous_selected = selected;
            continue;
        }

        printf("[TIME %d] [P%d] [SYNC] Entering protected time slice. "
               "Concurrent protected access is excluded.\n",
               current_time, job->id);
        fflush(stdout);

        /* OpenCV may finish before the configured burst estimate. Keep
           this job scheduled for its remaining burst time before completion. */
        if (job->actual_done) {
            sleep(1);
            record_gantt_segment(timeline, timeline_count, timeline_capacity,
                                 job->id, current_time, current_time + 1);
            if (job->remaining > 0)
                job->remaining--;
            current_time++;
            release_resource_lock(resource_fd, job->id, current_time);

            if (job->remaining == 0) {
                job->finished = 1;
                job->completion = current_time;
                completed++;
                printf("[TIME %d] [P%d] Configured burst time completed; process is complete.\n",
                       current_time, job->id);
            } else {
                printf("[TIME %d] [P%d] OpenCV work is done; consuming remaining burst time (%d seconds).\n",
                       current_time, job->id, job->remaining);
            }
            previous_selected = selected;
            continue;
        }

        if (kill(job->pid, SIGCONT) == -1) {
            fprintf(stderr, "[TIME %d] [P%d] SIGCONT failed: %s\n",
                    current_time, job->id, strerror(errno));

            release_resource_lock(resource_fd, job->id, current_time);

            job->finished = 1;
            job->success = 0;
            job->completion = current_time;
            completed++;
            previous_selected = selected;
            continue;
        }

        printf("[TIME %d] [P%d] Child resumed (PID=%ld).\n",
               current_time, job->id, (long)job->pid);
        fflush(stdout);

        sleep(1);
        record_gantt_segment(timeline, timeline_count, timeline_capacity,
                             job->id, current_time, current_time + 1);

        /*
         * Stop the child at the end of this scheduling time slice.
         */
        int stop_result = kill(job->pid, SIGSTOP);

        if (stop_result == -1 && errno != ESRCH) {
            fprintf(stderr, "[TIME %d] [P%d] SIGSTOP failed: %s\n",
                    current_time, job->id, strerror(errno));
        }

        int status;
        pid_t result;

        do {
            result = waitpid(job->pid, &status, WUNTRACED);
        } while (result == -1 && errno == EINTR);

        current_time++;

        /*
         * Release the shared lock after the time slice.
         */
        release_resource_lock(resource_fd, job->id, current_time);

        if (result == job->pid && WIFEXITED(status)) {
            job->actual_done = 1;
            job->success = (WEXITSTATUS(status) == 0);
            if (job->remaining > 0)
                job->remaining--;
            printf("[TIME %d] [P%d] OpenCV child exited with status %d; remaining configured burst = %d.\n",
                   current_time, job->id, WEXITSTATUS(status), job->remaining);
            if (job->remaining == 0) {
                job->finished = 1;
                job->completion = current_time;
                completed++;
                printf("[TIME %d] [P%d] Process finished after its configured burst time.\n",
                       current_time, job->id);
            }
        } else if (result == job->pid && WIFSIGNALED(status)) {
            job->actual_done = 1;
            job->success = 0;
            if (job->remaining > 0)
                job->remaining--;
            printf("[TIME %d] [P%d] Child terminated by signal %d; remaining configured burst = %d.\n",
                   current_time, job->id, WTERMSIG(status), job->remaining);
            if (job->remaining == 0) {
                job->finished = 1;
                job->completion = current_time;
                completed++;
            }
        } else if (result == job->pid && WIFSTOPPED(status)) {
            if (job->remaining > 0)
                job->remaining--;

            printf("[TIME %d] [P%d] Suspended after time slice. "
                   "Remaining estimated burst = %d.\n",
                   current_time, job->id, job->remaining);

            if (job->remaining == 0) {
                printf("[TIME %d] [P%d] Estimated burst exhausted; "
                       "process will be scheduled again until it exits.\n",
                       current_time, job->id);
            }
        } else if (result == -1) {
            fprintf(stderr, "[TIME %d] [P%d] waitpid failed: %s\n",
                    current_time, job->id, strerror(errno));

            job->finished = 1;
            job->success = 0;
            job->completion = current_time;
            completed++;
        }

        previous_selected = selected;
    }

    /*
     * Compute the standard scheduling metrics.
     *
     * These metrics use the simulated completion times and estimated
     * burst values. They are not measurements of actual CPU execution.
     */
    for (int i = 0; i < n; i++) {
        jobs[i].turnaround = jobs[i].completion - jobs[i].arrival;
        jobs[i].waiting = jobs[i].turnaround - jobs[i].burst;

        if (jobs[i].waiting < 0)
            jobs[i].waiting = 0;
    }

    log_message("SCHEDULER", "All scheduled jobs have been handled.");
}

static void print_process_table(Job jobs[], int n)
{
    double total_waiting = 0.0;
    double total_turnaround = 0.0;

    printf("\n===== PROCESS TABLE =====\n");
    printf("+------+----------+-------+------------+----------+------------+\n");
    printf("| %-4s | %-8s | %-5s | %-10s | %-8s | %-10s |\n",
           "PID", "Arrival", "Burst", "Completion",
           "Waiting", "Turnaround");
    printf("+------+----------+-------+------------+----------+------------+\n");

    for (int i = 0; i < n; i++) {
        printf("| P%-3d | %-8d | %-5d | %-10d | %-8d | %-10d |\n",
               jobs[i].id, jobs[i].arrival, jobs[i].burst,
               jobs[i].completion, jobs[i].waiting,
               jobs[i].turnaround);

        total_waiting += jobs[i].waiting;
        total_turnaround += jobs[i].turnaround;
    }

    printf("+------+----------+-------+------------+----------+------------+\n");

    printf("\nAverage Waiting Time    : %.2f\n", total_waiting / n);
    printf("Average Turnaround Time : %.2f\n", total_turnaround / n);
}

static void print_gantt_chart(const GanttSegment *timeline, int count)
{
    printf("\n===== GANTT CHART =====\n");
    if (count == 0) {
        printf("No execution intervals to display.\n");
        return;
    }

    for (int i = 0; i < count; ++i)
        printf("+----------");
    printf("+\n");

    for (int i = 0; i < count; ++i) {
        char label[16];
        if (timeline[i].job_id < 0)
            snprintf(label, sizeof label, "IDLE");
        else
            snprintf(label, sizeof label, "P%d", timeline[i].job_id);
        printf("| %-8s ", label);
    }
    printf("|\n");

    for (int i = 0; i < count; ++i)
        printf("+----------");
    printf("+\n");

    printf("%-11d", timeline[0].start);
    for (int i = 0; i < count; ++i)
        printf("%-11d", timeline[i].end);
    printf("\n");
}

int main(int argc, char *argv[])
{
    int n;
    char *default_images[] = { (char *)DEFAULT_IMAGE };
    char **images;

    if (argc < 2) {
        images = default_images;
        n = 1;
        printf("No image paths supplied. Using default image: %s\n",
               DEFAULT_IMAGE);
    } else {
        images = &argv[1];
        n = argc - 1;
    }

    Job *jobs = calloc((size_t)n, sizeof(Job));

    if (jobs == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    printf("\n============================================================\n");
    printf(" EXPERIMENT 4: PROCESS MANAGEMENT\n");
    printf(" PREEMPTIVE SJF + MUTUAL EXCLUSION\n");
    printf(" Urine Test Strip Image Analysis\n");
    printf("============================================================\n");
    printf("\nTotal images received: %d\n", n);

    printf("\nEnter arrival time and estimated burst time for each process.\n");
    printf("Arrival time must be non-negative; burst time must be positive.\n\n");

    for (int i = 0; i < n; i++) {
        jobs[i].id = i + 1;
        jobs[i].image = images[i];

        printf("P%d | Image: %s\n", jobs[i].id, jobs[i].image);
        printf("Arrival time: ");

        if (scanf("%d", &jobs[i].arrival) != 1 ||
            jobs[i].arrival < 0) {
            fprintf(stderr, "Invalid arrival time.\n");
            free(jobs);
            return EXIT_FAILURE;
        }

        printf("Burst time: ");

        if (scanf("%d", &jobs[i].burst) != 1 ||
            jobs[i].burst <= 0) {
            fprintf(stderr, "Invalid burst time.\n");
            free(jobs);
            return EXIT_FAILURE;
        }

        jobs[i].remaining = jobs[i].burst;
        jobs[i].pid = -1;
        jobs[i].success = 0;
    }

    /*
     * Ensure the shared-resource directory exists before creating
     * children. The directory should normally be created by the project.
     */
    if (access("experiments/exp2", F_OK) == -1) {
        fprintf(stderr,
                "Error: experiments/exp2 directory does not exist. "
                "Create it before running this experiment.\n");
        free(jobs);
        return EXIT_FAILURE;
    }

    printf("\n===== PROCESS CREATION =====\n");
    create_processes(jobs, n);

    GanttSegment *timeline = NULL;
    int timeline_count = 0;
    int timeline_capacity = 0;
    schedule_srtf(jobs, n, &timeline, &timeline_count, &timeline_capacity);
    print_process_table(jobs, n);
    print_gantt_chart(timeline, timeline_count);

    printf("\n===== PDF REPORT GENERATION =====\n");
    fflush(stdout);
    if (!generate_pdf_reports()) {
        fprintf(stderr,
                "PDF report generation failed. Compile experiments/exp2/file_operations.c first.\n");
        free(jobs);
        free(timeline);
        return EXIT_FAILURE;
    }

    int successful = 0;
    int failed = 0;

    for (int i = 0; i < n; i++) {
        if (jobs[i].success)
            successful++;
        else
            failed++;
    }

    printf("\n===== EXECUTION SUMMARY =====\n");
    printf("Total processes : %d\n", n);
    printf("Successful      : %d\n", successful);
    printf("Failed          : %d\n", failed);

    log_message("PARENT", "Execution completed.");

    free(jobs);
    free(timeline);

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
