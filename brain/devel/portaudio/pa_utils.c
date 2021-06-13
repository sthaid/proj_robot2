#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <portaudio.h>
#include <pa_utils.h>

typedef struct {
    int     max_chan;
    int     max_data;
    int     sample_rate;
    float **chan_data;
    int     data_idx;
    bool    done;
} user_data_t;

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

// -----------------  PLAY  ------------------------------------------------------

static int play_stream_cb(const void *input,
                          void *output,
                          unsigned long frame_count,
                          const PaStreamCallbackTimeInfo *timeinfo,
                          PaStreamCallbackFlags status_flags,
                          void *ud);
static void play_stream_finished_cb(void *ud);

int pa_play(char *output_device, int max_chan, int max_data, int sample_rate, float **chan_data)
{
    PaError             rc;
    PaStream           *stream = NULL;
    PaStreamParameters  output_params;
    PaDeviceIndex       devidx;
    user_data_t         ud;

    // init user_data
    memset(&ud, 0, sizeof(ud));
    ud.max_chan    = max_chan;
    ud.max_data    = max_data;
    ud.sample_rate = sample_rate;
    ud.chan_data   = chan_data;
    ud.data_idx    = 0;
    ud.done        = false;

    // get the output device idx
    devidx = pa_find_device(output_device);
    if (devidx == paNoDevice) {
        printf("ERROR: could not find %s\n", output_device);
	goto error;
    }
    pa_print_device_info(devidx);

    // init output_params and open the audio output stream
    output_params.device            = devidx;
    output_params.channelCount      = max_chan;
    output_params.sampleFormat      = paFloat32 | paNonInterleaved;
    output_params.suggestedLatency  = Pa_GetDeviceInfo(output_params.device)->defaultLowOutputLatency;
    output_params.hostApiSpecificStreamInfo = NULL;

    rc = Pa_OpenStream(&stream,
                       NULL,   // input_params
                       &output_params,
                       sample_rate,
                       paFramesPerBufferUnspecified,
                       0,       // stream flags
                       play_stream_cb,
                       &ud);   // user_data
    if (rc != paNoError) {
	printf("ERROR: Pa_OpenStream rc=%d, %s\n", rc, Pa_GetErrorText(rc));
	goto error;
    }

    // register callback for when the the audio output compltes
    rc = Pa_SetStreamFinishedCallback(stream, play_stream_finished_cb);
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

static int play_stream_cb(const void *input,
                          void *output,
                          unsigned long frame_count,
                          const PaStreamCallbackTimeInfo *timeinfo,
                          PaStreamCallbackFlags status_flags,
                          void *user_data)
{
    float **out = (void*)output;
    int chan;
    user_data_t *ud = user_data;

    // if more frames are requested than we have remaining data for then return paComplete
    if (frame_count > ud->max_data - ud->data_idx) {
        return paComplete;
    }

    // for each chan, copy the chan_data to out
    for (chan = 0; chan < ud->max_chan; chan++) {
	if (ud->chan_data[chan] == NULL) {
	    memset(out[chan], 0, frame_count*sizeof(float));
	} else {
	    memcpy(out[chan], &ud->chan_data[chan][ud->data_idx], frame_count*sizeof(float));
	}
    }

    // increase data_idx by the frame_count
    ud->data_idx += frame_count;
    
    // continue
    return paContinue;
}

static void play_stream_finished_cb(void *user_data)
{
    user_data_t *ud = user_data;
    ud->done = true;
}

// -----------------  RECORD  ----------------------------------------------------

static int record_stream_cb(const void *input,
                            void *output,
                            unsigned long frame_count,
                            const PaStreamCallbackTimeInfo *timeinfo,
                            PaStreamCallbackFlags status_flags,
                            void *ud);
static void record_stream_finished_cb(void *ud);

int pa_record(char *input_device, int max_chan, int max_data, int sample_rate, float **chan_data)
{
    PaError             rc;
    PaStream           *stream = NULL;
    PaStreamParameters  input_params;
    PaDeviceIndex       devidx;
    user_data_t         ud;

    // init user_data
    memset(&ud, 0, sizeof(ud));
    ud.max_chan    = max_chan;
    ud.max_data    = max_data;
    ud.sample_rate = sample_rate;
    ud.chan_data   = chan_data;
    ud.data_idx    = 0;
    ud.done        = false;

    // get the input device idx
    devidx = pa_find_device(input_device);
    if (devidx == paNoDevice) {
        printf("ERROR: could not find %s\n", input_device);
	goto error;
    }
    pa_print_device_info(devidx);

    // init input_params and open the audio input stream
    input_params.device            = devidx;
    input_params.channelCount      = max_chan;
    input_params.sampleFormat      = paFloat32 | paNonInterleaved;
    input_params.suggestedLatency  = Pa_GetDeviceInfo(input_params.device)->defaultLowOutputLatency;
    input_params.hostApiSpecificStreamInfo = NULL;

    rc = Pa_OpenStream(&stream,
                       &input_params,
                       NULL,   // output
                       sample_rate,
                       paFramesPerBufferUnspecified,
                       0,       // stream flags
                       record_stream_cb,
                       &ud);   // user_data
    if (rc != paNoError) {
	printf("ERROR: Pa_OpenStream rc=%d, %s\n", rc, Pa_GetErrorText(rc));
	goto error;
    }

    // register callback for when the the audio input compltes
    rc = Pa_SetStreamFinishedCallback(stream, record_stream_finished_cb);
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

static int record_stream_cb(const void *input,
                            void *output,
                            unsigned long frame_count,
                            const PaStreamCallbackTimeInfo *timeinfo,
                            PaStreamCallbackFlags status_flags,
                            void *user_data)
{
    float **in = (void*)input;
    user_data_t *ud = user_data;
    int chan;

    // reduce frame_count if there is not enough space remaining in data
    if (frame_count > ud->max_data - ud->data_idx) {
        frame_count = ud->max_data - ud->data_idx;
    }

    // for each channel make a copy of the input data
    for (chan = 0; chan < ud->max_chan; chan++) {
        memcpy(&ud->chan_data[chan][ud->data_idx], in[chan], frame_count*sizeof(float));
    }

    // update data_idx
    ud->data_idx += frame_count;

    // return either paComplete or paContinue
    return ud->data_idx == ud->max_data ? paComplete : paContinue;
}

static void record_stream_finished_cb(void *user_data)
{
    user_data_t *ud = user_data;
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
