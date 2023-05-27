#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#include <limits.h>
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#ifndef _WIN32


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



serial_port sp1;    /*!!!!!!!!!!!!!!!ע��!!!!!!!!!!!!!!!!!!!!*/


#endif



/******************************************************************************************************/
/*                                         PM3 UART STRART                                            */
/******************************************************************************************************/

// //����ʱ
// # define UART_USB_CLIENT_RX_TIMEOUT_MS  /*20*/1

// #define PM3_SUCCESS             0

// // �����������  �ͻ���֡���մ���
// #define PM3_EIO                -8

// // ͨ��TTY����
// #define PM3_ENOTTY            -14


// /* serial_port is declared as a void*, which you should cast to whatever type
//  * makes sense to your connection method. Both the posix and win32
//  * implementations define their own structs in place.
//  */
// typedef void *serial_port;

// /* ���ָ���Ĵ��ж˿���Ч������uart_open���ء�
//  */
// #define INVALID_SERIAL_PORT (void*)(~1)

// /* ���ָ���Ĵ��ж˿�������һ������ʹ�ã�����uart_open���ء�
//  */
// #define CLAIMED_SERIAL_PORT (void*)(~2)


// typedef struct {
//     HANDLE hPort;     // ���ж˿ھ��
//     DCB dcb;          // �豸��������
//     COMMTIMEOUTS ct;  // ���ж˿ڳ�ʱ����
// } serial_port_windows_t;


// uint32_t newtimeout_value = 0;
// bool newtimeout_pending = false;


// /* ���ô��ж˿ڵĵ�ǰ�ٶȣ��Բ���Ϊ��λ����
//  */
// bool uart_set_speed(serial_port sp, const uint32_t uiPortSpeed) {
//     serial_port_windows_t *spw;

//     // ���ö˿��ٶȣ�����������
//     switch (uiPortSpeed) {
//         case 9600:
//         case 19200:
//         case 38400:
//         case 57600:
//         case 115200:
//         case 230400:
//         case 460800:
//         case 921600:
//         case 1382400:
//             break;
//         default:
//             return false;
//     };

//     spw = (serial_port_windows_t *)sp;
//     spw->dcb.BaudRate = uiPortSpeed;
//     bool result = SetCommState(spw->hPort, &spw->dcb);
//     PurgeComm(spw->hPort, PURGE_RXABORT | PURGE_RXCLEAR);
//     if (result)

//     return result;
// }

// /* ��ȡ���ж˿ڵĵ�ǰ�ٶȣ����أ���
//  */
// uint32_t uart_get_speed(const serial_port sp) {
//     const serial_port_windows_t *spw = (serial_port_windows_t *)sp;
//     if (!GetCommState(spw->hPort, (serial_port) & spw->dcb))
//         return spw->dcb.BaudRate;

//     return 0;
// }

// /* �������ó�ʱ
//  */
// int uart_reconfigure_timeouts(uint32_t value) {
//     newtimeout_value = value;
//     newtimeout_pending = true;
//     return PM3_SUCCESS;
// }

// static int uart_reconfigure_timeouts_polling(serial_port sp) {
//     if (newtimeout_pending == false)
//         return PM3_SUCCESS;
//     newtimeout_pending = false;

//     serial_port_windows_t *spw;
//     spw = (serial_port_windows_t *)sp;
//     spw->ct.ReadIntervalTimeout         = newtimeout_value;
//     spw->ct.ReadTotalTimeoutMultiplier  = 0;
//     spw->ct.ReadTotalTimeoutConstant    = newtimeout_value;
//     spw->ct.WriteTotalTimeoutMultiplier = newtimeout_value;
//     spw->ct.WriteTotalTimeoutConstant   = 0;

//     if (!SetCommTimeouts(spw->hPort, &spw->ct)) {
//         uart_close(spw);
//         return PM3_EIO;
//     }

//     PurgeComm(spw->hPort, PURGE_RXABORT | PURGE_RXCLEAR);
//     return PM3_SUCCESS;
// }


// /* �����û�ָ���Ķ˿��������ӵ��ö˿ڲ��������ڽ������øö˿ڵĽṹ��
//  *
//  * ���ִ���ʱ���˷�������INVALID_SERIAL_PORT��CLAIMD_SERIAL_PORT��
//  */
// serial_port uart_open(const char *pcPortName, uint32_t speed) {
//     char acPortName[255] = {0};
//     serial_port_windows_t *sp = calloc(sizeof(serial_port_windows_t), sizeof(uint8_t));

//     if (sp == 0) {
//         printf("UART�޷������ڴ�\n");
//         return INVALID_SERIAL_PORT;
//     }
//     // ������� "com?" ת��Ϊ "\\.\COM?" ��ʽ
//     snprintf(acPortName, sizeof(acPortName), "\\\\.\\%s", pcPortName);
//     _strupr(acPortName);

//     // ���Դ򿪴��ж˿�
//     // r/w,  none-share comport, no security, existing, no overlapping, no templates
//     sp->hPort = CreateFileA(acPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
//     if (sp->hPort == INVALID_HANDLE_VALUE) {
//         uart_close(sp);
//         return INVALID_SERIAL_PORT;
//     }

//     // ׼���豸����
//     // ���޹ؽ�Ҫ����ΪPM3�豸����������CDC���usb_CDC.c�е�set_line_coding
//     memset(&sp->dcb, 0, sizeof(DCB));
//     sp->dcb.DCBlength = sizeof(DCB);
//     if (!BuildCommDCBA("baud=115200 parity=N data=8 stop=1", &sp->dcb)) {
//         uart_close(sp);
//         printf("UART����cdc����\n");
//         return INVALID_SERIAL_PORT;
//     }

//     // ���»���ж˿�
//     if (!SetCommState(sp->hPort, &sp->dcb)) {
//         uart_close(sp);
//         printf("����ͨ��״̬ʱUART��������\n");
//         return INVALID_SERIAL_PORT;
//     }

//     uart_reconfigure_timeouts(UART_USB_CLIENT_RX_TIMEOUT_MS);
//     uart_reconfigure_timeouts_polling(sp);

//     if (!uart_set_speed(sp, speed)) {
//         // �Զ����Ի���
//         speed = 115200;
//         if (!uart_set_speed(sp, speed)) {
//             uart_close(sp);
//             printf("���ò�����ʱUART��������\n");
//             return INVALID_SERIAL_PORT;
//         }
//     }
//     return sp;
// }

// /* �رո����˿ڡ�
//  */
// void uart_close(const serial_port sp) {
//     if (((serial_port_windows_t *)sp)->hPort != INVALID_HANDLE_VALUE)
//         CloseHandle(((serial_port_windows_t *)sp)->hPort);
//     free(sp);
// }

// /* �Ӹ������ж˿ڶ�ȡ����30ms��
//  *   pbtRx: ָ��Ҫд��ķ������ݵĻ�������ָ�롣
//  *   pszMaxRxLen: Ҫ��ȡ��������ݴ�С��
//  *   pszRxLen: ʵ�ʶ�ȡ���ֽ�����
//  *
//  * �����ȡ�������ݣ���ʹС��pszMaxRxLen��Ҳ����TRUE��
//  *
//  * �����ȡ�豸ʱ�����򷵻�FALSE��
//  * ��ע�⣬��Ӧ��ʵ�ֿ����Ѿ�����˶Ի������Ĳ��ֶ�ȡ�����Ӧ���pszRxLen�Բ鿴�Ƿ��ȡ��������
//  */
// int uart_receive(const serial_port sp, uint8_t *pbtRx, uint32_t pszMaxRxLen, uint32_t *pszRxLen) {
//     uart_reconfigure_timeouts_polling(sp);
//     int res = ReadFile(((serial_port_windows_t *)sp)->hPort, pbtRx, pszMaxRxLen, (LPDWORD)pszRxLen, NULL);
//     if (res)
//         return PM3_SUCCESS;

//     int errorcode = GetLastError();

//     if (res == 0 && errorcode == 2) {
//         return PM3_EIO;
//     }

//     return PM3_ENOTTY;
// }

// /* �����������͵������Ĵ��ж˿ڡ�
//  *   pbtTx: ָ�����Ҫ���͵����ݵĻ�������ָ�롣
//  *   len: Ҫ���͵���������
//  */
// int uart_send(const serial_port sp, const uint8_t *p_tx, const uint32_t len) {
//     DWORD txlen = 0;
//     int res = WriteFile(((serial_port_windows_t *)sp)->hPort, p_tx, len, &txlen, NULL);
//     if (res)
//         return PM3_SUCCESS;

//     int errorcode = GetLastError();
//     if (res == 0 && errorcode == 2) {
//         return PM3_EIO;
//     }
//     return PM3_ENOTTY;
// }


// serial_port sp1;    /*!!!!!!!!!!!!!!!ע��!!!!!!!!!!!!!!!!!!!!*/



/******************************************************************************************************/
/*                                          PM3 UART END                                              */
/******************************************************************************************************/


/******************************************************************************************************/
/*                                        PM3 FMKEY STRART                                            */
/******************************************************************************************************/

// //-----------------------------------------------------------------------------
// // Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
// //
// // This program is free software: you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation, either version 3 of the License, or
// // (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // See LICENSE.txt for the text of the license.
// //-----------------------------------------------------------------------------
// // Parity functions
// //-----------------------------------------------------------------------------

// // all functions defined in header file by purpose. Allows compiler optimizations.

// #ifndef __PARITY_H
// #define __PARITY_H

// static const uint8_t g_odd_byte_parity[256] = {
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
//     1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
// };

// //extern const uint8_t OddByteParity[256];

// // #define ODD_PARITY8(x)   { g_odd_byte_parity[x] }
// #define EVEN_PARITY8(x)  { !g_odd_byte_parity[x] }

// // static inline uint8_t oddparity8(const uint8_t x) {
// //     return g_odd_byte_parity[x];
// // }

// // static inline uint8_t evenparity8(const uint8_t x) {
// //     return !g_odd_byte_parity[x];
// // }

// // static inline uint8_t evenparity16(uint16_t x) {
// // #if !defined __GNUC__
// //     x ^= x >> 8;
// //     return EVEN_PARITY8(x) ;
// // #else
// //     return (__builtin_parity(x) & 0xFF);
// // #endif
// // }

// // static inline uint8_t oddparity16(uint16_t x) {
// // #if !defined __GNUC__
// //     x ^= x >> 8;
// //     return ODD_PARITY8(x);
// // #else
// //     return !__builtin_parity(x);
// // #endif
// // }

// static inline uint8_t evenparity32(uint32_t x) {
// #if !defined __GNUC__
//     x ^= x >> 16;
//     x ^= x >> 8;
//     return EVEN_PARITY8(x);
// #else
//     return (__builtin_parity(x) & 0xFF);
// #endif
// }

// // static inline uint8_t oddparity32(uint32_t x) {
// // #if !defined __GNUC__
// //     x ^= x >> 16;
// //     x ^= x >> 8;
// //     return ODD_PARITY8(x);
// // #else
// //     return !__builtin_parity(x);
// // #endif
// // }

// #endif /* __PARITY_H */







// //-----------------------------------------------------------------------------
// // Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
// //
// // This program is free software: you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation, either version 3 of the License, or
// // (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // See LICENSE.txt for the text of the license.
// //-----------------------------------------------------------------------------
// #ifndef BUCKETSORT_H__
// #define BUCKETSORT_H__

// typedef struct bucket {
//     uint32_t *head;
//     uint32_t *bp;
// } bucket_t;

// typedef bucket_t bucket_array_t[2][0x100];

// typedef struct bucket_info {
//     struct {
//         uint32_t *head, *tail;
//     } bucket_info[2][0x100];
//     uint32_t numbuckets;
// } bucket_info_t;

// void bucket_sort_intersect(uint32_t *const estart, uint32_t *const estop,
//                            uint32_t *const ostart, uint32_t *const ostop,
//                            bucket_info_t *bucket_info, bucket_array_t bucket);

// #endif








// //-----------------------------------------------------------------------------
// // Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
// //
// // This program is free software: you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation, either version 3 of the License, or
// // (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // See LICENSE.txt for the text of the license.
// //-----------------------------------------------------------------------------

// extern void bucket_sort_intersect(uint32_t *const estart, uint32_t *const estop,
//                                   uint32_t *const ostart, uint32_t *const ostop,
//                                   bucket_info_t *bucket_info, bucket_array_t bucket) {
//     uint32_t *p1, *p2;
//     uint32_t *start[2];
//     uint32_t *stop[2];

//     start[0] = estart;
//     stop[0] = estop;
//     start[1] = ostart;
//     stop[1] = ostop;

//     // init buckets to be empty
//     for (uint32_t i = 0; i < 2; i++) {
//         for (uint32_t j = 0x00; j <= 0xff; j++) {
//             bucket[i][j].bp = bucket[i][j].head;
//         }
//     }

//     // sort the lists into the buckets based on the MSB (contribution bits)
//     for (uint32_t i = 0; i < 2; i++) {
//         for (p1 = start[i]; p1 <= stop[i]; p1++) {
//             uint32_t bucket_index = (*p1 & 0xff000000) >> 24;
//             *(bucket[i][bucket_index].bp++) = *p1;
//         }
//     }

//     // write back intersecting buckets as sorted list.
//     // fill in bucket_info with head and tail of the bucket contents in the list and number of non-empty buckets.
//     for (uint32_t i = 0; i < 2; i++) {
//         p1 = start[i];
//         uint32_t nonempty_bucket = 0;
//         for (uint32_t j = 0x00; j <= 0xff; j++) {
//             if (bucket[0][j].bp != bucket[0][j].head && bucket[1][j].bp != bucket[1][j].head) { // non-empty intersecting buckets only
//                 bucket_info->bucket_info[i][nonempty_bucket].head = p1;
//                 for (p2 = bucket[i][j].head; p2 < bucket[i][j].bp; *p1++ = *p2++);
//                 bucket_info->bucket_info[i][nonempty_bucket].tail = p1 - 1;
//                 nonempty_bucket++;
//             }
//         }
//         bucket_info->numbuckets = nonempty_bucket;
//     }
// }



// //-----------------------------------------------------------------------------
// // Copyright (C) 2008-2014 bla <blapost@gmail.com>
// // Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
// //
// // This program is free software: you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation, either version 3 of the License, or
// // (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // See LICENSE.txt for the text of the license.
// //-----------------------------------------------------------------------------
// #ifndef CRAPTO1_INCLUDED
// #define CRAPTO1_INCLUDED

// struct Crypto1State {uint32_t odd, even;};
// void crypto1_init(struct Crypto1State *state, uint64_t key);
// void crypto1_deinit(struct Crypto1State *);
// #if !defined(__arm__) || defined(__linux__) || defined(_WIN32) || defined(__APPLE__) // bare metal ARM Proxmark lacks malloc()/free()
// struct Crypto1State *crypto1_create(uint64_t key);
// void crypto1_destroy(struct Crypto1State *);
// #endif
// void crypto1_get_lfsr(struct Crypto1State *, uint64_t *);
// uint8_t crypto1_bit(struct Crypto1State *, uint8_t, int);
// uint8_t crypto1_byte(struct Crypto1State *, uint8_t, int);
// uint32_t crypto1_word(struct Crypto1State *, uint32_t, int);
// uint32_t prng_successor(uint32_t x, uint32_t n);

// #if !defined(__arm__) || defined(__linux__) || defined(_WIN32) || defined(__APPLE__) // bare metal ARM Proxmark lacks malloc()/free()
// struct Crypto1State *lfsr_recovery32(uint32_t ks2, uint32_t in);
// struct Crypto1State *lfsr_recovery64(uint32_t ks2, uint32_t ks3);
// struct Crypto1State *
// lfsr_common_prefix(uint32_t pfx, uint32_t rr, uint8_t ks[8], uint8_t par[8][8], uint32_t no_par);
// #endif
// uint32_t *lfsr_prefix_ks(const uint8_t ks[8], int isodd);


// uint8_t lfsr_rollback_bit(struct Crypto1State *s, uint32_t in, int fb);
// uint8_t lfsr_rollback_byte(struct Crypto1State *s, uint32_t in, int fb);
// uint32_t lfsr_rollback_word(struct Crypto1State *s, uint32_t in, int fb);
// int nonce_distance(uint32_t from, uint32_t to);
// bool validate_prng_nonce(uint32_t nonce);
// #define FOREACH_VALID_NONCE(N, FILTER, FSIZE)\
//     uint32_t __n = 0,__M = 0, N = 0;\
//     int __i;\
//     for(; __n < 1 << 16; N = prng_successor(__M = ++__n, 16))\
//         for(__i = FSIZE - 1; __i >= 0; __i--)\
//             if(BIT(FILTER, __i) ^ evenparity32(__M & 0xFF01))\
//                 break;\
//             else if(__i)\
//                 __M = prng_successor(__M, (__i == 7) ? 48 : 8);\
//             else

// #define LF_POLY_ODD (0x29CE5C)
// #define LF_POLY_EVEN (0x870804)
#define BIT(x, n) ((x) >> (n) & 1)
#define BEBIT(x, n) BIT(x, (n) ^ 24)
#ifdef __OPTIMIZE_SIZE__
int filter(uint32_t const x);
#else
static inline int filter(uint32_t const x) {
    uint32_t f;

    f  = 0xf22c0 >> (x       & 0xf) & 16;
    f |= 0x6c9c0 >> (x >>  4 & 0xf) &  8;
    f |= 0x3c8b0 >> (x >>  8 & 0xf) &  4;
    f |= 0x1e458 >> (x >> 12 & 0xf) &  2;
    f |= 0x0d938 >> (x >> 16 & 0xf) &  1;
    return BIT(0xEC57E80A, f);
}
#endif
// #endif




// //-----------------------------------------------------------------------------
// // Copyright (C) 2008-2014 bla <blapost@gmail.com>
// // Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
// //
// // This program is free software: you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation, either version 3 of the License, or
// // (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // See LICENSE.txt for the text of the license.
// //-----------------------------------------------------------------------------

// #ifdef __OPTIMIZE_SIZE__
// int filter(uint32_t const x) {
//     uint32_t f;

//     f  = 0xf22c0 >> (x       & 0xf) & 16;
//     f |= 0x6c9c0 >> (x >>  4 & 0xf) &  8;
//     f |= 0x3c8b0 >> (x >>  8 & 0xf) &  4;
//     f |= 0x1e458 >> (x >> 12 & 0xf) &  2;
//     f |= 0x0d938 >> (x >> 16 & 0xf) &  1;
//     return BIT(0xEC57E80A, f);
// }
// #endif

// #define SWAPENDIAN(x)\
//     (x = (x >> 8 & 0xff00ff) | (x & 0xff00ff) << 8, x = x >> 16 | x << 16)

// void crypto1_init(struct Crypto1State *state, uint64_t key) {
//     if (state == NULL)
//         return;
//     state->odd = 0;
//     state->even = 0;
//     for (int i = 47; i > 0; i -= 2) {
//         state->odd  = state->odd  << 1 | BIT(key, (i - 1) ^ 7);
//         state->even = state->even << 1 | BIT(key, i ^ 7);
//     }
// }

// void crypto1_deinit(struct Crypto1State *state) {
//     state->odd = 0;
//     state->even = 0;
// }

// #if !defined(__arm__) || defined(__linux__) || defined(_WIN32) || defined(__APPLE__) // bare metal ARM Proxmark lacks calloc()/free()
// struct Crypto1State *crypto1_create(uint64_t key) {
//     struct Crypto1State *state = calloc(sizeof(*state), sizeof(uint8_t));
//     if (!state) return NULL;
//     crypto1_init(state, key);
//     return state;
// }

// void crypto1_destroy(struct Crypto1State *state) {
//     free(state);
// }
// #endif

// void crypto1_get_lfsr(struct Crypto1State *state, uint64_t *lfsr) {
//     int i;
//     for (*lfsr = 0, i = 23; i >= 0; --i) {
//         *lfsr = *lfsr << 1 | BIT(state->odd, i ^ 3);
//         *lfsr = *lfsr << 1 | BIT(state->even, i ^ 3);
//     }
// }
// uint8_t crypto1_bit(struct Crypto1State *s, uint8_t in, int is_encrypted) {
//     uint32_t feedin, t;
//     uint8_t ret = filter(s->odd);

//     feedin  = ret & (!!is_encrypted);
//     feedin ^= !!in;
//     feedin ^= LF_POLY_ODD & s->odd;
//     feedin ^= LF_POLY_EVEN & s->even;
//     s->even = s->even << 1 | (evenparity32(feedin));

//     t = s->odd;
//     s->odd = s->even;
//     s->even = t;

//     return ret;
// }
// uint8_t crypto1_byte(struct Crypto1State *s, uint8_t in, int is_encrypted) {
//     uint8_t ret = 0;
//     ret |= crypto1_bit(s, BIT(in, 0), is_encrypted) << 0;
//     ret |= crypto1_bit(s, BIT(in, 1), is_encrypted) << 1;
//     ret |= crypto1_bit(s, BIT(in, 2), is_encrypted) << 2;
//     ret |= crypto1_bit(s, BIT(in, 3), is_encrypted) << 3;
//     ret |= crypto1_bit(s, BIT(in, 4), is_encrypted) << 4;
//     ret |= crypto1_bit(s, BIT(in, 5), is_encrypted) << 5;
//     ret |= crypto1_bit(s, BIT(in, 6), is_encrypted) << 6;
//     ret |= crypto1_bit(s, BIT(in, 7), is_encrypted) << 7;
//     return ret;
// }
// uint32_t crypto1_word(struct Crypto1State *s, uint32_t in, int is_encrypted) {
//     uint32_t ret = 0;
//     // note: xor args have been swapped because some compilers emit a warning
//     // for 10^x and 2^x as possible misuses for exponentiation. No comment.
//     ret |= crypto1_bit(s, BEBIT(in, 0), is_encrypted) << (24 ^ 0);
//     ret |= crypto1_bit(s, BEBIT(in, 1), is_encrypted) << (24 ^ 1);
//     ret |= crypto1_bit(s, BEBIT(in, 2), is_encrypted) << (24 ^ 2);
//     ret |= crypto1_bit(s, BEBIT(in, 3), is_encrypted) << (24 ^ 3);
//     ret |= crypto1_bit(s, BEBIT(in, 4), is_encrypted) << (24 ^ 4);
//     ret |= crypto1_bit(s, BEBIT(in, 5), is_encrypted) << (24 ^ 5);
//     ret |= crypto1_bit(s, BEBIT(in, 6), is_encrypted) << (24 ^ 6);
//     ret |= crypto1_bit(s, BEBIT(in, 7), is_encrypted) << (24 ^ 7);

//     ret |= crypto1_bit(s, BEBIT(in, 8), is_encrypted) << (24 ^ 8);
//     ret |= crypto1_bit(s, BEBIT(in, 9), is_encrypted) << (24 ^ 9);
//     ret |= crypto1_bit(s, BEBIT(in, 10), is_encrypted) << (24 ^ 10);
//     ret |= crypto1_bit(s, BEBIT(in, 11), is_encrypted) << (24 ^ 11);
//     ret |= crypto1_bit(s, BEBIT(in, 12), is_encrypted) << (24 ^ 12);
//     ret |= crypto1_bit(s, BEBIT(in, 13), is_encrypted) << (24 ^ 13);
//     ret |= crypto1_bit(s, BEBIT(in, 14), is_encrypted) << (24 ^ 14);
//     ret |= crypto1_bit(s, BEBIT(in, 15), is_encrypted) << (24 ^ 15);

//     ret |= crypto1_bit(s, BEBIT(in, 16), is_encrypted) << (24 ^ 16);
//     ret |= crypto1_bit(s, BEBIT(in, 17), is_encrypted) << (24 ^ 17);
//     ret |= crypto1_bit(s, BEBIT(in, 18), is_encrypted) << (24 ^ 18);
//     ret |= crypto1_bit(s, BEBIT(in, 19), is_encrypted) << (24 ^ 19);
//     ret |= crypto1_bit(s, BEBIT(in, 20), is_encrypted) << (24 ^ 20);
//     ret |= crypto1_bit(s, BEBIT(in, 21), is_encrypted) << (24 ^ 21);
//     ret |= crypto1_bit(s, BEBIT(in, 22), is_encrypted) << (24 ^ 22);
//     ret |= crypto1_bit(s, BEBIT(in, 23), is_encrypted) << (24 ^ 23);

//     ret |= crypto1_bit(s, BEBIT(in, 24), is_encrypted) << (24 ^ 24);
//     ret |= crypto1_bit(s, BEBIT(in, 25), is_encrypted) << (24 ^ 25);
//     ret |= crypto1_bit(s, BEBIT(in, 26), is_encrypted) << (24 ^ 26);
//     ret |= crypto1_bit(s, BEBIT(in, 27), is_encrypted) << (24 ^ 27);
//     ret |= crypto1_bit(s, BEBIT(in, 28), is_encrypted) << (24 ^ 28);
//     ret |= crypto1_bit(s, BEBIT(in, 29), is_encrypted) << (24 ^ 29);
//     ret |= crypto1_bit(s, BEBIT(in, 30), is_encrypted) << (24 ^ 30);
//     ret |= crypto1_bit(s, BEBIT(in, 31), is_encrypted) << (24 ^ 31);
//     return ret;
// }

