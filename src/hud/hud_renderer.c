#include "hud_renderer.h"
#include <stdint.h>
#include <string.h>

void hud_state_init(HUDState *h) {
    memset(h, 0, sizeof(*h));
    h->ir_on        = 1;
    h->hud_enabled  = 1;
    h->ir_res_preset = 1;
}

void hud_state_set_mode(HUDState *h, int mode) {
    if (mode >= 0 && mode <= 4) h->mode = mode;
}

void hud_state_set_ir_intensity(HUDState *h, float v) {
    h->ir_intensity = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

void hud_state_set_temperature(HUDState *h, float celsius) {
    (void)celsius;
    h->has_temperature = 1;
}

void hud_state_clear_temperature(HUDState *h) {
    h->has_temperature = 0;
}

void hud_state_set_ir_on(HUDState *h, int on) {
    h->ir_on = on ? 1 : 0;
}

void hud_state_set_crosshair(HUDState *h, int on, float dist_m) {
    h->crosshair_on   = on ? 1 : 0;
    h->crosshair_dist = dist_m < 0.0f ? 0.0f : dist_m;
}

void hud_get_push_constants(const HUDState *h, int width, int height,
                            HUDPushConstants *out) {
    out->mode            = h->mode;
    out->ir_intensity    = h->ir_intensity;
    out->has_temperature = h->has_temperature;
    out->ir_on           = h->ir_on;
    out->crosshair_on    = h->crosshair_on;
    out->crosshair_dist  = h->crosshair_dist;
    out->settings_open   = h->settings_open;
    out->settings_cursor = h->settings_cursor;
    out->ir_res_preset   = h->ir_res_preset;
    out->hud_enabled     = h->hud_enabled;
    out->width           = width;
    out->height          = height;
}

float hud_compute_ir_intensity(const uint8_t *frame_data, int n_bytes) {
    if (!frame_data || n_bytes <= 0) return 0.0f;
    unsigned long sum = 0;
    int i;
    for (i = 0; i < n_bytes; i++) sum += frame_data[i];
    return (float)(sum / (unsigned long)n_bytes) / 255.0f;
}