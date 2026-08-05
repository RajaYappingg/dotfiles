#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>
#include <wayland-client.h>
#include "ext-idle-notify-v1-client-protocol.h"

#define MAX_JS 16
#define AXIS_DEADZONE 4000

static struct wl_display *display = NULL;
static struct wl_registry *registry = NULL;
static struct wl_seat *seat = NULL;
static struct ext_idle_notifier_v1 *idle_notifier = NULL;
static struct ext_idle_notification_v1 *idle_notification = NULL;

static uint32_t timeout_ms = 120000; // 2 minutes default (120 seconds)

static volatile time_t last_gamepad_activity = 0;
static volatile int is_idled = 0;
static volatile int lockscreen_launched = 0;
static volatile int daemon_running = 1;

static void *gamepad_monitor_thread(void *arg) {
    (void)arg;
    int js_fds[MAX_JS];
    char js_names[MAX_JS][128];

    for (int i = 0; i < MAX_JS; i++) {
        js_fds[i] = -1;
        js_names[i][0] = '\0';
    }

    while (daemon_running) {
        // Scan for joystick devices
        DIR *dir = opendir("/dev/input");
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                if (strncmp(ent->d_name, "js", 2) == 0 && ent->d_name[2] >= '0' && ent->d_name[2] <= '9') {
                    int num = atoi(ent->d_name + 2);
                    if (num >= 0 && num < MAX_JS && js_fds[num] < 0) {
                        char path[280];
                        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
                        int fd = open(path, O_RDONLY | O_NONBLOCK);
                        if (fd >= 0) {
                            js_fds[num] = fd;
                            char name[128] = "Gamepad";
                            if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0) {
                                snprintf(name, sizeof(name), "Gamepad (js%d)", num);
                            }
                            snprintf(js_names[num], sizeof(js_names[num]), "%s", name);
                            printf("[idle-daemon] Opened gamepad device %s (%s, fd=%d)\n", path, js_names[num], fd);
                            fflush(stdout);
                        }
                    }
                }
            }
            closedir(dir);
        }

        // Poll open joystick FDs
        struct pollfd pfds[MAX_JS];
        int nfds = 0;
        int map_num[MAX_JS];

        for (int i = 0; i < MAX_JS; i++) {
            if (js_fds[i] >= 0) {
                pfds[nfds].fd = js_fds[i];
                pfds[nfds].events = POLLIN;
                map_num[nfds] = i;
                nfds++;
            }
        }

        if (nfds > 0) {
            int ret = poll(pfds, nfds, 1000);
            if (ret > 0) {
                for (int i = 0; i < nfds; i++) {
                    int num = map_num[i];
                    int disconnected = 0;

                    if (pfds[i].revents & POLLIN) {
                        struct js_event e;
                        while (1) {
                            ssize_t bytes = read(pfds[i].fd, &e, sizeof(e));
                            if (bytes == sizeof(e)) {
                                if (!(e.type & JS_EVENT_INIT)) {
                                    uint8_t type = e.type & ~JS_EVENT_INIT;
                                    if (type == JS_EVENT_BUTTON) {
                                        last_gamepad_activity = time(NULL);
                                    } else if (type == JS_EVENT_AXIS) {
                                        if (abs(e.value) > AXIS_DEADZONE) {
                                            last_gamepad_activity = time(NULL);
                                        }
                                    }
                                }
                            } else if (bytes < 0) {
                                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                    disconnected = 1;
                                }
                                break;
                            } else if (bytes == 0) {
                                disconnected = 1;
                                break;
                            }
                        }
                    }

                    if (disconnected || (pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL))) {
                        printf("[idle-daemon] Closed gamepad js%d (%s)\n", num, js_names[num]);
                        fflush(stdout);
                        close(js_fds[num]);
                        js_fds[num] = -1;
                        js_names[num][0] = '\0';
                    }
                }
            }
        } else {
            sleep(2);
        }
    }

    for (int i = 0; i < MAX_JS; i++) {
        if (js_fds[i] >= 0) close(js_fds[i]);
    }
    return NULL;
}

