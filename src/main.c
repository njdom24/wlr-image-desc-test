#define _GNU_SOURCE

#include <wayland-client.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/memfd.h>
#include "cm.h"
#include "color-management-v1-client-protocol.h"

struct output_info {
    struct wl_output *output;
    int32_t x, y, width, height;
    int32_t refresh;
    char con_name[256];

    cm_state cm;
};

static struct output_info outputs[16];
static int output_count = 0;
static struct wp_color_manager_v1 *color_manager = NULL;

// --- Output ---

static void output_name(void *data, struct wl_output *wl_output, const char *name) {
    struct output_info *info = data;
    strncpy(info->con_name, name, 255);
}

static void output_description(void *data, struct wl_output *wl_output,
    const char *description) {}

static void output_geometry(void *data, struct wl_output *wl_output,
    int32_t x, int32_t y, int32_t w, int32_t h,
    int32_t subpixel, const char *make, const char *model, int32_t transform)
{
    struct output_info *info = data;
    info->x = x; info->y = y;
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        struct output_info *info = data;
        info->width = width;
        info->height = height;
        info->refresh = refresh;
    }
}

static void output_done(void *data, struct wl_output *wl_output) {}
static void output_scale(void *data, struct wl_output *wl_output, int32_t factor) {}

static const struct wl_output_listener output_listener = {
    output_geometry, output_mode, output_done, output_scale, output_name, output_description
};

// Hook up to display events
static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output_info *info = &outputs[output_count++];
        if (version < 2) {
            fprintf(stderr, "Requires %s protocol version 2. Provided %d.\n", interface, version);
            return;
        }
        if (version < 4) {
            fprintf(stderr, "Connector name requires %s protocol version 4. Provided %d.\n", interface, version);
            info->output = wl_registry_bind(registry, name, &wl_output_interface, 2);
        }
        else {
            info->output = wl_registry_bind(registry, name, &wl_output_interface, 4);
        }
        
        wl_output_add_listener(info->output, &output_listener, info);
    }
    else if (strcmp(interface, wp_color_manager_v1_interface.name) == 0) {
        color_manager = wl_registry_bind(registry, name, &wp_color_manager_v1_interface, 1); // version 1 is fine since we only care about PQ EOTF or not
        wp_color_manager_v1_add_listener(color_manager, &color_manager_listener, NULL);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

// Intro to Wayland from https://drewdevault.com/blog/Introduction-to-Wayland/
int main(void) {
    // Establish a connection to the Wayland server
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Cannot connect to Wayland\n");
        return 1;
    }

    // Enumerates the globals available on the server
    struct wl_registry *registry = wl_display_get_registry(display);

    // Listen to events emitted by the registry
    struct wl_registry_listener registry_listener = {
        .global        = registry_global,
        .global_remove = registry_global_remove,
    };
    wl_registry_add_listener(registry, &registry_listener, NULL); // void *data unused

    wl_display_roundtrip(display);   // collect globals
    wl_display_roundtrip(display);  // collect output events (may call global_remove when the server destroys globals)

    printf("Found %d display(s):\n", output_count);
    for (int i = 0; i < output_count; i++) {
        struct output_info *o = &outputs[i];
        printf("  Display %d (%s): @ %dx%d+%d+%d, %.2f Hz\n",
               i, o->con_name, o->width, o->height,
               o->x, o->y, o->refresh / 1000.0);
    }
    printf("------------------------------------------------------------------------------\n");

    if (!color_manager) {
        fprintf(stderr, "Compositor does not support wp_color_management_v1\n");
        wl_display_disconnect(display);
        return 1;
    }

    for (int i = 0; i < output_count; i++) {
        struct output_info *o = &outputs[i];

        o->cm.output = o->output;
        o->cm.output_name = o->con_name;

        fprintf(stderr, "output=%p\n", (void*)o);
        cm_init_output(color_manager, &o->cm);
        fprintf(stderr, "%s cm_output=%p\n", o->con_name, (void*)o->cm.cm_output);
        wl_display_roundtrip(display);
        wl_display_roundtrip(display);
    }

    while (wl_display_dispatch(display) != -1) {
        // keeps processing events until the connection drops or Ctrl+C
    }

    wl_display_disconnect(display);
    return 0;
}