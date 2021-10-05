#include <utils.h>
  
// -----------------  INIT  ---------------------------------------------

void misc_init(void)
{
    // nothing needed
}

// -----------------  TIME  ---------------------------------------------

uint64_t microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

char *time2str(time_t t, char *s)
{
    struct tm result;

    localtime_r(&t, &result);
    sprintf(s, "%4d/%02d/%02d %02d:%02d:%02d",
            result.tm_year+1900, result.tm_mon+1, result.tm_mday,
            result.tm_hour, result.tm_min, result.tm_sec);

    return s;
}

// -----------------  WAVELEN TO RGB  -----------------------------------

// ported from http://www.noah.org/wiki/Wavelength_to_RGB_in_Python
unsigned int wavelen_to_rgb(double wavelength)
{
    double attenuation;
    double gamma = 0.8;
    double R,G,B;

    if (wavelength >= 380 && wavelength <= 440) {
        double attenuation = 0.3 + 0.7 * (wavelength - 380) / (440 - 380);
        R = pow((-(wavelength - 440) / (440 - 380)) * attenuation, gamma);
        G = 0.0;
        B = pow(1.0 * attenuation, gamma);
    } else if (wavelength >= 440 && wavelength <= 490) {
        R = 0.0;
        G = pow((wavelength - 440) / (490 - 440), gamma);
        B = 1.0;
    } else if (wavelength >= 490 && wavelength <= 510) {
        R = 0.0;
        G = 1.0;
        B = pow(-(wavelength - 510) / (510 - 490), gamma);
    } else if (wavelength >= 510 && wavelength <= 580) {
        R = pow((wavelength - 510) / (580 - 510), gamma);
        G = 1.0;
        B = 0.0;
    } else if (wavelength >= 580 && wavelength <= 645) {
        R = 1.0;
        G = pow(-(wavelength - 645) / (645 - 580), gamma);
        B = 0.0;
    } else if (wavelength >= 645 && wavelength <= 750) {
        attenuation = 0.3 + 0.7 * (750 - wavelength) / (750 - 645);
        R = pow(1.0 * attenuation, gamma);
        G = 0.0;
        B = 0.0;
    } else {
        R = 0.0;
        G = 0.0;
        B = 0.0;
    }

    if (R < 0) R = 0; else if (R > 1) R = 1;
    if (G < 0) G = 0; else if (G > 1) G = 1;
    if (B < 0) B = 0; else if (B > 1) B = 1;

    return ((int)nearbyint(R*255) <<  0) |
           ((int)nearbyint(G*255) <<  8) |
           ((int)nearbyint(B*255) << 16);
}

// -----------------  RUN PROGRAM USING FORK AND EXEC  ------------------

void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...)
{
    char *args[100];
    int argc=0;
    int pipe_to_child[2], pipe_from_child[2];
    pid_t pid;
    va_list ap;
    sigset_t set;

    // block SIGPIPE
    sigemptyset(&set);
    sigaddset(&set,SIGPIPE);
    sigprocmask(SIG_BLOCK, &set, NULL);

    // construct args array
    args[argc++] = prog;
    va_start(ap, prog);
    while (true) {
        args[argc] = va_arg(ap, char*);
        if (args[argc] == NULL) break;
        argc++;
    }
    va_end(ap);

    // create pipes for prog input and output
    // - pipefd[0] is read end, pipefd[1] is write end
    pipe(pipe_to_child);
    pipe(pipe_from_child);

    // fork
    pid = fork();
    if (pid == -1) {
        printf("ERROR: fork failed, %s\n", strerror(errno));
        exit(1);
    }

    // if pid == 0, child is running, else parent
    if (pid == 0) {
        // child ..
        // close unused ends of the pipes
        close(pipe_to_child[1]);
        close(pipe_from_child[0]);

        // attach the 2 pipes to stdin and stdout for the child
        dup2(pipe_to_child[0], 0);
        dup2(pipe_from_child[1], 1);

        // execute the program
        execvp(prog, args);
        printf("ERROR: execvp %s, %s\n", prog, strerror(errno));
        exit(1);        
    } else {
        // parent ... 
        // close unused ends of the pipes
        close(pipe_to_child[0]);
        close(pipe_from_child[1]);

        // return values to caller
        *fd_to_prog = pipe_to_child[1];;
        *fd_from_prog = pipe_from_child[0];
        *prog_pid = pid;
    }
}

// -----------------  POLYNOMIAL FITTING  -------------------------------

// ported from:  https://www.bragitoff.com/2018/06/polynomial-fitting-c-program/

static void gaussEliminationLS(int m, int n, double a[m][n], double x[n - 1]);
static void printMatrix(int m, int n, double matrix[m][n])  __attribute__((unused));

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

// -----------------  GENERAL UTILS  ------------------------------------

double normalize_angle(double angle)
{
    if (angle < 0) {
        while (angle < 0) angle += 360;
    } else if (angle >= 360) {
        while (angle >= 360) angle -= 360;
    }
    return angle;
}

double max_doubles(double *x, int n, int *max_idx_arg)
{
    double max = x[0];
    int    max_idx = 0;

    for (int i = 1; i < n; i++) {
        if (x[i] > max) {
            max = x[i];
            max_idx = i;
        }
    }

    if (max_idx_arg) {
        *max_idx_arg = max_idx;
    }

    return max;
}

double min_doubles(double *x, int n, int *min_idx_arg)
{
    double min = x[0];
    int    min_idx = 0;

    for (int i = 1; i < n; i++) {
        if (x[i] < min) {
            min = x[i];
            min_idx = i;
        }
    }

    if (min_idx_arg) {
        *min_idx_arg = min_idx;
    }

    return min;
}

char *stars(double v, double max_v, int max_stars, char *s)
{
    int n;

    if (v < 0) v = 0;
    if (v > max_v) v = max_v;

    n = nearbyint(v / max_v * max_stars);
    memset(s, '*', n);
    s[n] = '\0';

    return s;
}