// /* prng_successor
//  * helper used to obscure the keystream during authentication
//  */
// uint32_t prng_successor(uint32_t x, uint32_t n) {
//     SWAPENDIAN(x);
//     while (n--)
//         x = x >> 1 | (x >> 16 ^ x >> 18 ^ x >> 19 ^ x >> 21) << 31;

//     return SWAPENDIAN(x);
// }




// //-----------------------------------------------------------------------------
// // Copyright (C) 2008-2014 bla <blapost@gmail.com>
// // Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
// //
// // This program is free software: you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation, either version 3 of the License, or
// // (at your option) any later version.
// //
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //
// // See LICENSE.txt for the text of the license.
// //-----------------------------------------------------------------------------

#if !defined LOWMEM && defined __GNUC__
static uint8_t filterlut[1 << 20];
static void __attribute__((constructor)) fill_lut(void) {
    uint32_t i;
    for (i = 0; i < 1 << 20; ++i)
        filterlut[i] = filter(i);
}
#define filter(x) (filterlut[(x) & 0xfffff])
#endif

// /** update_contribution
//  * helper, calculates the partial linear feedback contributions and puts in MSB
//  */
// static inline void update_contribution(uint32_t *item, const uint32_t mask1, const uint32_t mask2) {
//     uint32_t p = *item >> 25;

//     p = p << 1 | (evenparity32(*item & mask1));
//     p = p << 1 | (evenparity32(*item & mask2));
//     *item = p << 24 | (*item & 0xffffff);
// }

// /** extend_table
//  * using a bit of the keystream extend the table of possible lfsr states
//  */
// static inline void extend_table(uint32_t *tbl, uint32_t **end, int bit, int m1, int m2, uint32_t in) {
//     in <<= 24;
//     for (*tbl <<= 1; tbl <= *end; *++tbl <<= 1)
//         if (filter(*tbl) ^ filter(*tbl | 1)) {
//             *tbl |= filter(*tbl) ^ bit;
//             update_contribution(tbl, m1, m2);
//             *tbl ^= in;
//         } else if (filter(*tbl) == bit) {
//             *++*end = tbl[1];
//             tbl[1] = tbl[0] | 1;
//             update_contribution(tbl, m1, m2);
//             *tbl++ ^= in;
//             update_contribution(tbl, m1, m2);
//             *tbl ^= in;
//         } else
//             *tbl-- = *(*end)--;
// }
// /** extend_table_simple
//  * using a bit of the keystream extend the table of possible lfsr states
//  */
// static inline void extend_table_simple(uint32_t *tbl, uint32_t **end, int bit) {
//     for (*tbl <<= 1; tbl <= *end; *++tbl <<= 1) {
//         if (filter(*tbl) ^ filter(*tbl | 1)) { // replace
//             *tbl |= filter(*tbl) ^ bit;
//         } else if (filter(*tbl) == bit) {     // insert
//             *++*end = *++tbl;
//             *tbl = tbl[-1] | 1;
//         } else {                              // drop
//             *tbl-- = *(*end)--;
//         }
//     }
// }
// /** recover
//  * recursively narrow down the search space, 4 bits of keystream at a time
//  */
// static struct Crypto1State *
// recover(uint32_t *o_head, uint32_t *o_tail, uint32_t oks,
//         uint32_t *e_head, uint32_t *e_tail, uint32_t eks, int rem,
//         struct Crypto1State *sl, uint32_t in, bucket_array_t bucket) {
//     bucket_info_t bucket_info;

//     if (rem == -1) {
//         for (uint32_t *e = e_head; e <= e_tail; ++e) {
//             *e = *e << 1 ^ (evenparity32(*e & LF_POLY_EVEN)) ^ (!!(in & 4));
//             for (uint32_t *o = o_head; o <= o_tail; ++o, ++sl) {
//                 sl->even = *o;
//                 sl->odd = *e ^ (evenparity32(*o & LF_POLY_ODD));
//                 sl[1].odd = sl[1].even = 0;
//             }
//         }
//         return sl;
//     }

//     for (uint32_t i = 0; i < 4 && rem--; i++) {
//         oks >>= 1;
//         eks >>= 1;
//         in >>= 2;
//         extend_table(o_head, &o_tail, oks & 1, LF_POLY_EVEN << 1 | 1, LF_POLY_ODD << 1, 0);
//         if (o_head > o_tail)
//             return sl;

//         extend_table(e_head, &e_tail, eks & 1, LF_POLY_ODD, LF_POLY_EVEN << 1 | 1, in & 3);
//         if (e_head > e_tail)
//             return sl;
//     }

//     bucket_sort_intersect(e_head, e_tail, o_head, o_tail, &bucket_info, bucket);

//     for (int i = bucket_info.numbuckets - 1; i >= 0; i--) {
//         sl = recover(bucket_info.bucket_info[1][i].head, bucket_info.bucket_info[1][i].tail, oks,
//                      bucket_info.bucket_info[0][i].head, bucket_info.bucket_info[0][i].tail, eks,
//                      rem, sl, in, bucket);
//     }

//     return sl;
// }


// #if !defined(__arm__) || defined(__linux__) || defined(_WIN32) || defined(__APPLE__) // bare metal ARM Proxmark lacks malloc()/free()
// /** lfsr_recovery
//  * recover the state of the lfsr given 32 bits of the keystream
//  * additionally you can use the in parameter to specify the value
//  * that was fed into the lfsr at the time the keystream was generated
//  */
// struct Crypto1State *lfsr_recovery32(uint32_t ks2, uint32_t in) {
//     struct Crypto1State *statelist;
//     uint32_t *odd_head = 0, *odd_tail = 0, oks = 0;
//     uint32_t *even_head = 0, *even_tail = 0, eks = 0;
//     int i;

//     // split the keystream into an odd and even part
//     for (i = 31; i >= 0; i -= 2)
//         oks = oks << 1 | BEBIT(ks2, i);
//     for (i = 30; i >= 0; i -= 2)
//         eks = eks << 1 | BEBIT(ks2, i);

//     odd_head = odd_tail = calloc(1, sizeof(uint32_t) << 21);
//     even_head = even_tail = calloc(1, sizeof(uint32_t) << 21);
//     statelist =  calloc(1, sizeof(struct Crypto1State) << 18);
//     if (!odd_tail-- || !even_tail-- || !statelist) {
//         free(statelist);
//         statelist = 0;
//         goto out;
//     }

//     statelist->odd = statelist->even = 0;

//     // allocate memory for out of place bucket_sort
//     bucket_array_t bucket;

//     for (i = 0; i < 2; i++) {
//         for (uint32_t j = 0; j <= 0xff; j++) {
//             bucket[i][j].head = calloc(1, sizeof(uint32_t) << 14);
//             if (!bucket[i][j].head) {
//                 goto out;
//             }
//         }
//     }

//     // initialize statelists: add all possible states which would result into the rightmost 2 bits of the keystream
//     for (i = 1 << 20; i >= 0; --i) {
//         if (filter(i) == (oks & 1))
//             *++odd_tail = i;
//         if (filter(i) == (eks & 1))
//             *++even_tail = i;
//     }

//     // extend the statelists. Look at the next 8 Bits of the keystream (4 Bit each odd and even):
//     for (i = 0; i < 4; i++) {
//         extend_table_simple(odd_head,  &odd_tail, (oks >>= 1) & 1);
//         extend_table_simple(even_head, &even_tail, (eks >>= 1) & 1);
//     }

//     // the statelists now contain all states which could have generated the last 10 Bits of the keystream.
//     // 22 bits to go to recover 32 bits in total. From now on, we need to take the "in"
//     // parameter into account.
//     in = (in >> 16 & 0xff) | (in << 16) | (in & 0xff00); // Byte swapping
//     recover(odd_head, odd_tail, oks, even_head, even_tail, eks, 11, statelist, in << 1, bucket);

// out:
//     for (i = 0; i < 2; i++)
//         for (uint32_t j = 0; j <= 0xff; j++)
//             free(bucket[i][j].head);
//     free(odd_head);
//     free(even_head);
//     return statelist;
// }

// static const uint32_t S1[] = {     0x62141, 0x310A0, 0x18850, 0x0C428, 0x06214,
//                                    0x0310A, 0x85E30, 0xC69AD, 0x634D6, 0xB5CDE, 0xDE8DA, 0x6F46D, 0xB3C83,
//                                    0x59E41, 0xA8995, 0xD027F, 0x6813F, 0x3409F, 0x9E6FA
//                              };
// static const uint32_t S2[] = {  0x3A557B00, 0x5D2ABD80, 0x2E955EC0, 0x174AAF60,
//                                 0x0BA557B0, 0x05D2ABD8, 0x0449DE68, 0x048464B0, 0x42423258, 0x278192A8,
//                                 0x156042D0, 0x0AB02168, 0x43F89B30, 0x61FC4D98, 0x765EAD48, 0x7D8FDD20,
//                                 0x7EC7EE90, 0x7F63F748, 0x79117020
//                              };
// static const uint32_t T1[] = {
//     0x4F37D, 0x279BE, 0x97A6A, 0x4BD35, 0x25E9A, 0x12F4D, 0x097A6, 0x80D66,
//     0xC4006, 0x62003, 0xB56B4, 0x5AB5A, 0xA9318, 0xD0F39, 0x6879C, 0xB057B,
//     0x582BD, 0x2C15E, 0x160AF, 0x8F6E2, 0xC3DC4, 0xE5857, 0x72C2B, 0x39615,
//     0x98DBF, 0xC806A, 0xE0680, 0x70340, 0x381A0, 0x98665, 0x4C332, 0xA272C
// };
// static const uint32_t T2[] = {  0x3C88B810, 0x5E445C08, 0x2982A580, 0x14C152C0,
//                                 0x4A60A960, 0x253054B0, 0x52982A58, 0x2FEC9EA8, 0x1156C4D0, 0x08AB6268,
//                                 0x42F53AB0, 0x217A9D58, 0x161DC528, 0x0DAE6910, 0x46D73488, 0x25CB11C0,
//                                 0x52E588E0, 0x6972C470, 0x34B96238, 0x5CFC3A98, 0x28DE96C8, 0x12CFC0E0,
//                                 0x4967E070, 0x64B3F038, 0x74F97398, 0x7CDC3248, 0x38CE92A0, 0x1C674950,
//                                 0x0E33A4A8, 0x01B959D0, 0x40DCACE8, 0x26CEDDF0
//                              };
// static const uint32_t C1[] = { 0x846B5, 0x4235A, 0x211AD};
// static const uint32_t C2[] = { 0x1A822E0, 0x21A822E0, 0x21A822E0};
// /** Reverse 64 bits of keystream into possible cipher states
//  * Variation mentioned in the paper. Somewhat optimized version
//  */
// struct Crypto1State *lfsr_recovery64(uint32_t ks2, uint32_t ks3) {
//     struct Crypto1State *statelist, *sl;
//     uint8_t oks[32], eks[32], hi[32];
//     uint32_t low = 0,  win = 0;
//     uint32_t *tail, table[1 << 16];
//     int i, j;

//     sl = statelist = calloc(1, sizeof(struct Crypto1State) << 4);
//     if (!sl)
//         return 0;
//     sl->odd = sl->even = 0;

//     for (i = 30; i >= 0; i -= 2) {
//         oks[i >> 1] = BEBIT(ks2, i);
//         oks[16 + (i >> 1)] = BEBIT(ks3, i);
//     }
//     for (i = 31; i >= 0; i -= 2) {
//         eks[i >> 1] = BEBIT(ks2, i);
//         eks[16 + (i >> 1)] = BEBIT(ks3, i);
//     }

//     for (i = 0xfffff; i >= 0; --i) {
//         if (filter(i) != oks[0])
//             continue;

//         *(tail = table) = i;
//         for (j = 1; tail >= table && j < 29; ++j)
//             extend_table_simple(table, &tail, oks[j]);

//         if (tail < table)
//             continue;

//         for (j = 0; j < 19; ++j)
//             low = low << 1 | (evenparity32(i & S1[j]));
//         for (j = 0; j < 32; ++j)
//             hi[j] = evenparity32(i & T1[j]);

//         for (; tail >= table; --tail) {
//             for (j = 0; j < 3; ++j) {
//                 *tail = *tail << 1;
//                 *tail |= evenparity32((i & C1[j]) ^ (*tail & C2[j]));
//                 if (filter(*tail) != oks[29 + j])
//                     goto continue2;
//             }

//             for (j = 0; j < 19; ++j)
//                 win = win << 1 | (evenparity32(*tail & S2[j]));

//             win ^= low;
//             for (j = 0; j < 32; ++j) {
//                 win = win << 1 ^ hi[j] ^ (evenparity32(*tail & T2[j]));
//                 if (filter(win) != eks[j])
//                     goto continue2;
//             }

//             *tail = *tail << 1 | (evenparity32(LF_POLY_EVEN & *tail));
//             sl->odd = *tail ^ (evenparity32(LF_POLY_ODD & win));
//             sl->even = win;
//             ++sl;
//             sl->odd = sl->even = 0;
// continue2:
//             ;
//         }
//     }
//     return statelist;
// }
// #endif

// /** lfsr_rollback_bit
//  * Rollback the shift register in order to get previous states
//  */
// uint8_t lfsr_rollback_bit(struct Crypto1State *s, uint32_t in, int fb) {
//     int out;
//     uint8_t ret;
//     uint32_t t;

//     s->odd &= 0xffffff;
//     t = s->odd, s->odd = s->even, s->even = t;

//     out = s->even & 1;
//     out ^= LF_POLY_EVEN & (s->even >>= 1);
//     out ^= LF_POLY_ODD & s->odd;
//     out ^= !!in;
//     out ^= (ret = filter(s->odd)) & (!!fb);

//     s->even |= (evenparity32(out)) << 23;
//     return ret;
// }
// /** lfsr_rollback_byte
//  * Rollback the shift register in order to get previous states
//  */
// uint8_t lfsr_rollback_byte(struct Crypto1State *s, uint32_t in, int fb) {
//     uint8_t ret = 0;
//     ret |= lfsr_rollback_bit(s, BIT(in, 7), fb) << 7;
//     ret |= lfsr_rollback_bit(s, BIT(in, 6), fb) << 6;
//     ret |= lfsr_rollback_bit(s, BIT(in, 5), fb) << 5;
//     ret |= lfsr_rollback_bit(s, BIT(in, 4), fb) << 4;
//     ret |= lfsr_rollback_bit(s, BIT(in, 3), fb) << 3;
//     ret |= lfsr_rollback_bit(s, BIT(in, 2), fb) << 2;
//     ret |= lfsr_rollback_bit(s, BIT(in, 1), fb) << 1;
//     ret |= lfsr_rollback_bit(s, BIT(in, 0), fb) << 0;
//     return ret;
// }
// /** lfsr_rollback_word
//  * Rollback the shift register in order to get previous states
//  */
// uint32_t lfsr_rollback_word(struct Crypto1State *s, uint32_t in, int fb) {

//     uint32_t ret = 0;
//     // note: xor args have been swapped because some compilers emit a warning
//     // for 10^x and 2^x as possible misuses for exponentiation. No comment.
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 31), fb) << (24 ^ 31);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 30), fb) << (24 ^ 30);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 29), fb) << (24 ^ 29);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 28), fb) << (24 ^ 28);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 27), fb) << (24 ^ 27);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 26), fb) << (24 ^ 26);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 25), fb) << (24 ^ 25);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 24), fb) << (24 ^ 24);

//     ret |= lfsr_rollback_bit(s, BEBIT(in, 23), fb) << (24 ^ 23);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 22), fb) << (24 ^ 22);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 21), fb) << (24 ^ 21);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 20), fb) << (24 ^ 20);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 19), fb) << (24 ^ 19);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 18), fb) << (24 ^ 18);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 17), fb) << (24 ^ 17);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 16), fb) << (24 ^ 16);

//     ret |= lfsr_rollback_bit(s, BEBIT(in, 15), fb) << (24 ^ 15);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 14), fb) << (24 ^ 14);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 13), fb) << (24 ^ 13);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 12), fb) << (24 ^ 12);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 11), fb) << (24 ^ 11);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 10), fb) << (24 ^ 10);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 9), fb) << (24 ^ 9);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 8), fb) << (24 ^ 8);

//     ret |= lfsr_rollback_bit(s, BEBIT(in, 7), fb) << (24 ^ 7);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 6), fb) << (24 ^ 6);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 5), fb) << (24 ^ 5);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 4), fb) << (24 ^ 4);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 3), fb) << (24 ^ 3);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 2), fb) << (24 ^ 2);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 1), fb) << (24 ^ 1);
//     ret |= lfsr_rollback_bit(s, BEBIT(in, 0), fb) << (24 ^ 0);
//     return ret;
// }

// /** nonce_distance
//  * x,y valid tag nonces, then prng_successor(x, nonce_distance(x, y)) = y
//  */
// static uint16_t *dist = 0;
// int nonce_distance(uint32_t from, uint32_t to) {
//     if (!dist) {
//         // allocation 2bytes * 0xFFFF times.
//         dist = calloc(2 << 16,  sizeof(uint8_t));
//         if (!dist)
//             return -1;
//         uint16_t x = 1;
//         for (uint16_t i = 1; i; ++i) {
//             dist[(x & 0xff) << 8 | x >> 8] = i;
//             x = x >> 1 | (x ^ x >> 2 ^ x >> 3 ^ x >> 5) << 15;
//         }
//     }
//     return (65535 + dist[to >> 16] - dist[from >> 16]) % 65535;
// }

// /** validate_prng_nonce
//  * Determine if nonce is deterministic. ie: Suspectable to Darkside attack.
//  * returns
//  *   true = weak prng
//  *   false = hardend prng
//  */
// bool validate_prng_nonce(uint32_t nonce) {
//     // init prng table:
//     if (nonce_distance(nonce, nonce) == -1)
//         return false;
//     return ((65535 - dist[nonce >> 16] + dist[nonce & 0xffff]) % 65535) == 16;
// }

// static uint32_t fastfwd[2][8] = {
//     { 0, 0x4BC53, 0xECB1, 0x450E2, 0x25E29, 0x6E27A, 0x2B298, 0x60ECB},
//     { 0, 0x1D962, 0x4BC53, 0x56531, 0xECB1, 0x135D3, 0x450E2, 0x58980}
// };

// /** lfsr_prefix_ks
//  *
//  * Is an exported helper function from the common prefix attack
//  * Described in the "dark side" paper. It returns an -1 terminated array
//  * of possible partial(21 bit) secret state.
//  * The required keystream(ks) needs to contain the keystream that was used to
//  * encrypt the NACK which is observed when varying only the 3 last bits of Nr
//  * only correct iff [NR_3] ^ NR_3 does not depend on Nr_3
//  */
// uint32_t *lfsr_prefix_ks(const uint8_t ks[8], int isodd) {
//     uint32_t *candidates = calloc(4 << 10, sizeof(uint8_t));
//     if (!candidates) return 0;

//     int size = 0;

//     for (int i = 0; i < 1 << 21; ++i) {
//         int good = 1;
//         for (uint32_t c = 0; good && c < 8; ++c) {
//             uint32_t entry = i ^ fastfwd[isodd][c];
//             good &= (BIT(ks[c], isodd) == filter(entry >> 1));
//             good &= (BIT(ks[c], isodd + 2) == filter(entry));
//         }
//         if (good)
//             candidates[size++] = i;
//     }

//     candidates[size] = -1;

//     return candidates;
// }

// /** check_pfx_parity
//  * helper function which eliminates possible secret states using parity bits
//  */
// static struct Crypto1State *check_pfx_parity(uint32_t prefix, uint32_t rresp, uint8_t parities[8][8], uint32_t odd, uint32_t even, struct Crypto1State *sl, uint32_t no_par) {
//     uint32_t good = 1;

//     for (uint32_t c = 0; good && c < 8; ++c) {
//         sl->odd = odd ^ fastfwd[1][c];
//         sl->even = even ^ fastfwd[0][c];

//         lfsr_rollback_bit(sl, 0, 0);
//         lfsr_rollback_bit(sl, 0, 0);

//         uint32_t ks3 = lfsr_rollback_bit(sl, 0, 0);
//         uint32_t ks2 = lfsr_rollback_word(sl, 0, 0);
//         uint32_t ks1 = lfsr_rollback_word(sl, prefix | c << 5, 1);

//         if (no_par)
//             break;

//         uint32_t nr = ks1 ^ (prefix | c << 5);
//         uint32_t rr = ks2 ^ rresp;

//         good &= evenparity32(nr & 0x000000ff) ^ parities[c][3] ^ BIT(ks2, 24);
//         good &= evenparity32(rr & 0xff000000) ^ parities[c][4] ^ BIT(ks2, 16);
//         good &= evenparity32(rr & 0x00ff0000) ^ parities[c][5] ^ BIT(ks2,  8);
//         good &= evenparity32(rr & 0x0000ff00) ^ parities[c][6] ^ BIT(ks2,  0);
//         good &= evenparity32(rr & 0x000000ff) ^ parities[c][7] ^ ks3;
//     }

//     return sl + good;
// }

// #if !defined(__arm__) || defined(__linux__) || defined(_WIN32) || defined(__APPLE__) // bare metal ARM Proxmark lacks malloc()/free()
// /** lfsr_common_prefix
//  * Implementation of the common prefix attack.
//  * Requires the 28 bit constant prefix used as reader nonce (pfx)
//  * The reader response used (rr)
//  * The keystream used to encrypt the observed NACK's (ks)
//  * The parity bits (par)
//  * It returns a zero terminated list of possible cipher states after the
//  * tag nonce was fed in
//  */

// struct Crypto1State *lfsr_common_prefix(uint32_t pfx, uint32_t rr, uint8_t ks[8], uint8_t par[8][8], uint32_t no_par) {
//     struct Crypto1State *statelist, *s;
//     uint32_t *odd, *even, *o, *e, top;

//     odd = lfsr_prefix_ks(ks, 1);
//     even = lfsr_prefix_ks(ks, 0);

//     s = statelist = calloc(1, (sizeof * statelist) << 24); // was << 20. Need more for no_par special attack. Enough???
//     if (!s || !odd || !even) {
//         free(statelist);
//         statelist = 0;
//         goto out;
//     }

//     for (o = odd; *o + 1; ++o)
//         for (e = even; *e + 1; ++e)
//             for (top = 0; top < 64; ++top) {
//                 *o += 1 << 21;
//                 *e += (!(top & 7) + 1) << 21;
//                 s = check_pfx_parity(pfx, rr, par, *o, *e, s, no_par);
//             }

//     s->odd = s->even = 0;
// out:
//     free(odd);
//     free(even);
//     return statelist;
// }
// #endif




// int mfkey(void) {

//     int argc;

//     printf("����5:һ���Իָ���Կ\n����7:�����λỰ�лָ���Կ\n");
//     scanf("%d",&argc);

//     if (argc == 7) {

//         struct Crypto1State *s, *t;
//         uint64_t key;     // recovered key
//         uint32_t uid;     // serial number
//         uint32_t nt0;      // tag challenge first
//         uint32_t nt1;      // tag challenge second
//         uint32_t nr0_enc; // first encrypted reader challenge
//         uint32_t ar0_enc; // first encrypted reader response
//         uint32_t nr1_enc; // second encrypted reader challenge
//         uint32_t ar1_enc; // second encrypted reader response
//         uint32_t ks2;     // keystream used to encrypt reader response

//         printf("\n");
//         printf("-----------------------------------------------------\n");
//         printf("MIFARE Classic��Կ�ָ�-����32λ��Կ�� �汾v2\n");
//         printf("��������32λ��ȡ�������֤�𰸻ָ���Կ\n");
//         printf("����汾ʵ����Moebius���ֲ�ͬ��nonce�������(�糬����)\n");
//         printf("-----------------------------------------------------\n\n");
//         printf("����: <uid> <nt> <nr_0> <ar_0> <nt1> <nr_1> <ar_1>\n\n");
    
//         scanf("%x %x %x %x %x %x %x", &uid, &nt0, &nr0_enc, &ar0_enc, &nt1, &nr1_enc, &ar1_enc);

//         printf("\n���ڴ����¸����������лָ�����Կ:\n");
//         printf("    uid: %08x\n", uid);
//         printf("   nt_0: %08x\n", nt0);
//         printf(" {nr_0}: %08x\n", nr0_enc);
//         printf(" {ar_0}: %08x\n", ar0_enc);
//         printf("   nt_1: %08x\n", nt1);
//         printf(" {nr_1}: %08x\n", nr1_enc);
//         printf(" {ar_1}: %08x\n", ar1_enc);

//         // Generate lfsr successors of the tag challenge
//         printf("\nLFSR successors of the tag challenge:\n");
//         uint32_t p64 = prng_successor(nt0, 64);
//         uint32_t p64b = prng_successor(nt1, 64);

//         printf("  nt': %08x\n", p64);
//         printf(" nt'': %08x\n", prng_successor(p64, 32));

//         // Extract the keystream from the messages
//         printf("\n��������{ar}��{at}����Կ��:\n");
//         ks2 = ar0_enc ^ p64;
//         printf("  ks2: %08x\n", ks2);

//         s = lfsr_recovery32(ar0_enc ^ p64, 0);

