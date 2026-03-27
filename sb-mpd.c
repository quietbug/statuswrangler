// TODO: fix weirdness when sometimes connection 
// silently fails if no tracks were played in mpd

#include <mpd/client.h>
#include <mpd/status.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#define TRUNC_CHAR '+'
#define TRUNC_LEN 28

static struct mpd_connection* conn = NULL;

void cleanup(void) {
	if (conn) {
		mpd_send_noidle(conn);
		mpd_response_finish(conn);
		mpd_connection_free(conn);
		conn = NULL;
	}
}

void sig_handler(int signum) {
	(void)signum;
	fprintf(stderr, "Caught interrupt, cleaning up and exiting...\n");
	cleanup();
	exit(0);
}

size_t utf8_truncate(const char *s, size_t max_chars) {
    size_t i = 0;
    size_t chars = 0;

    while (s[i] && chars < max_chars) {
        unsigned char c = s[i];

        if ((c & 0x80) == 0)       i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else break;

        chars++;
    }

    return i;
}

int main() {
	const char *title = NULL, *artist = NULL, *uri = NULL;
	struct mpd_status *status = NULL;
	struct mpd_song *song = NULL;
	enum mpd_state state;
	bool status_code;
	char status_icon = '?';

	signal(SIGINT, sig_handler);
	atexit(cleanup);

main_loop:
	conn = mpd_connection_new(NULL, 0, 0);
	if (!conn) {
		fprintf(stderr, "Failed to allocate MPD connection.\n");
		sleep(10);
		goto main_loop;
	}

	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
		fprintf(stderr, "MPD connection failed: %s\n", mpd_connection_get_error_message(conn));
		mpd_connection_free(conn);
		conn = NULL;
		sleep(10);
		goto main_loop;
	}

	do {
		status_code = mpd_send_status(conn);
		if (!status_code) {
			fprintf(stderr, "MPD send_status() failed: %s\n",
			        mpd_connection_get_error_message(conn));
			sleep(10);
			goto main_loop;
		}

		status = mpd_recv_status(conn);
		if (!status) {
			fprintf(stderr, "MPD recv_status() returned NULL: %s\n",
			        mpd_connection_get_error_message(conn));
			sleep(10);
			goto main_loop;
		}

		state = mpd_status_get_state(status);
		switch (state) {
			case MPD_STATE_PLAY:  status_icon = '>'; break;
			case MPD_STATE_PAUSE: status_icon = '='; break;
			case MPD_STATE_STOP:  status_icon = 'x'; break;
			default:              status_icon = '?'; break;
		}

		if (state == MPD_STATE_PLAY || state == MPD_STATE_PAUSE) {
			if (!mpd_send_current_song(conn)) {
				fprintf(stderr, "MPD send_current_song() failed: %s\n",
				        mpd_connection_get_error_message(conn));
				mpd_status_free(status);
				mpd_response_finish(conn);
				sleep(10);
				goto main_loop;
			}

			song = mpd_recv_song(conn);
			if (!song) {
				fprintf(stderr, "MPD recv_song() returned NULL.\n");
				mpd_status_free(status);
				mpd_response_finish(conn);
				sleep(10);
				goto main_loop;
			}

			artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
			title  = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
			uri    = mpd_song_get_uri(song);

			if (artist && title) {
				size_t len_artist = utf8_truncate(artist, TRUNC_LEN / 2);
				size_t len_title  = utf8_truncate(title,  TRUNC_LEN / 2);
				printf("[%c] %.*s - %.*s", status_icon,
					   (int)len_artist, artist,
					   (int)len_title,  title);
				if (len_artist + 3 + len_title >= TRUNC_LEN) {
					putchar(TRUNC_CHAR);
				}
			} else if (uri) {
				size_t len = utf8_truncate(uri, TRUNC_LEN);
				printf("[%c] %.*s", status_icon, (int)len,
					   uri ? uri : "(unknown)");
				if (len >= TRUNC_LEN) {
					putchar(TRUNC_CHAR);
				}
			} else {
				printf("[%c] (no tag/uri available)", status_icon);
			}
			putchar('\n');

			fflush(stdout);
			mpd_song_free(song);
		} else {
			printf("\n");
			fflush(stdout);
		}

		mpd_status_free(status);
		mpd_response_finish(conn);

	} while (mpd_run_idle_mask(conn, MPD_IDLE_PLAYER));

	fprintf(stderr, "MPD idle ended unexpectedly: %s\n",
	        mpd_connection_get_error_message(conn));
	mpd_connection_free(conn);
	conn = NULL;
	sleep(10);
	goto main_loop;

	return 0;
}
