/*	Copyright © 2007 Apple Inc. All Rights Reserved.
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
 Apple Inc. ("Apple") in consideration of your agreement to the
 following terms, and your use, installation, modification or
 redistribution of this Apple software constitutes acceptance of these
 terms.  If you do not agree with these terms, please do not use,
 install, modify or redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software. 
 Neither the name, trademarks, service marks or logos of Apple Inc. 
 may be used to endorse or promote products derived from the Apple
 Software without specific prior written permission from Apple.  Except
 as expressly stated in this notice, no other rights or licenses, express
 or implied, are granted by Apple herein, including but not limited to
 any patent rights that may be infringed by your derivative works or by
 other works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

/*************************************************************************** 
	For RaopX: 
	Morten Hersson <mhersson@gmail.com>
	Tormod Omholt-Jensen <toj@pvv.org>
 
	Some minor code changes
	Moved the main into raop_play.c
 
****************************************************************************/


#include <AudioToolbox/AudioToolbox.h>
#include "audio_queue.h"

// ____________________________________________________________________________________
// Convert a C string to a 4-char code.
// interpret hex literals such as "\x00".
// return number of characters parsed.
int AQStrTo4CharCode(const char *str, FourCharCode *p4cc)
{
	char buf[4];
	const char *p = str;
	int i, x;
	for (i = 0; i < 4; ++i) {
		if (*p != '\\') {
			if ((buf[i] = *p++) == '\0') {
				// special-case for 'aac ': if we only got three characters, assume the last was a space
				if (i == 3) {
					--p;
					buf[i] = ' ';
					break;
				}
				goto fail;
			}
		} else {
			if (*++p != 'x') goto fail;
			if (sscanf(++p, "%02X", &x) != 1) goto fail;
			buf[i] = x;
			p += 2;
		}
	}
	*p4cc = CFSwapInt32BigToHost(*(UInt32 *)buf);
	return p - str;
fail:
	return 0;
}

// ____________________________________________________________________________________
// return true if testExt (should not include ".") is in the array "extensions".
Boolean AQMatchExtension(CFArrayRef extensions, CFStringRef testExt)
{
	CFIndex n = CFArrayGetCount(extensions), i;
	for (i = 0; i < n; ++i) {
		CFStringRef ext = (CFStringRef)CFArrayGetValueAtIndex(extensions, i);
		if (CFStringCompare(testExt, ext, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			return TRUE;
		}
	}
	return FALSE;
}

// ____________________________________________________________________________________
// Infer an audio file type from a filename's extension.
Boolean AQInferAudioFileFormatFromFilename(CFStringRef filename, AudioFileTypeID *outFiletype)
{
	OSStatus err;
	
	// find the extension in the filename.
	CFRange range = CFStringFind(filename, CFSTR("."), kCFCompareBackwards);
	if (range.location == kCFNotFound)
		return FALSE;
	range.location += 1;
	range.length = CFStringGetLength(filename) - range.location;
	CFStringRef extension = CFStringCreateWithSubstring(NULL, filename, range);
	
	UInt32 propertySize = sizeof(AudioFileTypeID);
	err = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), &extension, &propertySize, outFiletype);
	CFRelease(extension);
	
	return (err == noErr && propertySize > 0);
}

Boolean AQFileFormatRequiresBigEndian(AudioFileTypeID audioFileType, int bitdepth)
{
	AudioFileTypeAndFormatID ftf;
	UInt32 propertySize;
	OSStatus err;
	Boolean requiresBigEndian;
	
	ftf.mFileType = audioFileType;
	ftf.mFormatID = kAudioFormatLinearPCM;
	
	err = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(ftf), &ftf, &propertySize);
	if (err) return FALSE;
	
	AudioStreamBasicDescription *formats = (AudioStreamBasicDescription *)malloc(propertySize);
	err = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(ftf), &ftf, &propertySize, formats);
	requiresBigEndian = TRUE;
	int i, nFormats = propertySize / sizeof(AudioStreamBasicDescription);
	for (i = 0; i < nFormats; ++i) {
		if (formats[i].mBitsPerChannel == bitdepth
			&& !(formats[i].mFormatFlags & kLinearPCMFormatFlagIsBigEndian))
			return FALSE;
	}
	free(formats);
	return requiresBigEndian;
}

// ____________________________________________________________________________________
// generic error handler - if err is nonzero, prints error message and exits program.
void AQCheckError(OSStatus error, const char *operation)
{
	if (error == noErr) return;
	
	char str[20];
	// see if it appears to be a 4-char-code
	*(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error);
	if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
		str[0] = str[5] = '\'';
		str[6] = '\0';
	} else
		// no, format it as an integer
		sprintf(str, "%d", (int)error);
	
	fprintf(stderr, "Error: %s (%s)\n", operation, str);
	
	exit(1);
}