//         for (t = s; t->odd | t->even; ++t) {
//             lfsr_rollback_word(t, 0, 0);
//             lfsr_rollback_word(t, nr0_enc, 1);
//             lfsr_rollback_word(t, uid ^ nt0, 0);
//             crypto1_get_lfsr(t, &key);

//             crypto1_word(t, uid ^ nt1, 0);
//             crypto1_word(t, nr1_enc, 1);
//             if (ar1_enc == (crypto1_word(t, 0, 0) ^ p64b)) {
//                 printf("\n�ҵ���Կ: [%012" PRIx64 "]", key);
//                 break;
//             }
//         }
//         free(s);
//         return 0;
//     } else if(argc == 5) {

//         struct Crypto1State *revstate;
//         uint64_t key;     // recovered key
//         uint32_t uid;     // serial number
//         uint32_t nt;      // tag challenge
//         uint32_t nr_enc;  // encrypted reader challenge
//         uint32_t ar_enc;  // encrypted reader response
//         uint32_t at_enc;  // encrypted tag response
//         uint32_t ks2;     // keystream used to encrypt reader response
//         uint32_t ks3;     // keystream used to encrypt tag response

//         printf("\n");
//         printf("-----------------------------------------------------\n");
//         printf("MIFARE Classic��Կ�ָ�-����64λ��Կ��\n");
//         printf("����һ�����������֤�лָ���Կ!\n");
//         printf("-----------------------------------------------------\n\n");
//         printf("����: <uid> <nt> <{nr}> <{ar}> <{at}>\n\n");

//         scanf("%x %x %x %x %x", &uid, &nt, &nr_enc, &ar_enc, &at_enc);

//         printf("\n���ڴ����¸����������лָ�����Կ:\n");

//         printf("  uid: %08x\n", uid);
//         printf("   nt: %08x\n", nt);
//         printf(" {nr}: %08x\n", nr_enc);
//         printf(" {ar}: %08x\n", ar_enc);
//         printf(" {at}: %08x\n", at_enc);

//         // Generate lfsr successors of the tag challenge
//         printf("\nLFSR successors of the tag challenge:\n");
//         uint32_t p64 = prng_successor(nt, 64);
//         printf("  nt': %08x\n", p64);
//         printf(" nt'': %08x\n", prng_successor(p64, 32));

//         // Extract the keystream from the messages
//         printf("\n��������{ar}��{at}����Կ��:\n");
//         ks2 = ar_enc ^ p64;
//         ks3 = at_enc ^ prng_successor(p64, 32);
//         printf("  ks2: %08x\n", ks2);
//         printf("  ks3: %08x\n", ks3);

//         revstate = lfsr_recovery64(ks2, ks3);

//         lfsr_rollback_word(revstate, 0, 0);
//         lfsr_rollback_word(revstate, 0, 0);
//         lfsr_rollback_word(revstate, nr_enc, 1);
//         lfsr_rollback_word(revstate, uid ^ nt, 0);
//         crypto1_get_lfsr(revstate, &key);
//         printf("\n�ҵ���Կ: [%012" PRIx64 "]", key);
//         crypto1_destroy(revstate);
//         return 0;
//     }
// }



/******************************************************************************************************/
/*                                           PM3 FMKEY END                                            */
/******************************************************************************************************/


/******************************************************************************************************/
/*                                          ����˹ FMKEY Start                                         */
/******************************************************************************************************/



// // #include <stdio.h>
// // #include <stdint.h>
// // #include <stdbool.h>

typedef struct
{
    uint32_t odd;
    uint32_t even;
} Crypto1State;

#define LF_POLY_ODD (0x29CE5C)
#define LF_POLY_EVEN (0x870804)

// #define BIT(x, n) ((x) >> (n) & 1)
// #define BEBIT(x, n) BIT(x, (n) ^ 24)

// int filter(uint32_t const x) {
//     uint32_t f;

//     f  = 0xf22c0 >> (x       & 0xf) & 16;
//     f |= 0x6c9c0 >> (x >>  4 & 0xf) &  8;
//     f |= 0x3c8b0 >> (x >>  8 & 0xf) &  4;
//     f |= 0x1e458 >> (x >> 12 & 0xf) &  2;
//     f |= 0x0d938 >> (x >> 16 & 0xf) &  1;
//     return BIT(0xEC57E80A, f);
// }
uint8_t evenparity32(uint32_t x) {
    return (__builtin_parity(x) & 0xFF);
}
// int evenparity32(int x)
// {
//     x ^= x >> 16;
//     x ^= x >> 8;
//     x ^= x >> 4;
//     return BIT(0x6996, x & 0xf); //((0x6996 >>> (x & 0xf)) & 1);		//BIT(0x6996, x & 0xf);
// }

#define SWAPENDIAN(x)\
    (x = (x >> 8 & 0xff00ff) | (x & 0xff00ff) << 8, x = x >> 16 | x << 16)

/* prng_successor
 * helper used to obscure the keystream during authentication
 */
uint32_t prng_successor(uint32_t x, uint32_t n) {
    SWAPENDIAN(x);
    while (n--)
        x = x >> 1 | (x >> 16 ^ x >> 18 ^ x >> 19 ^ x >> 21) << 31;

    return SWAPENDIAN(x);
}

/** extend_table_simple
 * using a bit of the keystream extend the table of possible lfsr states
 */
static uint32_t extend_table_simple_2(uint32_t *tbl, uint32_t **end, int bit) {
    for (*tbl <<= 1; tbl <= *end; *++tbl <<= 1) {
        if (filter(*tbl) ^ filter(*tbl | 1)) { // replace
            *tbl |= filter(*tbl) ^ bit;
        } else if (filter(*tbl) == bit) {     // insert
            *++*end = *++tbl;
            *tbl = tbl[-1] | 1;
        } else {                              // drop
            *tbl-- = *(*end)--;
        }
    }
    return **end; // 2023-05-27
}

int extend_table_simple(uint32_t *data, int tbl, int end, int bit){

    for(data[ tbl ] <<= 1; tbl <= end; data[++tbl] <<= 1)
        if(filter(data[ tbl ]) ^ filter(data[ tbl ] | 1))
            data[ tbl ] |= filter(data[ tbl ]) ^ bit;
        else if(filter(data[ tbl ]) == bit) {
            data[ ++end ] = data[ ++tbl ];
            data[ tbl ] = data[ tbl - 1 ] | 1;
        } else
            data[ tbl-- ] = data[ end-- ];
    return end;
}

// /** update_contribution
//  * helper, calculates the partial linear feedback contributions and puts in MSB
//  */
// void update_contribution(uint32_t *item, const uint32_t mask1, const uint32_t mask2) {
//     uint32_t p = *item >> 25;

//     p = p << 1 | (evenparity32(*item & mask1));
//     p = p << 1 | (evenparity32(*item & mask2));
//     *item = p << 24 | (*item & 0xffffff);
// }

void update_contribution(uint32_t *data, int item, int mask1, int mask2) {
    uint32_t p = data[item] >> 25;

    p = p << 1 | evenparity32(data[item] & mask1);
    p = p << 1 | evenparity32(data[item] & mask2);
    data[item] = p << 24 | (data[item] & 0xffffff);
}

// /** extend_table
//  * using a bit of the keystream extend the table of possible lfsr states
//  */
// void extend_table(uint32_t *tbl, uint32_t **end, int bit, int m1, int m2, uint32_t in) {
//     in <<= 24;
//     for (*tbl <<= 1; tbl <= *end; *++tbl <<= 1)
//         if (filter(*tbl) ^ filter(*tbl | 1)) {
//             *tbl |= filter(*tbl) ^ bit;
//             update_contribution(tbl, m1, m2);
//             *tbl ^= in;
//         } else if (filter(*tbl) == bit) {
//             *++*end = tbl[1];
//             tbl[1] = tbl[0] | 1;
//             update_contribution(tbl, m1, m2);
//             *tbl++ ^= in;
//             update_contribution(tbl, m1, m2);
//             *tbl ^= in;
//         } else
//             *tbl-- = *(*end)--;
// }

int extend_table(uint32_t *data, int tbl, int end, int bit, int m1, int m2, uint32_t in) {

    in <<= 24;
    for (data[tbl] <<= 1; tbl <= end; data[++tbl] <<= 1)
        if (filter(data[tbl]) ^ filter(data[tbl] | 1)) {
            data[tbl] |= filter(data[tbl]) ^ bit;
            update_contribution(data, tbl, m1, m2);
            data[tbl] ^= in;
        } else if (filter(data[tbl]) == bit) {
            data[++end] = data[tbl + 1];
            data[tbl + 1] = data[tbl] | 1;
            update_contribution(data, tbl, m1, m2);
            data[tbl++] ^= in;
            update_contribution(data, tbl, m1, m2);
            data[tbl] ^= in;
        } else
            data[tbl--] = data[end--];
    return end;
}

void quicksort(uint32_t *data, int start, int stop){

    int it = start + 1, rit = stop, t;

    if(it > rit)
        return;

    while(it < rit)
        if( (data[it] ^ 0x80000000) <= (data[start] ^ 0x80000000) ) {
            ++it;
        }
        else if((data[rit] ^ 0x80000000) > (data[start] ^ 0x80000000)) {
            --rit;
        }
        else {
            t = data[it];
            data[it] = data[rit];
            data[rit] = t;
        }

    if((data[rit] ^ 0x80000000) >= (data[start] ^ 0x80000000)) {
        --rit;
    }
    if(rit != start) {
        t = data[rit];
        data[rit] = data[start];
        data[start] = t;
    }

    quicksort(data, start, rit - 1);
    quicksort(data, rit + 1, stop);
}


int binsearch(uint32_t *data, int start, int stop)
{
    int mid, val =data[stop] & 0xff000000;
    while(start != stop) {
        mid = (stop - start) >> 1;
        if( (data[start + mid] ^ 0x80000000) > (val ^ 0x80000000))
            stop = start + mid;
        else
            start += mid + 1;
    }
    return start;
}

int recover(uint32_t *odd, int o_head, int o_tail, int oks, uint32_t *even, int e_head, int e_tail, int eks, int rem,
            Crypto1State *sl, int s, int in) {

    int o, e, i;

    if (rem == -1) {
        for (e = e_head; e <= e_tail; ++e) {
            even[e] = even[e] << 1 ^ evenparity32(even[e] & LF_POLY_EVEN) ^ (((in & 4) != 0) ? 1 : 0);
            for (o = o_head; o <= o_tail; ++o, ++s) {
                sl[s].even = odd[o];
                sl[s].odd = even[e] ^ evenparity32(odd[o] & LF_POLY_ODD);
                sl[s + 1].odd = sl[s + 1].even = 0;
            }
        }
        return s;
    }

    for (i = 0; (i < 4) && (rem-- != 0); i++) {
        oks >>= 1;
        eks >>= 1;
        in >>= 2;
        o_tail = extend_table(odd, o_head, o_tail, oks & 1, LF_POLY_EVEN << 1 | 1, LF_POLY_ODD << 1, 0);
        if (o_head > o_tail)
            return s;

        e_tail = extend_table(even, e_head, e_tail, eks & 1, LF_POLY_ODD, LF_POLY_EVEN << 1 | 1, in & 3);
        if (e_head > e_tail)
            return s;
    }

    quicksort(odd, o_head, o_tail);
    quicksort(even, e_head, e_tail);

    while (o_tail >= o_head && e_tail >= e_head)
        if (((odd[o_tail] ^ even[e_tail]) >> 24) == 0) {
            o_tail = binsearch(odd, o_head, o = o_tail);
            e_tail = binsearch(even, e_head, e = e_tail);
            s = recover(odd, o_tail--, o, oks, even, e_tail--, e, eks, rem, sl, s, in);
        } else if ((odd[o_tail] ^ 0x80000000) > (even[e_tail] ^ 0x80000000))
            o_tail = binsearch(odd, o_head, o_tail) - 1;
        else
            e_tail = binsearch(even, e_head, e_tail) - 1;

    return s;
}

Crypto1State *lfsr_recovery32(int ks2, int in,Crypto1State *statelist,int *odd,int *even) {
    // Crypto1State *statelist =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    // int *odd  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    // int *even =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    int odd_head = 0, odd_tail = -1, oks = 0;
    int even_head = 0, even_tail = -1, eks = 0;
    int stl = 0;
    int i;

    for(i = 0; i < (1 << 18); i++) {
        statelist[i].odd = 0;
        statelist[i].even = 0;
    }

    for (i = 31; i >= 0; i -= 2)
        oks = oks << 1 | BEBIT(ks2, i);
    for (i = 30; i >= 0; i -= 2)
        eks = eks << 1 | BEBIT(ks2, i);

    statelist[stl].odd = statelist[stl].even = 0;

    for (i = 1 << 20; i >= 0; --i) {
        if (filter(i) == (oks & 1))
            odd[++odd_tail] = i; // *++odd_tail = i;
        if (filter(i) == (eks & 1))
            even[++even_tail] = i; // *++even_tail = i;
    }//printf("MB %lld\r\n",(uint64_t)((1 << 18)*sizeof(int)/1024/1024));printf("1 << 18 %lld\r\n",(uint64_t)(1 << 18));printf("odd_tail %lld\r\n",odd_tail);printf("even_tail %lld\r\n",even_tail);printf("--- %lld\r\n",(uint64_t)(((1 << 21) - even_tail)/1024/1024*sizeof(int)));

    for (i = 0; i < 4; i++) {
        odd_tail = extend_table_simple(odd, odd_head, odd_tail, ((oks >>= 1) & 1));
        even_tail = extend_table_simple(even, even_head, even_tail, ((eks >>= 1) & 1));
    }

    in = (in >> 16 & 0xff) | (in << 16) | (in & 0xff00);
    recover(odd, odd_head, odd_tail, oks, even, even_head, even_tail, eks, 11, statelist, 0, in << 1);

    return statelist;
}

static const uint32_t S1[] = {     0x62141, 0x310A0, 0x18850, 0x0C428, 0x06214,
                                   0x0310A, 0x85E30, 0xC69AD, 0x634D6, 0xB5CDE, 0xDE8DA, 0x6F46D, 0xB3C83,
                                   0x59E41, 0xA8995, 0xD027F, 0x6813F, 0x3409F, 0x9E6FA
                             };
static const uint32_t S2[] = {  0x3A557B00, 0x5D2ABD80, 0x2E955EC0, 0x174AAF60,
                                0x0BA557B0, 0x05D2ABD8, 0x0449DE68, 0x048464B0, 0x42423258, 0x278192A8,
                                0x156042D0, 0x0AB02168, 0x43F89B30, 0x61FC4D98, 0x765EAD48, 0x7D8FDD20,
                                0x7EC7EE90, 0x7F63F748, 0x79117020
                             };
static const uint32_t T1[] = {
    0x4F37D, 0x279BE, 0x97A6A, 0x4BD35, 0x25E9A, 0x12F4D, 0x097A6, 0x80D66,
    0xC4006, 0x62003, 0xB56B4, 0x5AB5A, 0xA9318, 0xD0F39, 0x6879C, 0xB057B,
    0x582BD, 0x2C15E, 0x160AF, 0x8F6E2, 0xC3DC4, 0xE5857, 0x72C2B, 0x39615,
    0x98DBF, 0xC806A, 0xE0680, 0x70340, 0x381A0, 0x98665, 0x4C332, 0xA272C
};
static const uint32_t T2[] = {  0x3C88B810, 0x5E445C08, 0x2982A580, 0x14C152C0,
                                0x4A60A960, 0x253054B0, 0x52982A58, 0x2FEC9EA8, 0x1156C4D0, 0x08AB6268,
                                0x42F53AB0, 0x217A9D58, 0x161DC528, 0x0DAE6910, 0x46D73488, 0x25CB11C0,
                                0x52E588E0, 0x6972C470, 0x34B96238, 0x5CFC3A98, 0x28DE96C8, 0x12CFC0E0,
                                0x4967E070, 0x64B3F038, 0x74F97398, 0x7CDC3248, 0x38CE92A0, 0x1C674950,
                                0x0E33A4A8, 0x01B959D0, 0x40DCACE8, 0x26CEDDF0
                             };
static const uint32_t C1[] = { 0x846B5, 0x4235A, 0x211AD};
static const uint32_t C2[] = { 0x1A822E0, 0x21A822E0, 0x21A822E0};

/** Reverse 64 bits of keystream into possible cipher states
 * Variation mentioned in the paper. Somewhat optimized version
 */
Crypto1State *lfsr_recovery64(uint32_t ks2, uint32_t ks3) {
    Crypto1State *statelist, *sl;
    uint8_t oks[32], eks[32], hi[32];
    uint32_t low = 0,  win = 0;
    uint32_t *tail, table[1 << 16];
    int i, j;

    sl = statelist = calloc(1, sizeof(Crypto1State) << 4);
    if (!sl)
        return 0;
    sl->odd = sl->even = 0;

    for (i = 30; i >= 0; i -= 2) {
        oks[i >> 1] = BEBIT(ks2, i);
        oks[16 + (i >> 1)] = BEBIT(ks3, i);
    }
    for (i = 31; i >= 0; i -= 2) {
        eks[i >> 1] = BEBIT(ks2, i);
        eks[16 + (i >> 1)] = BEBIT(ks3, i);
    }

    for (i = 0xfffff; i >= 0; --i) {
        if (filter(i) != oks[0])
            continue;

        *(tail = table) = i;
        for (j = 1; tail >= table && j < 29; ++j)
            extend_table_simple_2(table,&tail, oks[j]);

        if (tail < table)
            continue;

        for (j = 0; j < 19; ++j)
            low = low << 1 | (evenparity32(i & S1[j]));
        for (j = 0; j < 32; ++j)
            hi[j] = evenparity32(i & T1[j]);

        for (; tail >= table; --tail) {
            for (j = 0; j < 3; ++j) {
                *tail = *tail << 1;
                *tail |= evenparity32((i & C1[j]) ^ (*tail & C2[j]));
                if (filter(*tail) != oks[29 + j])
                    goto continue2;
            }

            for (j = 0; j < 19; ++j)
                win = win << 1 | (evenparity32(*tail & S2[j]));

            win ^= low;
            for (j = 0; j < 32; ++j) {
                win = win << 1 ^ hi[j] ^ (evenparity32(*tail & T2[j]));
                if (filter(win) != eks[j])
                    goto continue2;
            }

            *tail = *tail << 1 | (evenparity32(LF_POLY_EVEN & *tail));
            sl->odd = *tail ^ (evenparity32(LF_POLY_ODD & win));
            sl->even = win;
            ++sl;
            sl->odd = sl->even = 0;
continue2:
            ;
        }
    }
    return statelist;
}

uint8_t lfsr_rollback_bit(Crypto1State *s, int j, int in, int fb)
{
    int out;
    uint8_t ret;
    int t;

    s[j].odd &= 0xffffff;
    t = s[j].odd;
    s[j].odd = s[j].even;
    s[j].even = t;

    out = s[j].even & 1;
    out ^= LF_POLY_EVEN & (s[j].even >>= 1);
    out ^= LF_POLY_ODD & s[j].odd;
    out ^= (in != 0) ? 1 : 0;
    out ^= (ret = (uint8_t)filter(s[j].odd)) & ((fb != 0) ? 1 : 0);

    s[j].even |= evenparity32(out) << 23;
    return ret;
}

int lfsr_rollback_word( Crypto1State *s, int t, int in, int fb) {

    int i;
    int ret = 0;
    for (i = 31; i >= 0; --i)
        ret |= lfsr_rollback_bit(s, t, BEBIT(in, i), fb) << (i ^ 24);
    return ret;
}

uint64_t crypto1_get_lfsr(Crypto1State *state, int t, uint64_t lfsr){

    int i;
    for(lfsr = 0, i = 23; i >= 0; --i) {
        lfsr = lfsr << 1 | BIT(state[t].odd, i ^ 3);
        lfsr = lfsr << 1 | BIT(state[t].even, i ^ 3);
    }
    return lfsr;
}

uint8_t crypto1_bit(Crypto1State *s, int t, int in, int is_encrypted)
{
    int feedin;
    uint8_t ret = (uint8_t)filter(s[t].odd);

    feedin  = ret & ((is_encrypted != 0) ? 1 : 0);
    feedin ^= ((in != 0) ? 1 : 0);
    feedin ^= LF_POLY_ODD & s[t].odd;
    feedin ^= LF_POLY_EVEN & s[t].even;
    s[t].even = s[t].even << 1 | evenparity32(feedin);

    s[t].odd ^= s[t].even;
    s[t].even ^= s[t].odd;;
    s[t].odd ^= s[t].even;

    return ret;
}

int crypto1_word(Crypto1State *s, int t, int in, int is_encrypted)
{
    int i, ret = 0;

    for (i = 0; i < 32; ++i) {
        ret |= crypto1_bit(s, t, BEBIT(in, i), is_encrypted) << (i ^ 24);
    }

    return ret;
}

/******************************************************************************************************/
/*                                          ����˹ FMKEY END                                           */
/******************************************************************************************************/


/*************************************************************************************************************************/

// uint32_t uid    = 0;
// uint32_t chal   = 0;
// uint32_t rchal  = 0;
// uint32_t rresp  = 0;
// uint32_t chal2  = 0;
// uint32_t rchal2 = 0;
// uint32_t rresp2 = 0;
// uint64_t key    = 0;

// typedef struct
// {
//     uint32_t uid;
//     uint32_t chal[4];
//     uint32_t rchal[4];
//     uint32_t rresp[4];
//     uint64_t key;
// } CraptoData;

// bool RecoveryKey() {

//     Crypto1State *s;
//     int  t;

//     s = lfsr_recovery32(rresp ^ prng_successor(chal, 64), 0);

//     for(t = 0; (s[t].odd != 0) | (s[t].even != 0); ++t) {
//         lfsr_rollback_word(s, t, 0, 0);
//         lfsr_rollback_word(s, t, rchal, 1);
//         lfsr_rollback_word(s, t, uid ^ chal, 0);
//         key = crypto1_get_lfsr(s, t, key);
//         crypto1_word(s, t, uid ^ chal2, 0);
//         crypto1_word(s, t, rchal2, 1);
//         if (rresp2 == (crypto1_word(s, t, 0, 0) ^ prng_successor(chal2, 64))){
//             return true;
//         }
//     }
//     return false;
// }

/*************************************************************************************************************************/

// #define _KEY_FLOW_NUM 20 //��������������¼������2�ı�����2������һ����Կ

// uint64_t UID = 0xAABBCCDD;
// uint8_t buffer[128];  //���ܵ�������

// typedef struct
// {
//     uint8_t filter;
//     uint8_t nkey;
//     uint8_t keyAB[2];
//     uint8_t blockNumber[2];
//     uint32_t TagChall[2];
//     uint32_t ReadChall[2];
//     uint32_t ReadResp[2];
// } Sniff;

// int nSniff=_KEY_FLOW_NUM;

// Sniff sn[_KEY_FLOW_NUM];

int ByteArrayToInt(uint8_t *b, int n) {
    int s = 0xFF & b[n + 0];
    s <<= 8;
    s |= 0xFF & b[n + 1];
    s <<= 8;
    s |= 0xFF & b[n + 2];
    s <<= 8;
    s |= 0xFF & b[n + 3];
    return s;
}

// bool getsniff(int jsn) {

//     sn[jsn].filter = /*buffer[3]*/0;
//     sn[jsn].nkey = /*buffer[4]*/1;

//     int a = 5;
//     for (int i = 0; i < sn[jsn].nkey; i++) {
//         sn[jsn].keyAB[i] = buffer[a++];
//         sn[jsn].blockNumber[i] = buffer[a++];
//         sn[jsn].TagChall[i] = ByteArrayToInt(buffer, a);
//         a += 4;
//         sn[jsn].ReadChall[i] = ByteArrayToInt(buffer, a);
//         a += 4;
//         sn[jsn].ReadResp[i] = ByteArrayToInt(buffer, a);
//         a += 4;
//     }
//     return true;
// }

// typedef struct Craptor1
// {
//     uint64_t key;
//     uint8_t block;
//     uint8_t AB;
// } CryptoKey;

// bool NoDuble(CryptoKey *crk, int jcrk, uint64_t key, uint8_t block, uint8_t AB){
//     for(int i = 0; i < jcrk; i++){
//         if((crk[i].key == key ) &&
//                 (crk[i].block == block) &&
//                 (crk[i].AB == AB)){
//             return false;
//         }
//     }
//     return true;
// }

// CryptoKey Crk[_KEY_FLOW_NUM/2];  //����Ӧȡ����jcrk�Ĵ���,�ٱ�2022/9/2
// int jcrk = 0;
// void CulcKeys()
// {

//     int n = sn[0].nkey *(nSniff - 1)*nSniff / 2; //2=1 3=3 4=6 5=10 6=15
//     //int jcrk = 0;
//     uid = UID;

//     for (int i = 0; i < sn[0].nkey; i++) {
//         for (int j1 = 0; j1 < (nSniff - 1); j1++) {
//             for (int j2 = (j1 + 1); j2 < nSniff; j2++) {
//                 if((sn[j1].blockNumber[i] == sn[j2].blockNumber[i]) &&
//                         (sn[j1].keyAB[i] == sn[j2].keyAB[i])){
//                     chal = sn[j1].TagChall[i];
//                     rchal = sn[j1].ReadChall[i];
//                     rresp = sn[j1].ReadResp[i];

