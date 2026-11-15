/*
 * tsv-buttons: make the ThinkSmart View mic-mute button and camera privacy
 * slider act as a power key, like the LineageOS 15.1 (downstream) build did.
 *
 * The mainline gpio-keys node reports both as switches (EV_SW):
 *   SW_MUTE_DEVICE        - latching: 1 = mic muted (in hardware), 0 = mic live
 *   SW_CAMERA_LENS_COVER  - level: 1 = closed, 0 = open
 * Android ignores those switches for screen power. This daemon watches the
 * "gpio-keys" input device and injects a KEY_POWER tap through uinput:
 *   - mute button: one tap per change (it latches: 1 = mic muted, 0 = mic live)
 *   - privacy slider: one tap per change (both directions)
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "tsv-buttons"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <log/log.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#define SOURCE_NAME "gpio-keys"
#define DEBOUNCE_MS 300

static int find_source(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return -1;
    struct dirent *entry;
    int found = -1;
    while ((entry = readdir(dir)) != NULL && found < 0) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        char path[64], name[128] = {0};
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 && strcmp(name, SOURCE_NAME) == 0) {
            found = fd;
            ALOGI("source: %s (%s)", path, name);
        } else {
            close(fd);
        }
    }
    closedir(dir);
    return found;
}

static int create_power_key(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x17ef;   /* Lenovo */
    setup.id.product = 0x0001;
    strncpy(setup.name, "tsv-power-buttons", UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 || ioctl(fd, UI_SET_KEYBIT, KEY_POWER) < 0 ||
        ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void emit(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) ALOGE("uinput write failed: %s", strerror(errno));
}

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void power_tap(int out, const char *why) {
    emit(out, EV_KEY, KEY_POWER, 1);
    emit(out, EV_SYN, SYN_REPORT, 0);
    emit(out, EV_KEY, KEY_POWER, 0);
    emit(out, EV_SYN, SYN_REPORT, 0);
    ALOGI("power tap (%s)", why);
}

int main(void) {
    int src = -1;
    for (int tries = 0; src < 0; tries++) {   /* gpio-keys can probe after we start */
        src = find_source();
        if (src < 0) {
            if (tries == 0) ALOGW("waiting for %s", SOURCE_NAME);
            sleep(1);
        }
    }
    int out = create_power_key();
    if (out < 0) {
        ALOGE("cannot create uinput device: %s", strerror(errno));
        return 1;
    }
    /* EPOLLWAKEUP keeps the system awake until the event has been handled. */
    int ep = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event want = {.events = EPOLLIN | EPOLLWAKEUP, .data.fd = src};
    if (ep < 0 || epoll_ctl(ep, EPOLL_CTL_ADD, src, &want) < 0) {
        ALOGE("epoll setup failed: %s", strerror(errno));
        return 1;
    }
    ALOGI("ready: mute button and privacy slider -> KEY_POWER");
    long long last = 0;
    for (;;) {
        struct epoll_event got;
        int n = epoll_wait(ep, &got, 1, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            ALOGE("epoll_wait: %s", strerror(errno));
            return 1;
        }
        struct input_event events[16];
        ssize_t len = read(src, events, sizeof(events));
        if (len < (ssize_t)sizeof(struct input_event)) continue;
        for (size_t i = 0; i < (size_t)len / sizeof(struct input_event); i++) {
            const struct input_event *ev = &events[i];
            if (ev->type != EV_SW) continue;
            const char *why = NULL;
            if (ev->code == SW_MUTE_DEVICE) why = ev->value ? "mic muted" : "mic unmuted";
            else if (ev->code == SW_CAMERA_LENS_COVER) why = ev->value ? "slider closed" : "slider opened";
            if (!why) continue;
            long long t = now_ms();
            if (t - last < DEBOUNCE_MS) continue;
            last = t;
            power_tap(out, why);
        }
    }
}
