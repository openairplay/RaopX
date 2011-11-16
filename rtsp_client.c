/*****************************************************************************
 * rtsp_client.c: RTSP Client
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
 *****************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "aexcl_lib.h"
#include "rtsp_client.h"

#define MAX_NUM_KD 20
typedef struct rtspcl_data_t {
	int fd;
	char url[128];
	int cseq;
	key_data_t *kd;
	key_data_t *exthds;
	char *session;
	char *transport;
	u_int16_t server_port;
	struct in_addr host_addr;
	struct in_addr local_addr;
	const char *useragent;
} rtspcl_data_t;

static int exec_request(rtspcl_data_t *rtspcld, char *cmd, char *content_type,
			 char *content, int get_response, key_data_t *hds, key_data_t **kd);

rtspcl_t *rtspcl_open()
{
	rtspcl_data_t *rtspcld;
	rtspcld=malloc(sizeof(rtspcl_data_t));
	memset(rtspcld, 0, sizeof(rtspcl_data_t));
	rtspcld->useragent="RTSPClient";
	return (rtspcl_t *)rtspcld;
}

int rtspcl_close(rtspcl_t *p)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	rtspcl_disconnect(p);
	free(rtspcld);
	p=NULL;
	rtspcl_remove_all_exthds(p);
	return 0;
}

u_int16_t rtspcl_get_server_port(rtspcl_t *p)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	return rtspcld->server_port;
}

int rtspcl_set_useragent(rtspcl_t *p, const char *name)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	rtspcld->useragent=name;
	return 0;
}

int rtspcl_add_exthds(rtspcl_t *p, char *key, char *data)
{
	int i=0;
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	if(!rtspcld->exthds){
		if(realloc_memory((void*)&rtspcld->exthds, 17*sizeof(key_data_t),__func__)) return -1;
	}else{
		i=0;
		while(rtspcld->exthds[i].key) {
			if(rtspcld->exthds[i].key[0]==0xff) break;
			i++;
		}
		if(i && i%16==0 && rtspcld->exthds[i].key[0]!=0xff){
			if(realloc_memory((void*)&rtspcld->exthds,(16*((i%16)+1)+1)*sizeof(key_data_t),__func__))
				return -1;
			memset(rtspcld->exthds+16*(i/16),0,17*sizeof(key_data_t));
		}
	}
	if(realloc_memory((void*)&rtspcld->exthds[i].key,strlen(key),__func__)) return -1;
	strcpy((char*)rtspcld->exthds[i].key,key);
	if(realloc_memory((void*)&rtspcld->exthds[i].data,strlen(data),__func__)) return -1;
	strcpy((char*)rtspcld->exthds[i].data,data);
	rtspcld->exthds[i+1].key=NULL;
	return 0;
}

int rtspcl_mark_del_exthds(rtspcl_t *p, char *key)
{
	int i=0;
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	if(!rtspcld->exthds) return -1;
	while(rtspcld->exthds[i].key) {
		if(!strcmp((char*)key,(char*)rtspcld->exthds[i].key)){
			rtspcld->exthds[i].key[0]=0xff;
			return 0;
		}
		i++;
	}
	return -1;
}

int rtspcl_remove_all_exthds(rtspcl_t *p)
{
	int i=0;
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	while(rtspcld->exthds && rtspcld->exthds[i].key) {
		free(rtspcld->exthds[i].key);
		free(rtspcld->exthds[i].data);
	}
	free(rtspcld->exthds);
	rtspcld->exthds=NULL;
	return 0;
}

int rtspcl_connect(rtspcl_t *p, char *host, u_int16_t destport, char *sid)
{
	u_int16_t myport=0;
	struct sockaddr_in name;
	socklen_t namelen=sizeof(name);
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	if((rtspcld->fd=open_tcp_socket(NULL, &myport))==-1) return -1;
	if(get_tcp_connect_by_host(rtspcld->fd, host, destport)) return -1;
	getsockname(rtspcld->fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->local_addr,&name.sin_addr,sizeof(struct in_addr));
	sprintf(rtspcld->url,"rtsp://%s/%s",inet_ntoa(name.sin_addr),sid);
	getpeername(rtspcld->fd, (struct sockaddr*)&name, &namelen);
	memcpy(&rtspcld->host_addr,&name.sin_addr,sizeof(struct in_addr));
	return 0;
}

char* rtspcl_local_ip(rtspcl_t *p)
{
	rtspcl_data_t *rtspcld;

	if(!p) return NULL;
	rtspcld=(rtspcl_data_t *)p;
	return inet_ntoa(rtspcld->local_addr);
}


int rtspcl_disconnect(rtspcl_t *p)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	if(rtspcld->fd>0) close(rtspcld->fd);
	rtspcld->fd=0;
	return 0;
}

int rtspcl_annouce_sdp(rtspcl_t *p, char *sdp)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	return exec_request(rtspcld, "ANNOUNCE", "application/sdp", sdp, 1, NULL, &rtspcld->kd);
}

int rtspcl_setup(rtspcl_t *p, key_data_t **kd)
{
	key_data_t *rkd=NULL;
	key_data_t hds[2];
	const char delimiters[] = ";";
	char *buf=NULL;
	char *token,*pc;
	int rval=-1;
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	hds[0].key=(u_int8_t*)"Transport";
	hds[0].data=(u_int8_t*)"RTP/AVP/TCP;unicast;interleaved=0-1;mode=record";
	hds[1].key=NULL;
	if(exec_request(rtspcld, "SETUP", NULL, NULL, 1, hds, &rkd)) return -1;
	if(!(rtspcld->session=kd_lookup(rkd, "Session"))){
		ERRMSG("%s: no session in responce\n",__func__);
		goto erexit;
	}
	if(!(rtspcld->transport=kd_lookup(rkd, "Transport"))){
		ERRMSG("%s: no transport in responce\n",__func__);
		goto erexit;
	}
	if(realloc_memory((void*)&buf,strlen(rtspcld->transport)+1,__func__)) goto erexit;
	strcpy(buf,rtspcld->transport);
	token=strtok(buf,delimiters);
	rtspcld->server_port=0;
	while(token){
		if((pc=strstr(token,"="))){
			*pc=0;
			if(!strcmp(token,"server_port")){
				rtspcld->server_port=atoi(pc+1);
				break;
			}
		}
		token=strtok(NULL,delimiters);
	}
	if(rtspcld->server_port==0){
		ERRMSG("%s: no server_port in responce\n",__func__);
		goto erexit;
	}
	rval=0;
 erexit:
	if(buf) free(buf);
	if(rval) {
		free_kd(rkd);
		rkd=NULL;
	}
	*kd=rkd;
	return rval;
}

int rtspcl_record(rtspcl_t *p)
{
	key_data_t hds[3];
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	if(!rtspcld->session){
		ERRMSG("%s: no session in progress\n",__func__);
		return -1;
	}
	hds[0].key=(u_int8_t*)"Range";
	hds[0].data=(u_int8_t*)"npt=0-";
        hds[1].key=(u_int8_t*)"RTP-Info";
	hds[1].data=(u_int8_t*)"seq=0;rtptime=0";
	hds[2].key=NULL;
	return exec_request(rtspcld,"RECORD",NULL,NULL,1,hds,&rtspcld->kd);
}

int rtspcl_set_parameter(rtspcl_t *p, char *para)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	return exec_request(rtspcld, "SET_PARAMETER", "text/parameters", para, 1, NULL, &rtspcld->kd);
}

int rtspcl_flush(rtspcl_t *p)
{
	key_data_t hds[2];
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	hds[0].key=(u_int8_t*)"RTP-Info";
	hds[0].data=(u_int8_t*)"seq=0;rtptime=0";
	hds[1].key=NULL;
	return exec_request(rtspcld, "FLUSH", NULL, NULL, 1, hds, &rtspcld->kd);
}

int rtspcl_teardown(rtspcl_t *p)
{
	rtspcl_data_t *rtspcld;

	if(!p) return -1;
	rtspcld=(rtspcl_data_t *)p;
	return exec_request(rtspcld, "TEARDOWN", NULL, NULL, 0, NULL, &rtspcld->kd);
}

/*
 * send RTSP request, and get responce if it's needed
 * if this gets a success, *kd is allocated or reallocated (if *kd is not NULL)
 */
