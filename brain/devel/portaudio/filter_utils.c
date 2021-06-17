#if 0
#include <filter_utils.h>

double low_pass_filter(double v, double *cx)
{
    *cx = 0.95 * *cx + 0.05 * v;
    return *cx;
}

double low_pass_filter_new(double v, double *cx, int k)
{
    for (int i = 0; i < k; i++) {
        cx[i] = 0.95 * cx[i] + 0.05 * (i == 0 ? v : cx[i-1]);
    }
    return cx[k-1];

    //cx[0] = 0.95 * cx[0] + 0.05 * v;
    //cx[1] = 0.95 * cx[1] + 0.05 * cx[0];
    //return cx[1];
}

double high_pass_filter(double v, double *cx)
{
    *cx = *cx * .95 + .05 * v;
    return v - *cx;
}
#endif
