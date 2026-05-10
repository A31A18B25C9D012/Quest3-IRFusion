#ifndef KALMAN_H
#define KALMAN_H

typedef struct {
    float x[6];
    float P[6];
    float Q[6];
    float R[6];
} KalmanState6;

void kalman6_init(KalmanState6 *s, float q_diag, float r_diag);
void kalman6_predict(KalmanState6 *s);
void kalman6_update(KalmanState6 *s, float z[6]);
void kalman6_get_state(const KalmanState6 *s, float out[6]);

#endif