static int exec_request(rtspcl_data_t *rtspcld, char *cmd, char *content_type,
				char *content, int get_response, key_data_t *hds, key_data_t **kd)
{
        char line[1024];
	char req[1024];
	char reql[128];
	const char delimiters[] = " ";
	char *token,*dp;
	int i,j,dsize,rval;
	int timeout=5000; // msec unit


	if(!rtspcld) return -1;

        sprintf(req, "%s %s RTSP/1.0\r\nCSeq: %d\r\n",cmd,rtspcld->url,++rtspcld->cseq );

        if( rtspcld->session != NULL ){
		sprintf(reql,"Session: %s\r\n", rtspcld->session );
		strncat(req,reql,sizeof(req));
	}
	i=0;
        while( hds && hds[i].key != NULL ){
		sprintf(reql,"%s: %s\r\n", hds[i].key, hds[i].data);
		strncat(req,reql,sizeof(req));
		i++;
        }

        if( content_type && content) {
		sprintf(reql, "Content-Type: %s\r\nContent-Length: %d\r\n",
			content_type, (int)strlen(content));
		strncat(req,reql,sizeof(req));
        }

        sprintf(reql, "User-Agent: %s\r\n", rtspcld->useragent );
	strncat(req,reql,sizeof(req));

	i=0;
        while(rtspcld->exthds && rtspcld->exthds[i].key){
		if(rtspcld->exthds[i].key[0]==0xff) {i++;continue;}
		sprintf(reql,"%s: %s\r\n", rtspcld->exthds[i].key, rtspcld->exthds[i].data);
		strncat(req,reql,sizeof(req));
		i++;
        }
	strncat(req,"\r\n",sizeof(req));

        if( content_type && content)
		strncat(req,content,sizeof(req));

	rval=write(rtspcld->fd,req,strlen(req));
	//DBGMSG("%s: write %d: %d \n",__func__, strlen(req),rval);

        if( !get_response ) return 0;

	if(read_line(rtspcld->fd,line,sizeof(line),timeout,0)<=0)	{
		ERRMSG("%s: request failed\n",__func__);
		return -1;
	}

	token = strtok(line, delimiters);
	token = strtok(NULL, delimiters);
	if(token==NULL || strcmp(token,"200")){
		ERRMSG("%s: request failed, error %s\n",__func__,token);
		return -1;
	}

	i=0;
	while(read_line(rtspcld->fd,line,sizeof(line),timeout,0)>0){
		//DBGMSG("%s\n",line);
		timeout=1000; // once it started, it shouldn't take a long time
		if(i%16==0){
			if(realloc_memory((void*)kd,(16*(i/16+1)+1)*sizeof(key_data_t),__func__)) return -1;
			memset(*kd+16*(i/16),0,17*sizeof(key_data_t));
		}

		if(i && line[0]==' '){
			for(j=0;j<strlen(line);j++) if(line[j]!=' ') break;
			dsize+=strlen(line+j);
			if(realloc_memory((void*)&(*kd)[i].data,dsize,__func__)) return -1;
			strcat((char*)(*kd)[i].data,line+j);
			continue;
		}
		dp=strstr(line,":");
		if(!dp){
			ERRMSG("%s: Request failed, bad header\n",__func__);
			free_kd(*kd);
			*kd=NULL;
			return -1;
		}
		*dp=0;
		if(realloc_memory((void*)&(*kd)[i].key,strlen(line)+1,__func__)) return -1;
		strcpy((char*)(*kd)[i].key,line);
		dsize=strlen(dp+1)+1;
		if(realloc_memory((void*)&(*kd)[i].data,dsize,__func__)) return -1;
		strcpy((char*)(*kd)[i].data,dp+1);
		i++;
	}
	(*kd)[i].key=NULL;
	return 0;
}


