#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

volatile sig_atomic_t stop_flag = 0;
static void on_sigint(int sig) { (void)sig; stop_flag = 1; }

typedef struct {
    unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
} cpu_t;

static int read_cpu(cpu_t *t) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    int n = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &t->user, &t->nice, &t->sys, &t->idle,
                   &t->iowait, &t->irq, &t->softirq, &t->steal);
    fclose(f);
    return (n < 4) ? -1 : 0;
}

static unsigned long long total(const cpu_t *t) {
    return t->user + t->nice + t->sys + t->idle +
           t->iowait + t->irq + t->softirq + t->steal;
}
static unsigned long long idle(const cpu_t *t) {
    return t->idle + t->iowait;
}

static void sleep_frac(double s) {
    if (s <= 0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR && !stop_flag);
}

int main(int argc, char **argv) {
    double interval = 1.0;
    int decimals = 1;
    int opt;
    while ((opt = getopt(argc, argv, "st:d:")) != -1) {
        if (opt == 't') interval = atof(optarg);
        else if (opt == 'd') decimals = atoi(optarg);
    }
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    if (signal(SIGINT, on_sigint) == SIG_ERR) return 1;

    cpu_t prev, cur;
    if (read_cpu(&prev) != 0) return 1;

    while (!stop_flag) {
        sleep_frac(interval);
        if (stop_flag) break;

        if (read_cpu(&cur) != 0) break;
        unsigned long long d_total = total(&cur) - total(&prev);
        unsigned long long d_idle  = idle(&cur) - idle(&prev);

        double pct = 0.0;
        if (d_total > 0)
            pct = (1.0 - (double)d_idle / (double)d_total) * 100.0;
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;

        long long mult = 1;
        for (int i=0; i<decimals; ++i) mult *= 10;
        long long val = (long long)(pct * mult + 0.5);
        long long i_part = val / mult;
        long long f_part = llabs(val % mult);

        if (decimals == 0)
            printf("CPU: %3lld%%\n", i_part);
        else
            printf("CPU: %3lld.%0*lld%%\n", i_part, decimals, f_part);
        fflush(stdout);

        prev = cur;
    }
    return 0;
}
