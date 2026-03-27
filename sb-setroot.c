#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <sys/select.h>

#include <xcb/xcb.h>

#define SIZE 256  // root window maximum length
#define TIMEOUT 5 // retry ever TIMEOUT seconds

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

static xcb_atom_t intern_atom(xcb_connection_t *conn, const char *name) {
    xcb_intern_atom_cookie_t ck = xcb_intern_atom(conn, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(conn, ck, NULL);
    if (!r) return XCB_ATOM_NONE;
    xcb_atom_t a = r->atom;
    free(r);
    return a;
}

static int set_root_name(xcb_connection_t *conn, xcb_window_t root,
                         xcb_atom_t atom, xcb_atom_t type_atom,
                         const char *buf) {
    uint32_t len = (uint32_t)strlen(buf);
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        root,
                        atom,
                        type_atom,
                        8,            // format: 8 bits per item
                        len,
                        (const void *)buf);
    xcb_flush(conn);
    if (xcb_connection_has_error(conn)) return -1;
    return 0;
}

int main(void) {
    char buf[SIZE];
    char lastbuf[SIZE] = {0};
    ssize_t n;

    signal(SIGINT, sigint_handler);

    xcb_connection_t *conn = NULL;
    xcb_screen_t *screen = NULL;
    xcb_window_t root = XCB_WINDOW_NONE;
    xcb_atom_t atom_wm_name = XCB_ATOM_NONE;
    xcb_atom_t atom_utf8 = XCB_ATOM_NONE;
    int connected = 0;

    while (running) {
        if (!connected) {
            if (conn) {
                xcb_disconnect(conn);
                conn = NULL;
                screen = NULL;
                root = XCB_WINDOW_NONE;
                atom_wm_name = XCB_ATOM_NONE;
                atom_utf8 = XCB_ATOM_NONE;
            }

            conn = xcb_connect(NULL, NULL);
            if (xcb_connection_has_error(conn)) {
                fprintf(stderr, "[setroot] X server not available; retrying in %d seconds...\n", TIMEOUT);
                xcb_disconnect(conn);
                conn = NULL;

                // allow reading stdin while waiting
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(STDIN_FILENO, &rfds);
                struct timeval tv = { TIMEOUT, 0 };
                int sel = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
                if (sel > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
                    n = read(STDIN_FILENO, buf, SIZE - 1);
                    if (n > 0) {
                        if (n >= SIZE) n = SIZE - 1;
                        buf[n] = '\0';
                        size_t L = strlen(buf);
                        while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r')) { buf[L-1] = '\0'; --L; }
                        for (size_t i = 0; i < L; ++i) if ((unsigned char)buf[i] < 0x20) buf[i] = ' ';
                        strncpy(lastbuf, buf, SIZE);
                        lastbuf[SIZE-1] = '\0';
                    } else if (n == 0) {
                        running = 0;
                    } else {
                        if (errno != EINTR && errno != EAGAIN) {
                            perror("read");
                            running = 0;
                        }
                    }
                }
                continue;
            }

            // connected
            const xcb_setup_t *setup = xcb_get_setup(conn);
            xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
            screen = iter.data;
            if (!screen) {
                fprintf(stderr, "[setroot] connected but no screen found; disconnecting and retrying\n");
                xcb_disconnect(conn);
                conn = NULL;
                sleep(TIMEOUT);
                continue;
            }
            root = screen->root;
            atom_wm_name = intern_atom(conn, "WM_NAME");
            atom_utf8 = intern_atom(conn, "UTF8_STRING");
            fprintf(stderr, "[setroot] connected to X server; root=0x%08x\n", (unsigned)root);
            connected = 1;

            // apply cached lastbuf if present
            if (lastbuf[0]) {
                xcb_atom_t atom_net_wm = intern_atom(conn, "_NET_WM_NAME");
                xcb_atom_t type_for_utf = (atom_utf8 != XCB_ATOM_NONE) ? atom_utf8 : XCB_ATOM_STRING;
                if (atom_net_wm != XCB_ATOM_NONE) set_root_name(conn, root, atom_net_wm, type_for_utf, lastbuf);
                if (atom_wm_name != XCB_ATOM_NONE) set_root_name(conn, root, atom_wm_name, XCB_ATOM_STRING, lastbuf);
            }
        }

        // Wait for stdin but wake up periodically to detect X disconnect
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        struct timeval tv = { TIMEOUT, 0 };

        int sel = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        } else if (sel == 0) {
            // timeout: check connection health
            if (conn && xcb_connection_has_error(conn)) {
                fprintf(stderr, "[setroot] X connection lost; entering awaiting state and retrying\n");
                connected = 0;
            }
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            n = read(STDIN_FILENO, buf, SIZE - 1);
            if (n > 0) {
                if (n >= SIZE) n = SIZE - 1;
                buf[n] = '\0';
                size_t L = strlen(buf);
                while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r')) { buf[L-1] = '\0'; --L; }
                for (size_t i = 0; i < L; ++i) if ((unsigned char)buf[i] < 0x20) buf[i] = ' ';
                strncpy(lastbuf, buf, SIZE);
                lastbuf[SIZE-1] = '\0';

                if (connected && conn) {
                    xcb_atom_t atom_net_wm = intern_atom(conn, "_NET_WM_NAME");
                    xcb_atom_t type_for_utf = (atom_utf8 != XCB_ATOM_NONE) ? atom_utf8 : XCB_ATOM_STRING;

                    if (atom_net_wm != XCB_ATOM_NONE) {
                        if (set_root_name(conn, root, atom_net_wm, type_for_utf, lastbuf) < 0) {
                            fprintf(stderr, "[setroot] failed to set _NET_WM_NAME (connection lost)\n");
                            connected = 0;
                            continue;
                        }
                    }
                    if (atom_wm_name != XCB_ATOM_NONE) {
                        if (set_root_name(conn, root, atom_wm_name, XCB_ATOM_STRING, lastbuf) < 0) {
                            fprintf(stderr, "[setroot] failed to set WM_NAME (connection lost)\n");
                            connected = 0;
                            continue;
                        }
                    }
                } else {
                    // not connected: lastbuf saved for later
                }
            } else if (n == 0) {
                running = 0;
            } else {
                if (errno != EINTR && errno != EAGAIN) {
                    perror("read");
                    running = 0;
                }
            }
        }
    }

    if (conn) xcb_disconnect(conn);
    fprintf(stderr, "[setroot] exiting\n");
    return 0;
}
