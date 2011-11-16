/*****************************************************************************
 * rtsp_client.h: RTSP Client
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

#ifndef __RTSP_CLIENT_H_
#define __RTSP_CLIENT_H

typedef struct rtspcl_t {u_int32_t dummy;} rtspcl_t;


rtspcl_t *rtspcl_open();
int rtspcl_close(rtspcl_t *p);
int rtspcl_set_useragent(rtspcl_t *p, const char *name);
int rtspcl_connect(rtspcl_t *p, char *host, u_int16_t destport, char *sid);
char* rtspcl_local_ip(rtspcl_t *p);
int rtspcl_disconnect(rtspcl_t *p);
int rtspcl_annouce_sdp(rtspcl_t *p, char *sdp);
int rtspcl_setup(rtspcl_t *p, key_data_t **kd);
int rtspcl_record(rtspcl_t *p);
int rtspcl_set_parameter(rtspcl_t *p, char *para);
int rtspcl_flush(rtspcl_t *p);
int rtspcl_teardown(rtspcl_t *p);
int rtspcl_remove_all_exthds(rtspcl_t *p);
int rtspcl_add_exthds(rtspcl_t *p, char *key, char *data);
int rtspcl_mark_del_exthds(rtspcl_t *p, char *key);
u_int16_t rtspcl_get_server_port(rtspcl_t *p);


#endif
