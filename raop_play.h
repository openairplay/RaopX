#ifndef __RAOP_PLAY_H_
#define __RAOP_PLAY_H

#define VERSION "0.0.4"

#define	RAOP_FD_READ (1<<0)
#define RAOP_FD_WRITE (1<<1)

#define RAOP_CONNECTED "Connected"
#define RAOP_SONGDONE "Done"
#define RAOP_ERROR "Error"

#define BUFFER_SECONDS 9

typedef int (*fd_callback_t)(void *, int);
int set_fd_event(int fd, int flags, fd_callback_t cbf, void *p);
int clear_fd_event(int fd);
void deleteFileIfExists (const char * fname);


#endif
