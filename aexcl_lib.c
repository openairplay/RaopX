/*****************************************************************************
 * socket interface library
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

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>
#include "aexcl_lib.h"


/*
 * open tcp port
 */
int open_tcp_socket(char *hostname, unsigned short *port)
{
	int sd;

	/* socket creation */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd<0) {
		ERRMSG("cannot create tcp socket\n");
		return -1;
	}
	if(bind_host(sd, hostname,0, port)) {
		close(sd);
		return -1;
	}

	return sd;
}

/*
 * create tcp connection
 * as long as the socket is not non-blocking, this can block the process
 * nsport is network byte order
 */
int get_tcp_connect(int sd, struct sockaddr_in dest_addr)
{

	if(connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))){
		SLEEP_MSEC(100L);
		// try one more time
		if(connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))){
			ERRMSG("error:get_tcp_nconnect addr=%s, port=%d\n",
			       inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
			return -1;
		}
	}
	return 0;
}


int get_tcp_connect_by_host(int sd, char *host, u_int16_t destport)
{
	struct sockaddr_in addr;
	struct hostent *h;

	h = gethostbyname(host);
	if(h) {
		addr.sin_family = h->h_addrtype;
		memcpy((char *) &addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	}else{
		addr.sin_family = AF_INET;
		if((addr.sin_addr.s_addr=inet_addr(host))==0xFFFFFFFF){
			ERRMSG("gethostbyname: '%s' \n", host);
			return -1;
		}
	}
	addr.sin_port=htons(destport);

	return get_tcp_connect(sd, addr);
}

/* bind an opened socket to specified hostname and port.
 * if hostname=NULL, use INADDR_ANY.
 * if *port=0, use dynamically assigned port
 */
int bind_host(int sd, char *hostname, unsigned long ulAddr,unsigned short *port)
{
	struct sockaddr_in my_addr;
	socklen_t nlen=sizeof(struct sockaddr);
	struct hostent *h;

	memset(&my_addr, 0, sizeof(my_addr));
	/* use specified hostname */
	if(hostname){
		/* get server IP address (no check if input is IP address or DNS name */
		h = gethostbyname(hostname);
		if(h==NULL) {
			if(strstr(hostname, "255.255.255.255")==hostname){
				my_addr.sin_addr.s_addr=-1;
			}else{
				if((my_addr.sin_addr.s_addr=inet_addr(hostname))==0xFFFFFFFF){
					ERRMSG("gethostbyname: '%s' \n", hostname);
					return -1;
				}
			}
			my_addr.sin_family = AF_INET;
		}else{
			my_addr.sin_family = h->h_addrtype;
			memcpy((char *) &my_addr.sin_addr.s_addr,
			       h->h_addr_list[0], h->h_length);
		}
	} else {
		// if hostname=NULL, use INADDR_ANY
		if(ulAddr)
			my_addr.sin_addr.s_addr = ulAddr;
		else
			my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		my_addr.sin_family = AF_INET;
	}

	/* bind a specified port */
	my_addr.sin_port = htons(*port);

	if(bind(sd, (struct sockaddr *) &my_addr, sizeof(my_addr))<0){
		ERRMSG("bind error: %s\n", strerror(errno));
		return -1;
	}

	if(*port==0){
		getsockname(sd, (struct sockaddr *) &my_addr, &nlen);
		*port=ntohs(my_addr.sin_port);
	}

	return 0;
}

/*
 * read one line from the file descriptor
 * timeout: msec unit, -1 for infinite
 * if CR comes then following LF is expected
 * returned string in line is always null terminated, maxlen-1 is maximum string length
 */
int read_line(int fd, char *line, int maxlen, int timeout, int no_poll)
{
	int i,rval;
	int count=0;
	struct pollfd pfds={events:POLLIN};
	char ch;
	*line=0;
	pfds.fd=fd;
	for(i=0;i<maxlen;i++){
		if(no_poll || poll(&pfds, 1, timeout))
			rval=read(fd,&ch,1);
		else return 0;

		if(rval==-1){
			if(errno==EAGAIN) return 0;
			ERRMSG("%s:read error: %s\n", __func__, strerror(errno));
			return -1;
		}
		if(rval==0){
			INFMSG("%s:disconnected on the other end\n", __func__);
			return -1;
		}
		if(ch=='\n'){
			*line=0;
			return count;
		}
		if(ch=='\r') continue;
		*line++=ch;
		count++;
		if(count>=maxlen-1) break;
	}
	*line=0;
	return count;
}

/*
 * key_data type data look up
 */
char *kd_lookup(key_data_t *kd, char *key)
{
	int i=0;
	while(kd && kd[i].key){
		if(!strcmp((char*)kd[i].key,key)) return (char*)kd[i].data;
		i++;
	}
	return NULL;
}

void free_kd(key_data_t *kd)
{
	int i=0;
	while(kd && kd[i].key){
		free(kd[i].key);
		if(kd[i].data) free(kd[i].data);
		i++;
	}
	free(kd);
}

/*
 * remove one character from a string
 * return the number of deleted characters
 */
int remove_char_from_string(char *str, char rc)
{
	int i=0,j=0,len;
	int num=0;
	len=strlen(str);
	while(i<len){
		if(str[i]==rc){
			for(j=i;j<len;j++) str[j]=str[j+1];
			len--;
			num++;
		}else{
			i++;
		}
	}
	return num;
}

/*
 * invoke a new child process
 */
int child_start(char* const argv[], int *infd, int *outfd, int *errfd)
{
	int infds[2];
	int errfds[2];
	int outfds[2];
	int pid;
	pipe(infds);
	pipe(outfds);
	pipe(errfds);
	pid=fork();
	if(pid==0){
		// this is a child
		if(infd){
			dup2(infds[1],1);// copy output pipe to standard output
		}else{
			close(infds[1]);
		}
		close(infds[0]);

		if(errfd){
			dup2(errfds[1],2);// copy output pipe to standard error
		}else{
			close(errfds[1]);
		}
		close(infds[0]);

		if(outfd){
			dup2(outfds[0],0);// copy input pipe to standard input
		}else{
			close(outfds[0]);
		}
		close(outfds[1]);

		execvp(argv[0], argv);
		exit(0);
	}

	if(infd)
		*infd=infds[0];
	else
		close(infds[0]);
	close(infds[1]);//close output pipe

	if(errfd)
		*errfd=errfds[0];
	else
		close(errfds[0]);
	close(errfds[1]);//close output pipe

	if(outfd)
		*outfd=outfds[1];
	else
		close(outfds[1]);
	close(outfds[0]);// close read pipe

	return pid;
}


#if 0 // for debugging
void hex_dump(unsigned char *buf, int size, int addr)
{
	int i;
	int p=0;
	while(size){
		fprintf(stderr,"%08X - ", addr);
		for(i=addr&0xf;i>0;i--) fprintf(stderr,"   ");
		if((addr&0xf)>0x8) fprintf(stderr," ");
		for(i=addr&0xf;i<0x10;i++){
			if(i==0x8) fprintf(stderr," ");
			fprintf(stderr,"%02x ", buf[p++]);
			if(! --size) break;
		}
		addr=(addr+0x10)&~0xf;
		fprintf(stderr,"\n");
	}
}
#endif
