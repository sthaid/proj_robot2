#include <utils.h>

// The deviation of the max cross correlation from center is
// determined by the speed of sound, the distance between the mics, and
// the sample_rate. Using:
//
// Assuming mic pairs used are 0,2 an 
// - distance between mics = 0.080 m
// - speed of sond         = 343 m/s
// - sample rate           = 48000 samples per second
// time = .080 / 343 = .00023
// samples = .00023 * 48000 = 11.2

// mic numbers, from info found in picture here:
// https://wiki.seeedstudio.com/ReSpeaker_4_Mic_Array_for_Raspberry_Pi/
// ---------------------
// |1                 2|
// |                   |
// |     RESPEAKER     |
// |                   |
// |0                 3|
// ---------------------

//
// defines
//

#define SAMPLE_RATE         48000
#define MAX_CHAN            4

// mic numbers used to compute cross correlations
#if 0
#define CCA_MICX     0
#define CCA_MICY     1
#define CCB_MICX     0
#define CCB_MICY     3
#define ANGLE_OFFSET 0
#else
#define CCA_MICX     0
#define CCA_MICY     2
#define CCB_MICX     1
#define CCB_MICY     3
#define ANGLE_OFFSET 45
#endif

// N is half the number of cross correlations 
#define N  15

// duration of a sound block analysis
#define DURATION_MS 600

// general purpose
#define MAX_FRAME               (MS_TO_FRAMES(DURATION_MS))
#define MS_TO_FRAMES(ms)        (SAMPLE_RATE * (ms) / 1000)
#define FRAMES_TO_MS(frames)    (1000 * (frames) / SAMPLE_RATE)
#define FRAME_CNT_TO_TIME(fc)   ((double)(fc) / SAMPLE_RATE)

// access data array
#define DATA(_chan,_offset) \
    ( data [ _chan ] [ data_idx+(_offset) >= 0 ? data_idx+(_offset) : data_idx+(_offset)+MAX_FRAME ] )

//
// variables
//

static double   data[MAX_CHAN][MAX_FRAME];
static int      data_idx;

static double   cca[2*N+1];  
static double   ccb[2*N+1];

static uint64_t frame_cnt;

// -----------------  INIT  ------------------------------------------------------

void doa_init(void)
{
    // nothing needed
}

// -----------------  PROCESS MIC DATA  ------------------------------------------

void doa_feed(const short * frame)
{
    // increment data_idx, and frame_cnt
    data_idx = (data_idx + 1) % MAX_FRAME;
    frame_cnt++;

    // for each mic channel, copy the input frame to data arrays
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        DATA(chan,0) = frame[chan] * (1./32767);
    }

    // compute cross correlations for the 2 pairs of mic channels
    for (int i = -N; i <= N; i++) {
        cca[i+N] += DATA(CCA_MICX,-N) * DATA(CCA_MICY,-(N+i))  -
                    DATA(CCA_MICX,-((MAX_FRAME-1)-N)) * DATA(CCA_MICY,-((MAX_FRAME-1)-N+i));
        ccb[i+N] += DATA(CCB_MICX,-N) * DATA(CCB_MICY,-(N+i))  -
                    DATA(CCB_MICX,-((MAX_FRAME-1)-N)) * DATA(CCB_MICY,-((MAX_FRAME-1)-N+i));
    }
}

// -----------------  DETERMINE DIRECTION OF SOUND ARRIVAL  ----------------------

//             0
//             ^
//  270 <- RESPEAKER -> 90
//             v
//            180

// return -1 on error, otherise doa angle
double doa_get(void)
{
    int           max_cca_idx, max_ccb_idx;
    double        max_cca, max_ccb, coeffs[3], cca_x, ccb_x, angle;
    bool          discard;
    uint64_t      analysis_dur_us, start_us;
    static double x[2*N+1];

    // first call init
    if (x[0] == 0) {
        for (int i = -N; i <= N; i++) x[i+N] = i;
    }
    
    // time how long this analysis takes
    start_us = microsec_timer();

    // Each of the 2 pairs of mics has 2*N+1 cross correlation results that
    // is computed by the code near the top of this routine.
    // Determine the max cross-corr value for the sets of cross correlations
    // performed for the 2 pairs of mics.
    max_cca = max_doubles(cca, 2*N+1, &max_cca_idx);
    max_cca_idx -= N;
    max_ccb = max_doubles(ccb, 2*N+1, &max_ccb_idx);
    max_ccb_idx -= N;

    // Instead of using the max_cca/b_idx that is obtained above; this code
    // attempts to find a better value by fitting a 2nd degree polynomial 
    // (a parabola) using the max value, and the 2 values on either side of the max.
    //
    // For example, the max value is located at 7 samples; but the following
    // graph indicates that the 'true' max would be a little larger than 7.
    //   6:   5.3 **************************
    //   7:   6.2 ******************************
    //   8:   5.5 ***************************  
    // The vale computed, for this example, is: 7.064
    if (max_cca_idx > -N && max_cca_idx < N) {
        poly_fit(3, &x[max_cca_idx-1+N], &cca[max_cca_idx-1+N], 2, coeffs);
        cca_x = -coeffs[1] / (2 * coeffs[2]);
    } else {
        cca_x = (max_cca_idx <= -N ? -N : N);
    }

    if (max_ccb_idx > -N && max_ccb_idx < N) {
        poly_fit(3, &x[max_ccb_idx-1+N], &ccb[max_ccb_idx-1+N], 2, coeffs);
        ccb_x = -coeffs[1] / (2 * coeffs[2]);
    } else {
        ccb_x = (max_ccb_idx <= -N ? -N : N);
    }

    // for now the doa result is never discarded,
    // possibly consider discarding if
    // - max_cca < MIN || max_ccb < MIN
    // - cca_x*cca_x + ccb_x*ccb_x < MIN
    discard = false;

    // determine the direction of sound arrival angle
    angle = atan2(ccb_x, cca_x) * (180/M_PI);
    angle = normalize_angle(angle + ANGLE_OFFSET);

    // determine the duration of the analysis
    analysis_dur_us = microsec_timer() - start_us;

    // debug prints
    if (0) {
        double tnow = FRAME_CNT_TO_TIME(frame_cnt);
        char *prefix_str = (discard ? "DISCARD  " : "");
        INFO("%s%8.3f: ANALYZE SOUND - intvl=%0.3f ... %0.3f - analysis_dur_us = %lld\n", 
              prefix_str,
              FRAME_CNT_TO_TIME(frame_cnt), 
              tnow-(DURATION_MS/1000.), 
              tnow, 
              analysis_dur_us);
        for (int i = -N; i <= N; i++) {
            char s1[100], s2[100];
            INFO("%s%3d: %5.1f %-30s - %5.1f %-30s\n",
                   prefix_str,
                   i,
                   cca[i+N], stars(cca[i+N], max_cca, 30, s1),
                   ccb[i+N],  stars(ccb[i+N], max_ccb, 30, s2));
        }
        INFO("%s       LARGEST AT %-10.3f                 LARGEST AT %-10.3f\n", 
              prefix_str, cca_x, ccb_x);
        INFO("%s       DOA = %0.1f degs\n", 
              prefix_str, angle);
    }

    // return angle
    return discard ? -1 : angle;
}
