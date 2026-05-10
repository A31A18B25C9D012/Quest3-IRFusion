#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

#include <stdint.h>

typedef struct {
    int   mode;
    float ir_intensity;
    int   has_temperature;
    int   ir_on;
    int   crosshair_on;
    float crosshair_dist;
    int   settings_open;
    int   settings_cursor;
    int   ir_res_preset;
    int   hud_enabled;
} HUDState;

typedef struct {
    int   mode;
    float ir_intensity;
    int   has_temperature;
    int   ir_on;
    int   crosshair_on;
    float crosshair_dist;
    int   settings_open;
    int   settings_cursor;
    int   ir_res_preset;
    int   hud_enabled;
    int   width;
    int   height;
} HUDPushConstants;

void  hud_state_init(HUDState *h);
void  hud_state_set_mode(HUDState *h, int mode);
void  hud_state_set_ir_intensity(HUDState *h, float intensity);
void  hud_state_set_temperature(HUDState *h, float celsius);
void  hud_state_clear_temperature(HUDState *h);
void  hud_state_set_ir_on(HUDState *h, int on);
void  hud_state_set_crosshair(HUDState *h, int on, float dist_m);
void  hud_get_push_constants(const HUDState *h, int width, int height,
                              HUDPushConstants *out);

float hud_compute_ir_intensity(const uint8_t *frame_data, int n_bytes);

#endif