//                     chal2 = sn[j2].TagChall[i];
//                     rchal2 = sn[j2].ReadChall[i];
//                     rresp2 = sn[j2].ReadResp[i];
//                     key = -1L;


//                     if (RecoveryKey()) {
//                         if(NoDuble (Crk, jcrk, key, sn[j1].blockNumber[i],
//                                 sn[j1].keyAB[i])){
//                             Crk[jcrk].key = key;
//                             Crk[jcrk].block = sn[j1].blockNumber[i];
//                             Crk[jcrk].AB = sn[j1].keyAB[i];
//                             jcrk++;
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

// void crapto1()
// {
    
//     /************************************************/
//     printf("\r\n����������,�س�ȷ��\r\n");
//     scanf("%d",&nSniff);
//     /************************************************/

//     /************************************************/
//     printf("\r\n����8λUID��(4 Byte),�س�ȷ��\r\n");
//     scanf("%x %x %x %x",\
//     buffer+0,buffer+1,buffer+2,buffer+3,buffer+4);
//     UID=ByteArrayToInt(buffer,0);
//     /************************************************/

//     for(int i=0;i<nSniff;i++)
//     {
//         /************************************************/
//         printf("\r\n��%d���������,��%d��,����14λ����,�ո�����س�����!\r\n",i+1,nSniff);
//         scanf("%x %x %x %x %x %x %x %x %x %x %x %x %x %x",\
//         buffer+ 5,buffer+ 6,buffer+ 7,buffer+ 8,buffer+ 9,\
//         buffer+10,buffer+11,buffer+12,buffer+13,buffer+14,buffer+15,buffer+16,buffer+17,buffer+18\
//         );
//         buffer[5]-=0x60; //0:keyA 1:keyB
//         /************************************************/
//         getsniff(i);
//     }
    
//     printf("\r\n���Ժ�....\r\n");
//     CulcKeys();

//     printf("\r\n������¼: %d����Կ---------------------------------------------",jcrk);
//     uint8_t* StrKeyAB[2] = {"KeyA", "KeyB"};
//     for(int i=0;i<jcrk;i++)
//         printf("\r\n��ͷ��֤��ַ(��):%02d ��Կ����:%s �ػ����Կ(%d) = %012llX",Crk[i].block,StrKeyAB[Crk[i].AB],i, Crk[i].key );

//     memset(buffer,0,128);




// }





/******************************************************************************************************/

/*
 * ��ɫ����ֲ����
 * �����ն����
 * WindowsAPI���ڱ�̣�������ģ�https://www.cnblogs.com/milanleon/p/4244267.html
 * Դ�Դ����ķ�֧��https://blog.csdn.net/qq_44829047/article/details/106448325
 * ���߳����ݴ���������ģ�https://blog.csdn.net/yizhizainulii/article/details/124297432
 * Դ�Դ����ķ�֧��https://blog.csdn.net/yizhizainulii/article/details/124297432
 */

#define SCREEN_CLEAR  "\033[2J\033[3J\033[1;1H"

#define FMT_ANSIC_WHITE_BR          "\033[97m"

#define AEND         "\x1b[0m"
#define ACON         ""

#define _RED_BR_(s)     "\x1b[91m" s AEND
#define _YELLOW_BR_(s)  "\x1b[93m" s AEND
#define _GREEN_BR_(s)   "\x1b[92m" s AEND
#define _WHITE_BR_(s)   "\x1b[97m" s AEND
#define _CYAN_(s)       "\x1b[36m" s AEND
#define _WHITE_(s)      "\x1b[37m" s AEND

#define _ANSI_RED_BR_(s)     "\x1b[91m" s ACON
#define _ANSI_YELLOW_BR_(s)  "\x1b[93m" s ACON
#define _ANSI_GREEN_BR_(s)   "\x1b[92m" s ACON
#define _ANSI_WHITE_BR_(s)   "\x1b[97m" s ACON
#define _ANSI_CYAN_(s)       "\x1b[36m" s ACON
#define _ANSI_WHITE_(s)      "\x1b[37m" s ACON

#define OPTIONAL_ANSWER_TRAILER   _WHITE_BR_("\r\n\r\n[") _GREEN_BR_("usb") FMT_ANSIC_WHITE_BR "] -> "



/* ------------------------------------------------------ */
/*                           ����                          */
/* ------------------------------------------------------ */
#define UNUSED(X) (void)X      /* ��������gcc/g++���� */
/* ------------------------------------------------------ */


//����ִ��logProcessAuth�Գ�ʼ����Դ

uint8_t autoMfkey32src[256][18]={0}; //ִ��logProcessAuth����ʱ��ȡ�ľ����¼
uint8_t autoMfkey32srcNum=0;         //ִ��logProcessAuth����ʱ��ȡ�ľ����¼��Ŀ����

//��ӡ��ʼ����Դ
void autoMfkey32srcPrint()
{
    printf("\r\n-----------------����mfkey32v2��Կ�ָ���������־-----------------\r\n");
    
    for(uint8_t i=0;i<autoMfkey32srcNum;i++)
    {
        printf("��Ŀ:%03d | ",i);
        for(int j =0;j<18;j++)
            printf("%02X ",autoMfkey32src[i][j]);
        printf("\r\n");
    }
    printf("\r\n");
}

// int ByteArrayToInt(uint8_t *b, int n) {
//     int s = 0xFF & b[n + 0];
//     s <<= 8;
//     s |= 0xFF & b[n + 1];
//     s <<= 8;
//     s |= 0xFF & b[n + 2];
//     s <<= 8;
//     s |= 0xFF & b[n + 3];
//     return s;
// }



/*

kardClassNumber = 3

card 1
1111111111111111111------offset[0] = 0
1111111111111111111
:
:
card 2
2222222222222222222------offset[1] = xxx
2222222222222222222
:
:
card 3
3333333333333333333------offset[2] = xxx
3333333333333333333
NULLLLLLLLLLLLLLLLL------offset[3] = xxx

*/

uint8_t kardClassNumber=1; //��Ƭ����
uint8_t offset[256]={0};   //��¼�������Ÿ��ĵ�ƫ�Ƶ�ַ,���256��ƫ�Ƶ�ַ
void kardClass()
{
    int a,b;

    a=ByteArrayToInt(autoMfkey32src[0],2);
    for(uint8_t i=1;i<autoMfkey32srcNum;i++)
    {
        b=ByteArrayToInt(autoMfkey32src[i],2);
        if(a!=b)
        {
            a=b;
            offset[kardClassNumber]=i;
            kardClassNumber++;
        }
    }

    offset[kardClassNumber]=autoMfkey32srcNum+1;//������ַ

    printf("������%d�ſ�Ƭ��¼\r\n",kardClassNumber);
    for(int i=0;i<kardClassNumber;i++)
        printf("��⵽�л���Ƭ����Ŀ%d\r\n",offset[i]);
}

uint8_t secKey[16][128][15]={0};
uint16_t cardStreamNum; //������������

void autoMfkey32cardSecArrangement(uint8_t cardStart,uint8_t cardEnd)
{
    memset(secKey,0,sizeof(secKey));
    cardStreamNum=0;

    //printf("\r\n��ʼ����ת��\r\n");
    for(uint8_t block=0;block<16;block++)
    {
        uint8_t stream=0;
        //������������ ת��
        for(int offset=cardStart;offset<cardEnd;offset++)
        {
            if(autoMfkey32src[offset][0] && autoMfkey32src[offset][1]/4==block)  //�������ݵ�ǰ����,�ж������Կ����֤�ĵ�ַ�ǲ���0...15
            {
                //printf("ת��������:%02d\r\n",block);
                secKey[block][stream][0]=1; //���ݴ��ڱ�־
                memcpy(secKey[block][stream]+1,autoMfkey32src[offset]+6,12); //����12���ֽڽ�ȥ
                stream++;
            }
        }
    }

    int uid=ByteArrayToInt(autoMfkey32src[cardStart],2);
    printf("\r\n����:%08X [�����������ݱ�]\r\n",uid);
    for(uint8_t block=0;block<16;block++)
    {
        printf("����%02d--------------------------------------\r\n",block);
        for(uint8_t stream=0;stream<128;stream++)
        {
            if(secKey[block][stream][0]==1) //���ݴ���
            {
                printf("����%03d: ",stream);
                secKey[block][0][0]=stream+1; //����������������
                for(uint8_t data=1;data<13;data++)
                {
                    printf("%02X ",secKey[block][stream][data]);
                }
                printf("\r\n");
            }
        }
        cardStreamNum+=secKey[block][0][0];      //����������������
    }
}

// bool mfKey32(uint32_t UID ,uint8_t *one ,uint8_t *two)
// {
//     struct Crypto1State *s, *t;
//     uint64_t key;     // recovered key
//     uint32_t uid;     // serial number
//     uint32_t nt0;      // tag challenge first
//     uint32_t nt1;      // tag challenge second
//     uint32_t nr0_enc; // first encrypted reader challenge
//     uint32_t ar0_enc; // first encrypted reader response
//     uint32_t nr1_enc; // second encrypted reader challenge
//     uint32_t ar1_enc; // second encrypted reader response


//     uid     = UID;
//     nt0     = ByteArrayToInt(one,0);
//     nr0_enc = ByteArrayToInt(one,4);
//     ar0_enc = ByteArrayToInt(one,8);
//     nt1     = ByteArrayToInt(two,0);
//     nr1_enc = ByteArrayToInt(two,4);
//     ar1_enc = ByteArrayToInt(two,8);

//     uint32_t p64 = prng_successor(nt0, 64);
//     uint32_t p64b = prng_successor(nt1, 64);

//     s = lfsr_recovery32(ar0_enc ^ p64, 0);

//     for (t = s; t->odd | t->even; ++t) {
//         lfsr_rollback_word(t, 0, 0);
//         lfsr_rollback_word(t, nr0_enc, 1);
//         lfsr_rollback_word(t, uid ^ nt0, 0);
//         crypto1_get_lfsr(t, &key);

//         crypto1_word(t, uid ^ nt1, 0);
//         crypto1_word(t, nr1_enc, 1);
//         if (ar1_enc == (crypto1_word(t, 0, 0) ^ p64b)) {
//             printf("[+] �ҵ���Կ: [" _GREEN_BR_("%012" PRIx64 ) "]\r\n", key);
//             return 1;
//         }
//     }
//     free(s);
//     return 0;
// }



bool  mfKey32v2(uint32_t UID ,uint8_t *one ,uint8_t *two,Crypto1State *statelist,int *odd,int *even)
{
    uint64_t key;     // recovered key
    uint32_t uid;     // serial number
    uint32_t nt0;      // tag challenge first
    uint32_t nt1;      // tag challenge second
    uint32_t nr0_enc; // first encrypted reader challenge
    uint32_t ar0_enc; // first encrypted reader response
    uint32_t nr1_enc; // second encrypted reader challenge
    uint32_t ar1_enc; // second encrypted reader response


    uid     = UID;
    nt0     = ByteArrayToInt(one,0);
    nr0_enc = ByteArrayToInt(one,4);
    ar0_enc = ByteArrayToInt(one,8);
    nt1     = ByteArrayToInt(two,0);
    nr1_enc = ByteArrayToInt(two,4);
    ar1_enc = ByteArrayToInt(two,8);


    Crypto1State *s;
    int  t;

    clock_t start,stop;
    start = clock();

    s = lfsr_recovery32(ar0_enc ^ prng_successor(nt0, 64), 0,statelist,odd,even);

    for(t = 0; (s[t].odd != 0) | (s[t].even != 0); ++t) {
        lfsr_rollback_word(s, t, 0, 0);
        lfsr_rollback_word(s, t, nr0_enc, 1);
        lfsr_rollback_word(s, t, uid ^ nt0, 0);
        key = crypto1_get_lfsr(s, t, key);
        crypto1_word(s, t, uid ^ nt1, 0);
        crypto1_word(s, t, nr1_enc, 1);
        if (ar1_enc == (crypto1_word(s, t, 0, 0) ^ prng_successor(nt1, 64))){
            stop = clock();
            printf("\r[+] ��ʱ:%4d ms �ҵ���Կ: [ " _GREEN_BR_("%012" PRIx64 ) " ]      \r\n",stop - start,key);
            return true;
        }
    }
    return false;


}


int mfkey(void) {

    int argc;

    printf("����5:һ���Իָ���Կ\n����7:�����λỰ�лָ���Կ\n");
    scanf("%d",&argc);

    if (argc == 7) {
        Crypto1State *statelist =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
        int *odd  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
        int *even =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];

        Crypto1State *s;
        int  t;

        uint64_t key;     // recovered key
        uint32_t uid;     // serial number
        uint32_t nt0;      // tag challenge first
        uint32_t nt1;      // tag challenge second
        uint32_t nr0_enc; // first encrypted reader challenge
        uint32_t ar0_enc; // first encrypted reader response
        uint32_t nr1_enc; // second encrypted reader challenge
        uint32_t ar1_enc; // second encrypted reader response
        uint32_t ks2;     // keystream used to encrypt reader response

        printf("\n");
        printf("-----------------------------------------------------\n");
        printf("MIFARE Classic��Կ�ָ�-����32λ��Կ�� �汾v2\n");
        printf("��������32λ��ȡ�������֤�𰸻ָ���Կ\n");
        printf("����汾ʵ����Moebius���ֲ�ͬ��nonce�������(�糬����)\n");
        printf("-----------------------------------------------------\n\n");
        printf("����: <uid> <nt> <nr_0> <ar_0> <nt1> <nr_1> <ar_1>\n\n");
    
        scanf("%x %x %x %x %x %x %x", &uid, &nt0, &nr0_enc, &ar0_enc, &nt1, &nr1_enc, &ar1_enc);

        printf("\n���ڴ����¸����������лָ�����Կ:\n");
        printf("    uid: %08x\n", uid);
        printf("   nt_0: %08x\n", nt0);
        printf(" {nr_0}: %08x\n", nr0_enc);
        printf(" {ar_0}: %08x\n", ar0_enc);
        printf("   nt_1: %08x\n", nt1);
        printf(" {nr_1}: %08x\n", nr1_enc);
        printf(" {ar_1}: %08x\n", ar1_enc);

        // Generate lfsr successors of the tag challenge
        printf("\nLFSR successors of the tag challenge:\n");
        uint32_t p64 = prng_successor(nt0, 64);
        uint32_t p64b = prng_successor(nt1, 64);

        printf("  nt': %08x\n", p64);
        printf(" nt'': %08x\n", prng_successor(p64, 32));

        // Extract the keystream from the messages
        printf("\n��������{ar}��{at}����Կ��:\n");
        ks2 = ar0_enc ^ p64;
        printf("  ks2: %08x\n", ks2);

        s = lfsr_recovery32(ar0_enc ^ p64, 0,statelist,odd,even);

        for(t = 0; (s[t].odd != 0) | (s[t].even != 0); ++t) {
            lfsr_rollback_word(s, t, 0, 0);
            lfsr_rollback_word(s, t, nr0_enc, 1);
            lfsr_rollback_word(s, t, uid ^ nt0, 0);
            key = crypto1_get_lfsr(s, t, key);

            crypto1_word(s, t, uid ^ nt1, 0);
            crypto1_word(s, t, nr1_enc, 1);
            if (ar1_enc == (crypto1_word(s, t, 0, 0) ^ p64b)) {
                printf("\n�ҵ���Կ: [%012" PRIx64 "]", key);
                break;
            }
        }
        free(statelist);
        free(odd);
        free(even);

        return 0;
    } else if(argc == 5) {

        Crypto1State *revstate;

        uint64_t key;     // recovered key
        uint32_t uid;     // serial number
        uint32_t nt;      // tag challenge
        uint32_t nr_enc;  // encrypted reader challenge
        uint32_t ar_enc;  // encrypted reader response
        uint32_t at_enc;  // encrypted tag response
        uint32_t ks2;     // keystream used to encrypt reader response
        uint32_t ks3;     // keystream used to encrypt tag response

        printf("\n");
        printf("-----------------------------------------------------\n");
        printf("MIFARE Classic��Կ�ָ�-����64λ��Կ��\n");
        printf("����һ�����������֤�лָ���Կ!\n");
        printf("-----------------------------------------------------\n\n");
        printf("����: <uid> <nt> <{nr}> <{ar}> <{at}>\n\n");

        scanf("%x %x %x %x %x", &uid, &nt, &nr_enc, &ar_enc, &at_enc);

        printf("\n���ڴ����¸����������лָ�����Կ:\n");

        printf("  uid: %08x\n", uid);
        printf("   nt: %08x\n", nt);
        printf(" {nr}: %08x\n", nr_enc);
        printf(" {ar}: %08x\n", ar_enc);
        printf(" {at}: %08x\n", at_enc);

        // Generate lfsr successors of the tag challenge
        printf("\nLFSR successors of the tag challenge:\n");
        uint32_t p64 = prng_successor(nt, 64);
        printf("  nt': %08x\n", p64);
        printf(" nt'': %08x\n", prng_successor(p64, 32));

        // Extract the keystream from the messages
        printf("\n��������{ar}��{at}����Կ��:\n");
        ks2 = ar_enc ^ p64;
        ks3 = at_enc ^ prng_successor(p64, 32);
        printf("  ks2: %08x\n", ks2);
        printf("  ks3: %08x\n", ks3);

        revstate = lfsr_recovery64(ks2, ks3);

        lfsr_rollback_word(revstate, 0, 0, 0);
        lfsr_rollback_word(revstate, 0, 0, 0);
        lfsr_rollback_word(revstate, 0, nr_enc, 1);
        lfsr_rollback_word(revstate, 0, uid ^ nt, 0);
        key = crypto1_get_lfsr(revstate, 0, key);
        printf("\n�ҵ���Կ: [%012" PRIx64 "]", key);
        free(revstate);

        return 0;
    }
}



// #include <stdio.h>
// #include <stdbool.h>
// #include <windows.h>
 
// const unsigned int THREAD_NUM = 10;

// DWORD WINAPI  ThreadFunc(LPVOID p)
// {
//     int n = *(int*)p;
//     Sleep(1000*n);  //�� n ���߳�˯�� n ��
//     printf("���ǣ� pid = %d �����߳�\t", GetCurrentThreadId());   //������߳�pid
//     printf(" pid = %d �����߳��˳�\n\n", GetCurrentThreadId());   //��ʱ10s�����
 
//     return 0;
// }
 
// int main()
// {
//     printf("�������̣߳� pid = %d\n", GetCurrentThreadId());  //������߳�pid
//     HANDLE hThread[THREAD_NUM];
//     for (int i = 0; i < THREAD_NUM; i++)
//     {
//         hThread[i] = CreateThread(NULL, 0, ThreadFunc, &i, 0, NULL); // �����߳�
//     }
    
//     WaitForMultipleObjects(THREAD_NUM,hThread,true, INFINITE);  //һֱ�ȴ���ֱ���������߳�ȫ������
//     return 0;
// }
 



typedef struct ArgList
{
    uint32_t UID;
    uint8_t * one;
    uint8_t * two;
    uint8_t status;
    Crypto1State *statelist;
    int *odd;
    int *even;
    uint8_t block;
    uint8_t a;
}ArgList_t;

// void Mfkey32Process(LPVOID p)
// {
//     // ArgList_t *argList_t=(ArgList_t*)p;

//     // if(mfKey32(argList_t->UID,argList_t->one+1,argList_t->two+1)) //�ȶԳɹ�
//     // {
//     //     argList_t->one[13]=1; //�Ѵ���
//     //     argList_t->two[13]=1; //�Ѵ���
//     //     argList_t->status=1;
//     // }



//     /***********************************/

//     ArgList_t *argList_t=(ArgList_t*)p;
//     if(mfKey32v2(argList_t->UID,argList_t->one+1,argList_t->two+1,argList_t->statelist,argList_t->odd,argList_t->even)) //�ȶԳɹ�
//     {
//         argList_t->one[13]=1; //�Ѵ���
//         argList_t->two[13]=1; //�Ѵ���
//         argList_t->status=1;
//     }


// }

uint8_t secKeyFindSuccess;
uint16_t keyFindSuccessNum;
uint8_t scheduleNum;

// void Mfkey32ProcessFast(LPVOID p)
// {
//     ArgList_t *argList_t=(ArgList_t*)p;

//     uint8_t block=argList_t->block;

//     for(uint8_t a=argList_t->a;a<128;a++)  //ǰ��ָ��
//     {
//         if(secKey[block][a][0] && !secKey[block][a][13])  //����δ���������
//         {
//             uint8_t b=a+1;//��ָ��

//             secKey[block][a][13]++;     //�鶨
//             secKey[block][b][13]++;     //�鶨
//             if(secKey[block][a][13]==1) //�ж���û�б������߳��鶨,�ȵ��ȵ�
//             {
//                 if(secKey[block][b][0])  //����δ���������
//                 {


//                     /*************************************************************************************************************************/
//                     if(mfKey32v2(argList_t->UID,secKey[block][a]+1,secKey[block][b]+1,argList_t->statelist,argList_t->odd,argList_t->even))
//                     {
//                         secKey[block][a][13]=2; //�Ѵ���
//                         secKey[block][b][13]=2; //�Ѵ���

//                         secKeyFindSuccess=1;
//                         keyFindSuccessNum++;
//                     }
//                     else
//                     {
//                         secKey[block][a][13]=3; //��������
//                         secKey[block][b][13]=0; //����ʹ��
//                     }
//                     /*************************************************************************************************************************/

//                     uint8_t state[4]={'-','\\','|','/'};
//                     static char ing=0;
//                     if(ing==4)
//                         ing=0;

//                     //printf("\r[%c] (%02d,%02d) ",state[ing++],a,b);

//                     uint16_t cardScheduleNum=0;
//                     for(uint8_t i=0;i<block;i++)
//                     {
//                         cardScheduleNum+=secKey[i][0][0];//֮ǰ��������������
//                     }
//                     if(a>scheduleNum)
//                     {
//                         scheduleNum=a;
//                     }
//                     cardScheduleNum+=scheduleNum;
//                     //printf("\r %03d %03d %03d",scheduleNum,cardScheduleNum,cardStreamNum);
//                     //printf("\r[%c] ����: %02d %% ��ǰ��������: %02d %%",state[ing++],cardScheduleNum*100/cardStreamNum,scheduleNum*100/secKey[block][0][0]);

//                     char buf[128]={0};
//                     double num = ((double)cardScheduleNum/(double)cardStreamNum)*34;

//                     // for(int i = 0; i < 33; i++)
//                     // {
//                     //     if(i < num) //���num��">"
//                     //         memcpy(buf+i*2,"��",2);
//                     //     else
//                     //         memcpy(buf+i*2,"��",2);
//                     // }
//                     // printf(_YELLOW_BR_("\r[%c] %s��[%02d%%]"),state[ing++],buf,cardScheduleNum*100/cardStreamNum);

//                     static uint8_t color=1,colorLen=0;
//                     for(int i = 0; i < 34; i++)
//                     {
//                         if(i >= num) //��ɫ�ֽ紦
//                         {
//                             if(color)
//                             {
//                                 colorLen=strlen(AEND);  //��ɫ
//                                 memcpy(buf+i*2,AEND,colorLen);
//                                 color=0;
//                             }
//                             memcpy(buf+i*2+colorLen,"��",2);
//                         }
//                         else
//                         {
//                             memcpy(buf+i*2+colorLen,"��",2);
//                         }
                        
//                     }
//                     color=1,colorLen=0;
//                     printf(_YELLOW_BR_("\r[%c] %s(%02d%%)"),state[ing++],buf,cardScheduleNum*100/cardStreamNum);


//                 }
//             }
//         }
//     }
// }



uint8_t decryptionLevel=1; //���ܼ��� 0���ٽ� 1��ͨ�� 2��ȫ����

void autoMfkey32Fast()
{
    // autoMfkey32srcPrint();//���������ӡ,������ӡ...
    // kardClass();          //���ɻ�ȡ��Ƭ������ƫ�Ƶ�ַ

    // clock_t start,stop;
    // start = clock();        /*  ��ʼ��ʱ  */

    // keyFindSuccessNum=0;

    // for(uint8_t card=0;card<kardClassNumber;card++)
    // {
    //     printf(_YELLOW_BR_("\r\n[+] ��ʼ�����%d�ſ�Ƭ��Կ,��%d��.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] �������� ��ע������ɢ��."));
    //     autoMfkey32cardSecArrangement(offset[card],offset[card+1]); //��ȡ�������������

    //     uint32_t uid=(uint32_t )ByteArrayToInt(autoMfkey32src[offset[card]],2);
    //     printf(_YELLOW_BR_("\r\n[+] �������ܿ�ʼ.��%d��,��%d��.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] ��ע������ɢ��."));
    //     printf(_YELLOW_BR_("\r\n[+] UID:%08X\r\n"),uid);
    //     printf(_YELLOW_BR_("[+] ���ڽ�����Կ...\r\n"));


    //     Crypto1State *statelist =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist1 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd1  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even1 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist2 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd2  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even2 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist3 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd3  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even3 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist4 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd4  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even4 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist5 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd5  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even5 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];

    //     const unsigned int THREAD_NUM = 6;
    //     HANDLE hThread[THREAD_NUM];

        

    //     for(uint8_t block=0;block<16;block++)  //16��������λָ�
    //     {
    //         printf(_WHITE_BR_("\r����%02d-------------------------------------\r\n"),block);

    //         if(!secKey[block][0][0]) //����������������
    //         {
    //             printf(_RED_BR_("[!] ��������������.\r\n"));
    //         }
    //         else  //��������������
    //         {
    //             secKeyFindSuccess=0;
    //             scheduleNum=0;

    //             ArgList_t argList_t={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t.UID=uid;
    //             argList_t.statelist=statelist;
    //             argList_t.odd=odd;
    //             argList_t.even=even;
    //             argList_t.block=block;
    //             argList_t.a=0;
    //             hThread[0] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t, 0, NULL); // �����߳�

    //             ArgList_t argList_t1={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t1.UID=uid;
    //             argList_t1.statelist=statelist1;
    //             argList_t1.odd=odd1;
    //             argList_t1.even=even1;
    //             argList_t1.block=block;
    //             argList_t1.a=2;
    //             hThread[1] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t1, 0, NULL); // �����߳�

    //             ArgList_t argList_t2={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t2.UID=uid;
    //             argList_t2.statelist=statelist2;
    //             argList_t2.odd=odd2;
    //             argList_t2.even=even2;
    //             argList_t2.block=block;
    //             argList_t2.a=4;
    //             hThread[2] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t2, 0, NULL); // �����߳�

    //             ArgList_t argList_t3={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t3.UID=uid;
    //             argList_t3.statelist=statelist3;
    //             argList_t3.odd=odd3;
    //             argList_t3.even=even3;
    //             argList_t3.block=block;
    //             argList_t3.a=6;
    //             hThread[3] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t3, 0, NULL); // �����߳�

    //             ArgList_t argList_t4={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t4.UID=uid;
    //             argList_t4.statelist=statelist4;
    //             argList_t4.odd=odd4;
    //             argList_t4.even=even4;
    //             argList_t4.block=block;
    //             argList_t4.a=8;
    //             hThread[4] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t4, 0, NULL); // �����߳�

    //             ArgList_t argList_t5={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t5.UID=uid;
    //             argList_t5.statelist=statelist5;
    //             argList_t5.odd=odd5;
    //             argList_t5.even=even5;
    //             argList_t5.block=block;
    //             argList_t5.a=10;
    //             hThread[5] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t5, 0, NULL); // �����߳�

    //             WaitForMultipleObjects(THREAD_NUM,hThread,true, INFINITE);  //һֱ�ȴ���ֱ���������߳�ȫ������

    //             if(!secKeyFindSuccess)
    //                 printf(_RED_BR_("\r[!] δ�ҵ���Կ.                             \r\n"));
    //         }
    //     }
    //     printf("\r%*s",43," ");//���43���ո�
        
    //     free(statelist);
    //     free(odd);
    //     free(even);
    //     free(statelist1);
    //     free(odd1);
    //     free(even1);
    //     free(statelist2);
    //     free(odd2);
    //     free(even2);
    //     free(statelist3);
    //     free(odd3);
    //     free(even3);
    //     free(statelist4);
    //     free(odd4);
    //     free(even4);
    //     free(statelist5);
    //     free(odd5);
    //     free(even5);
    // }

    // stop = clock();     /*  ֹͣ��ʱ  */

    // printf("\r\n����ʱ��: %d s\r\n",(stop - start) / CLK_TCK);
    // printf("�����%d����Կ����\r\n",keyFindSuccessNum);
    // printf(_YELLOW_BR_("��־�ļ����������ѽ������.\r\n"));

    // // memset(autoMfkey32src,0,sizeof(autoMfkey32src)); //ִ��logProcessAuth����ʱ��ȡ�ľ����¼
    // // autoMfkey32srcNum=0;                             //ִ��logProcessAuth����ʱ��ȡ�ľ����¼��Ŀ����
    // kardClassNumber=1;                               //��Ƭ����
    // memset(offset,0,sizeof(offset));                 //��¼�������Ÿ��ĵ�ƫ�Ƶ�ַ,���256��ƫ�Ƶ�ַ
    // memset(secKey,0,sizeof(secKey));
}

void autoMfkey32()
{
    // autoMfkey32srcPrint();//���������ӡ,������ӡ...
    // kardClass();          //���ɻ�ȡ��Ƭ������ƫ�Ƶ�ַ

    // clock_t start,stop;
    // start = clock();        /*  ��ʼ��ʱ  */

    // uint16_t keyFindSuccessNum=0;

    // for(uint8_t card=0;card<kardClassNumber;card++)
    // {
    //     printf(_YELLOW_BR_("\r\n[+] ��ʼ�����%d�ſ�Ƭ��Կ,��%d��.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] �������� ��ע������ɢ��."));
    //     autoMfkey32cardSecArrangement(offset[card],offset[card+1]); //��ȡ�������������

    //     uint32_t uid=(uint32_t )ByteArrayToInt(autoMfkey32src[offset[card]],2);
    //     printf(_YELLOW_BR_("\r\n[+] �������ܿ�ʼ.��%d��,��%d��.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] ��ע������ɢ��."));
    //     printf(_YELLOW_BR_("\r\n[+] UID:%08X\r\n"),uid);
    //     printf(_YELLOW_BR_("[+] ���ڽ�����Կ...\r\n"));



    //     Crypto1State *statelist =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist1 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd1  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even1 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist2 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd2  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even2 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist3 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd3  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even3 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];
    //     Crypto1State *statelist4 =(Crypto1State*)malloc((1 << 18)*sizeof(Crypto1State));    //static Crypto1State statelist[1 << 18];
    //     int *odd4  =(int*)malloc((1 << 21)*sizeof(int));   //static int odd[1 << 21];
    //     int *even4 =(int*)malloc((1 << 21)*sizeof(int));   //static int even[1 << 21];

    //     const unsigned int THREAD_NUM = 5;
    //     HANDLE hThread[THREAD_NUM];

    //     uint8_t secKeyFindSuccess;

    //     for(uint8_t block=0;block<16;block++)  //16��������λָ�
    //     {
    //         printf(_WHITE_BR_("����%02d-------------------------------------\r\n"),block);
    //         secKeyFindSuccess=0;

    //         if(!secKey[block][0][0]) //����������������
    //         {
    //             printf(_RED_BR_("[!] ��������������.\r\n"));
    //         }
    //         else  //��������������
    //         {

    //             for(uint8_t a=0;a<128;a++)           //ǰ��ָ��
    //             {
    //                 if(secKey[block][a][0] && !secKey[block][a][13])          //����δ���������
    //                 {
    //                     for(uint8_t b=a+1;b<128;b++) //��ָ��
    //                     {
    //                         if(secKey[block][b][0] && !secKey[block][b][13])  //����δ���������
    //                         {
    //                             uint8_t state[4]={'-','\\','|','/'};
    //                             static char ing=0;
    //                             if(ing==4)
    //                                 ing=0;
    //                             printf("\r[%c] (%02d,%02d) ",state[ing++],a,b);

    //                             ArgList_t argList_t={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t.UID=uid;
    //                             argList_t.one=secKey[block][a];
    //                             argList_t.two=secKey[block][b];
    //                             argList_t.statelist=statelist;
    //                             argList_t.odd=odd;
    //                             argList_t.even=even;
    //                             hThread[0] = CreateThread(NULL, 0, Mfkey32Process, &argList_t, 0, NULL); // �����߳�

    //                             while(secKey[block][++b][13]);//����δ���������
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t1={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t1.UID=uid;
    //                             argList_t1.one=secKey[block][a];
    //                             argList_t1.two=secKey[block][b];
    //                             argList_t1.statelist=statelist1;
    //                             argList_t1.odd=odd1;
    //                             argList_t1.even=even1;
    //                             hThread[1] = CreateThread(NULL, 0, Mfkey32Process, &argList_t1, 0, NULL); // �����߳�

    //                             while(secKey[block][++b][13]);//��һ��δ���������
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t2={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t2.UID=uid;
    //                             argList_t2.one=secKey[block][a];
    //                             argList_t2.two=secKey[block][b];
    //                             argList_t2.statelist=statelist2;
    //                             argList_t2.odd=odd2;
    //                             argList_t2.even=even2;
    //                             hThread[2] = CreateThread(NULL, 0, Mfkey32Process, &argList_t2, 0, NULL); // �����߳�

    //                             while(secKey[block][++b][13]);//��һ��δ���������
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t3={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t3.UID=uid;
    //                             argList_t3.one=secKey[block][a];
    //                             argList_t3.two=secKey[block][b];
    //                             argList_t3.statelist=statelist3;
    //                             argList_t3.odd=odd3;
    //                             argList_t3.even=even3;
    //                             hThread[3] = CreateThread(NULL, 0, Mfkey32Process, &argList_t3, 0, NULL); // �����߳�

    //                             while(secKey[block][++b][13]);//��һ��δ���������
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t4={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t4.UID=uid;
    //                             argList_t4.one=secKey[block][a];
    //                             argList_t4.two=secKey[block][b];
    //                             argList_t4.statelist=statelist4;
    //                             argList_t4.odd=odd4;
    //                             argList_t4.even=even4;
    //                             hThread[4] = CreateThread(NULL, 0, Mfkey32Process, &argList_t4, 0, NULL); // �����߳�
                                
    //                             WaitForMultipleObjects(THREAD_NUM,hThread,true, INFINITE);  //һֱ�ȴ���ֱ���������߳�ȫ������

    //                             printf("\r");
                                
    //                             uint8_t find = argList_t.status + argList_t1.status + argList_t2.status + argList_t3.status + argList_t4.status;
    //                             if(find)
    //                             {
    //                                 secKeyFindSuccess=1;
    //                                 keyFindSuccessNum+=find;
    //                                 break;
    //                             }

    //                             if(decryptionLevel==1) //��ͨ��
    //                                 break;//�Ա�5���ǿ���˳�,���ٶԱ�
                                
    //                             // if(mfKey32(uid,secKey[block][a]+1,secKey[block][b]+1)) //�ȶԳɹ�
    //                             // {
    //                             //     secKey[block][a][13]=1; //�Ѵ���
    //                             //     secKey[block][b][13]=1; //�Ѵ���
    //                             //     break;               //��������ѭ��
    //                             // }
    //                         }
    //                     }
    //                 }
    //             }
    //             if(!secKeyFindSuccess)
    //                 printf(_RED_BR_("[!] δ�ҵ���Կ.\r\n"));
    //         }
    //     }
    //     free(statelist);
    //     free(odd);
    //     free(even);
    //     free(statelist1);
    //     free(odd1);
    //     free(even1);
    //     free(statelist2);
    //     free(odd2);
    //     free(even2);
    //     free(statelist3);
    //     free(odd3);
    //     free(even3);
    //     free(statelist4);
    //     free(odd4);
    //     free(even4);
    // }

    // stop = clock();     /*  ֹͣ��ʱ  */

    // printf("\r\n����ʱ��: %d s\r\n",(stop - start) / CLK_TCK);
    // printf("�����%d����Կ����\r\n",keyFindSuccessNum);
    // printf(_YELLOW_BR_("��־�ļ����������ѽ������.\r\n"));

    // // memset(autoMfkey32src,0,sizeof(autoMfkey32src)); //ִ��logProcessAuth����ʱ��ȡ�ľ����¼
    // // autoMfkey32srcNum=0;                             //ִ��logProcessAuth����ʱ��ȡ�ľ����¼��Ŀ����
    // kardClassNumber=1;                               //��Ƭ����
    // memset(offset,0,sizeof(offset));                 //��¼�������Ÿ��ĵ�ƫ�Ƶ�ַ,���256��ƫ�Ƶ�ַ
    // memset(secKey,0,sizeof(secKey));
}



//����ִ��logProcessAuth�Գ�ʼ����Դ

uint8_t autoMfkey64src[256][22]={0}; //ִ��logProcessAuth����ʱ��ȡ�ľ����¼
uint8_t autoMfkey64srcNum=0;         //ִ��logProcessAuth����ʱ��ȡ�ľ����¼��Ŀ����

void autoMfkey64()
{
    // Crypto1State *revstate;

    // uint64_t key;     // recovered key
    // uint32_t uid;     // serial number
    // uint32_t nt;      // tag challenge
    // uint32_t nr_enc;  // encrypted reader challenge
    // uint32_t ar_enc;  // encrypted reader response
    // uint32_t at_enc;  // encrypted tag response
    // uint32_t ks2;     // keystream used to encrypt reader response
    // uint32_t ks3;     // keystream used to encrypt tag response

    // clock_t start,stop;
    // start = clock();        /*  ��ʼ��ʱ  */
    // printf(AEND);
    // for(uint8_t i=0;i<autoMfkey64srcNum;i++)
    // {
    //     uid    = ByteArrayToInt(autoMfkey64src[i],2);
    //     nt     = ByteArrayToInt(autoMfkey64src[i],6);
    //     nr_enc = ByteArrayToInt(autoMfkey64src[i],10);
    //     ar_enc = ByteArrayToInt(autoMfkey64src[i],14);
    //     at_enc = ByteArrayToInt(autoMfkey64src[i],18);

    //     uint32_t p64 = prng_successor(nt, 64);

    //     ks2 = ar_enc ^ p64;
    //     ks3 = at_enc ^ prng_successor(p64, 32);

    //     revstate = lfsr_recovery64(ks2, ks3);

    //     lfsr_rollback_word(revstate, 0, 0, 0);
    //     lfsr_rollback_word(revstate, 0, 0, 0);
    //     lfsr_rollback_word(revstate, 0, nr_enc, 1);
    //     lfsr_rollback_word(revstate, 0, uid ^ nt, 0);
    //     key = crypto1_get_lfsr(revstate, 0, key);
    //     printf("\r[+] UID:%08X ����/���:(%02d/%02d) key%c:[" _GREEN_BR_("%012" PRIx64 ) "]\r\n",uid,autoMfkey64src[i][1]/4,autoMfkey64src[i][1],autoMfkey64src[i][0]-31, key);
        
    //     /***************************************************************/
    //     uint8_t state[4]={'-','\\','|','/'};
    //     static char ing=0;
    //     if(ing==4)
    //         ing=0;

    //     char buf[128]={0};
    //     double num = ((double)i/(double)autoMfkey64srcNum)*44;

    //     static uint8_t color=1,colorLen=0;
    //     for(int i = 0; i < 44; i++)
    //     {
    //         if(i >= num) //��ɫ�ֽ紦
    //         {
    //             if(color)
    //             {
    //                 colorLen=strlen(AEND);  //��ɫ
    //                 memcpy(buf+i*2,AEND,colorLen);
    //                 color=0;
    //             }
    //             memcpy(buf+i*2+colorLen,"��",2);
    //         }
    //         else
    //         {
    //             memcpy(buf+i*2+colorLen,"��",2);
    //         }
            
    //     }
    //     color=1,colorLen=0;
    //     printf(_YELLOW_BR_("\r[%c] %s��(%02d%%)"),state[ing++],buf,i*100/autoMfkey64srcNum);
    // }
    // stop = clock();     /*  ֹͣ��ʱ  */
    // printf("\r%*s",54," ");//���54���ո�
    // printf("\r\n����ʱ��: %d s\r\n",(stop - start) / CLK_TCK);
    // printf("�����%d����Կ����\r\n",autoMfkey64srcNum);
    // printf(_YELLOW_BR_("��־�ļ����������ѽ������.\r\n"));

    // free(revstate);
}


















#define logo "\x1b[93m\r\n\
              ,----------------,              ,---------,\r\n\
         ,-----------------------,          ,\"        ,\"|\r\n\
       ,\"                      ,\"|        ,\"        ,\"  |\r\n\
      +-----------------------+  |      ,\"        ,\"    |\r\n\
      |  .-----------------.  |  |     +---------+      |\r\n\
      |  |                 |  |  |     | -==----'|      |\r\n\
      |  |  I LOVE PM3!    |  |  |     |         |      |\r\n\
      |  |  Chameleon-Mini |  |  |/----|`---=    |      |\r\n\
      |  |  C:\\>\033[5m_\033[0m\x1b[93m          |  |  |   ,/|==== ooo |      ;\r\n\
      |  |                 |  |  |  // |(((( [33]|    ,\"\r\n\
      |  `-----------------'  |,\" .;'| |((((     |  ,\"\r\n\
      +-----------------------+  ;;  | |         |,\"\r\n\
         /_)______________(_/  //'   | +---------+\r\n\
    ___________________________/___  `,\r\n\
   /  oooooooooooooooo  .o.  oooo /,   \\,\"-----------\r\n\
  / ==ooooooooooooooo==.o.  ooo= //   ,`\\--{)B     ,\"\r\n\
 /_==__==========__==_ooo__ooo=_/'   /___________,\"\r\n\
 \x1b[0m"



#define loadlogo "\r\n\
 ___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___  \r\n\
/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\ \r\n\
\\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/ \r\n\
/   \\___/                                                   \\___/   \\ \r\n\
\\___/                                                           \\___/ \r\n\
/   \\                                                           /   \\ \r\n\
\\___/                                                           \\___/ \r\n\
/   \\       DOWNLOADING SELECTED CARD SLOT DATA !               /   \\ \r\n\
\\___/                                                           \\___/ \r\n\
/   \\        Then you can use winhex to view !!! :)             /   \\ \r\n\
\\___/                                                           \\___/ \r\n\
/   \\                                                           /   \\ \r\n\
\\___/                                                           \\___/ \r\n\
/   \\                               DESIGNED BY PM3 SE Hub      /   \\ \r\n\
\\___/                                                           \\___/ \r\n\
/   \\___                                                     ___/   \\ \r\n\
\\___/   \\___     ___     ___     ___     ___     ___     ___/   \\___/ \r\n\
/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\ \r\n\
\\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/ \r\n\
    \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/   \\___/     \r\n\
"
//  ___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___ 
// /   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \
// \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/
// /   \___/                                                   \___/   \
// \___/                                                           \___/
// /   \                                                           /   \
// \___/                                                           \___/
// /   \       DOWNLOADING SELECTED CARD SLOT DATA !               /   \
// \___/                                                           \___/
// /   \        Then you can use winhex to view !!! :)             /   \
// \___/                                                           \___/
// /   \                                                           /   \
// \___/                                                           \___/
// /   \                               DESIGNED BY PM3 SE Hub      /   \
// \___/                                                           \___/
// /   \___                                                     ___/   \
// \___/   \___     ___     ___     ___     ___     ___     ___/   \___/
// /   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \
// \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/
//     \___/   \___/   \___/   \___/   \___/   \___/   \___/   \___/    
#define loglogo "\r\n\
   .-.     .-.     .-.     .-.     .-.     .-.     .-.     .-.     .-.    \r\n\
 .'   `._.'   `._.'   `._.'   `._.'   `._.'   `._.'   `._.'   `._.'   `.  \r\n\
(    .     .-.     .-.     .-.     .-.     .-.     .-.     .-.     .    ) \r\n\
 `.   `._.'   `._.'   `._.'   `._.'   `._.'   `._.'   `._.'   `._.'   .'  \r\n\
   )    )                                                       (    (    \r\n\
 ,'   ,'                                                         `.   `.  \r\n\
(    (     1 - Specify a log file name                             )    ) \r\n\
 `.   `.                                                         .'   .'  \r\n\
   )    )                                                       (    (    \r\n\
 ,'   ,'   2 - Download logs to the specified file               `.   `.  \r\n\
(    (                                                             )    ) \r\n\
 `.   `.                                                         .'   .'  \r\n\
   )    )  3 - Find the log file in the program directory       (    (    \r\n\
 ,'   ,'                                                         `.   `.  \r\n\
(    (                                                             )    ) \r\n\
 `.   `.   4 - Open log file and enjoy it!                       .'   .'  \r\n\
   )    )      you can view the file with WinHex,               (    (    \r\n\
 ,'   ,'       or preview the parsing results in this program    `.   `.  \r\n\
(    (                                                             )    ) \r\n\
 `.   `.                                                         .'   .'  \r\n\
   )    )       _       _       _       _       _       _       (    (    \r\n\
 ,'   .' `.   .' `.   .' `.   .' `.   .' `.   .' `.   .' `.   .' `.   `.  \r\n\
(    '  _  `-'  _  `-'  _  `-'  _  `-'  _  `-'  _  `-'  _  `-'  _  `    ) \r\n\
 `.   .' `.   .' `.   .' `.   .' `.   .' `.   .' `.   .' `.   .' `.   .'  \r\n\
   `-'     `-'     `-'     `-'     `-'     `-'     `-'     `-'     `-'    \r\n\
"
//    .
//  .' 
// (   
//  `. 
//    )
//  ,' 
// (   
//  `. 
//    )
//  ,' 
// (   
//  `. 
//    )
//  ,' 
// (   
//  `. 
//    )
//  ,' 
// (   
//  `. 
//    )
//  ,' 
// (   
//  `. 
//    `
// #include <stdio.h>
// #include <stdint.h>
// #include <stdbool.h>
// #include <string.h>
// #include <windows.h>
// #include <process.h>

//#include "test.h"

#define INBUFLEN   4096
#define OUTBUFLEN  512

#ifdef _WIN32
HANDLE hCom;  //ȫ�ֱ�����ȫ�ֱ����������߳�ͨ��
DWORD rsize = 0;
DWORD wsize = 0;
#else
size_t rsize = 0;
size_t wsize = 0;
#endif
unsigned char rbuf[INBUFLEN]  = {0};
unsigned char wbuf[OUTBUFLEN] = {0};


char XmodeFlag=0;

char receiveFile[32]={0};

char downloadtype=0;

/*************************************************************************************************/

#define MFBLOCK_SIZE 16
#define UTIL_BUFFER_SIZE_SPRINT 8196


static char b2s(uint8_t v, bool uppercase) {
    // clear higher bits
    v &= 0xF;

    switch (v) {
        case 0xA :
            return (uppercase ? 'A' : 'a') ;
        case 0xB :
            return (uppercase ? 'B' : 'b') ;
        case 0xC :
            return (uppercase ? 'C' : 'c') ;
        case 0xD :
            return (uppercase ? 'D' : 'd') ;
        case 0xE :
            return (uppercase ? 'E' : 'e') ;
        case 0xF :
            return (uppercase ? 'F' : 'f') ;
        default:
            return (char)(v + 0x30);
    }
}

void hex_to_buffer(uint8_t *buf, const uint8_t *hex_data, const size_t hex_len, const size_t hex_max_len,
                   const size_t min_str_len, const size_t spaces_between, bool uppercase) {

    // sanity check
    if (buf == NULL || hex_len < 1)
        return;

    // 1. hex string length.
    // 2. byte array to be converted to string
    //

    size_t max_byte_len = (hex_len > hex_max_len) ? hex_max_len : hex_len;
    size_t max_str_len = (max_byte_len * (2 + spaces_between)) + 1;
    char *tmp_base = (char *)buf;
    char *tmp = tmp_base;

    size_t i;
    for (i = 0; (i < max_byte_len) && (max_str_len > strlen(tmp_base)) ; ++i) {

        *(tmp++) = b2s((hex_data[i] >> 4), uppercase);
        *(tmp++) = b2s(hex_data[i], uppercase);

        for (size_t j = 0; j < spaces_between; j++)
            *(tmp++) = ' ';
    }

    i *= (2 + spaces_between);

    size_t m = (min_str_len > i) ? min_str_len : 0;
    if (m > hex_max_len)
        m = hex_max_len;

    while (m--)
        *(tmp++) = ' ';

    // remove last space
    *tmp = '\0';

}

char *sprint_hex(const uint8_t *data, const size_t len) {
    static char buf[UTIL_BUFFER_SIZE_SPRINT] = {0};
    memset(buf, 0x00, sizeof(buf));
    hex_to_buffer((uint8_t *)buf, data, len, sizeof(buf) - 1, 0, 1, true);
    return buf;
}

char *sprint_hex_ascii(const uint8_t *data, const size_t len) {
    static char buf[UTIL_BUFFER_SIZE_SPRINT + 20] = {0};
    memset(buf, 0x00, sizeof(buf));

    char *tmp = buf;
    size_t max_len = (len > 1010) ? 1010 : len;

    int ret = snprintf(buf, sizeof(buf) - 1, "%s| ", sprint_hex(data, max_len));
    if (ret < 0) {
        goto out;
    }

    size_t i = 0;
    size_t pos = (max_len * 3) + 2;

    while (i < max_len) {
        char c = data[i];
        tmp[pos + i]  = ((c < 32) || (c == 127)) ? '.' : c;
        ++i;

		// uint8_t qh = data[i++]; //�����ַ���Ӧ�ĵ�1���ֽ�
		// uint8_t ql = data[i++]; //�����ַ���Ӧ�ĵ�2���ֽ�

		// /*Ӣ��***********************************************************************************************/
		// if (qh < 0x80) //ASCII
		// {
		// 	i--; //�ո�+1��,�ټ���ȥ
		// 	tmp[pos + i -1]  = ((qh < 32) || (qh == 127)) ? '.' : qh;
		// }
		// /*����***********************************************************************************************/
		// else
		// {
		// 	tmp[pos + i -2] = qh;
		// 	tmp[pos + i -1] = ql;
		// }
    }
out:
    return buf;
}

static void mf_print_block(uint8_t blockno, uint8_t *d) {
    if (blockno == 0) {
        printf("%3d | " _RED_BR_("%s") "\r\n", blockno, sprint_hex_ascii(d, MFBLOCK_SIZE));
    } else if ((blockno+1)%4==0) {
        printf("%3d | " _YELLOW_BR_("%s") "\r\n", blockno, sprint_hex_ascii(d, MFBLOCK_SIZE));
    } else {
        printf("%3d | %s " "\r\n", blockno, sprint_hex_ascii(d, MFBLOCK_SIZE));
    }
}

static void mf_print_blocks(uint16_t n, uint8_t *d) {
    printf("----+-------------------------------------------------+-----------------\r\n");
    printf("blk | data                                            | ascii\r\n");
    printf("----+-------------------------------------------------+-----------------\r\n");
    for (uint16_t i = 0; i < n; i++) {
        mf_print_block(i, d + (i * MFBLOCK_SIZE));
    }
    printf("----+-------------------------------------------------+-----------------\r\n");
}


/*********************************************************************************************/


static void print_log(uint16_t blockno, uint8_t *d) {
    if (blockno%64 == 0) {
        printf("0x%04X | " _RED_BR_("%s") "\r\n", blockno*0x10, sprint_hex_ascii(d, MFBLOCK_SIZE));
    } else if ((blockno+1)%4==0) {
        printf("0x%04X | " _YELLOW_BR_("%s") "\r\n", blockno*0x10, sprint_hex_ascii(d, MFBLOCK_SIZE));
    } else {
        printf("0x%04X | %s " "\r\n", blockno*0x10, sprint_hex_ascii(d, MFBLOCK_SIZE));
    }
}

static void print_logs(uint16_t n, uint8_t *d) {
    printf("-------+-------------------------------------------------+-----------------\r\n");
    printf("offset |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F | ascii\r\n");
    printf("-------+-------------------------------------------------+-----------------\r\n");
    for (uint16_t i = 0; i < n; i++) {
        print_log(i, d + (i * MFBLOCK_SIZE));
    }
    printf("-------+-------------------------------------------------+-----------------\r\n");
}
/*************************************************************************************************/

/* Generic */
#define  LOG_INFO_GENERIC                               0x10  ///< Unspecific log entry.
#define  LOG_INFO_CONFIG_SET                            0x11  ///< Configuration change.
#define  LOG_INFO_SETTING_SET                           0x12  ///< Setting change.
#define  LOG_INFO_UID_SET                               0x13  ///< UID change.
#define  LOG_INFO_RESET_APP                             0x20  ///< Application reset.

/* Codec */
#define  LOG_INFO_CODEC_RX_DATA                         0x40  ///< Currently active codec received data.
#define  LOG_INFO_CODEC_TX_DATA                         0x41  ///< Currently active codec sent data.
#define  LOG_INFO_CODEC_RX_DATA_W_PARITY                0x42  ///< Currently active codec received data.
#define  LOG_INFO_CODEC_TX_DATA_W_PARITY                0x43  ///< Currently active codec sent data.
#define  LOG_INFO_CODEC_SNI_READER_DATA                 0x44  //< Sniffing codec receive data from reader
#define  LOG_INFO_CODEC_SNI_READER_DATA_W_PARITY        0x45  //< Sniffing codec receive data from reader
#define  LOG_INFO_CODEC_SNI_CARD_DATA                   0x46  //< Sniffing codec receive data from card
#define  LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY          0x47  //< Sniffing codec receive data from card
#define  LOG_INFO_CODEC_READER_FIELD_DETECTED           0x48  ///< Add logging of the LEDHook case for FIELD_DETECTED

/* App */
#define  LOG_INFO_APP_CMD_READ                          0x80  ///< Application processed read command.
#define  LOG_INFO_APP_CMD_WRITE                         0x81  ///< Application processed write command.
#define  LOG_INFO_APP_CMD_INC                           0x84  ///< Application processed increment command.
#define  LOG_INFO_APP_CMD_DEC                           0x85  ///< Application processed decrement command.
#define  LOG_INFO_APP_CMD_TRANSFER                      0x86  ///< Application processed transfer command.
#define  LOG_INFO_APP_CMD_RESTORE                       0x87  ///< Application processed restore command.
#define  LOG_INFO_APP_CMD_AUTH                          0x90  ///< Application processed authentication command.
#define  LOG_INFO_APP_CMD_HALT                          0x91  ///< Application processed halt command.
#define  LOG_INFO_APP_CMD_UNKNOWN                       0x92  ///< Application processed an unknown command.
#define  LOG_INFO_APP_CMD_REQA                          0x93  ///< Application (ISO14443A handler) processed REQA.
#define  LOG_INFO_APP_CMD_WUPA                          0x94  ///< Application (ISO14443A handler) processed WUPA.
#define  LOG_INFO_APP_CMD_DESELECT                      0x95  ///< Application (ISO14443A handler) processed DESELECT.
#define  LOG_INFO_APP_AUTHING                           0xA0  ///< Application is in `authing` state.
#define  LOG_INFO_APP_AUTHED                            0xA1  ///< Application is in `auth` state.
#define  LOG_ERR_APP_AUTH_FAIL                          0xC0  ///< Application authentication failed.
#define  LOG_ERR_APP_CHECKSUM_FAIL                      0xC1  ///< Application had a checksum fail.
#define  LOG_ERR_APP_NOT_AUTHED                         0xC2  ///< Application is not authenticated.

typedef struct
{
	unsigned char Entry;   //��Ŀ����
	unsigned char str[64]; //����
}LOG_INFO;

LOG_INFO logInfo[] = {
/* Generic */
{LOG_INFO_GENERIC                        /*0x10*/,_ANSI_RED_BR_(   "���ض���־��Ŀ.                  !!!      | ")},
{LOG_INFO_CONFIG_SET                     /*0x11*/,_ANSI_GREEN_BR_( "���ø���.                        [+]      | ")},
{LOG_INFO_SETTING_SET                    /*0x12*/,_ANSI_GREEN_BR_( "���۸���.                        [+]      | ")},
{LOG_INFO_UID_SET                        /*0x13*/,_ANSI_GREEN_BR_( "���Ÿ���.                        [+]      | ")},
{LOG_INFO_RESET_APP                      /*0x20*/,_ANSI_RED_BR_(   "��������.                        !!!      | ")},

/* Codec */
{LOG_INFO_CODEC_RX_DATA                  /*0x40*/,_ANSI_WHITE_( "���ƽ������������               <<<      | ")},
{LOG_INFO_CODEC_TX_DATA                  /*0x41*/,_ANSI_WHITE_( "���ƽ������������               >>>      | ")},
{LOG_INFO_CODEC_RX_DATA_W_PARITY         /*0x42*/,_ANSI_WHITE_( "���ƽ������������(��żУ��)     <<<      | ")},
{LOG_INFO_CODEC_TX_DATA_W_PARITY         /*0x43*/,_ANSI_WHITE_( "���ƽ������������(��żУ��)     >>>      | ")},
{LOG_INFO_CODEC_SNI_READER_DATA          /*0x44*/,_ANSI_YELLOW_BR_( "�Ӷ�������̽���յ�����           [#]      | ")},
{LOG_INFO_CODEC_SNI_READER_DATA_W_PARITY /*0x45*/,_ANSI_YELLOW_BR_( "�Ӷ�������̽���յ�����(��żУ��) [#]      | ")},
{LOG_INFO_CODEC_SNI_CARD_DATA            /*0x46*/,_ANSI_WHITE_( "����Ƶ����̽���յ�����           [-]      | ")},
{LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY   /*0x47*/,_ANSI_WHITE_( "����Ƶ����̽���յ�����(��żУ��) [-]      | ")},
{LOG_INFO_CODEC_READER_FIELD_DETECTED    /*0x48*/,_ANSI_CYAN_(     "��⵽��������ǿ                 <<<      | ")},

/* App */
{LOG_INFO_APP_CMD_READ                  /*0x80*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ����ȡ����.          ---      | ")},
{LOG_INFO_APP_CMD_WRITE                 /*0x81*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ���д������.          ---      | ")},
{LOG_INFO_APP_CMD_INC                   /*0x84*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ����ż�����.        ---      | ")},
{LOG_INFO_APP_CMD_DEC                   /*0x85*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ����ż�����.        ---      | ")},
{LOG_INFO_APP_CMD_TRANSFER              /*0x86*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ���������.          ---      | ")},
{LOG_INFO_APP_CMD_RESTORE               /*0x87*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ���ָ�����.          ---      | ")},
{LOG_INFO_APP_CMD_AUTH                  /*0x90*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ��������֤����.      ---      | ")},
{LOG_INFO_APP_CMD_HALT                  /*0x91*/,_ANSI_YELLOW_BR_("Ӧ�ó����Ѵ������߿�����.        ---      | ")},
{LOG_INFO_APP_CMD_UNKNOWN               /*0x92*/,_ANSI_RED_BR_(   "Ӧ�ó�������δ֪����.          !!!      | ")},
{LOG_INFO_APP_CMD_REQA                  /*0x93*/,_ANSI_YELLOW_BR_("Ӧ�ó���(ISO14443A�������)�Ѵ���REQA.    | ")},
{LOG_INFO_APP_CMD_WUPA                  /*0x94*/,_ANSI_YELLOW_BR_("Ӧ�ó���(ISO14443A�������)�Ѵ���WUPA.    | ")},
{LOG_INFO_APP_CMD_DESELECT              /*0x95*/,_ANSI_YELLOW_BR_("Ӧ�ó���(ISO14443A�������)�Ѵ���DESELECT.| ")},
{LOG_INFO_APP_AUTHING                   /*0xA0*/,_ANSI_YELLOW_BR_("Ӧ�ó�����[�����֤����]״̬.  ---      | ")},
{LOG_INFO_APP_AUTHED                    /*0xA1*/,_ANSI_YELLOW_BR_("Ӧ�ó�����[�����֤�ɹ�]״̬.  ---      | ")},
{LOG_ERR_APP_AUTH_FAIL                  /*0xC0*/,_ANSI_RED_BR_(   "Ӧ�ó��������֤ʧ��.            !!!      | ")},
{LOG_ERR_APP_CHECKSUM_FAIL              /*0xC1*/,_ANSI_RED_BR_(   "Ӧ�ó���У���ʧ��.              !!!      | ")},
{LOG_ERR_APP_NOT_AUTHED                 /*0xC2*/,_ANSI_RED_BR_(   "Ӧ�ó���δͨ�������֤.          !!!      | ")},

};

static char logExplainFind(unsigned char Entry,unsigned char * BUFFER)
{
	for(unsigned char i=0;i<sizeof(logInfo)/sizeof(LOG_INFO);i++)
	{
		if(Entry==logInfo[i].Entry)
		{
			memcpy(BUFFER,logInfo[i].str,sizeof(logInfo[i].str));
			return Entry;
		}
	}
	return false;
}

int checkParityBit(uint8_t *data,int byteCount,uint8_t* out)
{
    // ��֡������żλ
    if (byteCount == 1)
    {
        out[0]=data[0];
        return true;
    }

    // 9λ��һ���飬��֤λ������������
    int bitCount = byteCount*8;
    uint8_t *parsedData = (uint8_t*)malloc((bitCount/9)*sizeof(uint8_t));
    memset(parsedData,0,(bitCount/9)*sizeof(uint8_t));

    uint8_t oneCounter = 0;            // Counter for count ones in a byte
    int i;
    for (i=0;i<bitCount;i++)
    {
        // ��ȡ��bit����
        int byteIndex = i/8;
        int bitIndex  = i%8;
        uint8_t bit = (data[byteIndex] >> bitIndex) & 0x01 ;

        // �����żУ��λ
        // ��ǰλΪ��żλ
        if(i % 9 == 8)
        {
            /*
            // ��ǰ�ֽ��е�ż��
            if((oneCounter % 2) && (bit == 1))
            {
                printf("ʧ��");
                free(parsedData);
                return false;
                goto end;
            }
            // ��ǰ�ֽ���������1
            else if((!(oneCounter % 2)) && (bit == 0))
            {
                printf("ʧ��");
                free(parsedData);
                return false;
                goto end;
            }
            oneCounter = 0;
            */
        }
        // ��ǰλΪ����λ
        else
        {
            oneCounter += bit;
            parsedData[i/9] |= (uint8_t)(bit << (i%9));
        }
    }

    //printf("�ɹ�");

    end:
    for(int j=0;j<i/9;j++)
    {
        //printf("%02X ",parsedData[j]);
        *out++=parsedData[j];
    }

    free(parsedData);
    return true;
}

static void logProcess(unsigned char * BUFFER,uint32_t loglength)
{
	printf(_WHITE_BR_("-------+--------------+-------------------------------------------+-------------------------------------------------------\r\n"));
	printf(_WHITE_BR_("offset | time         | event                                     | data                                                  \r\n"));
	printf(_WHITE_BR_("-------+--------------+-------------------------------------------+-------------------------------------------------------\r\n"));

	unsigned char outStr[64]={0};
	unsigned char * srartArr=BUFFER;

    //BUFFER+=2;  //+2�ǽ������˵���̽����
    int authnum=0;
	while((uint32_t)(BUFFER-srartArr)<loglength)
	{
		if(logExplainFind(*BUFFER++,outStr))
		{   
			printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr-1));
			char datasize = *BUFFER++; //��Ч���ݳ���
			unsigned int time = (*BUFFER++)<<8 ;
			time |= (*BUFFER++); //ʱ���
			printf("ʱ���:%05d | ",time);
			printf("%s",outStr); //�¼�
            if(*(BUFFER-4)==LOG_INFO_CONFIG_SET)
            {
                for(char j=0;j<datasize;j++)
                {
                    printf("%c",*BUFFER++);
                }
            }
            else if(*(BUFFER-4)==LOG_INFO_CODEC_RX_DATA_W_PARITY         || \
                    *(BUFFER-4)==LOG_INFO_CODEC_TX_DATA_W_PARITY         || \
                    *(BUFFER-4)==LOG_INFO_CODEC_SNI_READER_DATA_W_PARITY || \
                    *(BUFFER-4)==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY      )
            {
                uint8_t out[128];
                checkParityBit(BUFFER,datasize,out);
                for(int num=0;num<datasize*8/9;num++)
                {
                    printf("%02X ",out[num]);
                }
                if(datasize==1)
                    printf("%02X ",out[0]);
                BUFFER+=datasize;
            }
            else
            {
                for(char j=0;j<datasize;j++)
                {
                    printf("%02X ",*BUFFER++);
                }
                if(*(BUFFER-datasize-4)==LOG_INFO_APP_CMD_AUTH || (datasize==4 && *BUFFER==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER-datasize-4)==LOG_INFO_CODEC_SNI_READER_DATA && *(BUFFER+1)==5 && (*(BUFFER-datasize)==0x60 || *(BUFFER-datasize)==0x61)))
                {
                    authnum++;
                    printf(".....................[%d]",authnum);
                }
            }
			printf("\r\n");
		}
		else
		{
			printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr-1));
			unsigned char error_event=*(BUFFER-1);
			char datasize = *BUFFER++; //��Ч���ݳ���
			unsigned int time = (*BUFFER++)<<8 ; //ʱ���
			time |= (*BUFFER++); //ʱ���
			printf("ʱ���:%05d | ",time);
			//printf("%s ",outStr); //�¼�
			if(error_event!=0)
            {
                if(error_event==0xff)
                    printf(_ANSI_RED_BR_("[!] �豸������.                  !!!      | "));
                else
				    printf(_ANSI_RED_BR_("[!] δ֪�¼�.�¼�����[%02x]        ???      | "),error_event);
            }
			else
            {
                printf(_ANSI_CYAN_("���ݿ�������.                    [=]      | "));
            }
			for(char j=0;j<datasize;j++)
			{
				printf("%02X ",*BUFFER++);
			}
			printf("\r\n");
		}
	}
	printf(_WHITE_BR_("-------+--------------+-------------------------------------------+-------------------------------------------------------\r\n"));
    printf("�Ѵ������֤�������:%d\r\n",authnum);
}

