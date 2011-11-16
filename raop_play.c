/*****************************************************************************
 * rtsp_play.c: RAOP Client player
 *
 * Copyright (C) 2004 Shiro Ninomiya <shiron@snino.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************
 *
 *  RaopX:
 *
 *	Morten Hersson <mhersson@gmail.com>
 *	Tormod Omholt-Jensen <toj@pvv.org>
 *		- Minor code changes to make source compile on Os X
 *		- Added code for AudioQueue
 *		- Added code for ZeroConf
 *
 *****************************************************************************/

#include <stdio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <dns_sd.h>
#include <string.h>
#include <unistd.h>
#include "aexcl_lib.h"
#include "raop_client.h"
#include "audio_stream.h"
#include "raop_play.h"
#include "audio_queue.h"
#include "zero_conf.h"

#define SERVER_PORT 5000
#define MAX_NUM_OF_FDS 4


// For Zero Conf
host_port_t hostPortArray[5]; 
int numResolved = 0;

typedef struct fdev_t{
	int fd;
	void *dp;
	fd_callback_t cbf;
	int flags;
}dfev_t;

typedef struct raopld_t{
	raopcl_t *raopcl;
	auds_t *auds;
	dfev_t fds[MAX_NUM_OF_FDS];
}raopld_t;
static raopld_t *raopld;


#define MAIN_EVENT_TIMEOUT 3 // sec unit
static int main_event_handler()
{
	fd_set rdfds,wrfds;
	int fdmax=0;
	int i;
	struct timeval tout={.tv_sec=MAIN_EVENT_TIMEOUT, .tv_usec=0};

	FD_ZERO(&rdfds);
	FD_ZERO(&wrfds);
	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd<0) continue;
		if(raopld->fds[i].flags&RAOP_FD_READ)
			FD_SET(raopld->fds[i].fd, &rdfds);
		if(raopld->fds[i].flags&RAOP_FD_WRITE)
			FD_SET(raopld->fds[i].fd, &wrfds);
		fdmax=(fdmax<raopld->fds[i].fd)?raopld->fds[i].fd:fdmax;
	}
	if(raopcl_wait_songdone(raopld->raopcl,0))
		raopcl_aexbuf_time(raopld->raopcl, &tout);

	select(fdmax+1,&rdfds,&wrfds,NULL,&tout);

	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd<0) continue;
		if((raopld->fds[i].flags&RAOP_FD_READ) &&
		   FD_ISSET(raopld->fds[i].fd,&rdfds)){
			//DBGMSG("rd event i=%d, flags=%d\n",i,raopld->fds[i].flags);
			if(raopld->fds[i].cbf &&
			   raopld->fds[i].cbf(raopld->fds[i].dp, RAOP_FD_READ)) return -1;
		}
		if((raopld->fds[i].flags&RAOP_FD_WRITE) &&
		   FD_ISSET(raopld->fds[i].fd,&wrfds)){
			//DBGMSG("wr event i=%d, flags=%d\n",i,raopld->fds[i].flags);
			if(raopld->fds[i].cbf &&
			   raopld->fds[i].cbf(raopld->fds[i].dp, RAOP_FD_WRITE)) return -1;
		}
	}

	if(raopcl_wait_songdone(raopld->raopcl,0)){
		raopcl_aexbuf_time(raopld->raopcl, &tout);
		if(!tout.tv_sec && !tout.tv_usec){
			// AEX data buffer becomes empty, it means end of playing a song.
			printf("%s\n",RAOP_SONGDONE);
			fflush(stdout);
			raopcl_wait_songdone(raopld->raopcl,-1); // clear wait_songdone
		}
	}

	raopcl_pause_check(raopld->raopcl);

	return 0;
}

