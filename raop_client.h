/*****************************************************************************
 * rtsp_client.h: RAOP Client
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
 *****************************************************************************/
#ifndef __RAOP_CLIENT_H_
#define __RAOP_CLIENT_H

typedef struct raopcl_t {u_int32_t dummy;} raopcl_t;

typedef enum pause_state_t{
	NO_PAUSE=0,
	OP_PAUSE,
	NODATA_PAUSE,
}pause_state_t;

raopcl_t *raopcl_open();
int raopcl_close(raopcl_t *p);
int raopcl_connect(raopcl_t *p, char *host,u_int16_t destport);
int raopcl_disconnect(raopcl_t *p);
int raopcl_send_sample(raopcl_t *p, u_int8_t *sample, int count );
int raopcl_update_volume(raopcl_t *p, int vol);
int raopcl_sample_remsize(raopcl_t *p);
int raopcl_set_pause(raopcl_t *p, pause_state_t pause);
pause_state_t raopcl_get_pause(raopcl_t *p);
int raopcl_flush_stream(raopcl_t *p);
int raopcl_wait_songdone(raopcl_t *p, int set);
int raopcl_small_silent(raopcl_t *p);
int raopcl_pause_check(raopcl_t *p);
int raopcl_aexbuf_time(raopcl_t *p, struct timeval *dtv);

#endif
