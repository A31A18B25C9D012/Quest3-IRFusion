#include "kalman.h"
#include <string.h>

void kalman6_init(KalmanState6 *s, float q_diag, float r_diag) {
    int i;
    for (i = 0; i < 6; i++) {
        s->x[i] = 0.0f;
        s->P[i] = 1.0f;
        s->Q[i] = q_diag;
        s->R[i] = r_diag;
    }
}

void kalman6_predict(KalmanState6 *s) {
    int i;
    for (i = 0; i < 6; i++)
        s->P[i] += s->Q[i];
}

void kalman6_update(KalmanState6 *s, float z[6]) {
    int i;
    for (i = 0; i < 6; i++) {
        float K = s->P[i] / (s->P[i] + s->R[i]);
        s->x[i] = s->x[i] + K * (z[i] - s->x[i]);
        s->P[i] = (1.0f - K) * s->P[i];
    }
}

void kalman6_get_state(const KalmanState6 *s, float out[6]) {
    int i;
    for (i = 0; i < 6; i++)
        out[i] = s->x[i];
}