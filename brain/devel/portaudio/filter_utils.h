#ifndef __FILTER_UTILS_H__
#define __FILTER_UTILS_H__

static inline double low_pass_filter(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        cx[i] = k2 * cx[i] + (1 - k2) * (i == 0 ? v : cx[i-1]);
    }
    return cx[k1-1];
}

#endif
