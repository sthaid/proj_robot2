#include <filter_utils.h>

double low_pass_filter(double v, double *cx)
{
    *cx = 0.95 * *cx + 0.05 * v;
    return *cx;
}

double high_pass_filter(double v, double *cx)
{
    *cx = *cx * .95 + .05 * v;
    return v - *cx;
}

