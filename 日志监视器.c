



#ifndef _UART_H_
#define _UART_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>
typedef unsigned char byte_t;

/* serial_port is declared as a void*, which you should cast to whatever type
 * makes sense to your connection method. Both the posix and win32
 * implementations define their own structs in place.
 */
typedef void* serial_port;

/* Returned by uart_open if the serial port specified was invalid.
 */
#define INVALID_SERIAL_PORT (void*)(~1)

/* Returned by uart_open if the serial port specified is in use by another
 * process.
 */
#define CLAIMED_SERIAL_PORT (void*)(~2)

/* Given a user-specified port name, connect to the port and return a structure
 * used for future references to that port.
 *
 * On errors, this method returns INVALID_SERIAL_PORT or CLAIMED_SERIAL_PORT.
 */
serial_port uart_open(const char* pcPortName);

/* Closes the given port.
 */
void uart_close(const serial_port sp);

/* Reads from the given serial port for up to 30ms.
 *   pbtRx: A pointer to a buffer for the returned data to be written to.
 *   pszMaxRxLen: The maximum data size we want to be sent.
 *   pszRxLen: The number of bytes that we were actually sent.
 *
 * Returns TRUE if any data was fetched, even if it was less than pszMaxRxLen.
 *
 * Returns FALSE if there was an error reading from the device. Note that a
 * partial read may have completed into the buffer by the corresponding
 * implementation, so pszRxLen should be checked to see if any data was written. 
 */
bool uart_receive(const serial_port sp, byte_t* pbtRx, size_t pszMaxRxLen, size_t* pszRxLen);

/* Sends a buffer to a given serial port.
 *   pbtTx: A pointer to a buffer containing the data to send.
 *   len: The amount of data to be sent.
 */
bool uart_send(const serial_port sp, const byte_t* pbtTx, const size_t len);

/* Sets the current speed of the serial port, in baud.
 */
bool uart_set_speed(serial_port sp, const uint32_t uiPortSpeed);

/* Gets the current speed of the serial port, in baud.
 */
uint32_t uart_get_speed(const serial_port sp);

#endif // _UART_H_




/*
 * Generic uart / rs232/ serial port library
 *
 * Copyright (c) 2013, Roel Verdult
 * Copyright (c) 2018 Google
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file uart_posix.c
 *
 * This version of the library has functionality removed which was not used by
 * proxmark3 project.
 */

// Test if we are dealing with posix operating systems



#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

typedef struct termios term_info;
typedef struct {
  int fd;           // Serial port file descriptor
  term_info tiOld;  // Terminal info before using the port
  term_info tiNew;  // Terminal info during the transaction
} serial_port_unix;

// Set time-out on 30 miliseconds
struct timeval timeout = {
  .tv_sec  =     0, // 0 second
  .tv_usec = 30000  // 30000 micro seconds
};