static int console_command(char *cstr)
{
	int i;
	char *ps=NULL;

	if(strstr(cstr,"play")==cstr){
		if(raopcl_get_pause(raopld->raopcl)) {
			raopcl_set_pause(raopld->raopcl,NO_PAUSE);
			return 0;
		}
		for(i=0;i<strlen(cstr);i++) {
			if(cstr[i]==' ') {
				ps=cstr+i+1;
				break;
			}
		}
		if(!ps) return 0;
		// if there is a new song name, open the song
		if(!(raopld->auds=auds_open(ps, 0))){
			printf("%s\n",RAOP_ERROR);
			fflush(stdout);
			return -1;
		}
		raopcl_flush_stream(raopld->raopcl);
		return 0;
	}else if(!strcmp(cstr,"pause")){
		if(raopcl_wait_songdone(raopld->raopcl,0)){
			INFMSG("in this period, pause can't work\n");
			return -2;
		}
		if(raopld->auds) {
			raopcl_set_pause(raopld->raopcl,OP_PAUSE);
		}
	}else if(!strcmp(cstr,"stop")){
		if(raopcl_get_pause(raopld->raopcl)) raopcl_set_pause(raopld->raopcl,NO_PAUSE);
		if(raopld->auds) auds_close(raopld->auds);
		raopld->auds=NULL;
	}else if(!strcmp(cstr,"quit")){
		return -2;
	}else if((ps=strstr(cstr,"volume"))){
		i=atoi(ps+7);
		return raopcl_update_volume(raopld->raopcl,i);
	}
	return -1;
}

static int console_read(void *p, int flags)
{
	char line[256];
	if(read_line(0,line,sizeof(line),100,0)==-1){
		DBGMSG("stop reading from console\n");
		clear_fd_event(0);
	}else{
		DBGMSG("%s:%s\n",__func__,line);
	}
	if(console_command(line)==-2) return -1;
	// ignore console command errors, and return 0
	return 0;
}

static int terminate_cbf(void *p, int flags){
	return -1;
}

static void sig_action(int signo, siginfo_t *siginfo, void *extra)
{
	// SIGCHLD, a child process is terminated
	if(signo==SIGCHLD){
		auds_sigchld(raopld->auds, siginfo);
		return;
	}
	//SIGINT,SIGTERM
	//DBGMSG("SIGINT or SIGTERM\n");
	set_fd_event(1,RAOP_FD_WRITE,terminate_cbf,NULL);
	return;
}

void deleteFileIfExists (const char * fname) {
	// Check for tmp file.. If exist delete it
	// Should change this to stat...
	FILE * pFile;
	if( (pFile=fopen(fname, "r")) )
	{
		fclose(pFile);
		remove(fname);
	}
}

