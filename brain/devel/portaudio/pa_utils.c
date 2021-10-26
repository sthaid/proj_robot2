#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <portaudio.h>
#include <pa_utils.h>

#define SIZEOF_SAMPLE_FORMAT(sf) \
    ((sf) == PA_FLOAT32  ? 4 : \
     (sf) == PA_INT32    ? 4 : \
     (sf) == PA_INT24    ? 3 : \
     (sf) == PA_INT16    ? 2 : \
     (sf) == PA_INT8     ? 1 : \
     (sf) == PA_UINT8    ? 1 : \
                           -1)

// -----------------  INIT  ------------------------------------------------------

static void exit_hndlr(void);

int pa_init(void)
{
    Pa_Initialize();

    atexit(exit_hndlr);

    return 0;
}

static void exit_hndlr(void)
{
    Pa_Terminate();
}

// -----------------  PLAY - DATA SUPPLIED IN CALLER SUPPLIED ARRAY  -------------

typedef struct {
    int max_chan;
    int max_frames;
    int sizeof_sample_format;
    int frame_idx;
    void *data;
} play_cx_t;

static int play_cb(void *data, void *cx);

int pa_play(char *output_device, int max_chan, int max_data, int sample_rate, int sample_format, void *data)
{
    play_cx_t cx;

    if ((max_data % max_chan) != 0) {
        printf("ERROR: max_data=%d must be a multiple of max_chan=%d\n", max_data, max_chan);
        return -1;
    }

    cx.max_chan             = max_chan;
    cx.max_frames           = max_data / max_chan;
    cx.sizeof_sample_format = SIZEOF_SAMPLE_FORMAT(sample_format);
    cx.frame_idx            = 0;
    cx.data                 = data;

    if (cx.sizeof_sample_format == -1) {
        printf("ERROR: invalid sample_format 0x%x\n", sample_format);
        return -1;
    }

    return pa_play2(output_device, max_chan, sample_rate, sample_format, play_cb, &cx);
}

static int play_cb(void *data, void *cx_arg)
{
    play_cx_t *cx = cx_arg;

    // if no more frames available to play then return -1
    if (cx->frame_idx == cx->max_frames) {
        return -1;
    }

    // copy a frame to portaudio buffer
    memcpy(data, 
           cx->data + (cx->frame_idx * cx->max_chan * cx->sizeof_sample_format),
           cx->max_chan * cx->sizeof_sample_format);
    cx->frame_idx++;

    // return 0, meaning continue
    return 0;
}

// -----------------  PLAY - DATA SUPPLIED BY CALLER SUPPLIED CALLBACK PROC  -----

typedef struct {
    play2_get_frame_t  get_frame;
    void              *get_frame_cx;
    int                max_chan;
    int                sizeof_sample_format;
    bool               done;
} play2_user_data_t;

static int play_stream_cb2(const void *input,
                          void *output,
                          unsigned long frame_count,
                          const PaStreamCallbackTimeInfo *timeinfo,
                          PaStreamCallbackFlags status_flags,
                          void *user_data);

static void play_stream_finished_cb2(void *user_data);

int pa_play2(char *output_device, int max_chan, int sample_rate, int sample_format, play2_get_frame_t get_frame, void *get_frame_cx)
{
    PaError             rc;
    PaStream           *stream = NULL;
    PaStreamParameters  output_params;
    PaDeviceIndex       devidx;
    play2_user_data_t   ud;

    // init user_data
    memset(&ud, 0, sizeof(ud));
    ud.get_frame            = get_frame;
    ud.get_frame_cx         = get_frame_cx;
    ud.max_chan             = max_chan;
    ud.sizeof_sample_format = SIZEOF_SAMPLE_FORMAT(sample_format);
    ud.done                 = false;

    // check sizeof_sample_format for error
    if (ud.sizeof_sample_format == -1) {
        printf("ERROR: invalid sample_format 0x%x\n", sample_format);
        goto error;
    }

    // get the output device idx
    devidx = pa_find_device(output_device);
    if (devidx == paNoDevice) {
        printf("ERROR: could not find %s\n", output_device);
        goto error;
    }
    //pa_print_device_info(devidx);

    // init output_params and open the audio output stream
    output_params.device            = devidx;
    output_params.channelCount      = max_chan;
    output_params.sampleFormat      = sample_format;
    output_params.suggestedLatency  = Pa_GetDeviceInfo(output_params.device)->defaultLowOutputLatency;
    output_params.hostApiSpecificStreamInfo = NULL;

    rc = Pa_OpenStream(&stream,
                       NULL,   // input_params
                       &output_params,
                       sample_rate,
                       paFramesPerBufferUnspecified,
                       0,       // stream flags
                       play_stream_cb2,
                       &ud);   // user_data
    if (rc != paNoError) {
        printf("ERROR: Pa_OpenStream rc=%d, %s\n", rc, Pa_GetErrorText(rc));
        goto error;
    }

    // register callback for when the the audio output compltes
    rc = Pa_SetStreamFinishedCallback(stream, play_stream_finished_cb2);
    if (rc != paNoError) {
        printf("ERROR: Pa_SetStreamFinishedCallback rc=%d, %s\n", rc, Pa_GetErrorText(rc));
        goto error;
    }

    // start the audio output
    rc = Pa_StartStream(stream);
    if (rc != paNoError) {
        printf("ERROR: Pa_StartStream rc=%d, %s\n", rc, Pa_GetErrorText(rc));
        goto error;
    }

    // wait for audio output to complete
    while (!ud.done) {
        Pa_Sleep(10);  // 10 ms
    }

    // clean up, and return success
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    return 0;

    // error return path
error:
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    return -1;
}