serial_port uart_open(const char* pcPortName)
{
  serial_port_unix* sp = malloc(sizeof(serial_port_unix));
  if (sp == 0) return INVALID_SERIAL_PORT;
  
  if (memcmp(pcPortName, "tcp:", 4) == 0) {
    struct addrinfo *addr = NULL, *rp;
    char *addrstr = strdup(pcPortName + 4);
    if (addrstr == NULL) {
      printf("Error: strdup\n");
      return INVALID_SERIAL_PORT;
    }
    char *colon = strrchr(addrstr, ':');
    char *portstr;

    // Set time-out to 300 miliseconds only for TCP port
    timeout.tv_usec = 300000;

if (colon) {
      portstr = colon + 1;
      *colon = '\0';
    } else
      portstr = "7901";

    struct addrinfo info;

    memset (&info, 0, sizeof(info));

    info.ai_socktype = SOCK_STREAM;

    int s = getaddrinfo(addrstr, portstr, &info, &addr);
    if (s != 0) {
      printf("Error: getaddrinfo: %s\n", gai_strerror(s));
      return INVALID_SERIAL_PORT;
    }

    int sfd;
    for (rp = addr; rp != NULL; rp = rp->ai_next) {
      sfd = socket(rp->ai_family, rp->ai_socktype,
		   rp->ai_protocol);
      if (sfd == -1)
	continue;

      if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
	break;

      close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
      printf("Error: Could not connect\n");
      return INVALID_SERIAL_PORT;
    }

    freeaddrinfo(addr);
    free(addrstr);

    sp->fd = sfd;

    int one = 1;
    setsockopt(sp->fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
    return sp;
  }

  sp->fd = open(pcPortName, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
  if(sp->fd == -1) {
    uart_close(sp);
    return INVALID_SERIAL_PORT;
  }

  // Finally figured out a way to claim a serial port interface under unix
  // We just try to set a (advisory) lock on the file descriptor
  struct flock fl;
  fl.l_type   = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start  = 0;
  fl.l_len    = 0;
  fl.l_pid    = getpid();
  
  // Does the system allows us to place a lock on this file descriptor
  if (fcntl(sp->fd, F_SETLK, &fl) == -1) {
    // A conflicting lock is held by another process
    free(sp);
    return CLAIMED_SERIAL_PORT;
  }

  // Try to retrieve the old (current) terminal info struct
  if(tcgetattr(sp->fd,&sp->tiOld) == -1) {
    uart_close(sp);
    return INVALID_SERIAL_PORT;
  }
  
  // Duplicate the (old) terminal info struct
  sp->tiNew = sp->tiOld;
  
  // Configure the serial port
  sp->tiNew.c_cflag = CS8 | CLOCAL | CREAD;
  sp->tiNew.c_iflag = IGNPAR;
  sp->tiNew.c_oflag = 0;
  sp->tiNew.c_lflag = 0;
    
  // Block until n bytes are received
  sp->tiNew.c_cc[VMIN] = 0;
  // Block until a timer expires (n * 100 mSec.)
  sp->tiNew.c_cc[VTIME] = 0;
  
  // Try to set the new terminal info struct
  if(tcsetattr(sp->fd, TCSANOW, &sp->tiNew) == -1) {
    uart_close(sp);
    return INVALID_SERIAL_PORT;
  }
  
  // Flush all lingering data that may exist
  tcflush(sp->fd, TCIOFLUSH);

  // set speed, works for UBUNTU 14.04
  bool err = uart_set_speed(sp, 460800);
  if (!err)
	  uart_set_speed(sp, 115200);  		  
  return sp;
}

void uart_close(const serial_port sp) {
  serial_port_unix* spu = (serial_port_unix*)sp;
  tcflush(spu->fd, TCIOFLUSH);
  tcsetattr(spu->fd, TCSANOW, &(spu->tiOld));
  struct flock fl;
  fl.l_type   = F_UNLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start  = 0;
  fl.l_len    = 0;
  fl.l_pid    = getpid();

  // Does the system allows us to place a lock on this file descriptor
  int err = fcntl(spu->fd, F_SETLK, &fl);
  if ( err == -1) {
     //perror("fcntl");
  }  
  close(spu->fd);
  free(sp);
}

bool uart_receive(const serial_port sp, byte_t* pbtRx, size_t pszMaxRxLen, size_t* pszRxLen) {
  int res;
  int byteCount;
  fd_set rfds;
  struct timeval tv;
  
  // Reset the output count
  *pszRxLen = 0;
  
  do {
    // Reset file descriptor
    FD_ZERO(&rfds);
    FD_SET(((serial_port_unix*)sp)->fd,&rfds);
    tv = timeout;
    res = select(((serial_port_unix*)sp)->fd+1, &rfds, NULL, NULL, &tv);
    
    // Read error
    if (res < 0) {
      return false;
    }
 
    // Read time-out
    if (res == 0) {
      if (*pszRxLen == 0) {
        // Error, we received no data
        return false;
      } else {
        // We received some data, but nothing more is available
        return true;
      }
    }
 
    // Retrieve the count of the incoming bytes
    res = ioctl(((serial_port_unix*)sp)->fd, FIONREAD, &byteCount);
    if (res < 0) return false;
    
    // Cap the number of bytes, so we don't overrun the buffer
    if (pszMaxRxLen - (*pszRxLen) < byteCount) {
    	byteCount = pszMaxRxLen - (*pszRxLen);
    }

    // There is something available, read the data
    res = read(((serial_port_unix*)sp)->fd, pbtRx+(*pszRxLen), byteCount);

    // Stop if the OS has some troubles reading the data
    if (res <= 0) return false;
 
    *pszRxLen += res;

    if (*pszRxLen == pszMaxRxLen) {
    	// We have all the data we wanted.
      return true;
    }
    
  } while (byteCount);

  return true;
}

bool uart_send(const serial_port sp, const byte_t* pbtTx, const size_t len) {
  int32_t res;
  size_t pos = 0;
  fd_set rfds;
  struct timeval tv;

  while (pos < len) {
    // Reset file descriptor
    FD_ZERO(&rfds);
    FD_SET(((serial_port_unix*)sp)->fd, &rfds);
    tv = timeout;
    res = select(((serial_port_unix*)sp)->fd+1, NULL, &rfds, NULL, &tv);
    
    // Write error
    if (res < 0) {
		printf("UART:: write error (%d)\n", res);
		return false;
    }
    
    // Write time-out
    if (res == 0) {
		printf("UART:: write time-out\n");
		return false;
    }
    
    // Send away the bytes
    res = write(((serial_port_unix*)sp)->fd, pbtTx+pos, len-pos);
    
    // Stop if the OS has some troubles sending the data
    if (res <= 0) {
		printf("UART:: os troubles (%d)\n", res);
		return false;
	}
    
    pos += res;
  }
  return true;
}

bool uart_set_speed(serial_port sp, const uint32_t uiPortSpeed) {
  const serial_port_unix* spu = (serial_port_unix*)sp;
  speed_t stPortSpeed;
  switch (uiPortSpeed) {
    case 0: stPortSpeed = B0; break;
    case 50: stPortSpeed = B50; break;
    case 75: stPortSpeed = B75; break;
    case 110: stPortSpeed = B110; break;
    case 134: stPortSpeed = B134; break;
    case 150: stPortSpeed = B150; break;
    case 300: stPortSpeed = B300; break;
    case 600: stPortSpeed = B600; break;
    case 1200: stPortSpeed = B1200; break;
    case 1800: stPortSpeed = B1800; break;
    case 2400: stPortSpeed = B2400; break;
    case 4800: stPortSpeed = B4800; break;
    case 9600: stPortSpeed = B9600; break;
    case 19200: stPortSpeed = B19200; break;
    case 38400: stPortSpeed = B38400; break;
#  ifdef B57600
    case 57600: stPortSpeed = B57600; break;
#  endif
#  ifdef B115200
    case 115200: stPortSpeed = B115200; break;
#  endif
#  ifdef B230400
    case 230400: stPortSpeed = B230400; break;
#  endif
#  ifdef B460800
    case 460800: stPortSpeed = B460800; break;
#  endif
#  ifdef B921600
    case 921600: stPortSpeed = B921600; break;
#  endif
    default: return false;
  };
  struct termios ti;
  if (tcgetattr(spu->fd,&ti) == -1) return false;
  // Set port speed (Input and Output)
  cfsetispeed(&ti, stPortSpeed);
  cfsetospeed(&ti, stPortSpeed);
  return (tcsetattr(spu->fd, TCSANOW, &ti) != -1);
}

uint32_t uart_get_speed(const serial_port sp) {
  struct termios ti;
  uint32_t uiPortSpeed;
  const serial_port_unix* spu = (serial_port_unix*)sp;
  if (tcgetattr(spu->fd, &ti) == -1) return 0;
  // Set port speed (Input)
  speed_t stPortSpeed = cfgetispeed(&ti);
  switch (stPortSpeed) {
    case B0: uiPortSpeed = 0; break;
    case B50: uiPortSpeed = 50; break;
    case B75: uiPortSpeed = 75; break;
    case B110: uiPortSpeed = 110; break;
    case B134: uiPortSpeed = 134; break;
    case B150: uiPortSpeed = 150; break;
    case B300: uiPortSpeed = 300; break;
    case B600: uiPortSpeed = 600; break;
    case B1200: uiPortSpeed = 1200; break;
    case B1800: uiPortSpeed = 1800; break;
    case B2400: uiPortSpeed = 2400; break;
    case B4800: uiPortSpeed = 4800; break;
    case B9600: uiPortSpeed = 9600; break;
    case B19200: uiPortSpeed = 19200; break;
    case B38400: uiPortSpeed = 38400; break;
#  ifdef B57600
    case B57600: uiPortSpeed = 57600; break;
#  endif
#  ifdef B115200
    case B115200: uiPortSpeed = 115200; break;
#  endif
#  ifdef B230400
    case B230400: uiPortSpeed = 230400; break;
#  endif
#  ifdef B460800
    case B460800: uiPortSpeed = 460800; break;
#  endif
#  ifdef B921600
    case B921600: uiPortSpeed = 921600; break;
#  endif
    default: return 0;
  };
  return uiPortSpeed;
}



serial_port sp1;    /*!!!!!!!!!!!!!!!注意!!!!!!!!!!!!!!!!!!!!*/


#define INBUFLEN   4096
#define OUTBUFLEN  512

unsigned char rbuf[INBUFLEN]  = {0};
unsigned char wbuf[OUTBUFLEN] = {0};
size_t rsize = 0;
size_t wsize = 0;

void *readUartPM3(void *arg)
{
    while(1)
    {
        do{
            uart_receive(sp1,rbuf,INBUFLEN,&rsize);
        }while(!rsize);

        //printf("长度：%ld,数据：%s",rsize,rbuf);

        // 处理读取到的数据
        if (rsize > 0) {
            for(int i=0;i<rsize;i++)
            {
                printf("%c", rbuf[i]);
            }
            printf("\n");
            //printf("[+] 接收已完毕...\r\n");

        }
    }
}

#include <pthread.h>

int main(int argc, char* argv[]) {

    // char ttyACM[32]={0};
    // sprintf(ttyACM,"/dev/ttyACM%d",0);
    // sp1 = uart_open(ttyACM);
    sp1 = uart_open("tcp:localhost:4321");
    if(sp1 == INVALID_SERIAL_PORT) {
        printf("串口打开失败，重新插拔设备可能会解决问题\n\n");
    }
    else {
        printf("串口打开成功\n");
    }
    
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, readUartPM3, NULL);
    if (ret != 0) {
        printf("Failed to create thread.\n");
        return 1;
    }
    printf("Thread created successfully.\n");

    printf("\r\n");

    while (true) {
        //printf("[+] 等待输入...\r\n");
        char c ;
        uint8_t len=0;
        while((c=getc(stdin)) !='\n')
        {
            wbuf[len++]=c;
        }
        //printf("[+] 输入完毕...\r\n");
        wbuf[len++]='\r';
        wbuf[len++]='\n';

        uart_send(sp1, wbuf, strlen((const char *)wbuf));
    }


    pthread_join(thread, NULL);
    return 0;
}

