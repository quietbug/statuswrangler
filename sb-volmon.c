#define _STRUCT_TIMESPEC 1
#include <time.h> // IWYU pragma: keep
#include <alsa/asoundlib.h>
#include <alloca.h>
#include <math.h>

#define MIXER_NAME "default"
#define SELEM_NAME "Master"
#define CHANNEL SND_MIXER_SCHN_FRONT_LEFT

static void fatal(const char *msg, int err, snd_mixer_t *mixer)
{
    if (err < 0)
        fprintf(stderr, "%s: %s\n", msg, snd_strerror(err));
    else
        fprintf(stderr, "%s\n", msg);

    if (mixer)
        snd_mixer_close(mixer);

    exit(EXIT_FAILURE);
}

static long get_volume_once()
{
    snd_mixer_t *mixer = NULL;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;
    long vol = 0;
    long min = 0, max = 100; // will be replaced by actual range 
    int err;

    if ((err = snd_mixer_open(&mixer, 0)) < 0)
        fatal("Mixer open error", err, mixer);
    if ((err = snd_mixer_attach(mixer, MIXER_NAME)) < 0)
        fatal("Mixer attach error", err, mixer);
    if ((err = snd_mixer_selem_register(mixer, NULL, NULL)) < 0)
        fatal("Mixer register error", err, mixer);
    if ((err = snd_mixer_load(mixer)) < 0)
        fatal("Mixer load error", err, mixer);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, SELEM_NAME);

    elem = snd_mixer_find_selem(mixer, sid);
    if (!elem)
        fatal("Cannot find simple element (check scontrols / selem name)", 0, mixer);

    if ((err = snd_mixer_selem_get_playback_volume(elem, CHANNEL, &vol)) < 0) {
        snd_mixer_close(mixer);
        fatal("Failed to get playback volume", err, NULL);
    }

    // get actual range and normalize to 0..100 
    if ((err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max)) < 0) {
        // if range query fails, fall back
        min = 0;
        max = 65536;
    }

    snd_mixer_close(mixer);

    if (max <= min)
        return 0;

    long percent = lround(((double)(vol - min) / (double)(max - min)) * 100.0);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}

static void print_volume()
{
    long percent = get_volume_once();
    printf("%ld%%\n", percent);
    fflush(stdout);
}

static int open_ctl(const char *name, snd_ctl_t **ctlp)
{
    snd_ctl_t *ctl;
    int err;

    if ((err = snd_ctl_open(&ctl, name, SND_CTL_READONLY)) < 0)
        return err;
    if ((err = snd_ctl_subscribe_events(ctl, 1)) < 0) {
        snd_ctl_close(ctl);
        return err;
    }
    *ctlp = ctl;
    return 0;
}

static int process_event(snd_ctl_t *ctl)
{
    snd_ctl_event_t *event;
    snd_ctl_event_alloca(&event);

    int err = snd_ctl_read(ctl, event);
    if (err < 0)
        return err;

    if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
        return 0;

    unsigned int mask = snd_ctl_event_elem_get_mask(event);
    if (!(mask & SND_CTL_EVENT_MASK_VALUE))
        return 0;

    print_volume();
    return 0;
}

static int monitor(const char *name)
{
    snd_ctl_t *ctl = NULL;
    struct pollfd pfd;
    int err;
    int backoff = 1;

    while (1) {
        if ((err = open_ctl(name, &ctl)) < 0) {
            fprintf(stderr, "Cannot open control '%s': %s\n", name, snd_strerror(err));
            sleep(backoff);
            if (backoff < 10) backoff++;
            continue;
        }

        backoff = 1; // reset backoff once successful 
        // populate pfd with the proper poll descriptor(s) 
        if (snd_ctl_poll_descriptors(ctl, &pfd, 1) != 1) {
            snd_ctl_close(ctl);
            ctl = NULL;
            sleep(backoff);
            if (backoff < 10) backoff++;
            continue;
        }

        for (;;) {
            err = poll(&pfd, 1, -1);
            if (err < 0) {
                perror("poll");
                break;
            }

            unsigned short revents = 0;
            snd_ctl_poll_descriptors_revents(ctl, &pfd, 1, &revents);
            if (revents & POLLIN) {
                err = process_event(ctl);
                if (err < 0) {
                    fprintf(stderr, "ALSA read error: %s\n", snd_strerror(err));
                    break;
                }
            }
        }

        if (ctl) {
            snd_ctl_close(ctl);
            ctl = NULL;
        }

        fprintf(stderr, "Control lost, retrying in %d seconds...\n", backoff);
        sleep(backoff);
        if (backoff < 10) backoff++;
    }

    return 0;
}

int main(void)
{
    print_volume();
    return monitor("default");
}