uint8_t mfkey32v2[22];
uint8_t mfkey64[22];

static void logProcessAuth(unsigned char * BUFFER,uint32_t loglength)
{
    memset(autoMfkey32src,0,sizeof(autoMfkey32src)); //��һ��ִ��logProcessAuth����ʱ��ȡ�ľ����¼
    autoMfkey32srcNum=0;                             //��һ��ִ��logProcessAuth����ʱ��ȡ�ľ����¼��Ŀ����
    autoMfkey64srcNum=0;                             //��һ��ִ��logProcessAuth����ʱ��ȡ�ľ����¼��Ŀ����

    printf("��������־�б���ȡ����Կ��,����automfkey32/automfkey64�����Իָ���Կ\r\n");
	printf(_WHITE_BR_("-----+--------+--------------+------------------------------------+-------------------------------------------------------------------\r\n"));
	printf(_WHITE_BR_("numb | offset | time         | event                              |  <keyAB/blk>[2] <uid>[4] <nt>[4] <{nr}>[4] <{ar}>[4] <{at}>[4]    \r\n"));
	printf(_WHITE_BR_("-----+--------+--------------+------------------------------------+-------------------------------------------------------------------\r\n"));

	unsigned char outStr[64]={0};
	unsigned char * srartArr=BUFFER;

    uint8_t uid[4]={0};

    uint8_t nt0[4]={0};
    uint8_t nr0[4]={0};
    uint8_t ar0[4]={0};
    uint8_t at0[4]={0};

    uint8_t nt1[4]={0};
    uint8_t nr1[4]={0};
    uint8_t ar1[4]={0};

    uint8_t keysec=0;
    uint8_t keyType=0;

    uint8_t uidFlag=0;
    uint8_t authFlag=0;

    uint8_t event=0;
    uint32_t addr=0;
    char datasize =0;
    unsigned int time=0;

    int authnumsuccess=0;
    int authnum=0;
    int authnumfault=0;
    int authnumerror=0;
	while((uint32_t)(BUFFER-srartArr)<loglength)
	{
        event=logExplainFind(*BUFFER++,outStr); //���¼�,û���ҵ�����ƫ��(��������)

        addr=(uint32_t)(BUFFER-srartArr-1);     //�¼���ַ
        datasize = *BUFFER++;                   //�¼��������ݳ���
        time = (*BUFFER++)<<8 ;
        time |= (*BUFFER++);                    //�¼�ʱ��

        switch (event)                          //ƥ��
        {
        case LOG_INFO_CODEC_RX_DATA:
            if((*(BUFFER)==0x93 || *(BUFFER)==0x95) && datasize==9)  //����ײ ����uid
            {
                uid[0]=*(BUFFER+2);
                uid[1]=*(BUFFER+3);
                uid[2]=*(BUFFER+4);
                uid[3]=*(BUFFER+5);
                BUFFER+=datasize;   //ƫ������һ���¼�
            }
            else
                BUFFER+=datasize;   //ƫ������һ���¼�
            break;
        
        case LOG_INFO_APP_CMD_AUTH:
            authnum++;printf(_WHITE_BR_("%04d | "),authnum);
            if((*(BUFFER+datasize)==LOG_INFO_CODEC_TX_DATA) && *(BUFFER+datasize+1)==4)//��һ���¼��Ƿ��������ҷ�����4�ֽ� �Ǿ���nt
            {
                keyType=*BUFFER;
                keysec=*(BUFFER+1);

                BUFFER+=datasize;
                nt0[0]=*(BUFFER+4);
                nt0[1]=*(BUFFER+5);
                nt0[2]=*(BUFFER+6);
                nt0[3]=*(BUFFER+7);
                BUFFER+=8;


                if(*BUFFER==LOG_INFO_CODEC_RX_DATA && *(BUFFER+1)==8) //nr+ar
                {
                    nr0[0]=*(BUFFER+4);
                    nr0[1]=*(BUFFER+5);
                    nr0[2]=*(BUFFER+6);
                    nr0[3]=*(BUFFER+7);

                    ar0[0]=*(BUFFER+8);
                    ar0[1]=*(BUFFER+9);
                    ar0[2]=*(BUFFER+10);
                    ar0[3]=*(BUFFER+11);

                    BUFFER+=12;

                    if(*(BUFFER+8)==LOG_INFO_APP_AUTHED &&  *(BUFFER+1)==4)
                    {
                        authnumsuccess++;

                        BUFFER+=8;
                        nt1[0]=*(BUFFER+4);
                        nt1[1]=*(BUFFER+5);
                        nt1[2]=*(BUFFER+6);
                        nt1[3]=*(BUFFER+7);
                        BUFFER+=8;

                        printf(AEND "0x%04X | " ,(uint32_t)(BUFFER-srartArr));
                        printf(AEND "ʱ���:%05d | ",time);
                        printf(_ANSI_YELLOW_BR_("[+]�޿���̽:�����֤�ɹ�!          | "));
                        mfkey32v2[0]=keyType;
                        mfkey32v2[1]=keysec;
                        for(uint8_t i=0;i<4;i++)
                        {
                            mfkey32v2[2+i] =uid[i];
                            mfkey32v2[6+i] =nt0[i];
                            mfkey32v2[10+i]=nr0[i];
                            mfkey32v2[14+i]=ar0[i];
                            mfkey32v2[18+i]=nt1[i];
                        }
                            
                        for(uint8_t i=0;i<22;i++)
                            printf("%02X ",mfkey32v2[i]);
                        printf("\r\n");

                        memset(nt1,0,4);
                    }
                    else if(*(BUFFER+8)==LOG_ERR_APP_AUTH_FAIL)
                    {
                        authnumfault++;

                        BUFFER+=8;
                        printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                        printf("ʱ���:%05d | ",time);
                        printf(_ANSI_CYAN_("[+]�޿���̽.                       | "));
                        mfkey32v2[0]=keyType;
                        mfkey32v2[1]=keysec;
                        for(uint8_t i=0;i<4;i++)
                        {
                            mfkey32v2[2+i] =uid[i];
                            mfkey32v2[6+i] =nt0[i];
                            mfkey32v2[10+i]=nr0[i];
                            mfkey32v2[14+i]=ar0[i];
                            mfkey32v2[18+i]=nt1[i];
                        }
                        for(uint8_t i=0;i<22;i++)
                        {
                            printf("%02X ",mfkey32v2[i]);
                        }
                        printf("\r\n");

                        //ת����������
                        if(autoMfkey32srcNum<255) //���������洢256������
                        {
                            for(uint8_t i=0;i<18;i++)
                            {
                                autoMfkey32src[autoMfkey32srcNum][i]=mfkey32v2[i];
                            }
                            autoMfkey32srcNum++;
                        }
                    }
                    else
                    {
                        authnumerror++;
                        printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                        printf(AEND "ʱ���:%05d | ",time);
                        printf(_ANSI_RED_BR_("[!]�޿���̽:������Ҫ��.            | "));
                        for(uint8_t i=0;i<22;i++)
                        {
                            printf("-- ");
                        }
                        printf("\r\n");
                    }
                }
                else
                {
                    authnumerror++;
                    printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                    printf(AEND "ʱ���:%05d | ",time);
                    printf(_ANSI_RED_BR_("[!]�޿���̽:nr/ar���մ���.         | "));
                    for(uint8_t i=0;i<22;i++)
                    {
                        printf("-- ");
                    }
                    printf("\r\n");
                }
            }
            else
            {
                authnumerror++;
                printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                printf(AEND "ʱ���:%05d | ",time);
                printf(_ANSI_RED_BR_("[!]�޿���̽:������Ҫ��.            | "));
                for(uint8_t i=0;i<22;i++)
                {
                    printf("-- ");
                }
                printf("\r\n");
                BUFFER+=datasize;   //ƫ������һ���¼�
            }
            break;
        case LOG_INFO_CODEC_SNI_READER_DATA:
            if(datasize==9 && *BUFFER==0x93 && *(BUFFER+1)==0x70)  //ѡ������
            {
                //printf("ѡ������\r\n");
                uid[0]=*(BUFFER+2);
                uid[1]=*(BUFFER+3);
                uid[2]=*(BUFFER+4);
                uid[3]=*(BUFFER+5);
                BUFFER+=datasize;   //ƫ������һ���¼�
            }
            else if(datasize==4 && (*BUFFER==0x60 || *BUFFER==0x61) && *(BUFFER+datasize)==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER+datasize+1)==5)  //�����֤����
            {
                authnum++;printf(_WHITE_BR_("%04d | "),authnum);//��֤��¼

                //printf("�����֤����\r\n");
                keyType=*BUFFER;
                keysec=*(BUFFER+1);
                BUFFER+=datasize;   //ƫ������һ���¼�
                if(*BUFFER==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER+1)==5)  //nt
                {
                    //printf("nt\r\n");
                    BUFFER+=4;
                    checkParityBit(BUFFER,5,nt0);
                    BUFFER+=5;  //������һ���¼�
                    if(*BUFFER==LOG_INFO_CODEC_SNI_READER_DATA && *(BUFFER+1)==8)  //nr+ar
                    {
                        //printf("nr+ar\r\n");
                        nr0[0]=*(BUFFER+4);
                        nr0[1]=*(BUFFER+5);
                        nr0[2]=*(BUFFER+6);
                        nr0[3]=*(BUFFER+7);

                        ar0[0]=*(BUFFER+8);
                        ar0[1]=*(BUFFER+9);
                        ar0[2]=*(BUFFER+10);
                        ar0[3]=*(BUFFER+11);

                        BUFFER+=12; //������һ���¼�

                        if(*BUFFER==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER+1)==5)  //at
                        {
                            //printf("at\r\n");
                            BUFFER+=4;
                            checkParityBit(BUFFER,5,at0);
                            BUFFER+=5;  //������һ���¼�

                            authnumsuccess++;
                            printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                            printf(AEND "ʱ���:%05d | ",time);
                            printf(_ANSI_GREEN_BR_("[+]�п���̽.                       | "));
                            mfkey64[0]=keyType;
                            mfkey64[1]=keysec;
                            for(uint8_t i=0;i<4;i++)
                            {
                                mfkey64[2+i] =uid[i];
                                mfkey64[6+i] =nt0[i];
                                mfkey64[10+i]=nr0[i];
                                mfkey64[14+i]=ar0[i];
                                mfkey64[18+i]=at0[i];
                            }
                            for(uint8_t i=0;i<22;i++)
                            {
                                printf("%02X ",mfkey64[i]);
                            }
                            printf("\r\n");

                            //ת����������
                            if(autoMfkey64srcNum<255) //���������洢256������
                            {
                                for(uint8_t i=0;i<22;i++)
                                {
                                    autoMfkey64src[autoMfkey64srcNum][i]=mfkey64[i];
                                }
                                autoMfkey64srcNum++;
                            }
                        }
                        else
                        {
                            authnumerror++;
                            printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                            printf(AEND "ʱ���:%05d | ",time);
                            printf(_ANSI_RED_BR_("[!]�п���̽:at���մ���.            | "));
                            for(uint8_t i=0;i<22;i++)
                            {
                                printf("-- ");
                            }
                            printf("\r\n");
                        }
                    }
                    else
                    {
                        authnumerror++;
                        printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                        printf(AEND "ʱ���:%05d | ",time);
                        printf(_ANSI_RED_BR_("[!]�п���̽:nr+ar���մ���.         | "));
                        for(uint8_t i=0;i<22;i++)
                        {
                            printf("-- ");
                        }
                        printf("\r\n");
                    }
                }
                else
                {
                    authnumerror++;
                    printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                    printf(AEND "ʱ���:%05d | ",time);
                    printf(_ANSI_RED_BR_("[!]�п���̽:nt���մ���.            | "));
                    for(uint8_t i=0;i<22;i++)
                    {
                        printf("-- ");
                    }
                    printf("\r\n");
                }
            }
            else
                BUFFER+=datasize;   //ƫ������һ���¼�
            break;
        case LOG_INFO_CODEC_SNI_READER_DATA_W_PARITY:
            break;

        default:
            BUFFER+=datasize;   //ƫ������һ���¼�
            break;
        
        }
	}
	printf(_WHITE_BR_("-----+--------+--------------+------------------------------------+-------------------------------------------------------------------\r\n"));
    printf("�Ѵ������֤�������:%d\r\n",authnum);
    printf("�ɹ�:%d��,ʧ��:%d��\r\n",authnumsuccess,authnumfault);
    printf("����%d�ν�����������򲻷���Ҫ��\r\n\r\n",authnumerror);
    
    
}