static int play_stream_cb2(const void *input,
                           void *output,
                           unsigned long frame_count,
                           const PaStreamCallbackTimeInfo *timeinfo,
                           PaStreamCallbackFlags status_flags,
                           void *user_data)
{
    play2_user_data_t *ud = user_data;
    int i, rc;

    for (i = 0; i < frame_count; i++) {
        rc = ud->get_frame(output, ud->get_frame_cx);
        output += (ud->max_chan * ud->sizeof_sample_format);
        if (rc != 0) return paComplete;
    }
    return paContinue;
}

static void play_stream_finished_cb2(void *user_data)
{
    play2_user_data_t *ud = user_data;
    ud->done = true;
}

// -----------------  RECORD - DATA PROVIDED TO CALLER SUPPLIED ARRAY  -------------

typedef struct {
    int max_chan;
    int max_frames;
    int sizeof_sample_format;
    int frame_idx;
    void *data;
} record_cx_t;

static int record_cb(const void *data, void *cx);

int pa_record(char *input_device, int max_chan, int max_data, int sample_rate, int sample_format, void *data, int discard_samples)
{
    record_cx_t cx;

    if ((max_data % max_chan) != 0) {
        printf("ERROR: max_data=%d must be a multiple of max_chan=%d\n", max_data, max_chan);
        return -1;
    }

    cx.max_chan             = max_chan;
    cx.max_frames           = max_data / max_chan;
    cx.sizeof_sample_format = SIZEOF_SAMPLE_FORMAT(sample_format);
    cx.frame_idx            = 0;
    cx.data                 = data;

    if (cx.sizeof_sample_format == -1) {
        printf("ERROR: invalid sample_format 0x%x\n", sample_format);
        return -1;
    }

    return pa_record2(input_device, max_chan, sample_rate, sample_format, record_cb, &cx, discard_samples);
}

static int record_cb(const void *data, void *cx_arg)
{
    record_cx_t *cx = cx_arg;

    // if no more record frame buffers available then return -1
    if (cx->frame_idx == cx->max_frames) {
        return -1;
    }

    // copy a frame from portaudio buffer
    memcpy(cx->data + (cx->frame_idx * cx->max_chan * cx->sizeof_sample_format),
           data,
           cx->max_chan * cx->sizeof_sample_format);
    cx->frame_idx++;

    // return 0, meaning continue
    return 0;
}

// -----------------  RECORD - DATA PROVIDED TO CALLER SUPPLIED CALLBACK PROC  -----

typedef struct {
    record2_put_frame_t put_frame;
    void               *put_frame_cx;
    int                 max_chan;
    int                 sizeof_sample_format;
    int                 discard_samples;
    bool                done;
} record2_user_data_t;

static int record_stream_cb2(const void *input,
                             void *output,
                             unsigned long frame_count,
                             const PaStreamCallbackTimeInfo *timeinfo,
                             PaStreamCallbackFlags status_flags,
                             void *user_data);

static void record_stream_finished_cb2(void *user_data);

int pa_record2(char *input_device, int max_chan, int sample_rate, int sample_format, record2_put_frame_t put_frame, void *put_frame_cx, int discard_samples)
{
    PaError             rc;
    PaStream           *stream = NULL;
    PaStreamParameters  input_params;
    PaDeviceIndex       devidx;
    record2_user_data_t ud;

    // init user_data
    memset(&ud, 0, sizeof(ud));
    ud.put_frame            = put_frame;
    ud.put_frame_cx         = put_frame_cx;
    ud.max_chan             = max_chan;
    ud.sizeof_sample_format = SIZEOF_SAMPLE_FORMAT(sample_format);
    ud.discard_samples      = discard_samples;
    ud.done                 = false;

    // check sizeof_sample_format for error
    if (ud.sizeof_sample_format == -1) {
        printf("ERROR: invalid sample_format 0x%x\n", sample_format);
        goto error;
    }

    // get the input device idx
    devidx = pa_find_device(input_device);
    if (devidx == paNoDevice) {
        printf("ERROR: could not find %s\n", input_device);
        goto error;
    }
    //pa_print_device_info(devidx);

    // init input_params and open the audio input stream
    input_params.device            = devidx;
    input_params.channelCount      = max_chan;
    input_params.sampleFormat      = sample_format;
    input_params.suggestedLatency  = Pa_GetDeviceInfo(input_params.device)->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = NULL;

    rc = Pa_OpenStream(&stream,
                       &input_params,
                       NULL,   // output_params
                       sample_rate,
                       paFramesPerBufferUnspecified,
                       0,       // stream flags
                       record_stream_cb2,
                       &ud);   // user_data
    if (rc != paNoError) {
        printf("ERROR: Pa_OpenStream rc=%d, %s\n", rc, Pa_GetErrorText(rc));
        goto error;
    }

    // register callback for when the the audio input completes
    rc = Pa_SetStreamFinishedCallback(stream, record_stream_finished_cb2);
    if (rc != paNoError) {
        printf("ERROR: Pa_SetStreamFinishedCallback rc=%d, %s\n", rc, Pa_GetErrorText(rc));
        goto error;
    }

    // start the audio input
    rc = Pa_StartStream(stream);
    if (rc != paNoError) {
        printf("ERROR: Pa_StartStream rc=%d, %s\n", rc, Pa_GetErrorText(rc));
        goto error;
    }

    // wait for audio input to complete
    while (!ud.done) {
        Pa_Sleep(10);  // 10 ms
    }

    // clean up, and return success
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    return 0;

    // error return path
error:
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    return -1;
}

