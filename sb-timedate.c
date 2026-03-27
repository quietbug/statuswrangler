#define _POSIX_C_SOURCE 200809L
#include <stdio.h> 
#include <stdlib.h>
#include <time.h> 
#include <unistd.h>
#include <signal.h>

time_t rawtime;
struct tm * timeinfo;
int offset_minutes = 0;

void offsetAdd(int sig)   { (void)sig; offset_minutes += 60; }
void offsetSub(int sig)   { (void)sig; offset_minutes -= 60; }
void offsetReset(void)    { offset_minutes = 0;   }

volatile sig_atomic_t sigint_count = 0;
time_t last_sigint_time = 0;

void handle_sigint(int sig) {
	(void)sig;
    time_t current_time = time(NULL);
    
    // check if the last SIGINT was received within 2 seconds
    if (difftime(current_time, last_sigint_time) < 2) {
        sigint_count++;
    } else {
        sigint_count = 1;
    }
    
    last_sigint_time = current_time;

    if (sigint_count == 1) {
		offsetReset();
    } else if (sigint_count >= 2) {
        exit(0);
    }
}


int main ()
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

	signal(SIGHUP, offsetSub);
	signal(SIGUSR2, offsetAdd);
	
	while (1) {
		time (&rawtime);
		timeinfo = localtime (&rawtime);

		time_t adjusted_time = rawtime + offset_minutes * 60;
		timeinfo = localtime(&adjusted_time);

		printf ("%s", asctime(timeinfo));
		fflush(stdout);
		sleep(1);
	}

	return 0;
}
