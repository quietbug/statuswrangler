#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define SYSNET "/sys/class/net"
#define BUF_SZ 256

typedef struct {
    char name[64];
    unsigned long long prev_rx;
    unsigned long long curr_rx;
} iface_t;

int read_ulong(const char *path, unsigned long long *val) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[64];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) return -1;
    buf[len] = 0;
    *val = strtoull(buf, NULL, 10);
    return 0;
}

int is_up(const char *ifname) {
    char path[BUF_SZ];
    snprintf(path, sizeof(path), SYSNET"/%s/operstate", ifname);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[16];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) return 0;
    buf[len] = 0;
    return (strncmp(buf, "up", 2) == 0);
}

int list_up_ifaces(iface_t *arr, int max) {
    DIR *d = opendir(SYSNET);
    if (!d) return 0;
    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) && count < max) {
        if (ent->d_name[0] == '.') continue;
        if (strcmp(ent->d_name, "lo") == 0) continue;
        if (is_up(ent->d_name)) {
            strncpy(arr[count].name, ent->d_name, sizeof(arr[count].name)-1);
            arr[count].name[sizeof(arr[count].name)-1] = 0;
            arr[count].prev_rx = 0;
            arr[count].curr_rx = 0;
            count++;
        }
    }
    closedir(d);
    return count;
}

unsigned long long get_rx(const char *ifname) {
    char path[BUF_SZ];
    snprintf(path, sizeof(path), SYSNET"/%s/statistics/rx_bytes", ifname);
    unsigned long long val = 0;
    if (read_ulong(path, &val) < 0)
        return 0;
    return val;
}

void print_bandwidth(unsigned long long delta) {
    double kbps = delta * 8.0 / 1024.0;
    if (kbps > 1024 * 1024)
        printf("%.2f Gbps", kbps / (1024.0 * 1024.0));
    else if (kbps > 1024)
        printf("%.2f Mbps", kbps / 1024.0);
    else
        printf("%.1f kbps", kbps);
}

int main(int argc, char **argv) {
    char *target = NULL;
    if (argc == 3 && strcmp(argv[1], "-i") == 0)
        target = argv[2];

    iface_t ifs[32];
    int count = 0;

    if (target)
        count = 1, strncpy(ifs[0].name, target, sizeof(ifs[0].name)-1);
    else
        count = list_up_ifaces(ifs, 32);

    for (int i = 0; i < count; i++)
        ifs[i].curr_rx = get_rx(ifs[i].name);

    while (1) {
        sleep(1);

        if (!target) {
            // if none up, rediscover
            if (count == 0)
                count = list_up_ifaces(ifs, 32);
            else {
                // check if existing still up, else reset
                int alive = 0;
                for (int i = 0; i < count; i++)
                    if (is_up(ifs[i].name)) alive = 1;
                if (!alive)
                    count = 0;
            }
        }

        if (count == 0) {
            printf("\n");
            fflush(stdout);
            continue;
        }

        for (int i = 0; i < count; i++) {
            ifs[i].prev_rx = ifs[i].curr_rx;
            ifs[i].curr_rx = get_rx(ifs[i].name);
            unsigned long long delta = ifs[i].curr_rx - ifs[i].prev_rx;
			if (delta < 1000) continue;
            printf("%s: ", ifs[i].name);
            print_bandwidth(delta);
            if (i < count - 1) printf(" | ");
        }
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