// ____________________________________________________________________________________
// Determine the size, in bytes, of a buffer necessary to represent the supplied number
// of seconds of audio data.
int AQComputeRecordBufferSize(const AudioStreamBasicDescription *format, AudioQueueRef queue, float seconds)
{
	int packets, frames, bytes;
	
	frames = (int)ceil(seconds * format->mSampleRate);
	
	if (format->mBytesPerFrame > 0)
		bytes = frames * format->mBytesPerFrame;
	else {
		UInt32 maxPacketSize;
		if (format->mBytesPerPacket > 0)
			maxPacketSize = format->mBytesPerPacket;	// constant packet size
		else {
			UInt32 propertySize = sizeof(maxPacketSize); 
			AQCheckError(AudioQueueGetProperty(queue, kAudioConverterPropertyMaximumOutputPacketSize, &maxPacketSize,
											 &propertySize), "couldn't get queue's maximum output packet size");
		}
		if (format->mFramesPerPacket > 0)
			packets = frames / format->mFramesPerPacket;
		else
			packets = frames;	// worst-case scenario: 1 frame in a packet
		if (packets == 0)		// sanity check
			packets = 1;
		bytes = packets * maxPacketSize;
	}
	return bytes;
}

// ____________________________________________________________________________________
// Copy a queue's encoder's magic cookie to an audio file.
void AQCopyEncoderCookieToFile(AudioQueueRef theQueue, AudioFileID theFile)
{
	OSStatus err;
	UInt32 propertySize;
	
	// get the magic cookie, if any, from the converter		
	err = AudioQueueGetPropertySize(theQueue, kAudioConverterCompressionMagicCookie, &propertySize);
	
	if (err == noErr && propertySize > 0) {
		// there is valid cookie data to be fetched;  get it
		Byte *magicCookie = (Byte *)malloc(propertySize);
		AQCheckError(AudioQueueGetProperty(theQueue, kAudioConverterCompressionMagicCookie, magicCookie,
										 &propertySize), "get audio converter's magic cookie");
		// now set the magic cookie on the output file
		err = AudioFileSetProperty(theFile, kAudioFilePropertyMagicCookieData, propertySize, magicCookie);
		free(magicCookie);
	}
}

// ____________________________________________________________________________________
// AudioQueue callback function, called when a property changes.
void AQPropertyListener(void *userData, AudioQueueRef queue, AudioQueuePropertyID propertyID)
{
	AQRecorder *aqr = (AQRecorder *)userData;
	if (propertyID == kAudioQueueProperty_IsRunning)
		aqr->queueStartStopTime = CFAbsoluteTimeGetCurrent();
}

// ____________________________________________________________________________________
// AudioQueue callback function, called when an input buffers has been filled.
void AQInputBufferHandler(	void *                          inUserData,
								 AudioQueueRef                   inAQ,
								 AudioQueueBufferRef             inBuffer,
								 const AudioTimeStamp *          inStartTime,
								 UInt32							inNumPackets,
								 const AudioStreamPacketDescription *inPacketDesc)
{
	AQRecorder *aqr = (AQRecorder *)inUserData;
	
	if (aqr->verbose) {
		printf("buf data %p, 0x%x bytes, 0x%x packets\n", inBuffer->mAudioData,
			   (int)inBuffer->mAudioDataByteSize, (int)inNumPackets);
		
	}
	
	if (inNumPackets > 0) {
		// write packets to file
		
		AQCheckError(AudioFileWritePackets(aqr->recordFile, FALSE, inBuffer->mAudioDataByteSize,
										 inPacketDesc, aqr->recordPacket, &inNumPackets, inBuffer->mAudioData),
				   "AudioFileWritePackets failed");
		
		
		
		//AQCheckError(raopcl_send_sample(raopld->raopcl, (u_int8_t*)inBuffer->mAudioData,inBuffer->mAudioDataByteSize), "Here we go again!");
		
		//AQCheckError(raopcl_small_silent(raopld->raopcl), "Test stillhet");
		aqr->recordPacket += inNumPackets;
	}
	
	// if we're not stopping, re-enqueue the buffe so that it gets filled again
	if (aqr->running)
		AQCheckError(AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL), "AudioQueueEnqueueBuffer failed");
}

// ____________________________________________________________________________________
// get sample rate of the default input device
OSStatus	AQGetDefaultInputDeviceSampleRate(Float64 *outSampleRate)
{
	OSStatus err;
	AudioDeviceID deviceID = 0;
	
	// get the default input device
	AudioObjectPropertyAddress addr;
	UInt32 size;
	addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
	addr.mScope = kAudioObjectPropertyScopeGlobal;
	addr.mElement = 0;
	size = sizeof(AudioDeviceID);
#if USE_AUDIO_SERVICES
	err = AudioHardwareServiceGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &deviceID);
#else
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &deviceID);
#endif
	if (err) return err;
	
	// get its sample rate
	addr.mSelector = kAudioDevicePropertyNominalSampleRate;
	addr.mScope = kAudioObjectPropertyScopeGlobal;
	addr.mElement = 0;
	size = sizeof(Float64);
#if USE_AUDIO_SERVICES
	err = AudioHardwareServiceGetPropertyData(deviceID, &addr, 0, NULL, &size, outSampleRate);
#else
	err = AudioObjectGetPropertyData(deviceID, &addr, 0, NULL, &size, outSampleRate);
#endif
	
	return err;
}


