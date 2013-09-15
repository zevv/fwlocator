
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "audio.h"
#include "event.h"
#include "mainloop.h"

#define BLOCKSIZE 32

int fd[2];
static SLObjectItf engine_obj;
static SLEngineItf engine;
static SLObjectItf recorder_obj;
static SLRecordItf recorder;
static SLAndroidSimpleBufferQueueItf bufq;
static int16_t buf[BLOCKSIZE];


void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	write(fd[1], buf, sizeof buf);
	(*bufq)->Enqueue(bufq, buf, 2 * BLOCKSIZE);
}


int on_audio(int fd, void *data)
{
	int16_t buf[BLOCKSIZE];
	read(fd, buf, sizeof buf);
	audio_handle(buf, BLOCKSIZE);
	return 0;
}

void audio_android_init(void)
{
	int r;

	slCreateEngine(&engine_obj, 0, NULL, 0, NULL, NULL);
	(*engine_obj)->Realize(engine_obj, SL_BOOLEAN_FALSE);
	(*engine_obj)->GetInterface(engine_obj, SL_IID_ENGINE, &engine);

	SLDataLocator_IODevice loc_dev = {
		SL_DATALOCATOR_IODEVICE, 
		SL_IODEVICE_AUDIOINPUT,
		SL_DEFAULTDEVICEID_AUDIOINPUT, 
		NULL
	};

	SLDataSource audioSrc = {
		&loc_dev, 
		NULL
	};

	SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

	SLDataFormat_PCM format_pcm = {
		SL_DATAFORMAT_PCM, 
		1, 
		SL_SAMPLINGRATE_8,
		SL_PCMSAMPLEFORMAT_FIXED_16, 
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_CENTER, 
		SL_BYTEORDER_LITTLEENDIAN
	};

	SLDataSink audioSnk = { 
		&loc_bq, 
		&format_pcm 
	};

	const SLInterfaceID id[1] = { 
		SL_IID_ANDROIDSIMPLEBUFFERQUEUE 
	};

	const SLboolean req[1] = { 
		SL_BOOLEAN_TRUE 
	};

	r = (*engine)->CreateAudioRecorder(engine, &recorder_obj, &audioSrc, &audioSnk, 1, id, req);
	if (SL_RESULT_SUCCESS != r) goto end_recopen;

	// realize the audio recorder
	r = (*recorder_obj)->Realize(recorder_obj, SL_BOOLEAN_FALSE);
	if (SL_RESULT_SUCCESS != r) goto end_recopen;
	
	// get the record interface
	r = (*recorder_obj)->GetInterface(recorder_obj, SL_IID_RECORD, &(recorder));
	if (SL_RESULT_SUCCESS != r) goto end_recopen;
	
	// get the buffer queue interface
	r = (*recorder_obj)->GetInterface(recorder_obj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &(bufq));
	if (SL_RESULT_SUCCESS != r) goto end_recopen;
	
	// register callback on the buffer queue
	r = (*bufq)->RegisterCallback(bufq, bqRecorderCallback, NULL);
	if (SL_RESULT_SUCCESS != r) goto end_recopen;

	r = (*recorder)->SetRecordState(recorder, SL_RECORDSTATE_RECORDING);
	if (SL_RESULT_SUCCESS != r) goto end_recopen;

	r = (*bufq)->Enqueue(bufq, buf, 2 * BLOCKSIZE);
	if (SL_RESULT_SUCCESS != r) goto end_recopen;

	pipe(fd);
	mainloop_fd_add(fd[0], FD_READ, on_audio, NULL);
	
	printf("ok\n");
end_recopen:
	return;

}



/*
 * End
 */
