#include <utils.h>
  
// -----------------  INIT  ---------------------------------------------

void misc_init(void)
{
    assert(PAGE_SIZE == 4096);
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

// -----------------  NETWORKING  ---------------------------------------

int getsockaddr(char * node, int port, struct sockaddr_in * ret_addr)
{
    struct addrinfo   hints;
    struct addrinfo * result;
    char              port_str[20];
    int               ret;

    sprintf(port_str, "%d", port);

    bzero(&hints, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags    = AI_NUMERICSERV;

    ret = getaddrinfo(node, port_str, &hints, &result);
    if (ret != 0) {
        //ERROR("failed to get address of %s, %s\n", node, gai_strerror(ret));
        return -1;
    }
    if (result->ai_addrlen != sizeof(*ret_addr)) {
        //ERROR("getaddrinfo result addrlen=%d, expected=%d\n",
        //    (int)result->ai_addrlen, (int)sizeof(*ret_addr));
        return -1;
    }

    *ret_addr = *(struct sockaddr_in*)result->ai_addr;
    freeaddrinfo(result);
    return 0;
}

char * sock_addr_to_str(char * s, int slen, struct sockaddr * addr)
{
    char addr_str[100];
    int port;

    if (addr->sa_family == AF_INET) {
        inet_ntop(AF_INET,
                  &((struct sockaddr_in*)addr)->sin_addr,
                  addr_str, sizeof(addr_str));
        port = ((struct sockaddr_in*)addr)->sin_port;
    } else if (addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6,
                  &((struct sockaddr_in6*)addr)->sin6_addr,
                 addr_str, sizeof(addr_str));
        port = ((struct sockaddr_in6*)addr)->sin6_port;
    } else {
        snprintf(s,slen,"Invalid AddrFamily %d", addr->sa_family);
        return s;
    }

    snprintf(s,slen,"%s:%d",addr_str,htons(port));
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

// -----------------  CRC32  --------------------------------------------

// https://web.mit.edu/freebsd/head/sys/libkern/crc32.c
static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };
/*
 * A function that calculates the CRC-32 based on the table above is
 * given below for documentation purposes. An equivalent implementation
 * of this function that's actually used in the kernel can be found
 * in sys/libkern.h, where it can be inlined.
 */

uint32_t crc32(const void *buf, size_t size)
{
    const uint8_t *p = buf;
    uint32_t crc;

    crc = ~0U;
    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc ^ ~0U;
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

void shuffle(void *array, int elem_size, int num_elem)
{
    void *tmp = malloc(elem_size);
    for (int i = 0; i < 2*num_elem; i++) {
        void *elem1 = array + ((random()%num_elem) * elem_size);
        void *elem2 = array + ((random()%num_elem) * elem_size);
        memcpy(tmp, elem1, elem_size);
        memcpy(elem1, elem2, elem_size);
        memcpy(elem2, tmp, elem_size);
    }
    free(tmp);
}

int get_filenames(char *dirname, char **names, int *max_names)
{
    DIR *dir;
    struct dirent *dirent;

    *max_names = 0;

    dir = opendir(dirname);
    if (dir == NULL) {
        ERROR("failed to open dir %s, %s\n", dirname, strerror(errno));
        return -1;
    }

    while (true) {
        dirent = readdir(dir);
        if (dirent == NULL) break;
        if ((dirent->d_type & DT_REG) == 0) continue;
        names[(*max_names)++] = strdup(dirent->d_name);
    }

    closedir(dir); 
    return 0;
}

int clip_int(int val, int min, int max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

double clip_double(double val, double min, double max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

bool strmatch(char *s, ...)
{
    va_list ap;
    bool match = false;
    char *s1;

    va_start(ap, s);
    while ((s1 = va_arg(ap, char*))) {
        if (strcmp(s, s1) == 0) {
            match = true;
            break;
        }
    }
    va_end(ap);

    return match;
}

