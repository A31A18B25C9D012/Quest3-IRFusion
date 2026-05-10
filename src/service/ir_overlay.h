#ifndef IR_OVERLAY_H
#define IR_OVERLAY_H

typedef enum {
    IR_MODE_PASSTHROUGH = 0,
    IR_MODE_IR_OVERLAY  = 1,
    IR_MODE_IR_ONLY     = 2,
    IR_MODE_EDGE        = 3,
    IR_MODE_DEPTH_IR    = 4
} IROverlayMode;

typedef struct {
    float alpha;
    float beta;
    float gamma;
    float depth_k;
    IROverlayMode mode;
} IROverlayState;

void ir_overlay_init(IROverlayState *s);
void ir_overlay_set_mode(IROverlayState *s, IROverlayMode mode);
void ir_overlay_get_coefficients(const IROverlayState *s, float *a, float *b, float *g, float *dk);

#endif