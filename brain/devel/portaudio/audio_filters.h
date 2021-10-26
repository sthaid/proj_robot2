#ifndef __AUDIO_FILTERS_H__
#define __AUDIO_FILTERS_H__

// XXX notes on the cx arg

// Low pass filter

static inline double low_pass_filter(double v, double *cx, double k2)
{
    *cx = k2 * *cx + (1-k2) * v;
    return *cx;
}

static inline double low_pass_filter_ex(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        v = low_pass_filter(v, &cx[i], k2);
    }
    return v;
}

// High pass filter

static inline double high_pass_filter(double v, double *cx, double k2)
{
    *cx = *cx * k2 + (1-k2) * v;
    return v - *cx;
}

static inline double high_pass_filter_ex(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        v = high_pass_filter(v, &cx[i], k2);
    }
    return v;
}

// Band pass filter

static inline double band_pass_filter_ex(double v, double *cx, int lpf_k1, double lpf_k2, int hpf_k1, double hpf_k2)
{
    v = low_pass_filter_ex(v, &cx[0], lpf_k1, lpf_k2);
    v = high_pass_filter_ex(v, &cx[lpf_k1], hpf_k1, hpf_k2);
    return v;
}

#endif
