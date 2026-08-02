#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "ext-idle-notify-v1-client-protocol.h"

static struct wl_display *display = NULL;
static struct wl_registry *registry = NULL;
static struct wl_seat *seat = NULL;
static struct ext_idle_notifier_v1 *idle_notifier = NULL;
static struct ext_idle_notification_v1 *idle_notification = NULL;
static uint32_t timeout_ms = 120000; // 2 minutes default (120 seconds)

static void handle_idled(void *data, struct ext_idle_notification_v1 *notification) {
    (void)data;
    (void)notification;
    printf("[idle-daemon] IDLE timeout (%u ms) reached -> launching lockscreen via hyprctl\n", timeout_ms);
    fflush(stdout);
    system("hyprctl dispatch 'hl.dsp.exec_cmd(\"/home/zaki/.local/bin/lockscreen\")'");
}

static void handle_resumed(void *data, struct ext_idle_notification_v1 *notification) {
    (void)data;
    (void)notification;
    printf("[idle-daemon] User activity detected -> RESUMED\n");
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

    printf("[idle-daemon] Started successfully with %u ms timeout\n", timeout_ms);
    fflush(stdout);

    while (wl_display_dispatch(display) != -1) {
    }

    if (idle_notification) ext_idle_notification_v1_destroy(idle_notification);
    if (idle_notifier) ext_idle_notifier_v1_destroy(idle_notifier);
    if (seat) wl_seat_destroy(seat);
    if (registry) wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return 0;
}
