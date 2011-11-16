#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>

#ifndef __AEXCL_LIB_H_
#define __AEXCL_LIB_H_

#ifdef USE_SYSLOG
#include <syslog.h>
#define ERRMSG(args...) syslog(LOG_ERR, args)
#define INFMSG(args...) syslog(LOG_INFO, args)
#define DBGMSG(args...) syslog(LOG_DEBUG, args)
#else
#define ERRMSG(args...) fprintf(stderr,"ERR: " args)
#define INFMSG(args...) fprintf(stderr,"INFO: " args)
#define DBGMSG(args...) fprintf(stderr,"DBG: " args)
#endif

#define SLEEP_SEC(val) sleep(val)
#define SLEEP_MSEC(val) usleep(val*1000)
#define SLEEP_USEC(val) usleep(val)

#undef BEGIN_C_DECLS
#undef END_C_DECLS
#ifdef __cplusplus
#define BEGIN_C_DECLS extern "C" {
#define END_C_DECLS }
#else
#define BEGIN_C_DECLS
#define END_C_DECLS
#endif


BEGIN_C_DECLS

static inline int get_10munit_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec&0x00FFFFFF)*100+tv.tv_usec/10000;
}

static inline int lf_to_null(char *sline, int linelen)
{
	int i;
	for(i=0;i<linelen;i++) {
		if(sline[i]=='\n') {
			sline[i]=0;
			return i;
		}else if(sline[i]==0){
			return i;
		}
	}
	return -1;
}


/*
 * if newsize < 4096, align the size to power of 2
 */
static inline int realloc_memory(void **p, int newsize, const char *func)
{
	void *np;
	int i=0;
	int n=16;
	for(i=0;i<8;i++){
		if(newsize<=n){
			newsize=n;
			break;
		}
		n=n<<1;
	}
	newsize=newsize;
	np=realloc(*p,newsize);
	if(!np){
		ERRMSG("%s: realloc failed: %s\n",func,strerror(errno));
		return -1;
	}
	*p=np;
	return 0;
}

#define GET_BIGENDIAN_INT(x) (*(u_int8_t*)(x)<<24)|(*((u_int8_t*)(x)+1)<<16)|(*((u_int8_t*)(x)+2)<<8)|(*((u_int8_t*)(x)+3))

typedef struct key_data_t {
	u_int8_t *key;
	u_int8_t *data;
}key_data_t;


int open_tcp_socket(char *hostname, unsigned short *port);
int open_udp_socket(char *hostname, unsigned short *port);
int get_tcp_connect_by_host(int sd, char *host, u_int16_t destport);
int get_tcp_connect(int sd, struct sockaddr_in dest_addr);
int bind_host(int sd, char *hostname, unsigned long ulAddr,unsigned short *port);
int read_line(int fd, char *line, int maxlen, int timeout, int no_poll);
char *kd_lookup(key_data_t *kd, char *key);
void free_kd(key_data_t *kd);
int remove_char_from_string(char *str, char rc);
int child_start(char* const argv[], int *infd, int *outfd, int *errfd);

END_C_DECLS

#endif
