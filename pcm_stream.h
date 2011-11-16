/*****************************************************************************
 * pcm_stream.h: pcme file stream, header file
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
 *****************************************************************************/
#ifndef __PCM_STREAM_H_
#define __PCM_STREAM_H_

typedef struct pcm_t {
/* public variables */
/* private variables */
#ifdef PCM_STREAM_C_
	int dfd;
	u_int8_t *buffer;
#else
	u_int32_t dummy;
#endif
} pcm_t;


int pcm_open(auds_t *auds, char *fname);
int pcm_close(auds_t *auds);
int pcm_get_next_sample(auds_t *auds, u_int8_t **data, int *size);
int pcm_poll_next_sample(auds_t *auds);

#endif
