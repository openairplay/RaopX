/*****************************************************************************
 * audio_stream.c: audio file stream
 *
 * Copyright (C) 2005 Shiro Ninomiya <shiron@snino.com>
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
 *****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <samplerate.h>
#define AUDIO_STREAM_C
#include "audio_stream.h"
#include "pcm_stream.h"
#include "aexcl_lib.h"


static data_type_t get_data_type(char *fname);
static int16_t *pcm_resample(int16_t *ps, SRC_STATE *st, SRC_DATA *sd,
			   data_source_t *ds, int *bsize, int channels);

auds_t *auds_open(char *fname, data_type_t adt)
{
	auds_t *auds=malloc(sizeof(auds_t));
	if(!auds) return NULL;
	memset(auds, 0, sizeof(auds_t));
	int rval=-1;
	int err;

	auds->channels=2; //default is stereo
	if(adt==AUD_TYPE_NONE)
		auds->data_type=get_data_type(fname);
	else
		auds->data_type=adt;
	switch(auds->data_type){
	case AUD_TYPE_PCM:
		rval=pcm_open(auds, fname);
		break;
	case AUD_TYPE_NONE:
		ERRMSG("unknown audio data type\n");
		break;
	}
	if(rval) goto erexit;
	if(auds->sample_rate != DEFAULT_SAMPLE_RATE){
		auds->resamp_st=src_new(SRC_SINC_FASTEST, auds->channels, &err);
		if(!auds->resamp_st) {
			ERRMSG("can't open libsamplerate\n");
			goto erexit;
		}
		auds->resamp_buf=(u_int16_t*)malloc(MAX_SAMPLES_IN_CHUNK*2*2);
		auds->resamp_sd.data_in=(float *)malloc(sizeof(float)*MAX_SAMPLES_IN_CHUNK*2);
		auds->resamp_sd.data_out=(float *)malloc(sizeof(float)*MAX_SAMPLES_IN_CHUNK*2);
		auds->resamp_sd.src_ratio=(double)DEFAULT_SAMPLE_RATE/(double)auds->sample_rate;
		if(!auds->resamp_buf || !auds->resamp_sd.data_in || !auds->resamp_sd.data_out)
			goto erexit;
	}
	return auds;
 erexit:
	ERRMSG("errror: %s\n",__func__);
	auds_close(auds);
	return NULL;
}

int auds_close(auds_t *auds)
{
	if(auds->stream){
		switch(auds->data_type){
		case AUD_TYPE_PCM:
			pcm_close(auds);
			break;
		case AUD_TYPE_NONE:
			ERRMSG("### shouldn't come here\n");
			break;
		}
	}
	if(auds->resamp_st){
		if(auds->resamp_buf) free(auds->resamp_buf);
		if(auds->resamp_sd.data_in) free(auds->resamp_sd.data_in);
		if(auds->resamp_sd.data_out) free(auds->resamp_sd.data_out);
		src_delete(auds->resamp_st);
	}
	free(auds);
	return 0;
}

int auds_get_top_sample(auds_t *auds, u_int8_t **data, int *size)
{
	switch(auds->data_type){
	case AUD_TYPE_PCM:
	case AUD_TYPE_NONE:
		ERRMSG("%s:### shouldn't come here\n",__func__);
		return -1;
	}
	return -1;
}

int auds_get_next_sample(auds_t *auds, u_int8_t **data, int *size)
{
	int rval;
	auds_t *lauds=auds;
	if(auds->auds) lauds=auds->auds;
	switch(lauds->data_type){
	case AUD_TYPE_PCM:
		rval=pcm_get_next_sample(lauds, data, size);
		break;
	case AUD_TYPE_NONE:
		ERRMSG("%s:### shouldn't come here\n",__func__);
		return -1;
	}

	return rval;
}

/* return -- 1: the next sample exists, 0: it doesn't */
int auds_poll_next_sample(auds_t *auds)
{
	switch(auds->data_type){
	case AUD_TYPE_PCM:
		return pcm_poll_next_sample(auds);
	default:
		break;
	}
	return 1;
}

int auds_sigchld(auds_t *auds, siginfo_t *siginfo)
{
	if(!auds) return 0;
	if(auds->auds && auds->auds->sigchld_cb){
		auds->auds->sigchld_cb(auds->auds->stream, siginfo);
		return 0;
	}
	if(auds->sigchld_cb){
		auds->sigchld_cb(auds->stream, siginfo);
		return 0;
	}
	return 0;
}


