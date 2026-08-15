# StatusWrangler bug report

Tracking key: `[ ]` open, `[x]` fixed, `[~]` verified but deferred. Priority reflects user impact and failure severity.

## High priority

- [x] **BUG-H01 — Broken supervisor installation** — `Makefile:32`
  - **Issue:** `make install` attempts to install nonexistent `statusd` instead of `sb-statusd`. The leading `-` suppresses the error, so installation can report success while omitting the supervisor.
  - **Fix:** Install `sb-statusd` as `/usr/local/bin/sb-statusd`; do not suppress this failure.

- [x] **BUG-H02 — Destructive FIFO setup can delete arbitrary existing files** — `multicat.c:76-93`
  - **Issue:** Every generated path is unlinked before `mkfifo()`, including regular files or other unexpected filesystem objects.
  - **Fix:** `lstat()` each path, reject unexpected objects, and only remove an existing FIFO created for this instance.

- [ ] **BUG-H03 — Supervisor cleanup can terminate unrelated processes** — `start.sh:7`
  - **Issue:** `kill 0` sends a signal to the entire current process group, potentially killing the invoking shell and unrelated jobs.
  - **Fix:** Track child PIDs or use a dedicated process group and terminate only owned children.

- [ ] **BUG-H04 — Shared `/tmp` status directory permits instance collisions** — `start.sh:4,13-18`
  - **Issue:** Concurrent users or instances share `/tmp/.status.pipes`; one instance removes or recreates another instance’s FIFOs.
  - **Fix:** Use a per-user runtime directory or a unique `mktemp -d` directory with restrictive permissions.

- [ ] **BUG-H05 — Uptime reader can dereference invalid pointers on `/proc` failure** — `sb-uptime.c:65-69`
  - **Issue:** `fopen()` and `getdelim()` results are unchecked. A missing/unreadable `/proc/uptime` can lead to `getdelim(NULL, ...)` or `strtol(NULL, ...)`.
  - **Fix:** Check the stream, `getdelim()` return value, allocated line, and numeric conversion before use.

- [ ] **BUG-H06 — MPD retry paths leak connections** — `sb-mpd.c:80-121`
  - **Issue:** Several `goto main_loop` paths leave `conn` allocated. Repeated MPD failures accumulate connections and resources.
  - **Fix:** Centralize retry cleanup and always free the connection, status, song, and pending response before reconnecting.

- [ ] **BUG-H07 — MPD signal handler performs unsafe library and stdio operations** — `sb-mpd.c:27-31`
  - **Issue:** The SIGINT handler calls `fprintf`, MPD functions, `exit`, and cleanup while the main thread may be inside the same libraries. This can deadlock or corrupt state.
  - **Fix:** Set a `volatile sig_atomic_t` flag in the handler and perform cleanup from the main loop.

- [ ] **BUG-H08 — Uptime signal handlers call non-async-signal-safe functions** — `sb-uptime.c:30-44`
  - **Issue:** SIGUSR1/SIGUSR2 handlers call `printf()` and `fflush()` and mutate shared state during normal output.
  - **Fix:** Handlers should only set flags/counters; print and update the display in the main loop.

- [ ] **BUG-H09 — Volume monitor exits on transient or missing mixer conditions** — `sb-volmon.c:11-21,24-70,171-174`
  - **Issue:** Initial volume acquisition and event-time acquisition call `fatal()`. Missing ALSA hardware, a missing `Master` element, or a temporary control failure terminates the daemon instead of using the existing retry loop.
  - **Fix:** Return errors to `monitor()` and reopen/retry the mixer control.

- [ ] **BUG-H10 — Malformed UTF-8 can cause out-of-bounds reads** — `sb-mpd.c:34-50`
  - **Issue:** `utf8_truncate()` advances according to a leading byte without checking continuation bytes, remaining length, or NUL termination.
  - **Fix:** Validate each UTF-8 sequence before advancing; stop safely on malformed or truncated input.

## Medium priority

- [ ] **BUG-M01 — Partial FIFO initialization can close stdin during cleanup** — `multicat.c:19,26-31`
  - **Issue:** Global `pipe_fds` entries start at zero. If setup fails partway through, cleanup treats uninitialized entries as descriptor 0.
  - **Fix:** Initialize all entries to `-1` before setup.

- [ ] **BUG-M02 — FIFO data is not line-framed** — `multicat.c:110-117`
  - **Issue:** One `read()` replaces the cached value. Multiple lines can be merged, and long lines are split into unrelated status updates.
  - **Fix:** Maintain a receive buffer per FIFO and process complete newline-delimited records.

- [ ] **BUG-M03 — Closed producers leave stale status visible** — `multicat.c:118-147`
  - **Issue:** EOF causes a reopen but does not clear the previous cached value, so dead producers can display old data indefinitely.
  - **Fix:** Clear the cache on EOF or display an explicit unavailable marker.