/************************************************************************************************/

#define MEMORY_SIZE_PER_SETTING		8192
char XmodeSendStr[MEMORY_SIZE_PER_SETTING];

#define RECEIVE_BUF_SIZE	          1024*18+20 /*�������������־�ļ�,��ɫ��һ����󷵻�18kb����־�ļ�,����20byte�ռ�*/
char XmodeReceiveStr[RECEIVE_BUF_SIZE]={0};

void TerminalSendChar(char c) {
	// CDC_Device_SendByte(&TerminalHandle, c);
    #ifdef _WIN32
	WriteFile(hCom, &c, 1, &wsize, NULL);
    #else
    uart_send(sp1,&c,1);
    #endif
}
void TerminalSendByte(uint8_t Byte) {
	// CDC_Device_SendByte(&TerminalHandle, Byte);
    #ifdef _WIN32
	WriteFile(hCom, &Byte, 1, &wsize, NULL);
    #else
    uart_send(sp1,&Byte,1);
    #endif
}

void TerminalSendBlock(const void *Buffer, uint16_t ByteCount) {
	// CDC_Device_SendData(&TerminalHandle, Buffer, ByteCount);
    #ifdef _WIN32
	WriteFile(hCom, Buffer, ByteCount, &wsize, NULL);
    #else
    uart_send(sp1,Buffer,ByteCount);
    #endif
}



typedef bool (*XModemCallbackType)(void *ByteBuffer, uint32_t BlockAddress, uint16_t ByteCount);

void XModemReceive(XModemCallbackType CallbackFunc);
void XModemSend(XModemCallbackType CallbackFunc);

bool XModemProcessByte(uint8_t Byte);
void XModemTick(void);

#define BYTE_NAK        0x15
#define BYTE_SOH        0x01
#define BYTE_ACK        0x06
#define BYTE_CAN        0x18
#define BYTE_EOF        0x1A
#define BYTE_EOT        0x04
#define BYTE_ESC		0x1B

#define XMODEM_BLOCK_SIZE   128

#define RECV_INIT_TIMEOUT   1/*5*/  /* #Ticks between sending of NAKs to the sender */
#define RECV_INIT_COUNT     60 /* #Timeouts until receive failure */
#define SEND_INIT_TIMEOUT   300 /* #Ticks waiting for NAKs from the receiver before failure */

#define FIRST_FRAME_NUMBER  1
#define CHECKSUM_INIT_VALUE 0

static enum {
    STATE_OFF,
    STATE_RECEIVE_INIT,
    STATE_RECEIVE_WAIT,
    STATE_RECEIVE_FRAMENUM1,
    STATE_RECEIVE_FRAMENUM2,
    STATE_RECEIVE_DATA,
    STATE_RECEIVE_PROCESS,
    STATE_SEND_INIT,
    STATE_SEND_WAIT,
    STATE_SEND_EOT
} State = STATE_OFF;

static uint8_t CurrentFrameNumber;
static uint8_t ReceivedFrameNumber;
static uint8_t Checksum;
static uint8_t RetryCount;
static uint16_t RetryTimeout;
static uint16_t BufferIdx;
static uint32_t BlockAddress;

static XModemCallbackType CallbackFunc;

static uint8_t CalcChecksum(const void *Buffer, uint16_t ByteCount) {
    uint8_t Checksum_1 = CHECKSUM_INIT_VALUE;
    uint8_t *DataPtr = (uint8_t *) Buffer;

    while (ByteCount--) {
        Checksum_1 += *DataPtr++;
    }

    return Checksum_1;
}

void XModemReceive(XModemCallbackType TheCallbackFunc) {
    State = STATE_RECEIVE_INIT;
    CurrentFrameNumber = FIRST_FRAME_NUMBER;
    RetryCount = RECV_INIT_COUNT;
    RetryTimeout = RECV_INIT_TIMEOUT;
    BlockAddress = 0;

    CallbackFunc = TheCallbackFunc;
}

void XModemSend(XModemCallbackType TheCallbackFunc) {
    State = STATE_SEND_INIT;
    RetryTimeout = SEND_INIT_TIMEOUT;
    BlockAddress = 0;

    CallbackFunc = TheCallbackFunc;
}

#define TERMINAL_BUFFER_SIZE	512
uint8_t TerminalBuffer[TERMINAL_BUFFER_SIZE] = { 0x00 };
bool XModemProcessByte(uint8_t Byte) {
    switch (State) {
        case STATE_RECEIVE_INIT:
        case STATE_RECEIVE_WAIT:
            if (Byte == BYTE_SOH) {
                /* Next frame incoming */
                BufferIdx = 0;
                Checksum = CHECKSUM_INIT_VALUE;
                State = STATE_RECEIVE_FRAMENUM1;
            } else if (Byte == BYTE_EOT) {
                /* Transmission finished */
                TerminalSendByte(BYTE_ACK);
                State = STATE_OFF;
				XmodeFlag=0;  
				/*��ȡ��������*****************************************************/
				FILE *fp;
				fp = fopen(receiveFile, "wb");
				fwrite((const void *)XmodeReceiveStr, sizeof(unsigned char), BlockAddress, fp);
				fclose(fp);

				printf("\r\n");

				if(downloadtype==1)
				{
					for(int addr=0;addr<BlockAddress/16;addr++)                                    //Ѱַ��Χ0~3f(16����*4��=64��)
					{
						if(addr%4==0)                                                          //�Ŀ�Ϊһ������,��ʾ�ָ�
							printf(_YELLOW_BR_("\r\n%02d����\r\n") 
							"-----------------------------------------------\r\n",addr/4);
						unsigned char data[16];                                                //���ݻ���
						memcpy(data,(const void *)(XmodeReceiveStr+addr*16),16);
						for(unsigned char i=0;i<16;i++)                                                 //һ��16���ֽ�
						{
							printf("%02X ",data[i]);                                           //��ӡһ���ֽ�
						}
						printf("\r\n");                                                        //һ�����ݴ�ӡ��ϻ���
					}
					printf("\r\n");  
				}

				else if(downloadtype==2)
				{
                    if(BlockAddress==2048)
                    {
                        printf(_RED_BR_("��־Ϊ��.û���κο��Խ��������ݼ�¼."));
                        goto LOGNULL;
                    }
					printf("\r\n��־Ԥ��\r\n");
                    if(BlockAddress==18432)
					    print_logs((BlockAddress)/0x10,(uint8_t*)XmodeReceiveStr);
                    else
                        print_logs((BlockAddress-1024*2)/0x10,(uint8_t*)XmodeReceiveStr); //��־û���ܻ�ഫ��2K�Ŀ����ݣ�����ʾ��
					printf("\r\n��־���ݽ������\r\n");
                    if(BlockAddress==18432)
					    logProcess((uint8_t*)XmodeReceiveStr,BlockAddress);
                    else
                        logProcess((uint8_t*)XmodeReceiveStr,BlockAddress-1024*2); //��־û���ܻ�ഫ��2K�Ŀ����ݣ�����ʾ��
					printf("\r\n");
                    /***/
                    logProcessAuth((uint8_t*)XmodeReceiveStr,BlockAddress);
                    //aotuMfkey32srcPrint();
                    //kardClass();
                    //for(uint8_t change=0;change<kardClassNumber;change++)
                    //    aotuMfkey32cardSecArrangement(offset[change],offset[change+1]);
                    //aotuMfkey32();
                    /***/
					
				}
				printf(_GREEN_BR_("[=] �������.\r\n"));
                if(BlockAddress==18432)
                    printf(_RED_BR_("[!] ע��:��ɫ����־�洢�ռ�����,�뼰ʱ���� [��־�洢�����ѹر�].\r\n"));
                else
                    printf(_CYAN_("[!] ע��:��ɫ�����丽�ӵ�2kb�հ���־�������Զ�����.\r\n"));
				printf(_YELLOW_BR_("[=] ��д���ļ�:%s ����%d byte.\r\n"),receiveFile,BlockAddress);
                if(downloadtype==2)
                    printf(_YELLOW_BR_("[?] ��Ҫ��������־�ļ���ȡ����������Կ,�����<automfkey32>/<automfkey64>����.�����ĵȴ��������н���."));
                LOGNULL:
                downloadtype=0;
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
				memset(receiveFile,0,sizeof(receiveFile));
				memset(XmodeReceiveStr,0,RECEIVE_BUF_SIZE);

            } else if ((Byte == BYTE_CAN) || (Byte == BYTE_ESC)) {
                /* Cancel transmission */
                State = STATE_OFF;
            } else {
                /* Ignore other bytes */
            }

            break;

        case STATE_RECEIVE_FRAMENUM1:
            /* Store frame number */
            ReceivedFrameNumber = Byte;
            State = STATE_RECEIVE_FRAMENUM2;
            break;

        case STATE_RECEIVE_FRAMENUM2:
            if (Byte == (255 - ReceivedFrameNumber)) {
                /* frame-number check passed. */
                State = STATE_RECEIVE_DATA;
            } else {
                /* Something went wrong. Try to recover by sending NAK */
                TerminalSendByte(BYTE_NAK);
                State = STATE_RECEIVE_WAIT;
            }

            break;

        case STATE_RECEIVE_DATA:
            /* Process byte and update checksum */
            TerminalBuffer[BufferIdx++] = Byte;

            if (BufferIdx == XMODEM_BLOCK_SIZE) {
                /* Block full */
                State = STATE_RECEIVE_PROCESS;
            }

            break;

        case STATE_RECEIVE_PROCESS:
            if (ReceivedFrameNumber == CurrentFrameNumber) {
                /* This is the expected frame. Calculate and verify checksum */

                if (CalcChecksum(TerminalBuffer, XMODEM_BLOCK_SIZE) == Byte) {
                    /* Checksum is valid. Pass received data to callback function */
                    if (CallbackFunc(TerminalBuffer, BlockAddress, XMODEM_BLOCK_SIZE)) {
                        /* Proceed to next frame and send ACK */
                        CurrentFrameNumber++;
                        BlockAddress += XMODEM_BLOCK_SIZE;
                        TerminalSendChar(BYTE_ACK);
                        State = STATE_RECEIVE_WAIT;
                    } else {
                        /* Application signals to cancel the transmission */
                        TerminalSendByte(BYTE_CAN);
                        TerminalSendByte(BYTE_CAN);
                        State = STATE_OFF;
                    }
                } else {
                    /* Data seems to be damaged */
                    TerminalSendByte(BYTE_NAK);
                    State = STATE_RECEIVE_WAIT;
                }
            } else if (ReceivedFrameNumber == (CurrentFrameNumber - 1)) {
                /* This is a retransmission */
                TerminalSendByte(BYTE_ACK);
                State = STATE_RECEIVE_WAIT;
            } else {
                /* This frame is completely out of order. Just cancel */
                TerminalSendByte(BYTE_CAN);
                State = STATE_OFF;
            }

            break;

        case STATE_SEND_INIT:
            /* Start sending on NAK */
            if (Byte == BYTE_NAK) {
                CurrentFrameNumber = FIRST_FRAME_NUMBER - 1;
                Byte = BYTE_ACK;
            } else if (Byte == BYTE_ESC) {
                State = STATE_OFF;
            }

        /* Fallthrough */

        case STATE_SEND_WAIT:
            if (Byte == BYTE_CAN) {
                /* Cancel */
                TerminalSendByte(BYTE_ACK);
                State = STATE_OFF;
            } else if (Byte == BYTE_ACK) {
                /* Acknowledge. Proceed to next frame, get data and calc checksum */
                CurrentFrameNumber++;

                if (CallbackFunc(TerminalBuffer, BlockAddress, XMODEM_BLOCK_SIZE)) {
                    TerminalSendByte(BYTE_SOH);
                    TerminalSendByte(CurrentFrameNumber);
                    TerminalSendByte(255 - CurrentFrameNumber);
                    TerminalSendBlock(TerminalBuffer, XMODEM_BLOCK_SIZE);
                    TerminalSendByte(CalcChecksum(TerminalBuffer, XMODEM_BLOCK_SIZE));

                    BlockAddress += XMODEM_BLOCK_SIZE;
                } else {
                    TerminalSendByte(BYTE_EOT);
                    State = STATE_SEND_EOT;
                }
            } else if (Byte == BYTE_NAK) {
                /* Resend frame */
                TerminalSendByte(BYTE_SOH);
                TerminalSendByte(CurrentFrameNumber);
                TerminalSendByte(255 - CurrentFrameNumber);
                TerminalSendBlock(TerminalBuffer, XMODEM_BLOCK_SIZE);
                TerminalSendByte(CalcChecksum(TerminalBuffer, XMODEM_BLOCK_SIZE));
            } else {
                /* Ignore other chars */
            }

            break;

        case STATE_SEND_EOT:
            /* Receive Ack */
            State = STATE_OFF;
			XmodeFlag=0;
			memset(XmodeSendStr,0,MEMORY_SIZE_PER_SETTING);
			printf(_GREEN_BR_("\r\n[=] �ϴ��ɹ�."));
			printf(OPTIONAL_ANSWER_TRAILER);//����usb->
            break;

        default:
            return false;
            break;
    }

    return true;
}

uint8_t receive=0;

void XModemTick(void) {
    /* Timeouts go here */
    switch (State) {
        case STATE_RECEIVE_INIT:
            if (RetryTimeout-- == 0) {
                if (RetryCount-- > 0) {
                    /* Put out communication request */
                    TerminalSendChar(BYTE_NAK);
                } else {
                    /* Just shut off after some time. */
                    State = STATE_OFF;
					XmodeFlag=0;
					printf(_RED_BR_("[!] ��ɫ������Ӧ.�ȴ��ѳ�ʱ."));
					printf(OPTIONAL_ANSWER_TRAILER);//����usb->
                }

                RetryTimeout = RECV_INIT_TIMEOUT;
            }
            break;

        case STATE_SEND_INIT:
            if (RetryTimeout-- == 0) {
                /* Abort */
                State = STATE_OFF;
				XmodeFlag=0;
				printf(_RED_BR_("[!] ��ɫ������Ӧ.�ȴ��ѳ�ʱ."));
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
            }
            break;

/**************************************************************************/
/*                      ����������ɶ���ɵĶ�������                           */
/**************************************************************************/
        case STATE_SEND_WAIT:
            if (RetryTimeout-- == 0) {
                /* Abort */
                State = STATE_OFF;
				XmodeFlag=0;
				printf(_RED_BR_("\r\n[!] ����ʧ��,��������������."));
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
            }
            break;

        case STATE_OFF:
            if(XmodeFlag != 1)
                return;
            if (RetryTimeout-- == 0) {
                /* Abort */
                State = STATE_OFF;
				XmodeFlag=0;
				printf(_RED_BR_("\r\n[!] δ֪����,��������������."));
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
            }
            break;

/**************************************************************************/
        default:
            //printf("%d,%d",(char)State,RetryTimeout);
            break;
    }
}

void *XModemTickProcess(void* arg)
{
    UNUSED(arg);
	while(1)
	{
        #ifdef _WIN32
		Sleep(1);
        #else
        sleep(1);
        #endif
		XModemTick();
		if(XmodeFlag==0)
			return NULL;
	}
}

#define MIN(x,y) ( (x) < (y) ? (x) : (y) )

bool MemoryDownloadBlock(void *Buffer, uint32_t BlockAddress_1, uint16_t ByteCount) {
    if (BlockAddress_1 >= MEMORY_SIZE_PER_SETTING) {
        /* There are bytes out of bounds to be read. Notify that we are done. */
        return false;
    } else {
        /* Calculate bytes left in memory and issue reading */
        uint32_t BytesLeft = MEMORY_SIZE_PER_SETTING - BlockAddress_1;
        ByteCount = MIN(ByteCount, BytesLeft);

        /* Output local memory contents */
        memcpy(Buffer, XmodeSendStr+BlockAddress_1, ByteCount);

        return true;
    }
}

bool MemoryUploadBlock(void *Buffer, uint32_t BlockAddress_1, uint16_t ByteCount) {
    if (BlockAddress_1 >= /*MEMORY_SIZE_PER_SETTING*/RECEIVE_BUF_SIZE) {
        /* Prevent writing out of bounds by silently ignoring it */
        return true;
    } else {
        /* Calculate bytes left in memory and start writing */
        uint32_t BytesLeft = /*MEMORY_SIZE_PER_SETTING*/RECEIVE_BUF_SIZE - BlockAddress_1;
        ByteCount = MIN(ByteCount, BytesLeft);

        /* Store to local memory */
        memcpy(XmodeReceiveStr+BlockAddress_1, Buffer, ByteCount);

        return true;
    }
}

/************************************************************************************************/

#ifdef _WIN32

/* ���ô��ڲ���
 * �ɸ�Ϊ���θ���
 * ��ʱ�������Ӧ����
 */
int setUart(HANDLE hCom_1)
{
	COMMTIMEOUTS timeouts;
	DCB dcb;

	// ����ʱ
	timeouts.ReadIntervalTimeout         = 1/*MAXDWORD*/; // ������ʱ�����ַ���ļ����ʱ
	timeouts.ReadTotalTimeoutMultiplier  = 0; // �������ڶ�ȡÿ���ַ�ʱ�ĳ�ʱ
	timeouts.ReadTotalTimeoutConstant    = 1/*0*/; // �������Ĺ̶���ʱ
	// д��ʱ
	timeouts.WriteTotalTimeoutMultiplier = 0/*0*/; // д������дÿ���ַ�ʱ�ĳ�ʱ
	timeouts.WriteTotalTimeoutConstant   = 0; // д�����Ĺ̶���ʱ

	// ���ó�ʱ
	SetCommTimeouts(hCom_1, &timeouts);

	// �������������������С
	SetupComm(hCom_1, INBUFLEN, OUTBUFLEN);

	// ��ȡ���ڲ���
	if (GetCommState(hCom_1, &dcb) == 0)
	{
		return -1;
	}

	// ���ô��ڲ���
	dcb.BaudRate = CBR_115200; // ������
	dcb.ByteSize = 8;          // ����λ��
	dcb.Parity   = NOPARITY;   // У��λ
	dcb.StopBits = ONESTOPBIT; // ֹͣλ
	if (SetCommState(hCom_1, &dcb) == 0)
	{
		return -1;
	}

	return 0;
}

void initUart(void)
{
    int iCOM=128;
    char cCom[MAX_PATH] = " "; 
    bool bFirstTime = TRUE;

    START:
    for(int i = 1; i<=iCOM; i++)
    {
        HANDLE hCom; //ȫ�ֱ��������ھ�� 
        char cTemp[MAX_PATH]; 
        char cTempFull[MAX_PATH]; 
        sprintf(cTemp, "COM%d", i);
        sprintf(cTempFull, "\\\\.\\COM%d", i);
        hCom=CreateFile(cTempFull,//COM1�� 
            GENERIC_READ|GENERIC_WRITE, //�������д 
            0, //��ռ��ʽ 
            NULL, 
            OPEN_EXISTING, //�򿪶����Ǵ��� 
            0, //ͬ����ʽ 
            NULL); 
        if(hCom==(HANDLE)-1) 
        { 
            //printf("��COM%dʧ��!\r\n",i); 
        } 
        else
        {
            if(bFirstTime==TRUE)
            {
                bFirstTime = FALSE;
                sprintf(cCom,"%s",cTemp);
            }
            else
            {
                sprintf(cCom,"%s,%s",cCom,cTemp);
            }
        }
        CloseHandle(hCom);
    }

    if(bFirstTime)
    {
        // printf("�޿��ô���,��ȷ����ǰ���������ȷ��װChameleonMini��������.\r\n");
        // getch();
        // exit(0);

        int userSelect = MessageBoxA(0, "δ��⵽���ô��ڣ�\r\n��ȷ����ǰ���������ȷ��װChameleonMini��������δ����������ռ��", "����", MB_ABORTRETRYIGNORE|MB_ICONERROR);

        if(userSelect == IDABORT) //��ֹ 
            exit(0);
        else if(userSelect == IDRETRY)//����
            goto START;
        else if(userSelect == IDIGNORE)//����
        {
            userSelect = MessageBoxA(0, "�Ƿ��������ڼ��?\r\n��: �������ڳ�ʼ��,ֱ�ӽ���������(���ڹ��ܲ���)\r\n��: ������ô��ں�(����δ��ö�ٵ�ֵ)", "���ڼ����", MB_YESNO|MB_ICONQUESTION);
            if(userSelect == IDYES)
            {
                return; /* ֱ���˳� */
            }
            else if(userSelect == IDNO)
            {
                /* �������κ��� */
            }
        }
    }
    else
        printf("���ô���: %s\r\n",cCom);

	OPEN:
	//printf(logo);
	printf("\r\nѡ��ChameleonMini���ں�(����\"5\"):");

	char com[32]={0};
	int num=0;
	scanf("%d",&num);
	getc(stdin); //�Ե�����Ļس�

	if(num<10)
		sprintf(com,"COM%d",num);
	else
		sprintf(com,"\\\\.\\COM%d",num);
	
	// �򿪴���
	hCom = CreateFile(com, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hCom != INVALID_HANDLE_VALUE)
	{
		printf("���ڴ򿪳ɹ�\n");
	}
	else
	{
		printf("���ڴ�ʧ�ܣ����²���豸���ܻ�������\n\n");
		goto OPEN;
	}

	// ���ô���
	if (setUart(hCom) == -1)
	{
		if (INVALID_HANDLE_VALUE != hCom)
			CloseHandle(hCom); // �رմ���
	}

    // sprintf(com,"COM%d",num);
    // sp1 = uart_open(com,115200);
    // if(sp1 == INVALID_SERIAL_PORT)
    // {
    //     printf("���ڴ�ʧ�ܣ����²���豸���ܻ�������\n\n");
	// 	goto OPEN;
    // }
    printf("���ڴ򿪳ɹ�\n");
}

#endif

void deInitUart(void)
{
    #ifdef _WIN32
    CloseHandle(hCom);
    #else
    uart_close(sp1);
    #endif
}