int auds_write_pcm(auds_t *auds, u_int8_t *buffer, u_int8_t **data, int *size,
		   int bsize, data_source_t *ds)
{
	u_int8_t one[4];
	int count=0;
	int bpos=0;
	u_int8_t *bp=buffer;
	int i,nodata=0;
	int16_t *resamp=NULL, *pr=NULL;
	int channels=2;
	if(auds) channels=auds->channels;
	if(auds && auds->sample_rate != DEFAULT_SAMPLE_RATE){
		resamp=pcm_resample((int16_t*)auds->resamp_buf, auds->resamp_st,
				    &auds->resamp_sd, ds, &bsize, channels);
		if(!resamp) return -1;
		pr=resamp;
	}
	bits_write(&bp,1,3,&bpos); // channel=1, stereo
	bits_write(&bp,0,4,&bpos); // unknown
	bits_write(&bp,0,8,&bpos); // unknown
	bits_write(&bp,0,4,&bpos); // unknown
	if(bsize!=4096)
		bits_write(&bp,1,1,&bpos); // hassize
	else
		bits_write(&bp,0,1,&bpos); // hassize
	bits_write(&bp,0,2,&bpos); // unused
	bits_write(&bp,1,1,&bpos); // is-not-compressed
	if(bsize!=4096){
		bits_write(&bp,(bsize>>24)&0xff,8,&bpos); // size of data, integer, big endian
		bits_write(&bp,(bsize>>16)&0xff,8,&bpos);
		bits_write(&bp,(bsize>>8)&0xff,8,&bpos);
		bits_write(&bp,bsize&0xff,8,&bpos);
	}
	while(1){
		if(pr){
			if(channels==1)
				*((int16_t*)one)=*pr;
			else
				*((int16_t*)one)=*pr++;
			*((int16_t*)one+1)=*pr++;
		}else {
			switch(ds->type){
			case DESCRIPTOR:
				if(channels==1){
					if(read(ds->u.fd, one, 2)!=2) nodata=1;
					*((int16_t*)one+1)=*((int16_t*)one);
				}else{
					if(read(ds->u.fd, one, 4)!=4) nodata=1;
				}
				break;
			case STREAM:
				if(channels==1){
					if(fread(one,1,2,ds->u.inf)!=2) nodata=1;
					*((int16_t*)one+1)=*((int16_t*)one);
				}else{
					if(fread(one,1,4,ds->u.inf)!=4) nodata=1;
				}
				break;
			case MEMORY:
				if(channels==1){
					if(ds->u.mem.size<=count*2) nodata=1;
					*((int16_t*)one)=ds->u.mem.data[count];
					*((int16_t*)one+1)=*((int16_t*)one);
				}else{
					if(ds->u.mem.size<=count*4) nodata=1;
					*((int16_t*)one)=ds->u.mem.data[count*2];
					*((int16_t*)one+1)=ds->u.mem.data[count*2+1];
				}
				break;
			}
		}
		if(nodata) break;

		bits_write(&bp,one[1],8,&bpos);
		bits_write(&bp,one[0],8,&bpos);
		bits_write(&bp,one[3],8,&bpos);
		bits_write(&bp,one[2],8,&bpos);
		if(++count==bsize) break;
	}
	if(!count) return -1; // when no data at all, it should stop playing
	/* when readable size is less than bsize, fill 0 at the bottom */
	for(i=0;i<(bsize-count)*4;i++){
		bits_write(&bp,0,8,&bpos);
	}
	*size=bp-buffer;
	if(bpos) *size+=1;
	*data=buffer;
	return 0;
}

int aud_clac_chunk_size(int sample_rate)
{
	int bsize=MAX_SAMPLES_IN_CHUNK;
	int ratio=DEFAULT_SAMPLE_RATE*100/sample_rate;
	// to make suer the resampled size is <= 4096
	if(ratio>100) bsize=bsize*100/ratio-1;
	return bsize;
}


static data_type_t get_data_type(char *fname)
{
	int i;
	for(i=strlen(fname)-1;i>=0;i--)
		if(fname[i]=='.') break;
	if(i<0) return AUD_TYPE_PCM;
	if(i>=strlen(fname)-1) return AUD_TYPE_NONE;
	if(!strcasecmp(fname+i+1,"pcm")) return AUD_TYPE_PCM;
	return AUD_TYPE_NONE;
}

static int16_t *pcm_resample(int16_t *ps, SRC_STATE *st, SRC_DATA *sd,
			   data_source_t *ds, int *bsize, int channels)
{
	int samples, tsamples=0;


	if(!ps || !sd->data_in || !sd->data_out) return NULL;

	while(tsamples<MINIMUM_SAMPLE_SIZE && tsamples<*bsize){
		switch(ds->type){
		case DESCRIPTOR:
			samples=read(ds->u.fd,ps+channels*tsamples,*bsize*2*channels);
			samples=samples/(2*channels);
			break;
		case STREAM:
			samples=fread(ps+channels*tsamples,2,*bsize*channels,ds->u.inf);
			samples=samples/channels;
			break;
		case MEMORY:
			return NULL; // reample doesn't support MEMORY
		}
		if(samples<=0) return NULL;
		tsamples+=samples;
	}

	src_short_to_float_array (ps, sd->data_in, tsamples*channels);
	sd->input_frames=tsamples;
	sd->output_frames=MAX_SAMPLES_IN_CHUNK;
#if 0
	if(tsamples<*bsize)
		sd->end_of_input=1;
	else
		sd->end_of_input=0;
#endif
	if(src_process(st,sd)) return NULL;
	//DBGMSG("tsamples=%d, *bsize=%d, used=%ld, gen=%ld\n",tsamples, *bsize, sd->input_frames_used, sd->output_frames_gen);
	*bsize=sd->output_frames_gen;
	src_float_to_short_array(sd->data_out,ps,sd->output_frames_gen*channels);
	return ps;
}