- [ ] **BUG-M04 — CPU parser accepts partially initialized samples** — `sb-cpuload.c:19-23`
  - **Issue:** `read_cpu()` accepts four parsed fields, while later calculations read all eight fields.
  - **Fix:** Zero-initialize the structure and require all expected fields, or parse each field safely.

- [ ] **BUG-M05 — CPU counter reset/rollback creates false load spikes** — `sb-cpuload.c:62-67`
  - **Issue:** Unsigned subtraction wraps when `/proc/stat` counters decrease.
  - **Fix:** Detect decreases, discard the sample, and rebaseline.

- [ ] **BUG-M06 — Invalid CPU interval values are accepted** — `sb-cpuload.c:46-48`
  - **Issue:** `atof()` accepts malformed, negative, NaN, and infinite values; invalid values can reach an unsafe numeric conversion.
  - **Fix:** Parse with `strtod()`, validate `errno`, finiteness, and a positive range.

- [ ] **BUG-M07 — Memory calculation can unsigned-underflow** — `sb-memwatch.c:73-78`
  - **Issue:** Missing or inconsistent fields can make `mem_total - ...` wrap to a huge value.
  - **Fix:** Validate required fields and use checked arithmetic.

- [ ] **BUG-M08 — Memory parsing stops at unitless `/proc/meminfo` lines** — `sb-memwatch.c:54-69`
  - **Issue:** The loop requires exactly three tokens. Unitless entries terminate parsing, so required fields after one are ignored.
  - **Fix:** Parse one line at a time and accept both unit-bearing and unitless records.

- [ ] **BUG-M09 — MPD output exceeds `TRUNC_LEN`** — `sb-mpd.c:128-143`
  - **Issue:** Artist/title truncation ignores the `"[x] "` prefix, separator, and truncation marker. The final output can exceed 28 characters.
  - **Fix:** Reserve the complete formatting overhead before truncating fields.

- [ ] **BUG-M10 — Network counter read failure creates false bandwidth** — `sb-net.c:63-69,121-124`
  - **Issue:** Failed reads return zero; subtracting from a prior nonzero value wraps and reports a huge rate.
  - **Fix:** Return success separately from the counter and discard/rebaseline failed samples.

- [ ] **BUG-M11 — Network counter reset creates false bandwidth** — `sb-net.c:124`
  - **Issue:** Interface resets or counter rollback cause unsigned subtraction underflow.
  - **Fix:** Detect `curr_rx < prev_rx` and rebaseline.

- [ ] **BUG-M12 — Newly appearing interfaces are not discovered** — `sb-net.c:101-112`
  - **Issue:** Automatic discovery rescans only when all tracked interfaces are down. A new interface is missed while an old one remains up.
  - **Fix:** Periodically rescan and reconcile the complete interface set.

- [ ] **BUG-M13 — Network output separators are wrong when samples are skipped** — `sb-net.c:124-129`
  - **Issue:** Low-traffic interfaces are skipped, but separators use original indexes, producing trailing ` | ` or malformed output.
  - **Fix:** Track whether an entry has already been printed.

- [ ] **BUG-M14 — Oversized explicit interface name may be unterminated** — `sb-net.c:90-93`
  - **Issue:** `strncpy()` does not append NUL for a name at least 64 bytes long; later path operations can read past the buffer.
  - **Fix:** Explicitly terminate the buffer and reject oversized names.

- [ ] **BUG-M15 — Root-name input is not line-framed** — `sb-setroot.c:153-162`
  - **Issue:** Partial or multiple stdin lines are treated as complete values, and input over 255 bytes is split into separate root-name updates.
  - **Fix:** Buffer stdin and process complete newline-delimited records.

- [ ] **BUG-M16 — Empty root status is not restored after X reconnect** — `sb-setroot.c:124-130`
  - **Issue:** Reconnect logic reapplies cached data only when the string is nonempty. An intentionally empty status leaves the old X property visible.
  - **Fix:** Track a separate “cached value exists” flag.

- [ ] **BUG-M17 — Cached X property update errors are ignored** — `sb-setroot.c:124-130`
  - **Issue:** Reconnection marks the client connected even when cached property updates fail.
  - **Fix:** Check `set_root_name()` results and transition back to reconnecting on failure.

- [ ] **BUG-M18 — ALSA poll error events are ignored** — `sb-volmon.c:140-155`
  - **Issue:** `POLLERR`, `POLLHUP`, and `POLLNVAL` are not handled. A dead descriptor can cause a busy loop or prevent recovery.
  - **Fix:** Treat error events as control loss and restart the monitor.

