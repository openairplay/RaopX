/*****************************************************************************
 * wav_stream.c: wave file stream
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

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/poll.h>
#include <AudioToolbox/AudioToolbox.h>
#define PCM_STREAM_C_
#include "audio_stream.h"
#include "pcm_stream.h"
#include "aexcl_lib.h"
#include "raop_play.h"


int pcm_open(auds_t *auds, char *fname)
{
	pcm_t *pcm=malloc(sizeof(pcm_t));
	if(!pcm) return -1;
	memset(pcm,0,sizeof(pcm_t));
	auds->stream=(void *)pcm;
	pcm->dfd=open(fname, O_RDONLY|O_NONBLOCK);
	if(pcm->dfd<0) goto erexit;
	auds->sample_rate=DEFAULT_SAMPLE_RATE;
	auds->chunk_size=aud_clac_chunk_size(auds->sample_rate);
	pcm->buffer=(u_int8_t *)malloc(MAX_SAMPLES_IN_CHUNK*4+16);
	if(!pcm->buffer) goto erexit;
	return 0;
 erexit:
	pcm_close(auds);
	return -1;
}

int pcm_close(auds_t *auds)
{
	pcm_t *pcm=(pcm_t *)auds->stream;
	if(!pcm) return -1;
	if(pcm->dfd>=0) close(pcm->dfd);
	if(pcm->buffer) free(pcm->buffer);
	free(pcm);
	auds->stream=NULL;
	return 0;
}

int pcm_get_next_sample(auds_t *auds, u_int8_t **data, int *size)
{
	int rval;
	pcm_t *pcm=(pcm_t *)auds->stream;
	int bsize=auds->chunk_size;
	data_source_t ds={.type=MEMORY};
	u_int8_t *rbuf=NULL;
	if(!(rbuf=(u_int8_t*)malloc(bsize*4))) return -1;
	memset(rbuf,0,bsize*4);
	/* ds.u.mem.size, ds.u.mem.data */
	ds.u.mem.size=read(pcm->dfd,rbuf,bsize*4);
	if(ds.u.mem.size<0){
		//ERRMSG("%s: data read error(%s)\n", __func__, strerror(errno));
		return -1;
	}
	if(ds.u.mem.size<MINIMUM_SAMPLE_SIZE*4){
		//INFMSG("%s: too small chunk of data(size=%d), add null data\n",__func__, ds.u.mem.size);
		ds.u.mem.size=MINIMUM_SAMPLE_SIZE*4;
	}
	ds.u.mem.data=(int16_t*)rbuf;
	bsize=ds.u.mem.size/4;
	rval=auds_write_pcm(auds, pcm->buffer, data, size, bsize, &ds);
	free(rbuf);
	return rval;
}


int pcm_poll_next_sample(auds_t *auds)
{
	struct  pollfd pfds[1];
	pcm_t *pcm=(pcm_t *)auds->stream;
	pfds[0].fd=pcm->dfd;
	pfds[0].events=POLLIN;
	poll(pfds,1,0);
	//DBGMSG("%s:revents=0x%x\n",__func__,pfds[0].revents);
	if(!pfds[0].revents)
		set_fd_event(pcm->dfd, RAOP_FD_READ, NULL, NULL);
	else
		clear_fd_event(pcm->dfd);

	return pfds[0].revents&POLLIN;
}
