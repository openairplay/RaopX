/*
 * audio_queue.h
 * RaopX
 *
 *************************************************************************/

#ifndef __AUDIO_QUEUE_H_
#define __AUDIO_QUEUE_H


#define USE_AUDIO_SERVICES 1
#define kNumberRecordBuffers	3

typedef struct AQRecorder {
	AudioQueueRef				queue;
	
	CFAbsoluteTime				queueStartStopTime;
	AudioFileID					recordFile;
	SInt64						recordPacket; // current packet number in record file
	Boolean						running;
	Boolean						verbose;
} AQRecorder;


void AQMissingArgument(const char *opt);
void AQParseError(const char *opt, const char *val);
int AQStrTo4CharCode(const char *str, FourCharCode *p4cc);
Boolean AQMatchExtension(CFArrayRef extensions, CFStringRef testExt);
Boolean AQInferAudioFileFormatFromFilename(CFStringRef filename, AudioFileTypeID *outFiletype);
Boolean AQFileFormatRequiresBigEndian(AudioFileTypeID audioFileType, int bitdepth);
void AQCheckError(OSStatus error, const char *operation);
int AQComputeRecordBufferSize(const AudioStreamBasicDescription *format, AudioQueueRef queue, float seconds);
void AQCopyEncoderCookieToFile(AudioQueueRef theQueue, AudioFileID theFile);
void AQPropertyListener(void *userData, AudioQueueRef queue, AudioQueuePropertyID propertyID);
void AQInputBufferHandler(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer, const AudioTimeStamp *inStartTime, UInt32	inNumPackets, const AudioStreamPacketDescription *inPacketDesc);
OSStatus	AQGetDefaultInputDeviceSampleRate(Float64 *outSampleRate);


#endif
