#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

static volatile sig_atomic_t uptime = 0;

void pretty_time(unsigned int seconds)
{
    int awake_hours = seconds / 3600;
    int awake_minutes = (seconds / 60) % 60;

    int ideal = 16 * 3600;
    int diff = ideal - seconds;

    int left_hours = diff / 3600;
    int left_minutes = abs((diff / 60) % 60);

    char left_sign = (diff >= 0) ? '-' : '+';

    printf("up %d:%02d [%c%d:%02d]\n",
           awake_hours, awake_minutes,
           left_sign, abs(left_hours), left_minutes);

    fflush(stdout);
}

void reset_clock(int signum)
{
    (void)signum;
    // zero clock on USR1
    uptime = 60;
    pretty_time(uptime);
}

void add_time(int signum)
{
    (void)signum;
    // add 1 hour on USR2
    uptime += 60 * 60;
    pretty_time(uptime);
}

int main()
{
    FILE *stream;
    char *line = NULL;
    char delim = ' ';
    size_t len = 0;
    
    struct sigaction sa;
    
    sa.sa_handler = reset_clock;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    sa.sa_handler = add_time;
    // block SIGUSR1 while handling SIGUSR2 and vice versa
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaction(SIGUSR2, &sa, NULL);

    stream = fopen("/proc/uptime", "r");
    getdelim(&line, &len, delim, stream);
    fclose(stream);
    uptime = (int) strtol(line, (char **)NULL, 10);
    free(line);

    while (1) 
    {
        pretty_time(uptime);
        
        // handle interrupted sleep
        unsigned int sleep_time = 60;
        while (sleep_time > 0) {
            sleep_time = sleep(sleep_time);
            if (errno != EINTR) {
                break;
            }
        }
        
        uptime = uptime + 60;
    }
    return 0;
}
