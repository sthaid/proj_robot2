#ifndef __POLY_FIT_H__
#define __POLY_FIT_H__

// notes:
// - the number of coefficients is 1 greater than the degree_of_poly

void poly_fit(int max_data, double *x_data, double *y_data, int degree_of_poly, double *coefficients);

#endif
