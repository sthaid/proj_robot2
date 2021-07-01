// Polynomial Fitting
// 
// ported from:  https://www.bragitoff.com/2018/06/polynomial-fitting-c-program/

#include <stdio.h>
#include <math.h>

#include <poly_fit.h>

static void gaussEliminationLS(int m, int n, double a[m][n], double x[n - 1]);
static void printMatrix(int m, int n, double matrix[m][n])  __attribute__((unused));

// -----------------  POLY FIT  ----------------------------------------------

void poly_fit(int max_data, double *x_data, double *y_data, int degree_of_poly, double *coefficients)
{
    int     N = max_data;
    double *x = x_data;
    double *y = y_data;
    int     n = degree_of_poly;
    int     i, j;

    // an array of size 2*n+1 for storing N, Sig xi, Sig xi^2, ...., etc. 
    // which are the independent components of the normal matrix
    double X[2 * n + 1];
    for (i = 0; i <= 2 * n; i++) {
	X[i] = 0;
	for (j = 0; j < N; j++) {
	    X[i] = X[i] + pow(x[j], i);
	}
    }

    // the normal augmented matrix
    double B[n + 1][n + 2];

    // rhs
    double Y[n + 1];
    for (i = 0; i <= n; i++) {
	Y[i] = 0;
	for (j = 0; j < N; j++) {
	    Y[i] = Y[i] + pow(x[j], i) * y[j];
	}
    }
    for (i = 0; i <= n; i++) {
	for (j = 0; j <= n; j++) {
	    B[i][j] = X[i + j];
	}
    }
    for (i = 0; i <= n; i++) {
	B[i][n + 1] = Y[i];
    }

    //printMatrix(n + 1, n + 2, B);
    gaussEliminationLS(n + 1, n + 2, B, coefficients);
}

// -----------------  MATRIX SUPPORT  ----------------------------------------

// Function that performs Gauss-Elimination and returns the Upper triangular 
// matrix and solution of equations:
// 
// There are two options to do this in C.
// 1. Pass the augmented matrix (a) as the parameter, and calculate and store the 
//    upperTriangular(Gauss-Eliminated Matrix) in it.
// 2. Use malloc and make the function of pointer type and return the pointer.
//
// This program uses the first option.
static void gaussEliminationLS(int m, int n, double a[m][n], double x[n - 1])
{
    int i, j, k;
    for (i = 0; i < m - 1; i++) {
	//Partial Pivoting
	for (k = i + 1; k < m; k++) {
	    //If diagonal element(absolute vallue) is smaller than any of the terms below it
	    if (fabs(a[i][i]) < fabs(a[k][i])) {
		//Swap the rows
		for (j = 0; j < n; j++) {
		    double temp;
		    temp = a[i][j];
		    a[i][j] = a[k][j];
		    a[k][j] = temp;
		}
	    }
	}
	//Begin Gauss Elimination
	for (k = i + 1; k < m; k++) {
	    double term = a[k][i] / a[i][i];
	    for (j = 0; j < n; j++) {
		a[k][j] = a[k][j] - term * a[i][j];
	    }
	}

    }
    //Begin Back-substitution
    for (i = m - 1; i >= 0; i--) {
	x[i] = a[i][n - 1];
	for (j = i + 1; j < n - 1; j++) {
	    x[i] = x[i] - a[i][j] * x[j];
	}
	x[i] = x[i] / a[i][i];
    }
}

// Function that prints the elements of a matrix row-wise
// Parameters: rows(m),columns(n),matrix[m][n] 
static void printMatrix(int m, int n, double matrix[m][n])
{
    int i, j;
    for (i = 0; i < m; i++) {
	for (j = 0; j < n; j++) {
	    printf("%lf\t", matrix[i][j]);
	}
	printf("\n");
    }
}

// -----------------  UNIT TEST  ------------------------------------------

#ifdef TEST
// gcc -Wall -g -O2 -DTEST -I. -o poly_fit poly_fit.c -lm

int main(int argc, char **argv)
{
    #define MAX_DATA    10
    #define POLY_DEGREE 2

    double x[MAX_DATA], y[MAX_DATA], coeffs[POLY_DEGREE+1];
    int i;

    for (i = 0; i < MAX_DATA; i++) {
        x[i] = i;
        y[i] = 5.*x[i]*x[i] + 10.*x[i] + 20.;
    }
    poly_fit(MAX_DATA, x, y, POLY_DEGREE, coeffs);
    printf("y = %0.3f * x^2 + %0.3f * x + %0.3f\n", coeffs[2], coeffs[1], coeffs[0]);

    return 0;
}
#endif