static int record_stream_cb2(const void *input,
                             void *output,
                             unsigned long frame_count,
                             const PaStreamCallbackTimeInfo *timeinfo,
                             PaStreamCallbackFlags status_flags,
                             void *user_data)
{
    record2_user_data_t *ud = user_data;
    int i, rc;

    for (i = 0; i < frame_count; i++) {
        if (ud->discard_samples > 0) {
            ud->discard_samples--;
            continue;
        }

        rc = ud->put_frame(input, ud->put_frame_cx);
        input += (ud->max_chan * ud->sizeof_sample_format);
        if (rc != 0) return paComplete;
    }
    return paContinue;
}

static void record_stream_finished_cb2(void *user_data)
{
    record2_user_data_t *ud = user_data;
    ud->done = true;
}

// -----------------  UTILS  -----------------------------------------------------

PaDeviceIndex pa_find_device(char *name)
{
    int dev_cnt = Pa_GetDeviceCount();
    int i;

    if (strcmp(name, DEFAULT_OUTPUT_DEVICE) == 0) {
        return Pa_GetDefaultOutputDevice();
    }
    if (strcmp(name, DEFAULT_INPUT_DEVICE) == 0) {
        return Pa_GetDefaultInputDevice();
    }

    for (i = 0; i < dev_cnt; i++) {
        const PaDeviceInfo *di = Pa_GetDeviceInfo(i);
        if (di == NULL) {
            return paNoDevice;
        }
        if (strncmp(di->name, name, strlen(name)) == 0) {
            return i;
        }
    }

    return paNoDevice;
}

void pa_print_device_info(PaDeviceIndex idx)
{
    const PaDeviceInfo *di;
    const PaHostApiInfo *hai;
    char host_api_info_str[100];

    di = Pa_GetDeviceInfo(idx);
    hai = Pa_GetHostApiInfo(di->hostApi);

    sprintf(host_api_info_str, "%s%s%s",
            hai->name,
            hai->defaultInputDevice == idx ? " DEFAULT_INPUT" : "",
            hai->defaultOutputDevice == idx ? " DEFAULT_OUTPUT" : "");

    printf("PaDeviceIndex = %d\n", idx);
    printf("  name                       = %s\n",    di->name);
    printf("  hostApi                    = %s\n",    host_api_info_str);
    printf("  maxInputChannels           = %d\n",    di->maxInputChannels);
    printf("  maxOutputChannels          = %d\n",    di->maxOutputChannels);
    printf("  defaultLowInputLatency     = %0.3f\n", di->defaultLowInputLatency);
    printf("  defaultLowOutputLatency    = %0.3f\n", di->defaultLowOutputLatency);
    printf("  defaultHighInputLatency    = %0.3f\n", di->defaultHighInputLatency);
    printf("  defaultHighOutputLatency   = %0.3f\n", di->defaultHighOutputLatency);
    printf("  defaultSampleRate          = %0.0f\n", di->defaultSampleRate);  // XXX get all rates
    printf("\n");
}

void pa_print_device_info_all(void)
{
    int i;
    int dev_cnt = Pa_GetDeviceCount();
    const PaHostApiInfo *hai = Pa_GetHostApiInfo(0);

    if (dev_cnt != hai->deviceCount) {
        printf("ERROR: BUG dev_cnt=%d hai->deviceCount=%d\n", dev_cnt, hai->deviceCount);
        return; 
    }

    printf("hostApi = %s  device_count = %d  default_input = %d  default_output = %d\n",
           hai->name,
           hai->deviceCount,
           hai->defaultInputDevice,
           hai->defaultOutputDevice);
    printf("\n");

    for (i = 0; i < dev_cnt; i++) {
        pa_print_device_info(i);
    }
}