- [ ] **BUG-M19 — ALSA monitor assumes one poll descriptor** — `sb-volmon.c:131-133`
  - **Issue:** Devices with multiple descriptors are rejected because the code allocates and polls only one.
  - **Fix:** Query the descriptor count, allocate the required array, and process all descriptors.

- [ ] **BUG-M20 — Supervisor does not reliably terminate grandchildren** — `sb-statusd:10-13`
  - **Issue:** `pkill -P $$` targets direct children only; worker descendants can survive and retain FIFO descriptors.
  - **Fix:** Supervise a dedicated process group or explicitly track and terminate the complete worker tree.

- [ ] **BUG-M21 — FIFO setup failures are ignored** — `sb-statusd:8,19-20`
  - **Issue:** Failed `mkdir`/`mkfifo` operations do not stop startup, so the daemon may run partially configured.
  - **Fix:** Enable failure checking and abort on setup errors.

- [ ] **BUG-M22 — Producer diagnostics are discarded** — `sb-statusd:33`
  - **Issue:** All producer stderr is redirected to `/dev/null`, hiding missing dependencies and runtime failures.
  - **Fix:** Preserve stderr or send it to a controlled log.

- [ ] **BUG-M23 — Main multicat/setroot pipeline is not supervised** — `sb-statusd:23,50`
  - **Issue:** If the pipeline exits, producers continue or block against a dead reader while the supervisor waits indefinitely.
  - **Fix:** Monitor the pipeline and restart or terminate the whole service when it exits.

- [ ] **BUG-M24 — Child processes are not restarted by `start.sh`** — `start.sh:20-27`
  - **Issue:** A failed producer leaves a permanently stale FIFO while the script continues waiting.
  - **Fix:** Add restart supervision or use the restart-capable supervisor script.

## Low priority

- [ ] **BUG-L01 — Unknown CPU options are silently ignored** — `sb-cpuload.c:46-49`
  - **Issue:** Invalid options do not produce usage text or a failure status.
  - **Fix:** Handle `getopt()`’s `?` result explicitly.

- [ ] **BUG-L02 — Automatic network mode silently accepts invalid arguments** — `sb-net.c:82-93`
  - **Issue:** Any command line other than exactly `-i NAME` falls back to automatic discovery.
  - **Fix:** Validate arguments and reject unknown forms.

- [ ] **BUG-L03 — Uptime reset value contradicts its behavior description** — `sb-uptime.c:33-35`
  - **Issue:** The reset handler says it zeros the clock but sets it to 60 seconds.
  - **Fix:** Set it to zero, or update the command semantics and comment consistently.

- [ ] **BUG-L04 — Uptime counter uses narrow signed arithmetic** — `sb-uptime.c:8,68,84`
  - **Issue:** Long uptimes or repeated hour additions can overflow `int`.
  - **Fix:** Use a checked unsigned 64-bit counter or `time_t`.

- [ ] **BUG-L05 — Timedate signal handlers perform unsafe work** — `sb-timedate.c:12-36`
  - **Issue:** Handlers call `time()`, `difftime()`, and `exit()` and modify ordinary globals.
  - **Fix:** Set signal flags/counters only, then process them in the main loop.

- [ ] **BUG-L06 — Timedate signal installation failures are ignored** — `sb-timedate.c:52-53`
  - **Issue:** The process can run without responding to offset signals.
  - **Fix:** Use checked `sigaction()` calls for all signals.

- [ ] **BUG-L07 — Timedate offset updates can race or overflow** — `sb-timedate.c:8-14`
  - **Issue:** Repeated signal delivery updates a non-atomic `int` without bounds.
  - **Fix:** Process signals synchronously or block them during updates and use checked wider arithmetic.

- [ ] **BUG-L08 — SIGTERM is not handled by status components** — `multicat.c:34-37`, `sb-memwatch.c:9-14`, `sb-setroot.c:17-20`
  - **Issue:** Normal supervisor termination bypasses component shutdown paths.
  - **Fix:** Handle SIGTERM with the same flag-based shutdown mechanism as SIGINT.

- [ ] **BUG-L09 — Fixed root/status paths are not validated before recursive deletion** — `sb-statusd:5,12`, `start.sh:4,13-14`
  - **Issue:** A malformed runtime-directory environment can redirect cleanup to an unintended location.
  - **Fix:** Validate path ownership, absoluteness, and expected suffix before deletion.

- [ ] **BUG-L10 — Explicit network target is not validated as an interface** — `sb-net.c:82-96`
  - **Issue:** `-i` accepts arbitrary path components, which can cause reads outside the intended interface entry under `/sys/class/net`.
  - **Fix:** Validate the name against the interface-name rules and confirm the path is an expected sysfs entry.

## Files reviewed without identified behavioral defects

- `LICENSE` — no executable behavior.
- `init.d/sb-statusd` — no additional defect beyond the installation/environment dependency documented above.