static void handle_idled(void *data, struct ext_idle_notification_v1 *notification) {
    (void)data;
    (void)notification;
    is_idled = 1;
    lockscreen_launched = 0;
    printf("[idle-daemon] Wayland seat IDLE timeout reached (%u ms)\n", timeout_ms);
    fflush(stdout);
}

static void handle_resumed(void *data, struct ext_idle_notification_v1 *notification) {
    (void)data;
    (void)notification;
    is_idled = 0;
    lockscreen_launched = 0;
    printf("[idle-daemon] Seat user activity detected -> RESUMED\n");
    fflush(stdout);
}

static const struct ext_idle_notification_v1_listener idle_listener = {
    .idled = handle_idled,
    .resumed = handle_resumed,
};

static void registry_handle_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
    } else if (strcmp(interface, ext_idle_notifier_v1_interface.name) == 0) {
        idle_notifier = wl_registry_bind(reg, name, &ext_idle_notifier_v1_interface, 1);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *reg, uint32_t name) {
    (void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

int main(int argc, char **argv) {
    if (argc > 1) {
        int t = atoi(argv[1]);
        if (t > 0) timeout_ms = t * 1000;
    }

    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "[idle-daemon] Failed to connect to Wayland display\n");
        return 1;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!seat || !idle_notifier) {
        fprintf(stderr, "[idle-daemon] Wayland seat or idle notifier not available\n");
        wl_display_disconnect(display);
        return 1;
    }

    idle_notification = ext_idle_notifier_v1_get_idle_notification(idle_notifier, timeout_ms, seat);
    ext_idle_notification_v1_add_listener(idle_notification, &idle_listener, NULL);
    wl_display_flush(display);

    printf("[idle-daemon] Started successfully with %u ms timeout (Silent Gamepad Detection Enabled)\n", timeout_ms);
    fflush(stdout);

    pthread_t gp_thread;
    if (pthread_create(&gp_thread, NULL, gamepad_monitor_thread, NULL) != 0) {
        fprintf(stderr, "[idle-daemon] Failed to create gamepad monitor thread\n");
    }

    int wl_fd = wl_display_get_fd(display);

    while (daemon_running) {
        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) == -1) {
                daemon_running = 0;
                break;
            }
        }
        if (!daemon_running) break;

        wl_display_flush(display);

        struct pollfd pfd = { .fd = wl_fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 1000);

        if (ret < 0) {
            wl_display_cancel_read(display);
            if (errno == EINTR) continue;
            break;
        }

        if (pfd.revents & POLLIN) {
            if (wl_display_read_events(display) == -1) {
                break;
            }
            if (wl_display_dispatch_pending(display) == -1) {
                break;
            }
        } else {
            wl_display_cancel_read(display);
        }

        if (is_idled && !lockscreen_launched) {
            time_t now = time(NULL);
            uint32_t timeout_sec = timeout_ms / 1000;
            long sec_since_gamepad = (last_gamepad_activity > 0) ? (long)(now - last_gamepad_activity) : 999999;

            // Check if media (video/audio) is currently playing via playerctl
            int is_playing = (system("playerctl status 2>/dev/null | grep -qi Playing") == 0);

            if (is_playing) {
                // Media is playing, suppress lockscreen
            } else if (sec_since_gamepad < (long)timeout_sec) {
                // Gamepad active within idle window, suppress lockscreen
            } else {
                printf("[idle-daemon] IDLE timeout (%u ms) reached -> launching lockscreen\n", timeout_ms);
                fflush(stdout);
                system("hyprctl dispatch 'hl.dsp.exec_cmd(\"/home/zaki/.local/bin/lockscreen\")'");
                lockscreen_launched = 1;
            }
        }
    }

    daemon_running = 0;
    pthread_join(gp_thread, NULL);

    if (idle_notification) ext_idle_notification_v1_destroy(idle_notification);
    if (idle_notifier) ext_idle_notifier_v1_destroy(idle_notifier);
    if (seat) wl_seat_destroy(seat);
    if (registry) wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return 0;
}