/* ���ڹرմ��ڶ�ȡ�߳� */
uint8_t shutdowmread = 0;

#ifdef _WIN32


void readUart(void)
{
	while(1)
	{
        if(shutdowmread)
        {
            shutdowmread=0;
            return;
        }

		if (INVALID_HANDLE_VALUE != hCom)
		{
			//printf(_RED_BR_("[+] ���ڽ���...\r\n"));
			ReadFile(hCom, rbuf, sizeof(rbuf), &rsize, NULL);
            // �����ȡ��������
            if (rsize > 0) {
                //printf("Received: %.*s\n", rsize, rbuf);
                for(int i=0;i<rsize;i++)
                {
                    static int arr=0;
                    if(XmodeFlag==1)  //�ϴ�
                    {
                        //printf("%02x ", rbuf[i]);
                        //printf(".");
                        receive++;
                        char buf[128]={0};
                        double rate = (receive*60/66);  //����8KB(8*128byte)��һ����128byte�������յ�64�ΰ��������ȷ���źţ����Ͽ�ʼ�ͽ����źţ���66��

                        static uint8_t color=1,colorLen=0;
                        for(int j = 0; j < 60; j++)
                        {
                            if(j >= rate) //��ɫ�ֽ紦
                            {
                                if(color)
                                {
                                    colorLen=strlen("\x1b[92m");  //��ɫ
                                    memcpy(buf+j*2,"\x1b[92m",colorLen);
                                    color=0;
                                }
                            }
                            memcpy(buf+j*2+colorLen,"��",2);
                        }
                        color=1,colorLen=0;
                        printf(_YELLOW_BR_("\r[+] %s [%02d%%] "),buf,receive*100/66);
                        if(receive==66)
                            receive=0;

                        XModemProcessByte(rbuf[i]);
                    }
                    else if(XmodeFlag==2)  //����
                    {
                        int len = downloadtype==1 ? strlen(loadlogo) : strlen(loglogo);
                        const char *pictureBuf = downloadtype==1 ? loadlogo : loglogo;

                        if(arr<len)
                            printf("%c", pictureBuf[arr++]);
                        else
                        {
                            //printf(".");
                                
                            char buf[128]={0};
                            double rate = (BlockAddress*50/(uint32_t)(RECEIVE_BUF_SIZE-20));
                            for(int j = 0; j < 50; j++)
                            {
                                if(j < rate) //���num��">"
                                    memcpy(buf+j*2,"��",2);
                                else
                                    memcpy(buf+j*2,"��",2);
                            }
                            printf(_YELLOW_BR_("\r[+] �ռ�ʹ����: %s��[%02d%%] "),buf,BlockAddress*100/(uint32_t)(RECEIVE_BUF_SIZE-20));
                            printf("%d/%d byte",BlockAddress,RECEIVE_BUF_SIZE-20);
                        }
                        
                        //printf("%02x ", rbuf[i]);
                        XModemProcessByte(rbuf[i]);
                    }
                    else
                    {
                        arr=0;
                        printf("%c", rbuf[i]);
                    }
                }
                //printf("\n");
                //printf(_RED_BR_("[+] ���������...\r\n"));
            }
		}
	}
}

#else


void *readUartPM3(void* arg)
{
    UNUSED(arg);
    while(1)
    {
        do{
            uart_receive(sp1,rbuf,INBUFLEN,&rsize);
            if(shutdowmread)
            {
                shutdowmread=0;
                return NULL;
            }
        }while(!rsize);

        //printf("���ȣ�%ld,���ݣ�%s",rsize,rbuf);

        // �����ȡ��������
        if (rsize > 0) {
            //printf("Received: %.*s\n", rsize, rbuf);
            for(int i=0;i<rsize;i++)
            {
                static int arr=0;
                if(XmodeFlag==1)  //�ϴ�
                {
                    //printf("%02x ", rbuf[i]);
                    //printf(".");
                    receive++;
                    char buf[128]={0};
                    double rate = (receive*60/66);  //����8KB(8*128byte)��һ����128byte�������յ�64�ΰ��������ȷ���źţ����Ͽ�ʼ�ͽ����źţ���66��

                    static uint8_t color=1,colorLen=0;
                    for(int j = 0; j < 60; j++)
                    {
                        if(j >= rate) //��ɫ�ֽ紦
                        {
                            if(color)
                            {
                                colorLen=strlen("\x1b[92m");  //��ɫ
                                memcpy(buf+j*2,"\x1b[92m",colorLen);
                                color=0;
                            }
                        }
                        memcpy(buf+j*2+colorLen,"��",2);
                    }
                    color=1,colorLen=0;
                    printf(_YELLOW_BR_("\r[+] %s [%02d%%] "),buf,receive*100/66);
                    if(receive==66)
                        receive=0;

                    XModemProcessByte(rbuf[i]);
                }
                else if(XmodeFlag==2)  //����
                {
                    int len = downloadtype==1 ? strlen(loadlogo) : strlen(loglogo);
                    const char *pictureBuf = downloadtype==1 ? loadlogo : loglogo;

                    if(arr<len)
                        printf("%c", pictureBuf[arr++]);
                    else
                    {
                        //printf(".");
                            
                        char buf[128]={0};
                        double rate = (BlockAddress*50/(uint32_t)(RECEIVE_BUF_SIZE-20));
                        for(int j = 0; j < 50; j++)
                        {
                            if(j < rate) //���num��">"
                                memcpy(buf+j*2,"��",2);
                            else
                                memcpy(buf+j*2,"��",2);
                        }
                        printf(_YELLOW_BR_("\r[+] �ռ�ʹ����: %s��[%02d%%] "),buf,BlockAddress*100/(uint32_t)(RECEIVE_BUF_SIZE-20));
                        printf("%d/%d byte",BlockAddress,RECEIVE_BUF_SIZE-20);
                    }
                    
                    //printf("%02x ", rbuf[i]);
                    XModemProcessByte(rbuf[i]);
                }
                else
                {
                    arr=0;
                    printf("%c", rbuf[i]);
                }
            }
            //printf("\n");
            //printf(_RED_BR_("[+] ���������...\r\n"));
        }
    }
}

#endif

char uploadflag=0;
char downloadflag=0;
char upgradeflag=0;

#define avr   0
#define stm32 1

int main(int argc,char *argv[])
{

#ifdef _WIN32

	MessageBoxA(0, "��ɫ��WIN32�������ն�Ӧ�ã�δ�������Ĳ��԰汾��\r\n����ɷ��� https://www.PM3_SE_hub.com", "����!", MB_OK|MB_ICONWARNING);

    int mcu = MessageBoxA(0, "��ʹ�õ���STM32�豸��?\r\n\r\n��: ѡ��STM32��ʼ\r\n��: �Թٷ�AVR�汾��ʼ", "�豸ѡ��", MB_YESNO|MB_ICONQUESTION);
    if(mcu == IDYES)
    {
        mcu=stm32;
    }
    else if(mcu == IDNO)
    {
        mcu=avr;
    }

    start:

	initUart(); //��ʼ������

	printf(SCREEN_CLEAR); //����

	//�����̶߳�����
	_beginthread((void *)readUart, 0, NULL);

    //�����̶߳�����
	//_beginthread((void *)readUartPM3, 0, NULL);

#else

    printf("����!��ɫ��WIN32�������ն�Ӧ�ã�δ�������Ĳ��԰汾��\r\n����ɷ��� https://www.PM3_SE_hub.com");
    char mcu;
    printf("�豸ѡ�� - ��ʹ�õ���STM32�豸��?\r\n\r\nY: ѡ��STM32��ʼ\r\nN: �Թٷ�AVR�汾��ʼ");
    scanf("%s",&mcu);
    if(mcu == 'Y')
    {
        mcu=stm32;
    }
    else if(mcu == 'N')
    {
        mcu=avr;
    }

    start:

    printf("%s",argv[1]);
    if (argv[1] == NULL) {
        printf("ERROR!\n");
    }
    sp1 = uart_open(argv[1]);

    //sp1 = uart_open("tcp:localhost:4321");

    if(sp1 == INVALID_SERIAL_PORT) {
        printf("���ڴ�ʧ�ܣ����²���豸���ܻ�������\n\n");
    }
    else {
        printf("���ڴ򿪳ɹ�\n");
    }

    getc(stdin); //�Ե�����Ļس�

    printf(SCREEN_CLEAR); //����

    //�����̶߳�����
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, readUartPM3, NULL);
    if (ret != 0) {
        printf("Failed to create thread.\n");
        return 1;
    }
    printf("Thread created successfully.\n");

    printf("\r\n");

#endif

	//���߳�д����
	while(1)
	{
        #ifdef _WIN32
		if (INVALID_HANDLE_VALUE != hCom)
		{
        #endif
			//printf(_RED_BR_("[+] �ȴ�����...\r\n"));
			char c ;
			uint8_t len=0;
			while((c=getc(stdin)) !='\n')
			{
				wbuf[len++]=c;
			}
            if(strlen(wbuf)==0)
            {
                printf(_RED_BR_("[!] ��Ч����������.����[help]�Ի�ȡ������Ϣ.\r\n"));
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
                
				memset(wbuf,0,OUTBUFLEN);
				continue;
            }
            //printf(_RED_BR_("[+] �������...\r\n"));
			wbuf[len++]='\r';
			wbuf[len++]='\n';
			if(strcasecmp((const char *)wbuf,"cls\r\n")==0)
			{
				printf(SCREEN_CLEAR); //����
                #ifdef _WIN32
				WriteFile(hCom, "ping\r\n", strlen("ping\r\n"), &wsize, NULL);
                #else
                uart_send(sp1,"ping\r\n", strlen("ping\r\n"));
                #endif
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
            if(strcasecmp((const char *)wbuf,"make\r\n")==0)
			{
                /* ʹ�þ���·��------------------------------------------- */
                /*
                char path[PATH_MAX]; //PATH_MAX is defined in limits.h
                char *p; // ������һ��б���ַ���λ��
                getcwd(path, sizeof(path)); // ��ȡ��ǰ����Ŀ¼
                uint8_t i = 0;
                while(path[i])
                {
                    if(path[i]=='\\')
                        path[i]='/';
                    i++;
                }
                p = strrchr(path, '\\'); // �ҵ����һ��б���ַ�
                if (p) {
                    *p = '\0'; // �����滻Ϊ\0
                }
                strcat(path,"/Make");
                char pathMakeCMD[PATH_MAX];
                sprintf(pathMakeCMD,"make -C %s size -j8",path);

                system(pathMakeCMD);
                */
                /* ʹ�����·��------------------------------------------- */

                system("make -C ../Make size -j8");

                printf(OPTIONAL_ANSWER_TRAILER);//����usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			// if(strcasecmp((const char *)wbuf,"reconnect\r\n")==0)
			// {
            //     //deInitUart();
            //     initUart(); //��ʼ������
            //     printf(SCREEN_CLEAR); //����
            //     //�����̶߳�����
            //     _beginthread((void *)readUart, 0, NULL);
			// 	memset(wbuf,0,OUTBUFLEN);
			// 	continue;
			// }
			if(strcasecmp((const char *)wbuf,"mfkey\r\n")==0)
			{
                //logProcessAuth((uint8_t*)XmodeReceiveStr,BlockAddress);
                mfkey();
                //crapto1();
                getc(stdin); //�Ե��س�
				printf(OPTIONAL_ANSWER_TRAILER);//����usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			if(strcasecmp((const char *)wbuf,"autoMfkey32\r\n")==0)
			{
                printf(_RED_BR_("[!] ִ�д�����ǰ��ر�����Ӧ�ó���,��ȷ�������ɢ���������.\r\n"));
                printf(_YELLOW_BR_("[+] ѡ����ܷ�ʽ: \r\n"
                                   "[+] 0 - ���ٽ� (�߲���ִ���������ε���������,�����ڴ�������)\r\n"
                                   "[+] 1 - �ɿ��� (����ִ�����ڵĶ�����������,�����ٽⲻ������Ҫ��ʱӦѡ�ô˷�ʽ)\r\n"
                                   "[+] 2 - ��ȫ�� (���������ݽ�����ȫƥ�����,���ý������ȷ,����ʱ����)\r\n"
                                   "[+] "));
                scanf("%d",&decryptionLevel);
                getc(stdin); //�Ե�����Ļس�
                if(decryptionLevel)
                    autoMfkey32();
                else
                    autoMfkey32Fast();
                printf(OPTIONAL_ANSWER_TRAILER);//����usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			if(strcasecmp((const char *)wbuf,"autoMfkey64\r\n")==0)
			{
                autoMfkey64();

                printf(OPTIONAL_ANSWER_TRAILER);//����usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
            if(strcasecmp((const char *)wbuf,"upgrade\r\n")==0 && (mcu == stm32))
			{
                memset(wbuf,0,OUTBUFLEN);
                
                printf(_YELLOW_BR_("[+] ѡ��һ��ѡ���Կ�ʼ���������豸\r\n"
                                   "[+] 1 - �ӵ�ǰĿ¼���� Chameleon-Mini.elf �Ը����豸\r\n"
                                   "[+] 2 - �����Լ��Ĺ̼������ļ�������·��\r\n"
                ));

                scanf("%d",&decryptionLevel);
                getc(stdin); //�Ե�����Ļس�

                #ifdef _WIN32
                WriteFile(hCom, "upgrade\r\n", strlen("upgrade\r\n"), &wsize, NULL);  //���͸���
                #else
                uart_send(sp1,"upgrade\r\n", strlen("upgrade\r\n"));
                #endif

                //uart_close(sp1);//�رմ���
                deInitUart();
                shutdowmread=1; //�رս����߳�

                printf(_GREEN_BR_("[+] STM32������������...\r\n"));
                printf(_GREEN_BR_("[+] STM32�ѽ���BootLoader\r\n"));

                if(decryptionLevel == 1)
                {
                    char path[PATH_MAX]; //PATH_MAX is defined in limits.h
                    getcwd(path,sizeof(path));
                    strcat(path,"\\�̼�����.exe");
                    system(path);

                    printf("\r\n\r\n");
                    uint8_t state[4]={'-','\\','|','/'};
                    static char ing=0;
                    for(int i=0;i<20;i++)
                    {
                        if(ing==4)
                            ing=0;
                        printf(_YELLOW_BR_("\r[%c] ���ڸ��¿��ö˿�...."),state[ing++]);
                        #ifdef _WIN32
                        Sleep(100);
                        #else
                        sleep(1);
                        #endif
                    }
                    goto start;

                    return 0;
                }
                
                //��ʱ�ѶϿ�����
				printf(_YELLOW_BR_("[+] ָ���̼�����·���͹̼�����,��ֹ���������ַ�,���� D:/MyFiles/Chameleon-Mini.elf\r\n[+] "));
				upgradeflag=1;
                
				continue;
			}
			if(upgradeflag)
			{
				upgradeflag=0;
				wbuf[len-2]='\0';
				FILE *fp;
				//char XmodeSendStr[1024];
				//�ж��ļ��Ƿ��ʧ��
				if ( (fp = fopen((const char *)wbuf, "rb")) == NULL ) //rt ��txt rb ��bin wb дbin 
				{
					printf(_RED_BR_("[!] �Ҳ����ļ�.ָ�����ļ����ļ�·��������.\r\n"));
                    printf(_RED_BR_("[!] %s"),wbuf);
					printf(OPTIONAL_ANSWER_TRAILER);//����usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}

                printf("\r\n");
                printf(_YELLOW_BR_("[+] ȷ���Ƿ�����û�����\r\n"
                                   "[+] ----------------------------\r\n"
                                   "[+] 1- �����û��洢�Ŀ�Ƭ��������Ϣ\r\n"
                                   "[+] 2- �����û��洢�Ŀ�Ƭ��������Ϣ\r\n"
                                   "[+] "
                ));

                scanf("%d",&decryptionLevel);
                getc(stdin); //�Ե�����Ļس�

                //�����ⲿ�ӿ�����
                char path[PATH_MAX]; //PATH_MAX is defined in limits.h
                getcwd(path,sizeof(path));
                strcat(path,"/FlashDownload/STM32CubeProgrammer/STM32_Programmer_CLI.exe");
                printf("��¼����·����%s\r\n",path);

                char pathDownload[PATH_MAX];
                char pathEarse[PATH_MAX];
                char pathStart[PATH_MAX];
                strcpy(pathDownload,path);
                strcpy(pathEarse,path);
                strcpy(pathStart,path);

                
                strcat(pathDownload," -c port=usb1 -w ");
                strcat(pathDownload,wbuf);
                strcat(pathDownload," 0x08000000");
                printf("��¼���%s\r\n",pathDownload);

                
                strcat(pathEarse," -c port=usb1 -e all");
                printf("�������%s\r\n",pathEarse);
                strcat(pathStart," -c port=usb1 -s");
                printf("�������%s\r\n",pathStart);

                if(decryptionLevel == 1)
                {
                    system(pathEarse);
                }
                system("chcp 850");
                system("cls");
                system(pathDownload);
                system("chcp 936");
                system("cls");
                system(pathStart);

				memset(wbuf,0,OUTBUFLEN);

                printf(_WHITE_BR_("                                                                         \r\n"
                                  "      -------------------------------------------------------------------\r\n"
                                  "                   ���,���ڿ�ʹ���ն��������STM32,��ȫ�γ�.                \r\n"
                                  "      -------------------------------------------------------------------\r\n"
                                  "                                                                         \r\n"
                                  "STM32��ɫ������.��Ȩ����,����ؾ�!                                           \r\n"
                ));

                // ��������
                // Sleep(1000);
                // execl(argv[0], NULL, NULL);
                printf("\r\n\r\n");
                uint8_t state[4]={'-','\\','|','/'};
                static char ing=0;
                for(int i=0;i<20;i++)
                {
                    if(ing==4)
                        ing=0;
                    printf(_YELLOW_BR_("\r[%c] ���ڸ��¿��ö˿�...."),state[ing++]);
                    #ifdef _WIN32
                    Sleep(100);
                    #else
                    sleep(1);
                    #endif
                }

                goto start;
                return 0;
                
			}
			if(strcasecmp((const char *)wbuf,"upload\r\n")==0)
			{
                receive=0; /*����������*/
				printf(_YELLOW_BR_("[+] ָ���ļ�·�����ļ���,�� D:/�ҵ��ĵ�/�ſ�.dump\r\n[+] "));
				uploadflag=1;
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			else if (strcasecmp((const char *)wbuf,"download\r\n")==0 || strcasecmp((const char *)wbuf,"logdownload\r\n"/*"DETECTION?\r\n"*/)==0)
			{
				memset(XmodeReceiveStr,0,RECEIVE_BUF_SIZE);
				if(strcasecmp((const char *)wbuf,"download\r\n")==0)
				{
					printf(_YELLOW_BR_("[+] ָ��������ļ���,��:�ſ�1.dump\r\n[+] "));
					downloadflag=1;
					downloadtype=1;
				}
				else
				{
					printf(_YELLOW_BR_("[+] ָ��������ļ���,��:log.bin\r\n[+] "));
					downloadflag=2;
					downloadtype=2;
				}
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			if(uploadflag)
			{
				uploadflag=0;
				wbuf[len-2]='\0';
				FILE *fp;
				//char XmodeSendStr[1024];
				//�ж��ļ��Ƿ��ʧ��
				if ( (fp = fopen((const char *)wbuf, "rb")) == NULL ) //rt ��txt rb ��bin wb дbin 
				{
					printf(_RED_BR_("[!] �Ҳ����ļ�.ָ�����ļ����ļ�·��������.\r\n"));
                    printf(_RED_BR_("[!] %s"),wbuf);
					printf(OPTIONAL_ANSWER_TRAILER);//����usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}
				memset(wbuf,0,OUTBUFLEN);

				//��ȡ�ļ���С
				fseek(fp, 0, SEEK_END);//��λ�ļ�ָ�뵽�ļ�β��
				long int size=ftell(fp);//��ȡ�ļ�ָ��ƫ���������ļ���С��
				if(size>1024*8)
				{
					printf(_RED_BR_("[!] �ļ���С������Χ:%ld byte ��� %d byte."),size,MEMORY_SIZE_PER_SETTING);
					printf(OPTIONAL_ANSWER_TRAILER);//����usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}
				printf(_YELLOW_BR_("[=] �ļ���С:%ld byte.\r\n"),size);

				//ѭ����ȡ�ļ���ÿһ������
				fseek(fp, 0, SEEK_SET);//��λ�ļ�ָ�뵽�ļ�ͷ��
				fread(XmodeSendStr, size, 1, fp); // �����ƶ� != NULL )
				fclose(fp);
				/*��ȡ��������*****************************************************/
				mf_print_blocks(size/16,(uint8_t *)XmodeSendStr);
				printf("\r\n");
				//WriteFile(hCom, str, 1024, &wsize, NULL);
				XModemSend(MemoryDownloadBlock);
                #ifdef _WIN32
				WriteFile(hCom, "upload\r\n", strlen("upload\r\n"), &wsize, NULL);
                Sleep(500);
				_beginthread((void *)XModemTickProcess, 0, NULL);
                #else
                uart_send(sp1,"upload\r\n", strlen("upload\r\n"));
                sleep(1);
                pthread_t thread;
                pthread_create(&thread, NULL, XModemTickProcess, NULL);
                #endif
				printf(_YELLOW_BR_("[!] ��ʼ�ϴ�.\r\n"));
				XmodeFlag=1;
				continue;
			}
			else if(downloadflag)
			{
				wbuf[len-2]='\0';
				FILE *fp;
				if ( (fp = fopen((const char *)wbuf, "w+")) == NULL ) //�½��ɶ���д���ļ�
				{
					printf(_RED_BR_("[!] �����ļ�ʱ����.�ļ���ռ�û�ֻ���ļ�"));
					printf(OPTIONAL_ANSWER_TRAILER);//����usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}
				printf(_GREEN_BR_("[=] �����ļ�:%s\r\n"),wbuf);
				strcpy(receiveFile,(const char *)wbuf);
				memset(wbuf,0,OUTBUFLEN);
				fclose(fp);
				XModemReceive(MemoryUploadBlock);
				if(downloadflag==1)
                {
                    #ifdef _WIN32
					WriteFile(hCom, "download\r\n", strlen("download\r\n"), &wsize, NULL);
                    #else
                    uart_send(sp1,"download\r\n", strlen("download\r\n"));
                    #endif
                }
				else
                {
                    #ifdef _WIN32
					WriteFile(hCom, "logdownload\r\n"/*"DETECTION?\r\n"*/, strlen("logdownload\r\n"/*"DETECTION?\r\n"*/), &wsize, NULL);
                    #else
                    uart_send(sp1,"logdownload\r\n"/*"DETECTION?\r\n"*/, strlen("logdownload\r\n"/*"DETECTION?\r\n"*/));
                    #endif
                }
				downloadflag=0;
                #ifdef _WIN32
				Sleep(500);  //�ȴ����ս��̴��������һ����Ϣ
				_beginthread((void *)XModemTickProcess, 0, NULL);
                #else
                sleep(1);  //�ȴ����ս��̴��������һ����Ϣ
                pthread_t thread;
				pthread_create(&thread, NULL, XModemTickProcess, NULL);
                #endif
				printf(_YELLOW_BR_("[!] ��ʼ����.\r\n"));
				XmodeFlag=2;
				continue;
			}
			//printf(_RED_BR_("[+] ���ڷ���...\r\n"));
            #ifdef _WIN32
			WriteFile(hCom, wbuf, strlen((const char *)wbuf), &wsize, NULL);
            #else
            uart_send(sp1, wbuf, strlen((const char *)wbuf));
            #endif
			memset(wbuf,0,OUTBUFLEN);
			//printf(_RED_BR_("[+] ���������...\r\n"));
        #ifdef _WIN32
		}
        #endif
	}

	return 0;
}

/*
2
1E 5C FB EB
60 2C 0F D2 8F 50 C6 10 D8 E3 35 D8 12 50
60 2C DF 6E 18 8C BC B0 F8 5A 80 70 CA 23
��ͷ��֤��ַ(��):44 ��Կ����:KeyA �ػ����Կ(1) = 111111111111
*/


// #include "windows.h"
 
// CRITICAL_SECTION cs;
// BOOL flag;
 
// DWORD WINAPI Thread1_Proc( PVOID pvParam)
// {
//      while (1)
//      {
//          while (flag==1);
//          EnterCriticalSection(&cs);
//          printf ( "Hello," );
//          LeaveCriticalSection(&cs);
//          flag = 1;
//          Sleep(50);
//      }
// }
 
// DWORD WINAPI Thread2_Proc( PVOID pvParam)
// {
//      while (1)
//      {
//          while (flag==0);
//          EnterCriticalSection(&cs);
//          printf ( "World!\n" );
//          LeaveCriticalSection(&cs);
//          flag = 0;
//          Sleep(50);
//      }
// }
 
// int main( int argc, char * argv[])
// {
//      InitializeCriticalSection(&cs);
//      CreateThread(NULL, 0, Thread1_Proc, NULL, 0, NULL);
//      CreateThread(NULL, 0, Thread2_Proc, NULL, 0, NULL);
//      while (1)
//      {
//          ;
//      }
//      DeleteCriticalSection(&cs);
//      return 0;
// }
