#ifndef __FILTER_UTILS_H__
#define __FILTER_UTILS_H__

#if 0
static inline double low_pass_filter(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        cx[i] = k2 * cx[i] + (1 - k2) * (i == 0 ? v : cx[i-1]);
    }
    return cx[k1-1];
}
#endif

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

#endif