int main(int argc, char *argv[])
{
	char *host=NULL;
	char *fname="/tmp/raopxtmpbuffer";
	int port=SERVER_PORT;
	int rval=-1,i;
	int size;
	int volume=100;
	u_int8_t *buf;
	struct sigaction act;
	
	printf("\nRaopX %s\n", VERSION);

	/* Assign sig_term as our SIGTERM handler  */
	act.sa_sigaction = sig_action;
	sigemptyset(&act.sa_mask); // no other signals are blocked
	act.sa_flags = SA_SIGINFO; // use sa_sigaction instead of sa_handler
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);

	deleteFileIfExists(fname); //Delete TempBufferFile if exists
	
	/************** Zero Conf ********************/
	printf("\nSearching for RAOP capable devices\n");
	DNSServiceErrorType error = ZCDNSServiceBrowse();
	if (error) fprintf(stderr, "DNSServiceDiscovery returned %d\n", error);
	if (numResolved == 0) {
		printf("No RAOP capable devices found\n");
		return -1;
	}else if (numResolved == 1) {
		host = hostPortArray[0].raop_host;
		port = hostPortArray[0].raop_port;
	}else {
		printf("\nMultiple devices found:\n");
		for (i=0; i < numResolved; i++) {
			printf("%i) %s\n",i, hostPortArray[i].raop_host);
		}
		printf("\nWhich device would you like to use\nEnter number and press <Return>: ");
		scanf("%d", &i);
		getchar(); // To remove newline
		if (i > numResolved) {
			printf("Not a valid menu option. Program terminating\n\n"); 
			return -1;
		}
		host = hostPortArray[i].raop_host;
		port = hostPortArray[i].raop_port;
	}
	
	

	
	raopld=(raopld_t*)malloc(sizeof(raopld_t));
	if(!raopld) goto erexit;
	memset(raopld,0,sizeof(raopld_t));
	for(i=0;i<MAX_NUM_OF_FDS;i++) raopld->fds[i].fd=-1;

	/* This is where the connection opens */
	raopld->raopcl=raopcl_open();
	if(!raopld->raopcl) goto erexit;
	if(raopcl_connect(raopld->raopcl,host,port)) goto erexit; //This is the connect
	if(raopcl_update_volume(raopld->raopcl,volume)) goto erexit; //Setting Volume
	printf("\n%s to %s\n",RAOP_CONNECTED, host);
	fflush(stdout);
	
	/* ########################################## Start of AudioQueue code ################################ */
	
	printf("Opening AudioQueue\n");
	
	int bufferByteSize;
	float seconds = 0;
	AudioStreamBasicDescription recordFormat;
	AQRecorder aqr;
	UInt32 aqsize;
	CFURLRef url;
	
	// fill structures with 0/NULL
	memset(&recordFormat, 0, sizeof(recordFormat));
	memset(&aqr, 0, sizeof(aqr));

	//if (recordFileName == NULL) // no record file path provided
	//	usage();
 
	// determine file format
	AudioFileTypeID audioFileType = kAudioFileCAFType;	// default to CAF
	CFStringRef cfRecordFileName = CFStringCreateWithCString(NULL, fname, kCFStringEncodingUTF8);
	AQInferAudioFileFormatFromFilename(cfRecordFileName, &audioFileType);
	CFRelease(cfRecordFileName);

	 // adapt record format to hardware and apply defaults
    if (recordFormat.mSampleRate == 0.)
	    AQGetDefaultInputDeviceSampleRate(&recordFormat.mSampleRate);

    if (recordFormat.mChannelsPerFrame == 0)
	    recordFormat.mChannelsPerFrame = 2;

    if (recordFormat.mFormatID == 0 || recordFormat.mFormatID == kAudioFormatLinearPCM) {
	    // default to PCM, 16 bit int
	    recordFormat.mFormatID = kAudioFormatLinearPCM;
	    recordFormat.mFormatFlags =  kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	    recordFormat.mBitsPerChannel = 16;
	    if (AQFileFormatRequiresBigEndian(audioFileType, recordFormat.mBitsPerChannel))
		    recordFormat.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
	    recordFormat.mBytesPerPacket = recordFormat.mBytesPerFrame =
	    (recordFormat.mBitsPerChannel / 8) * recordFormat.mChannelsPerFrame;
	    recordFormat.mFramesPerPacket = 1;
	    recordFormat.mReserved = 0;
    } 

	
	
	// create the queue
	AQCheckError(AudioQueueNewInput(
								  &recordFormat,
								  AQInputBufferHandler,
								  &aqr /* userData */,
								  NULL /* run loop */, NULL /* run loop mode */,
								  0 /* flags */, &aqr.queue), "AudioQueueNewInput failed");
	
	// get the record format back from the queue's audio converter --
	// the file may require a more specific stream description than was necessary to create the encoder.
	aqsize = sizeof(recordFormat);
	AQCheckError(AudioQueueGetProperty(aqr.queue, kAudioConverterCurrentOutputStreamDescription,
									 &recordFormat, &aqsize), "couldn't get queue's format");
	
	// convert recordFileName from C string to CFURL
	url = CFURLCreateFromFileSystemRepresentation(NULL, (Byte *)fname, strlen(fname), FALSE);
	
	// create the audio file
	AQCheckError(AudioFileCreateWithURL(url, audioFileType, &recordFormat, kAudioFileFlags_EraseFile,
									  &aqr.recordFile), "AudioFileCreateWithURL failed");
	CFRelease(url);
	
	// copy the cookie first to give the file object as much info as we can about the data going in
	AQCopyEncoderCookieToFile(aqr.queue, aqr.recordFile);
	
	// allocate and enqueue buffers
	bufferByteSize = AQComputeRecordBufferSize(&recordFormat, aqr.queue, 0.5);	// enough bytes for half a second
	for (i = 0; i < kNumberRecordBuffers; ++i) {
		AudioQueueBufferRef buffer;
		AQCheckError(AudioQueueAllocateBuffer(aqr.queue, bufferByteSize, &buffer),
				   "AudioQueueAllocateBuffer failed");
		AQCheckError(AudioQueueEnqueueBuffer(aqr.queue, buffer, 0, NULL),
				   "AudioQueueEnqueueBuffer failed");
	}
	
	// record
	if (seconds > 0) {
		// user requested a fixed-length recording (specified a duration with -s)
		// to time the recording more accurately, watch the queue's IsRunning property
		AQCheckError(AudioQueueAddPropertyListener(aqr.queue, kAudioQueueProperty_IsRunning,
												 AQPropertyListener, &aqr), "AudioQueueAddPropertyListener failed");
		
		// start the queue
		aqr.running = TRUE;
		AQCheckError(AudioQueueStart(aqr.queue, NULL), "AudioQueueStart failed");
		CFAbsoluteTime waitUntil = CFAbsoluteTimeGetCurrent() + 10;
		
		// wait for the started notification
		while (aqr.queueStartStopTime == 0.) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.010, FALSE);
			if (CFAbsoluteTimeGetCurrent() >= waitUntil) {
				fprintf(stderr, "Timeout waiting for the queue's IsRunning notification\n");
				goto cleanup;
			}
		}
		printf("Recording...\n");
		CFAbsoluteTime stopTime = aqr.queueStartStopTime + seconds;
		CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, stopTime - now, FALSE);
	} else {
		// start the queue
		aqr.running = TRUE;
		AQCheckError(AudioQueueStart(aqr.queue, NULL), "AudioQueueStart failed");
		
		
		//Wait for the tempfile to get some data
		printf("Building %i seconds buffer. Please wait\n", BUFFER_SECONDS);
		sleep(BUFFER_SECONDS);
		
		printf("\nStarting stream\nPress <Ctrl>+C, then <Return> to Quit:\n");
		/* ########################################## Org Raop play loop code ################################ */
		
		/* Starting to play */
		if(fname && !(raopld->auds=auds_open(fname,0))) goto erexit; 
		set_fd_event(0,RAOP_FD_READ,console_read,NULL);
		rval=0;
		while(!rval){
			if(!raopld->auds){
				// if audio data is not opened, just check events
				rval=main_event_handler(raopld);
				continue;
			}
			switch(raopcl_get_pause(raopld->raopcl)){
				case OP_PAUSE:
					rval=main_event_handler();
					continue;
				case NODATA_PAUSE:
					if(auds_poll_next_sample(raopld->auds)){
						raopcl_set_pause(raopld->raopcl,NO_PAUSE);
					}else{
						rval=main_event_handler();
						continue;
					}
				case NO_PAUSE:
					if(!auds_poll_next_sample(raopld->auds)){
						// no next data, turn into pause status
						raopcl_set_pause(raopld->raopcl,NODATA_PAUSE);
						continue;
					}
					if(auds_get_next_sample(raopld->auds, &buf, &size)){
						auds_close(raopld->auds);
						raopld->auds=NULL;
						raopcl_wait_songdone(raopld->raopcl,1);
					}
					if(raopcl_send_sample(raopld->raopcl,buf,size)) break;
					do{
						if((rval=main_event_handler())) break;
					}while(raopld->auds && raopcl_sample_remsize(raopld->raopcl));
					break;
				default:
					rval=-1;
					break;
			}
		}
		//rval=raopcl_close(raopld->raopcl);	
		
		
		
		
	/* ##################################### End Org Raop play loop code ############################## */
		
		getchar();
	}
	
	// end recording
	printf("Stream stopped\n");
	aqr.running = FALSE;
	AQCheckError(AudioQueueStop(aqr.queue, TRUE), "AudioQueueStop failed");
	
	// a codec may update its cookie at the end of an encoding session, so reapply it to the file now
	AQCopyEncoderCookieToFile(aqr.queue, aqr.recordFile);
	
