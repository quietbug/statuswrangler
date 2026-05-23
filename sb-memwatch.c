// Note: does not consider SReclaimable as used

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

static double adjust_unit(double value_kb, const char **suffix) {
    static const char *units[] = {"kB", "MB", "GB", "TB"};
    int i = 0;
    double v = value_kb;

    while (v > 1024.0 && i < 3) {
        v /= 1024.0;
        i++;
    }

    *suffix = units[i];
    return v;
}

static int read_meminfo(
        double *used_kb,
        double *available_kb,
        double *shared_kb)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    char key[64];
    unsigned long value;
    char unit[16];

    unsigned long mem_total = 0;
    unsigned long mem_free = 0;
    unsigned long mem_available = 0;
    unsigned long buffers = 0;
    unsigned long cached = 0;
    unsigned long sreclaimable = 0;
    unsigned long shmem = 0;

    while (fscanf(f, "%63s %lu %15s\n", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0)
            mem_total = value;
        else if (strcmp(key, "MemFree:") == 0)
            mem_free = value;
        else if (strcmp(key, "MemAvailable:") == 0)
            mem_available = value;
        else if (strcmp(key, "Buffers:") == 0)
            buffers = value;
        else if (strcmp(key, "Cached:") == 0)
            cached = value;
        else if (strcmp(key, "SReclaimable:") == 0)
            sreclaimable = value;
        else if (strcmp(key, "Shmem:") == 0)
            shmem = value;
    }

    fclose(f);

    unsigned long used =
        mem_total - mem_free - buffers - cached - sreclaimable;

    *used_kb = (double)used;
    *available_kb = (double)mem_available;
    *shared_kb = (double)shmem;

    return 0;
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    while (running) {
        double used_kb = 0;
        double available_kb = 0;
        double shared_kb = 0;

        if (read_meminfo(
                    &used_kb,
                    &available_kb,
                    &shared_kb) == 0)
        {
            const char *suf_used;
            const char *suf_available;
            const char *suf_shared;

            double used_hr =
                adjust_unit(used_kb, &suf_used);

            double available_hr =
                adjust_unit(available_kb, &suf_available);

            double shared_hr =
                adjust_unit(shared_kb, &suf_shared);

            printf(
                "Used: %.2f %s | "
                "Available: %.2f %s | "
                "Shared: %.2f %s\n",
                used_hr, suf_used,
                available_hr, suf_available,
                shared_hr, suf_shared
            );

            fflush(stdout);
        } else {
            fprintf(stderr, "Failed to read /proc/meminfo\n");
            break;
        }

        sleep(1);
    }

    return 0;
}