cleanup:
	AudioQueueDispose(aqr.queue, TRUE);
	AudioFileClose(aqr.recordFile);
	deleteFileIfExists(fname);
	rval=raopcl_close(raopld->raopcl);
	
	return 0;
	
	
	/* ########################################### End of AudioQueue code ################################## */
	
 erexit:
	if(raopld->auds) auds_close(raopld->auds);
	if(raopld) free(raopld);
	return rval;
}

int set_fd_event(int fd, int flags, fd_callback_t cbf, void *p)
{
	int i;
	// check the same fd first. if it exists, update it
	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd==fd){
			raopld->fds[i].dp=p;
			raopld->fds[i].cbf=cbf;
			raopld->fds[i].flags=flags;
			return 0;
		}
	}
	// then create a new one
	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd<0){
			raopld->fds[i].fd=fd;
			raopld->fds[i].dp=p;
			raopld->fds[i].cbf=cbf;
			raopld->fds[i].flags=flags;
			return 0;
		}
	}
	return -1;
}

int clear_fd_event(int fd)
{
	int i;
	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd==fd){
			raopld->fds[i].fd=-1;
			raopld->fds[i].dp=NULL;
			raopld->fds[i].cbf=NULL;
			raopld->fds[i].flags=0;
			return 0;
		}
	}
	return -1;
}

