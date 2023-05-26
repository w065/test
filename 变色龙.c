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



serial_port sp1;    /*!!!!!!!!!!!!!!!注意!!!!!!!!!!!!!!!!!!!!*/


#endif



/******************************************************************************************************/
/*                                         PM3 UART STRART                                            */
/******************************************************************************************************/

// //读超时
// # define UART_USB_CLIENT_RX_TIMEOUT_MS  /*20*/1

// #define PM3_SUCCESS             0

// // 输入输出错误  客户端帧接收错误
// #define PM3_EIO                -8

// // 通用TTY错误
// #define PM3_ENOTTY            -14


// /* serial_port is declared as a void*, which you should cast to whatever type
//  * makes sense to your connection method. Both the posix and win32
//  * implementations define their own structs in place.
//  */
// typedef void *serial_port;

// /* 如果指定的串行端口无效，则由uart_open返回。
//  */
// #define INVALID_SERIAL_PORT (void*)(~1)

// /* 如果指定的串行端口正被另一个进程使用，则由uart_open返回。
//  */
// #define CLAIMED_SERIAL_PORT (void*)(~2)


// typedef struct {
//     HANDLE hPort;     // 串行端口句柄
//     DCB dcb;          // 设备控制设置
//     COMMTIMEOUTS ct;  // 串行端口超时配置
// } serial_port_windows_t;


// uint32_t newtimeout_value = 0;
// bool newtimeout_pending = false;


// /* 设置串行端口的当前速度（以波特为单位）。
//  */
// bool uart_set_speed(serial_port sp, const uint32_t uiPortSpeed) {
//     serial_port_windows_t *spw;

//     // 设置端口速度（输入和输出）
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

// /* 获取串行端口的当前速度（波特）。
//  */
// uint32_t uart_get_speed(const serial_port sp) {
//     const serial_port_windows_t *spw = (serial_port_windows_t *)sp;
//     if (!GetCommState(spw->hPort, (serial_port) & spw->dcb))
//         return spw->dcb.BaudRate;

//     return 0;
// }

// /* 重新配置超时
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


// /* 给定用户指定的端口名，连接到该端口并返回用于将来引用该端口的结构。
//  *
//  * 出现错误时，此方法返回INVALID_SERIAL_PORT或CLAIMD_SERIAL_PORT。
//  */
// serial_port uart_open(const char *pcPortName, uint32_t speed) {
//     char acPortName[255] = {0};
//     serial_port_windows_t *sp = calloc(sizeof(serial_port_windows_t), sizeof(uint8_t));

//     if (sp == 0) {
//         printf("UART无法分配内存\n");
//         return INVALID_SERIAL_PORT;
//     }
//     // 将输入的 "com?" 转换为 "\\.\COM?" 格式
//     snprintf(acPortName, sizeof(acPortName), "\\\\.\\%s", pcPortName);
//     _strupr(acPortName);

//     // 尝试打开串行端口
//     // r/w,  none-share comport, no security, existing, no overlapping, no templates
//     sp->hPort = CreateFileA(acPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
//     if (sp->hPort == INVALID_HANDLE_VALUE) {
//         uart_close(sp);
//         return INVALID_SERIAL_PORT;
//     }

//     // 准备设备控制
//     // 这无关紧要，因为PM3设备忽略了以下CDC命令：usb_CDC.c中的set_line_coding
//     memset(&sp->dcb, 0, sizeof(DCB));
//     sp->dcb.DCBlength = sizeof(DCB);
//     if (!BuildCommDCBA("baud=115200 parity=N data=8 stop=1", &sp->dcb)) {
//         uart_close(sp);
//         printf("UART错误cdc设置\n");
//         return INVALID_SERIAL_PORT;
//     }

//     // 更新活动串行端口
//     if (!SetCommState(sp->hPort, &sp->dcb)) {
//         uart_close(sp);
//         printf("设置通信状态时UART发生错误\n");
//         return INVALID_SERIAL_PORT;
//     }

//     uart_reconfigure_timeouts(UART_USB_CLIENT_RX_TIMEOUT_MS);
//     uart_reconfigure_timeouts_polling(sp);

//     if (!uart_set_speed(sp, speed)) {
//         // 自动尝试回退
//         speed = 115200;
//         if (!uart_set_speed(sp, speed)) {
//             uart_close(sp);
//             printf("设置波特率时UART发生错误\n");
//             return INVALID_SERIAL_PORT;
//         }
//     }
//     return sp;
// }

// /* 关闭给定端口。
//  */
// void uart_close(const serial_port sp) {
//     if (((serial_port_windows_t *)sp)->hPort != INVALID_HANDLE_VALUE)
//         CloseHandle(((serial_port_windows_t *)sp)->hPort);
//     free(sp);
// }

// /* 从给定串行端口读取长达30ms。
//  *   pbtRx: 指向要写入的返回数据的缓冲区的指针。
//  *   pszMaxRxLen: 要读取的最大数据大小。
//  *   pszRxLen: 实际读取的字节数。
//  *
//  * 如果读取到了数据，即使小于pszMaxRxLen，也返回TRUE。
//  *
//  * 如果读取设备时出错，则返回FALSE。
//  * 请注意，相应的实现可能已经完成了对缓冲区的部分读取，因此应检查pszRxLen以查看是否读取到了数据
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

// /* 将缓冲区发送到给定的串行端口。
//  *   pbtTx: 指向包含要发送的数据的缓冲区的指针。
//  *   len: 要发送的数据量。
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


// serial_port sp1;    /*!!!!!!!!!!!!!!!注意!!!!!!!!!!!!!!!!!!!!*/



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

//     printf("键入5:一次性恢复秘钥\n键入7:从两次会话中恢复秘钥\n");
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
//         printf("MIFARE Classic密钥恢复-基于32位密钥流 版本v2\n");
//         printf("仅从两个32位读取器身份验证答案恢复密钥\n");
//         printf("这个版本实现了Moebius两种不同的nonce解决方案(如超级卡)\n");
//         printf("-----------------------------------------------------\n\n");
//         printf("键入: <uid> <nt> <nr_0> <ar_0> <nt1> <nr_1> <ar_1>\n\n");
    
//         scanf("%x %x %x %x %x %x %x", &uid, &nt0, &nr0_enc, &ar0_enc, &nt1, &nr1_enc, &ar1_enc);

//         printf("\n正在从以下给定的数据中恢复的密钥:\n");
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
//         printf("\n用于生成{ar}和{at}的密钥流:\n");
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
//                 printf("\n找到秘钥: [%012" PRIx64 "]", key);
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
//         printf("MIFARE Classic密钥恢复-基于64位密钥流\n");
//         printf("仅从一次完整身份验证中恢复密钥!\n");
//         printf("-----------------------------------------------------\n\n");
//         printf("键入: <uid> <nt> <{nr}> <{ar}> <{at}>\n\n");

//         scanf("%x %x %x %x %x", &uid, &nt, &nr_enc, &ar_enc, &at_enc);

//         printf("\n正在从以下给定的数据中恢复的密钥:\n");

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
//         printf("\n用于生成{ar}和{at}的密钥流:\n");
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
//         printf("\n找到秘钥: [%012" PRIx64 "]", key);
//         crypto1_destroy(revstate);
//         return 0;
//     }
// }



/******************************************************************************************************/
/*                                           PM3 FMKEY END                                            */
/******************************************************************************************************/


/******************************************************************************************************/
/*                                          俄罗斯 FMKEY Start                                         */
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
    return end;
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
/*                                          俄罗斯 FMKEY END                                           */
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

// #define _KEY_FLOW_NUM 20 //设置最大的密流记录次数，2的倍数，2个数据一个秘钥

// uint64_t UID = 0xAABBCCDD;
// uint8_t buffer[128];  //接受到的数据

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

// CryptoKey Crk[_KEY_FLOW_NUM/2];  //具体应取决于jcrk的次数,少兵2022/9/2
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
//     printf("\r\n输入侦测次数,回车确认\r\n");
//     scanf("%d",&nSniff);
//     /************************************************/

//     /************************************************/
//     printf("\r\n输入8位UID号(4 Byte),回车确认\r\n");
//     scanf("%x %x %x %x",\
//     buffer+0,buffer+1,buffer+2,buffer+3,buffer+4);
//     UID=ByteArrayToInt(buffer,0);
//     /************************************************/

//     for(int i=0;i<nSniff;i++)
//     {
//         /************************************************/
//         printf("\r\n第%d次侦测数据,共%d次,输入14位数据,空格隔开回车走起!\r\n",i+1,nSniff);
//         scanf("%x %x %x %x %x %x %x %x %x %x %x %x %x %x",\
//         buffer+ 5,buffer+ 6,buffer+ 7,buffer+ 8,buffer+ 9,\
//         buffer+10,buffer+11,buffer+12,buffer+13,buffer+14,buffer+15,buffer+16,buffer+17,buffer+18\
//         );
//         buffer[5]-=0x60; //0:keyA 1:keyB
//         /************************************************/
//         getsniff(i);
//     }
    
//     printf("\r\n请稍后....\r\n");
//     CulcKeys();

//     printf("\r\n解析记录: %d个秘钥---------------------------------------------",jcrk);
//     uint8_t* StrKeyAB[2] = {"KeyA", "KeyB"};
//     for(int i=0;i<jcrk;i++)
//         printf("\r\n读头验证地址(块):%02d 秘钥类型:%s 截获的秘钥(%d) = %012llX",Crk[i].block,StrKeyAB[Crk[i].AB],i, Crk[i].key );

//     memset(buffer,0,128);




// }





/******************************************************************************************************/

/*
 * 变色龙移植工程
 * 串口终端软件
 * WindowsAPI串口编程，另请参阅：https://www.cnblogs.com/milanleon/p/4244267.html
 * 源自创建的分支：https://blog.csdn.net/qq_44829047/article/details/106448325
 * 多线程数据处理，另请参阅：https://blog.csdn.net/yizhizainulii/article/details/124297432
 * 源自创建的分支：https://blog.csdn.net/yizhizainulii/article/details/124297432
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







//首先执行logProcessAuth以初始化资源

uint8_t autoMfkey32src[256][18]={0}; //执行logProcessAuth函数时获取的精简记录
uint8_t autoMfkey32srcNum=0;         //执行logProcessAuth函数时获取的精简记录条目数量

//打印初始化资源
void autoMfkey32srcPrint()
{
    printf("\r\n-----------------用于mfkey32v2秘钥恢复的密流日志-----------------\r\n");
    
    for(uint8_t i=0;i<autoMfkey32srcNum;i++)
    {
        printf("条目:%03d | ",i);
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

uint8_t kardClassNumber=1; //卡片数量
uint8_t offset[256]={0};   //记录密流表卡号更改的偏移地址,最多256个偏移地址
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

    offset[kardClassNumber]=autoMfkey32srcNum+1;//结束地址

    printf("共存在%d张卡片记录\r\n",kardClassNumber);
    for(int i=0;i<kardClassNumber;i++)
        printf("检测到切换卡片的条目%d\r\n",offset[i]);
}

uint8_t secKey[16][128][15]={0};
uint16_t cardStreamNum; //整卡密流数量

void autoMfkey32cardSecArrangement(uint8_t cardStart,uint8_t cardEnd)
{
    memset(secKey,0,sizeof(secKey));
    cardStreamNum=0;

    //printf("\r\n开始密流转储\r\n");
    for(uint8_t block=0;block<16;block++)
    {
        uint8_t stream=0;
        //遍历整卡密流 转储
        for(int offset=cardStart;offset<cardEnd;offset++)
        {
            if(autoMfkey32src[offset][0] && autoMfkey32src[offset][1]/4==block)  //存在数据的前提下,判断这个密钥流验证的地址是不是0...15
            {
                //printf("转储到扇区:%02d\r\n",block);
                secKey[block][stream][0]=1; //数据存在标志
                memcpy(secKey[block][stream]+1,autoMfkey32src[offset]+6,12); //复制12个字节进去
                stream++;
            }
        }
    }

    int uid=ByteArrayToInt(autoMfkey32src[cardStart],2);
    printf("\r\n卡号:%08X [扇区密流数据表]\r\n",uid);
    for(uint8_t block=0;block<16;block++)
    {
        printf("扇区%02d--------------------------------------\r\n",block);
        for(uint8_t stream=0;stream<128;stream++)
        {
            if(secKey[block][stream][0]==1) //数据存在
            {
                printf("密流%03d: ",stream);
                secKey[block][0][0]=stream+1; //更新扇区密流数量
                for(uint8_t data=1;data<13;data++)
                {
                    printf("%02X ",secKey[block][stream][data]);
                }
                printf("\r\n");
            }
        }
        cardStreamNum+=secKey[block][0][0];      //更新整卡密流数量
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
//             printf("[+] 找到秘钥: [" _GREEN_BR_("%012" PRIx64 ) "]\r\n", key);
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
            printf("\r[+] 耗时:%4d ms 找到秘钥: [ " _GREEN_BR_("%012" PRIx64 ) " ]      \r\n",stop - start,key);
            return true;
        }
    }
    return false;


}


int mfkey(void) {

    int argc;

    printf("键入5:一次性恢复秘钥\n键入7:从两次会话中恢复秘钥\n");
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
        printf("MIFARE Classic密钥恢复-基于32位密钥流 版本v2\n");
        printf("仅从两个32位读取器身份验证答案恢复密钥\n");
        printf("这个版本实现了Moebius两种不同的nonce解决方案(如超级卡)\n");
        printf("-----------------------------------------------------\n\n");
        printf("键入: <uid> <nt> <nr_0> <ar_0> <nt1> <nr_1> <ar_1>\n\n");
    
        scanf("%x %x %x %x %x %x %x", &uid, &nt0, &nr0_enc, &ar0_enc, &nt1, &nr1_enc, &ar1_enc);

        printf("\n正在从以下给定的数据中恢复的密钥:\n");
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
        printf("\n用于生成{ar}和{at}的密钥流:\n");
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
                printf("\n找到秘钥: [%012" PRIx64 "]", key);
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
        printf("MIFARE Classic密钥恢复-基于64位密钥流\n");
        printf("仅从一次完整身份验证中恢复密钥!\n");
        printf("-----------------------------------------------------\n\n");
        printf("键入: <uid> <nt> <{nr}> <{ar}> <{at}>\n\n");

        scanf("%x %x %x %x %x", &uid, &nt, &nr_enc, &ar_enc, &at_enc);

        printf("\n正在从以下给定的数据中恢复的密钥:\n");

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
        printf("\n用于生成{ar}和{at}的密钥流:\n");
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
        printf("\n找到秘钥: [%012" PRIx64 "]", key);
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
//     Sleep(1000*n);  //第 n 个线程睡眠 n 秒
//     printf("我是， pid = %d 的子线程\t", GetCurrentThreadId());   //输出子线程pid
//     printf(" pid = %d 的子线程退出\n\n", GetCurrentThreadId());   //延时10s后输出
 
//     return 0;
// }
 
// int main()
// {
//     printf("我是主线程， pid = %d\n", GetCurrentThreadId());  //输出主线程pid
//     HANDLE hThread[THREAD_NUM];
//     for (int i = 0; i < THREAD_NUM; i++)
//     {
//         hThread[i] = CreateThread(NULL, 0, ThreadFunc, &i, 0, NULL); // 创建线程
//     }
    
//     WaitForMultipleObjects(THREAD_NUM,hThread,true, INFINITE);  //一直等待，直到所有子线程全部返回
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

//     // if(mfKey32(argList_t->UID,argList_t->one+1,argList_t->two+1)) //比对成功
//     // {
//     //     argList_t->one[13]=1; //已处理
//     //     argList_t->two[13]=1; //已处理
//     //     argList_t->status=1;
//     // }



//     /***********************************/

//     ArgList_t *argList_t=(ArgList_t*)p;
//     if(mfKey32v2(argList_t->UID,argList_t->one+1,argList_t->two+1,argList_t->statelist,argList_t->odd,argList_t->even)) //比对成功
//     {
//         argList_t->one[13]=1; //已处理
//         argList_t->two[13]=1; //已处理
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

//     for(uint8_t a=argList_t->a;a<128;a++)  //前级指针
//     {
//         if(secKey[block][a][0] && !secKey[block][a][13])  //存在未处理的数据
//         {
//             uint8_t b=a+1;//后级指针

//             secKey[block][a][13]++;     //瞄定
//             secKey[block][b][13]++;     //瞄定
//             if(secKey[block][a][13]==1) //判断有没有被其他线程瞄定,先到先得
//             {
//                 if(secKey[block][b][0])  //存在未处理的数据
//                 {


//                     /*************************************************************************************************************************/
//                     if(mfKey32v2(argList_t->UID,secKey[block][a]+1,secKey[block][b]+1,argList_t->statelist,argList_t->odd,argList_t->even))
//                     {
//                         secKey[block][a][13]=2; //已处理
//                         secKey[block][b][13]=2; //已处理

//                         secKeyFindSuccess=1;
//                         keyFindSuccessNum++;
//                     }
//                     else
//                     {
//                         secKey[block][a][13]=3; //错误数据
//                         secKey[block][b][13]=0; //继续使用
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
//                         cardScheduleNum+=secKey[i][0][0];//之前扇区的密流总数
//                     }
//                     if(a>scheduleNum)
//                     {
//                         scheduleNum=a;
//                     }
//                     cardScheduleNum+=scheduleNum;
//                     //printf("\r %03d %03d %03d",scheduleNum,cardScheduleNum,cardStreamNum);
//                     //printf("\r[%c] 进度: %02d %% 当前扇区进度: %02d %%",state[ing++],cardScheduleNum*100/cardStreamNum,scheduleNum*100/secKey[block][0][0]);

//                     char buf[128]={0};
//                     double num = ((double)cardScheduleNum/(double)cardStreamNum)*34;

//                     // for(int i = 0; i < 33; i++)
//                     // {
//                     //     if(i < num) //输出num个">"
//                     //         memcpy(buf+i*2,"█",2);
//                     //     else
//                     //         memcpy(buf+i*2,"─",2);
//                     // }
//                     // printf(_YELLOW_BR_("\r[%c] %s▏[%02d%%]"),state[ing++],buf,cardScheduleNum*100/cardStreamNum);

//                     static uint8_t color=1,colorLen=0;
//                     for(int i = 0; i < 34; i++)
//                     {
//                         if(i >= num) //颜色分界处
//                         {
//                             if(color)
//                             {
//                                 colorLen=strlen(AEND);  //白色
//                                 memcpy(buf+i*2,AEND,colorLen);
//                                 color=0;
//                             }
//                             memcpy(buf+i*2+colorLen,"▓",2);
//                         }
//                         else
//                         {
//                             memcpy(buf+i*2+colorLen,"█",2);
//                         }
                        
//                     }
//                     color=1,colorLen=0;
//                     printf(_YELLOW_BR_("\r[%c] %s(%02d%%)"),state[ing++],buf,cardScheduleNum*100/cardStreamNum);


//                 }
//             }
//         }
//     }
// }



uint8_t decryptionLevel=1; //解密级别 0快速解 1普通解 2完全解析

void autoMfkey32Fast()
{
    // autoMfkey32srcPrint();//密流缓存打印,仅做打印...
    // kardClass();          //换成获取卡片数量及偏移地址

    // clock_t start,stop;
    // start = clock();        /*  开始计时  */

    // keyFindSuccessNum=0;

    // for(uint8_t card=0;card<kardClassNumber;card++)
    // {
    //     printf(_YELLOW_BR_("\r\n[+] 开始解算第%d张卡片秘钥,共%d张.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] 并发解密 请注意计算机散热."));
    //     autoMfkey32cardSecArrangement(offset[card],offset[card+1]); //获取已整理的密流表

    //     uint32_t uid=(uint32_t )ByteArrayToInt(autoMfkey32src[offset[card]],2);
    //     printf(_YELLOW_BR_("\r\n[+] 并发解密开始.第%d张,共%d张.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] 请注意计算机散热."));
    //     printf(_YELLOW_BR_("\r\n[+] UID:%08X\r\n"),uid);
    //     printf(_YELLOW_BR_("[+] 正在解算秘钥...\r\n"));


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

        

    //     for(uint8_t block=0;block<16;block++)  //16个扇区逐次恢复
    //     {
    //         printf(_WHITE_BR_("\r扇区%02d-------------------------------------\r\n"),block);

    //         if(!secKey[block][0][0]) //此扇区不存在数据
    //         {
    //             printf(_RED_BR_("[!] 不存在密流数据.\r\n"));
    //         }
    //         else  //此扇区存在数据
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
    //             hThread[0] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t, 0, NULL); // 创建线程

    //             ArgList_t argList_t1={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t1.UID=uid;
    //             argList_t1.statelist=statelist1;
    //             argList_t1.odd=odd1;
    //             argList_t1.even=even1;
    //             argList_t1.block=block;
    //             argList_t1.a=2;
    //             hThread[1] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t1, 0, NULL); // 创建线程

    //             ArgList_t argList_t2={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t2.UID=uid;
    //             argList_t2.statelist=statelist2;
    //             argList_t2.odd=odd2;
    //             argList_t2.even=even2;
    //             argList_t2.block=block;
    //             argList_t2.a=4;
    //             hThread[2] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t2, 0, NULL); // 创建线程

    //             ArgList_t argList_t3={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t3.UID=uid;
    //             argList_t3.statelist=statelist3;
    //             argList_t3.odd=odd3;
    //             argList_t3.even=even3;
    //             argList_t3.block=block;
    //             argList_t3.a=6;
    //             hThread[3] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t3, 0, NULL); // 创建线程

    //             ArgList_t argList_t4={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t4.UID=uid;
    //             argList_t4.statelist=statelist4;
    //             argList_t4.odd=odd4;
    //             argList_t4.even=even4;
    //             argList_t4.block=block;
    //             argList_t4.a=8;
    //             hThread[4] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t4, 0, NULL); // 创建线程

    //             ArgList_t argList_t5={0,NULL,NULL,0,NULL,NULL,NULL};
    //             argList_t5.UID=uid;
    //             argList_t5.statelist=statelist5;
    //             argList_t5.odd=odd5;
    //             argList_t5.even=even5;
    //             argList_t5.block=block;
    //             argList_t5.a=10;
    //             hThread[5] = CreateThread(NULL, 0, Mfkey32ProcessFast, &argList_t5, 0, NULL); // 创建线程

    //             WaitForMultipleObjects(THREAD_NUM,hThread,true, INFINITE);  //一直等待，直到所有子线程全部返回

    //             if(!secKeyFindSuccess)
    //                 printf(_RED_BR_("\r[!] 未找到秘钥.                             \r\n"));
    //         }
    //     }
    //     printf("\r%*s",43," ");//输出43个空格
        
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

    // stop = clock();     /*  停止计时  */

    // printf("\r\n运行时间: %d s\r\n",(stop - start) / CLK_TCK);
    // printf("共解得%d个秘钥数据\r\n",keyFindSuccessNum);
    // printf(_YELLOW_BR_("日志文件密流数据已解算结束.\r\n"));

    // // memset(autoMfkey32src,0,sizeof(autoMfkey32src)); //执行logProcessAuth函数时获取的精简记录
    // // autoMfkey32srcNum=0;                             //执行logProcessAuth函数时获取的精简记录条目数量
    // kardClassNumber=1;                               //卡片数量
    // memset(offset,0,sizeof(offset));                 //记录密流表卡号更改的偏移地址,最多256个偏移地址
    // memset(secKey,0,sizeof(secKey));
}

void autoMfkey32()
{
    // autoMfkey32srcPrint();//密流缓存打印,仅做打印...
    // kardClass();          //换成获取卡片数量及偏移地址

    // clock_t start,stop;
    // start = clock();        /*  开始计时  */

    // uint16_t keyFindSuccessNum=0;

    // for(uint8_t card=0;card<kardClassNumber;card++)
    // {
    //     printf(_YELLOW_BR_("\r\n[+] 开始解算第%d张卡片秘钥,共%d张.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] 并发解密 请注意计算机散热."));
    //     autoMfkey32cardSecArrangement(offset[card],offset[card+1]); //获取已整理的密流表

    //     uint32_t uid=(uint32_t )ByteArrayToInt(autoMfkey32src[offset[card]],2);
    //     printf(_YELLOW_BR_("\r\n[+] 并发解密开始.第%d张,共%d张.\r\n"),card+1,kardClassNumber);
    //     printf(_RED_BR_("[!] 请注意计算机散热."));
    //     printf(_YELLOW_BR_("\r\n[+] UID:%08X\r\n"),uid);
    //     printf(_YELLOW_BR_("[+] 正在解算秘钥...\r\n"));



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

    //     for(uint8_t block=0;block<16;block++)  //16个扇区逐次恢复
    //     {
    //         printf(_WHITE_BR_("扇区%02d-------------------------------------\r\n"),block);
    //         secKeyFindSuccess=0;

    //         if(!secKey[block][0][0]) //此扇区不存在数据
    //         {
    //             printf(_RED_BR_("[!] 不存在密流数据.\r\n"));
    //         }
    //         else  //此扇区存在数据
    //         {

    //             for(uint8_t a=0;a<128;a++)           //前级指针
    //             {
    //                 if(secKey[block][a][0] && !secKey[block][a][13])          //存在未处理的数据
    //                 {
    //                     for(uint8_t b=a+1;b<128;b++) //后级指针
    //                     {
    //                         if(secKey[block][b][0] && !secKey[block][b][13])  //存在未处理的数据
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
    //                             hThread[0] = CreateThread(NULL, 0, Mfkey32Process, &argList_t, 0, NULL); // 创建线程

    //                             while(secKey[block][++b][13]);//存在未处理的数据
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t1={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t1.UID=uid;
    //                             argList_t1.one=secKey[block][a];
    //                             argList_t1.two=secKey[block][b];
    //                             argList_t1.statelist=statelist1;
    //                             argList_t1.odd=odd1;
    //                             argList_t1.even=even1;
    //                             hThread[1] = CreateThread(NULL, 0, Mfkey32Process, &argList_t1, 0, NULL); // 创建线程

    //                             while(secKey[block][++b][13]);//下一个未处理的数据
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t2={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t2.UID=uid;
    //                             argList_t2.one=secKey[block][a];
    //                             argList_t2.two=secKey[block][b];
    //                             argList_t2.statelist=statelist2;
    //                             argList_t2.odd=odd2;
    //                             argList_t2.even=even2;
    //                             hThread[2] = CreateThread(NULL, 0, Mfkey32Process, &argList_t2, 0, NULL); // 创建线程

    //                             while(secKey[block][++b][13]);//下一个未处理的数据
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t3={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t3.UID=uid;
    //                             argList_t3.one=secKey[block][a];
    //                             argList_t3.two=secKey[block][b];
    //                             argList_t3.statelist=statelist3;
    //                             argList_t3.odd=odd3;
    //                             argList_t3.even=even3;
    //                             hThread[3] = CreateThread(NULL, 0, Mfkey32Process, &argList_t3, 0, NULL); // 创建线程

    //                             while(secKey[block][++b][13]);//下一个未处理的数据
    //                             printf("(%02d,%02d) ",a,b);

    //                             ArgList_t argList_t4={0,NULL,NULL,0,NULL,NULL,NULL};
    //                             argList_t4.UID=uid;
    //                             argList_t4.one=secKey[block][a];
    //                             argList_t4.two=secKey[block][b];
    //                             argList_t4.statelist=statelist4;
    //                             argList_t4.odd=odd4;
    //                             argList_t4.even=even4;
    //                             hThread[4] = CreateThread(NULL, 0, Mfkey32Process, &argList_t4, 0, NULL); // 创建线程
                                
    //                             WaitForMultipleObjects(THREAD_NUM,hThread,true, INFINITE);  //一直等待，直到所有子线程全部返回

    //                             printf("\r");
                                
    //                             uint8_t find = argList_t.status + argList_t1.status + argList_t2.status + argList_t3.status + argList_t4.status;
    //                             if(find)
    //                             {
    //                                 secKeyFindSuccess=1;
    //                                 keyFindSuccessNum+=find;
    //                                 break;
    //                             }

    //                             if(decryptionLevel==1) //普通解
    //                                 break;//对比5组就强制退出,不再对比
                                
    //                             // if(mfKey32(uid,secKey[block][a]+1,secKey[block][b]+1)) //比对成功
    //                             // {
    //                             //     secKey[block][a][13]=1; //已处理
    //                             //     secKey[block][b][13]=1; //已处理
    //                             //     break;               //结束本次循环
    //                             // }
    //                         }
    //                     }
    //                 }
    //             }
    //             if(!secKeyFindSuccess)
    //                 printf(_RED_BR_("[!] 未找到秘钥.\r\n"));
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

    // stop = clock();     /*  停止计时  */

    // printf("\r\n运行时间: %d s\r\n",(stop - start) / CLK_TCK);
    // printf("共解得%d个秘钥数据\r\n",keyFindSuccessNum);
    // printf(_YELLOW_BR_("日志文件密流数据已解算结束.\r\n"));

    // // memset(autoMfkey32src,0,sizeof(autoMfkey32src)); //执行logProcessAuth函数时获取的精简记录
    // // autoMfkey32srcNum=0;                             //执行logProcessAuth函数时获取的精简记录条目数量
    // kardClassNumber=1;                               //卡片数量
    // memset(offset,0,sizeof(offset));                 //记录密流表卡号更改的偏移地址,最多256个偏移地址
    // memset(secKey,0,sizeof(secKey));
}



//首先执行logProcessAuth以初始化资源

uint8_t autoMfkey64src[256][22]={0}; //执行logProcessAuth函数时获取的精简记录
uint8_t autoMfkey64srcNum=0;         //执行logProcessAuth函数时获取的精简记录条目数量

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
    // start = clock();        /*  开始计时  */
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
    //     printf("\r[+] UID:%08X 扇区/块号:(%02d/%02d) key%c:[" _GREEN_BR_("%012" PRIx64 ) "]\r\n",uid,autoMfkey64src[i][1]/4,autoMfkey64src[i][1],autoMfkey64src[i][0]-31, key);
        
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
    //         if(i >= num) //颜色分界处
    //         {
    //             if(color)
    //             {
    //                 colorLen=strlen(AEND);  //白色
    //                 memcpy(buf+i*2,AEND,colorLen);
    //                 color=0;
    //             }
    //             memcpy(buf+i*2+colorLen,"─",2);
    //         }
    //         else
    //         {
    //             memcpy(buf+i*2+colorLen,"█",2);
    //         }
            
    //     }
    //     color=1,colorLen=0;
    //     printf(_YELLOW_BR_("\r[%c] %s▏(%02d%%)"),state[ing++],buf,i*100/autoMfkey64srcNum);
    // }
    // stop = clock();     /*  停止计时  */
    // printf("\r%*s",54," ");//输出54个空格
    // printf("\r\n运行时间: %d s\r\n",(stop - start) / CLK_TCK);
    // printf("共解得%d个秘钥数据\r\n",autoMfkey64srcNum);
    // printf(_YELLOW_BR_("日志文件密流数据已解算结束.\r\n"));

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
HANDLE hCom;  //全局变量，全局变量，用于线程通信
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

		// uint8_t qh = data[i++]; //中文字符对应的第1个字节
		// uint8_t ql = data[i++]; //中文字符对应的第2个字节

		// /*英文***********************************************************************************************/
		// if (qh < 0x80) //ASCII
		// {
		// 	i--; //刚刚+1了,再减回去
		// 	tmp[pos + i -1]  = ((qh < 32) || (qh == 127)) ? '.' : qh;
		// }
		// /*中文***********************************************************************************************/
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
	unsigned char Entry;   //条目代码
	unsigned char str[64]; //释义
}LOG_INFO;

LOG_INFO logInfo[] = {
/* Generic */
{LOG_INFO_GENERIC                        /*0x10*/,_ANSI_RED_BR_(   "非特定日志条目.                  !!!      | ")},
{LOG_INFO_CONFIG_SET                     /*0x11*/,_ANSI_GREEN_BR_( "配置更改.                        [+]      | ")},
{LOG_INFO_SETTING_SET                    /*0x12*/,_ANSI_GREEN_BR_( "卡槽更改.                        [+]      | ")},
{LOG_INFO_UID_SET                        /*0x13*/,_ANSI_GREEN_BR_( "卡号更改.                        [+]      | ")},
{LOG_INFO_RESET_APP                      /*0x20*/,_ANSI_RED_BR_(   "程序重置.                        !!!      | ")},

/* Codec */
{LOG_INFO_CODEC_RX_DATA                  /*0x40*/,_ANSI_WHITE_( "调制解调器接收数据               <<<      | ")},
{LOG_INFO_CODEC_TX_DATA                  /*0x41*/,_ANSI_WHITE_( "调制解调器发送数据               >>>      | ")},
{LOG_INFO_CODEC_RX_DATA_W_PARITY         /*0x42*/,_ANSI_WHITE_( "调制解调器接收数据(奇偶校验)     <<<      | ")},
{LOG_INFO_CODEC_TX_DATA_W_PARITY         /*0x43*/,_ANSI_WHITE_( "调制解调器发送数据(奇偶校验)     >>>      | ")},
{LOG_INFO_CODEC_SNI_READER_DATA          /*0x44*/,_ANSI_YELLOW_BR_( "从读卡器嗅探接收的数据           [#]      | ")},
{LOG_INFO_CODEC_SNI_READER_DATA_W_PARITY /*0x45*/,_ANSI_YELLOW_BR_( "从读卡器嗅探接收的数据(奇偶校验) [#]      | ")},
{LOG_INFO_CODEC_SNI_CARD_DATA            /*0x46*/,_ANSI_WHITE_( "从射频卡嗅探接收的数据           [-]      | ")},
{LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY   /*0x47*/,_ANSI_WHITE_( "从射频卡嗅探接收的数据(奇偶校验) [-]      | ")},
{LOG_INFO_CODEC_READER_FIELD_DETECTED    /*0x48*/,_ANSI_CYAN_(     "检测到读卡器场强                 <<<      | ")},

/* App */
{LOG_INFO_APP_CMD_READ                  /*0x80*/,_ANSI_YELLOW_BR_("应用程序已处理读取命令.          ---      | ")},
{LOG_INFO_APP_CMD_WRITE                 /*0x81*/,_ANSI_YELLOW_BR_("应用程序已处理写入命令.          ---      | ")},
{LOG_INFO_APP_CMD_INC                   /*0x84*/,_ANSI_YELLOW_BR_("应用程序已处理卡号加命令.        ---      | ")},
{LOG_INFO_APP_CMD_DEC                   /*0x85*/,_ANSI_YELLOW_BR_("应用程序已处理卡号减命令.        ---      | ")},
{LOG_INFO_APP_CMD_TRANSFER              /*0x86*/,_ANSI_YELLOW_BR_("应用程序已处理传输命令.          ---      | ")},
{LOG_INFO_APP_CMD_RESTORE               /*0x87*/,_ANSI_YELLOW_BR_("应用程序已处理恢复命令.          ---      | ")},
{LOG_INFO_APP_CMD_AUTH                  /*0x90*/,_ANSI_YELLOW_BR_("应用程序已处理身份验证命令.      ---      | ")},
{LOG_INFO_APP_CMD_HALT                  /*0x91*/,_ANSI_YELLOW_BR_("应用程序已处理休眠卡命令.        ---      | ")},
{LOG_INFO_APP_CMD_UNKNOWN               /*0x92*/,_ANSI_RED_BR_(   "应用程序处理了未知命令.          !!!      | ")},
{LOG_INFO_APP_CMD_REQA                  /*0x93*/,_ANSI_YELLOW_BR_("应用程序(ISO14443A处理程序)已处理REQA.    | ")},
{LOG_INFO_APP_CMD_WUPA                  /*0x94*/,_ANSI_YELLOW_BR_("应用程序(ISO14443A处理程序)已处理WUPA.    | ")},
{LOG_INFO_APP_CMD_DESELECT              /*0x95*/,_ANSI_YELLOW_BR_("应用程序(ISO14443A处理程序)已处理DESELECT.| ")},
{LOG_INFO_APP_AUTHING                   /*0xA0*/,_ANSI_YELLOW_BR_("应用程序处于[身份验证进行]状态.  ---      | ")},
{LOG_INFO_APP_AUTHED                    /*0xA1*/,_ANSI_YELLOW_BR_("应用程序处于[身份验证成功]状态.  ---      | ")},
{LOG_ERR_APP_AUTH_FAIL                  /*0xC0*/,_ANSI_RED_BR_(   "应用程序身份验证失败.            !!!      | ")},
{LOG_ERR_APP_CHECKSUM_FAIL              /*0xC1*/,_ANSI_RED_BR_(   "应用程序校验和失败.              !!!      | ")},
{LOG_ERR_APP_NOT_AUTHED                 /*0xC2*/,_ANSI_RED_BR_(   "应用程序未通过身份验证.          !!!      | ")},

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
    // 短帧，无奇偶位
    if (byteCount == 1)
    {
        out[0]=data[0];
        return true;
    }

    // 9位是一个组，验证位计数计算如下
    int bitCount = byteCount*8;
    uint8_t *parsedData = (uint8_t*)malloc((bitCount/9)*sizeof(uint8_t));
    memset(parsedData,0,(bitCount/9)*sizeof(uint8_t));

    uint8_t oneCounter = 0;            // Counter for count ones in a byte
    int i;
    for (i=0;i<bitCount;i++)
    {
        // 获取本bit数据
        int byteIndex = i/8;
        int bitIndex  = i%8;
        uint8_t bit = (data[byteIndex] >> bitIndex) & 0x01 ;

        // 检查奇偶校验位
        // 当前位为奇偶位
        if(i % 9 == 8)
        {
            /*
            // 当前字节中的偶数
            if((oneCounter % 2) && (bit == 1))
            {
                printf("失败");
                free(parsedData);
                return false;
                goto end;
            }
            // 当前字节中奇数个1
            else if((!(oneCounter % 2)) && (bit == 0))
            {
                printf("失败");
                free(parsedData);
                return false;
                goto end;
            }
            oneCounter = 0;
            */
        }
        // 当前位为正常位
        else
        {
            oneCounter += bit;
            parsedData[i/9] |= (uint8_t)(bit << (i%9));
        }
    }

    //printf("成功");

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

    //BUFFER+=2;  //+2是解析冰人的嗅探数据
    int authnum=0;
	while((uint32_t)(BUFFER-srartArr)<loglength)
	{
		if(logExplainFind(*BUFFER++,outStr))
		{   
			printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr-1));
			char datasize = *BUFFER++; //有效数据长度
			unsigned int time = (*BUFFER++)<<8 ;
			time |= (*BUFFER++); //时间戳
			printf("时间戳:%05d | ",time);
			printf("%s",outStr); //事件
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
			char datasize = *BUFFER++; //有效数据长度
			unsigned int time = (*BUFFER++)<<8 ; //时间戳
			time |= (*BUFFER++); //时间戳
			printf("时间戳:%05d | ",time);
			//printf("%s ",outStr); //事件
			if(error_event!=0)
            {
                if(error_event==0xff)
                    printf(_ANSI_RED_BR_("[!] 设备已重启.                  !!!      | "));
                else
				    printf(_ANSI_RED_BR_("[!] 未知事件.事件代码[%02x]        ???      | "),error_event);
            }
			else
            {
                printf(_ANSI_CYAN_("数据空闲区域.                    [=]      | "));
            }
			for(char j=0;j<datasize;j++)
			{
				printf("%02X ",*BUFFER++);
			}
			printf("\r\n");
		}
	}
	printf(_WHITE_BR_("-------+--------------+-------------------------------------------+-------------------------------------------------------\r\n"));
    printf("已处理的验证请求次数:%d\r\n",authnum);
}

uint8_t mfkey32v2[22];
uint8_t mfkey64[22];

static void logProcessAuth(unsigned char * BUFFER,uint32_t loglength)
{
    memset(autoMfkey32src,0,sizeof(autoMfkey32src)); //上一次执行logProcessAuth函数时获取的精简记录
    autoMfkey32srcNum=0;                             //上一次执行logProcessAuth函数时获取的精简记录条目数量
    autoMfkey64srcNum=0;                             //上一次执行logProcessAuth函数时获取的精简记录条目数量

    printf("从上述日志列表提取的密钥流,键入automfkey32/automfkey64命令以恢复密钥\r\n");
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
        event=logExplainFind(*BUFFER++,outStr); //找事件,没有找到照样偏移(继续运行)

        addr=(uint32_t)(BUFFER-srartArr-1);     //事件地址
        datasize = *BUFFER++;                   //事件附加数据长度
        time = (*BUFFER++)<<8 ;
        time |= (*BUFFER++);                    //事件时间

        switch (event)                          //匹配
        {
        case LOG_INFO_CODEC_RX_DATA:
            if((*(BUFFER)==0x93 || *(BUFFER)==0x95) && datasize==9)  //防冲撞 更新uid
            {
                uid[0]=*(BUFFER+2);
                uid[1]=*(BUFFER+3);
                uid[2]=*(BUFFER+4);
                uid[3]=*(BUFFER+5);
                BUFFER+=datasize;   //偏移至下一个事件
            }
            else
                BUFFER+=datasize;   //偏移至下一个事件
            break;
        
        case LOG_INFO_APP_CMD_AUTH:
            authnum++;printf(_WHITE_BR_("%04d | "),authnum);
            if((*(BUFFER+datasize)==LOG_INFO_CODEC_TX_DATA) && *(BUFFER+datasize+1)==4)//下一个事件是发送数据且发送了4字节 那就是nt
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
                        printf(AEND "时间戳:%05d | ",time);
                        printf(_ANSI_YELLOW_BR_("[+]无卡嗅探:身份验证成功!          | "));
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
                        printf("时间戳:%05d | ",time);
                        printf(_ANSI_CYAN_("[+]无卡嗅探.                       | "));
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

                        //转储到密流表
                        if(autoMfkey32srcNum<255) //密流表最多存储256条密流
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
                        printf(AEND "时间戳:%05d | ",time);
                        printf(_ANSI_RED_BR_("[!]无卡嗅探:不符合要求.            | "));
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
                    printf(AEND "时间戳:%05d | ",time);
                    printf(_ANSI_RED_BR_("[!]无卡嗅探:nr/ar接收错误.         | "));
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
                printf(AEND "时间戳:%05d | ",time);
                printf(_ANSI_RED_BR_("[!]无卡嗅探:不符合要求.            | "));
                for(uint8_t i=0;i<22;i++)
                {
                    printf("-- ");
                }
                printf("\r\n");
                BUFFER+=datasize;   //偏移至下一个事件
            }
            break;
        case LOG_INFO_CODEC_SNI_READER_DATA:
            if(datasize==9 && *BUFFER==0x93 && *(BUFFER+1)==0x70)  //选卡命令
            {
                //printf("选卡命令\r\n");
                uid[0]=*(BUFFER+2);
                uid[1]=*(BUFFER+3);
                uid[2]=*(BUFFER+4);
                uid[3]=*(BUFFER+5);
                BUFFER+=datasize;   //偏移至下一个事件
            }
            else if(datasize==4 && (*BUFFER==0x60 || *BUFFER==0x61) && *(BUFFER+datasize)==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER+datasize+1)==5)  //身份验证请求
            {
                authnum++;printf(_WHITE_BR_("%04d | "),authnum);//验证记录

                //printf("身份验证请求\r\n");
                keyType=*BUFFER;
                keysec=*(BUFFER+1);
                BUFFER+=datasize;   //偏移至下一个事件
                if(*BUFFER==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER+1)==5)  //nt
                {
                    //printf("nt\r\n");
                    BUFFER+=4;
                    checkParityBit(BUFFER,5,nt0);
                    BUFFER+=5;  //移至下一个事件
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

                        BUFFER+=12; //移至下一个事件

                        if(*BUFFER==LOG_INFO_CODEC_SNI_CARD_DATA_W_PARITY && *(BUFFER+1)==5)  //at
                        {
                            //printf("at\r\n");
                            BUFFER+=4;
                            checkParityBit(BUFFER,5,at0);
                            BUFFER+=5;  //移至下一个事件

                            authnumsuccess++;
                            printf(AEND "0x%04X | ",(uint32_t)(BUFFER-srartArr));
                            printf(AEND "时间戳:%05d | ",time);
                            printf(_ANSI_GREEN_BR_("[+]有卡嗅探.                       | "));
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

                            //转储到密流表
                            if(autoMfkey64srcNum<255) //密流表最多存储256条密流
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
                            printf(AEND "时间戳:%05d | ",time);
                            printf(_ANSI_RED_BR_("[!]有卡嗅探:at接收错误.            | "));
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
                        printf(AEND "时间戳:%05d | ",time);
                        printf(_ANSI_RED_BR_("[!]有卡嗅探:nr+ar接收错误.         | "));
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
                    printf(AEND "时间戳:%05d | ",time);
                    printf(_ANSI_RED_BR_("[!]有卡嗅探:nt接收错误.            | "));
                    for(uint8_t i=0;i<22;i++)
                    {
                        printf("-- ");
                    }
                    printf("\r\n");
                }
            }
            else
                BUFFER+=datasize;   //偏移至下一个事件
            break;
        case LOG_INFO_CODEC_SNI_READER_DATA_W_PARITY:
            break;

        default:
            BUFFER+=datasize;   //偏移至下一个事件
            break;
        
        }
	}
	printf(_WHITE_BR_("-----+--------+--------------+------------------------------------+-------------------------------------------------------------------\r\n"));
    printf("已处理的验证请求次数:%d\r\n",authnum);
    printf("成功:%d次,失败:%d次\r\n",authnumsuccess,authnumfault);
    printf("包含%d次接受数据有误或不符合要求\r\n\r\n",authnumerror);
    
    
}


/************************************************************************************************/

#define MEMORY_SIZE_PER_SETTING		8192
char XmodeSendStr[MEMORY_SIZE_PER_SETTING];

#define RECEIVE_BUF_SIZE	          1024*18+20 /*最大下载量即日志文件,变色龙一次最大返回18kb的日志文件,留出20byte空间*/
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
				/*读取整卡数据*****************************************************/
				FILE *fp;
				fp = fopen(receiveFile, "wb");
				fwrite((const void *)XmodeReceiveStr, sizeof(unsigned char), BlockAddress, fp);
				fclose(fp);

				printf("\r\n");

				if(downloadtype==1)
				{
					for(int addr=0;addr<BlockAddress/16;addr++)                                    //寻址范围0~3f(16扇区*4块=64块)
					{
						if(addr%4==0)                                                          //四块为一个扇区,显示分割
							printf(_YELLOW_BR_("\r\n%02d扇区\r\n") 
							"-----------------------------------------------\r\n",addr/4);
						unsigned char data[16];                                                //数据缓存
						memcpy(data,(const void *)(XmodeReceiveStr+addr*16),16);
						for(unsigned char i=0;i<16;i++)                                                 //一块16个字节
						{
							printf("%02X ",data[i]);                                           //打印一个字节
						}
						printf("\r\n");                                                        //一块数据打印完毕换行
					}
					printf("\r\n");  
				}

				else if(downloadtype==2)
				{
                    if(BlockAddress==2048)
                    {
                        printf(_RED_BR_("日志为空.没有任何可以解析的数据记录."));
                        goto LOGNULL;
                    }
					printf("\r\n日志预览\r\n");
                    if(BlockAddress==18432)
					    print_logs((BlockAddress)/0x10,(uint8_t*)XmodeReceiveStr);
                    else
                        print_logs((BlockAddress-1024*2)/0x10,(uint8_t*)XmodeReceiveStr); //日志没满总会多传输2K的空数据，不显示它
					printf("\r\n日志数据解析结果\r\n");
                    if(BlockAddress==18432)
					    logProcess((uint8_t*)XmodeReceiveStr,BlockAddress);
                    else
                        logProcess((uint8_t*)XmodeReceiveStr,BlockAddress-1024*2); //日志没满总会多传输2K的空数据，不显示它
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
				printf(_GREEN_BR_("[=] 接收完毕.\r\n"));
                if(BlockAddress==18432)
                    printf(_RED_BR_("[!] 注意:变色龙日志存储空间已满,请及时清理 [日志存储器现已关闭].\r\n"));
                else
                    printf(_CYAN_("[!] 注意:变色龙传输附加的2kb空白日志数据已自动忽略.\r\n"));
				printf(_YELLOW_BR_("[=] 已写入文件:%s 共计%d byte.\r\n"),receiveFile,BlockAddress);
                if(downloadtype==2)
                    printf(_YELLOW_BR_("[?] 若要从上述日志文件提取密流解算秘钥,请键入<automfkey32>/<automfkey64>命令.并耐心等待程序运行结束."));
                LOGNULL:
                downloadtype=0;
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
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
			printf(_GREEN_BR_("\r\n[=] 上传成功."));
			printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
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
					printf(_RED_BR_("[!] 变色龙无响应.等待已超时."));
					printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
                }

                RetryTimeout = RECV_INIT_TIMEOUT;
            }
            break;

        case STATE_SEND_INIT:
            if (RetryTimeout-- == 0) {
                /* Abort */
                State = STATE_OFF;
				XmodeFlag=0;
				printf(_RED_BR_("[!] 变色龙无响应.等待已超时."));
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
            }
            break;

/**************************************************************************/
/*                      针对数据线松动造成的丢包问题                           */
/**************************************************************************/
        case STATE_SEND_WAIT:
            if (RetryTimeout-- == 0) {
                /* Abort */
                State = STATE_OFF;
				XmodeFlag=0;
				printf(_RED_BR_("\r\n[!] 发送失败,请重新启动传输."));
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
            }
            break;

        case STATE_OFF:
            if(XmodeFlag != 1)
                return;
            if (RetryTimeout-- == 0) {
                /* Abort */
                State = STATE_OFF;
				XmodeFlag=0;
				printf(_RED_BR_("\r\n[!] 未知错误,请重新启动传输."));
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
            }
            break;

/**************************************************************************/
        default:
            //printf("%d,%d",(char)State,RetryTimeout);
            break;
    }
}

void *XModemTickProcess(void)
{
	while(1)
	{
        #ifdef _WIN32
		Sleep(1);
        #else
        sleep(1);
        #endif
		XModemTick();
		if(XmodeFlag==0)
			return ;
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

/* 设置串口参数
 * 可改为传参给定
 * 超时按最快响应设置
 */
int setUart(HANDLE hCom_1)
{
	COMMTIMEOUTS timeouts;
	DCB dcb;

	// 读超时
	timeouts.ReadIntervalTimeout         = 1/*MAXDWORD*/; // 读操作时两个字符间的间隔超时
	timeouts.ReadTotalTimeoutMultiplier  = 0; // 读操作在读取每个字符时的超时
	timeouts.ReadTotalTimeoutConstant    = 1/*0*/; // 读操作的固定超时
	// 写超时
	timeouts.WriteTotalTimeoutMultiplier = 0/*0*/; // 写操作在写每个字符时的超时
	timeouts.WriteTotalTimeoutConstant   = 0; // 写操作的固定超时

	// 设置超时
	SetCommTimeouts(hCom_1, &timeouts);

	// 设置输入输出缓冲区大小
	SetupComm(hCom_1, INBUFLEN, OUTBUFLEN);

	// 获取串口参数
	if (GetCommState(hCom_1, &dcb) == 0)
	{
		return -1;
	}

	// 设置串口参数
	dcb.BaudRate = CBR_115200; // 波特率
	dcb.ByteSize = 8;          // 数据位数
	dcb.Parity   = NOPARITY;   // 校验位
	dcb.StopBits = ONESTOPBIT; // 停止位
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
        HANDLE hCom; //全局变量，串口句柄 
        char cTemp[MAX_PATH]; 
        char cTempFull[MAX_PATH]; 
        sprintf(cTemp, "COM%d", i);
        sprintf(cTempFull, "\\\\.\\COM%d", i);
        hCom=CreateFile(cTempFull,//COM1口 
            GENERIC_READ|GENERIC_WRITE, //允许读和写 
            0, //独占方式 
            NULL, 
            OPEN_EXISTING, //打开而不是创建 
            0, //同步方式 
            NULL); 
        if(hCom==(HANDLE)-1) 
        { 
            //printf("打开COM%d失败!\r\n",i); 
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
        // printf("无可用串口,请确保当前计算机已正确安装ChameleonMini驱动程序.\r\n");
        // getch();
        // exit(0);

        int userSelect = MessageBoxA(0, "未检测到可用串口！\r\n请确保当前计算机已正确安装ChameleonMini驱动程序并未被其他程序占用", "错误", MB_ABORTRETRYIGNORE|MB_ICONERROR);

        if(userSelect == IDABORT) //中止 
            exit(0);
        else if(userSelect == IDRETRY)//重试
            goto START;
        else if(userSelect == IDIGNORE)//忽略
        {
            userSelect = MessageBoxA(0, "是否跳过串口检查?\r\n是: 跳过串口初始化,直接进入主程序(用于功能测试)\r\n否: 键入可用串口号(可能未被枚举的值)", "串口检查项", MB_YESNO|MB_ICONQUESTION);
            if(userSelect == IDYES)
            {
                return; /* 直接退出 */
            }
            else if(userSelect == IDNO)
            {
                /* 无需做任何事 */
            }
        }
    }
    else
        printf("可用串口: %s\r\n",cCom);

	OPEN:
	//printf(logo);
	printf("\r\n选择ChameleonMini串口号(例如\"5\"):");

	char com[32]={0};
	int num=0;
	scanf("%d",&num);
	getc(stdin); //吃掉输入的回车

	if(num<10)
		sprintf(com,"COM%d",num);
	else
		sprintf(com,"\\\\.\\COM%d",num);
	
	// 打开串口
	hCom = CreateFile(com, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hCom != INVALID_HANDLE_VALUE)
	{
		printf("串口打开成功\n");
	}
	else
	{
		printf("串口打开失败，重新插拔设备可能会解决问题\n\n");
		goto OPEN;
	}

	// 配置串口
	if (setUart(hCom) == -1)
	{
		if (INVALID_HANDLE_VALUE != hCom)
			CloseHandle(hCom); // 关闭串口
	}

    // sprintf(com,"COM%d",num);
    // sp1 = uart_open(com,115200);
    // if(sp1 == INVALID_SERIAL_PORT)
    // {
    //     printf("串口打开失败，重新插拔设备可能会解决问题\n\n");
	// 	goto OPEN;
    // }
    printf("串口打开成功\n");
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



/* 用于关闭串口读取线程 */
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
			//printf(_RED_BR_("[+] 正在接收...\r\n"));
			ReadFile(hCom, rbuf, sizeof(rbuf), &rsize, NULL);
            // 处理读取到的数据
            if (rsize > 0) {
                //printf("Received: %.*s\n", rsize, rbuf);
                for(int i=0;i<rsize;i++)
                {
                    static int arr=0;
                    if(XmodeFlag==1)  //上传
                    {
                        //printf("%02x ", rbuf[i]);
                        //printf(".");
                        receive++;
                        char buf[128]={0};
                        double rate = (receive*60/66);  //发送8KB(8*128byte)，一个包128byte，共可收到64次包接收完毕确认信号，加上开始和结束信号，共66次

                        static uint8_t color=1,colorLen=0;
                        for(int j = 0; j < 60; j++)
                        {
                            if(j >= rate) //颜色分界处
                            {
                                if(color)
                                {
                                    colorLen=strlen("\x1b[92m");  //绿色
                                    memcpy(buf+j*2,"\x1b[92m",colorLen);
                                    color=0;
                                }
                            }
                            memcpy(buf+j*2+colorLen,"█",2);
                        }
                        color=1,colorLen=0;
                        printf(_YELLOW_BR_("\r[+] %s [%02d%%] "),buf,receive*100/66);
                        if(receive==66)
                            receive=0;

                        XModemProcessByte(rbuf[i]);
                    }
                    else if(XmodeFlag==2)  //接收
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
                                if(j < rate) //输出num个">"
                                    memcpy(buf+j*2,"█",2);
                                else
                                    memcpy(buf+j*2,"─",2);
                            }
                            printf(_YELLOW_BR_("\r[+] 空间使用率: %s▏[%02d%%] "),buf,BlockAddress*100/(uint32_t)(RECEIVE_BUF_SIZE-20));
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
                //printf(_RED_BR_("[+] 接收已完毕...\r\n"));
            }
		}
	}
}

#else


void *readUartPM3(void)
{
    while(1)
    {
        do{
            uart_receive(sp1,rbuf,INBUFLEN,&rsize);
            if(shutdowmread)
            {
                shutdowmread=0;
                return;
            }
        }while(!rsize);

        //printf("长度：%ld,数据：%s",rsize,rbuf);

        // 处理读取到的数据
        if (rsize > 0) {
            //printf("Received: %.*s\n", rsize, rbuf);
            for(int i=0;i<rsize;i++)
            {
                static int arr=0;
                if(XmodeFlag==1)  //上传
                {
                    //printf("%02x ", rbuf[i]);
                    //printf(".");
                    receive++;
                    char buf[128]={0};
                    double rate = (receive*60/66);  //发送8KB(8*128byte)，一个包128byte，共可收到64次包接收完毕确认信号，加上开始和结束信号，共66次

                    static uint8_t color=1,colorLen=0;
                    for(int j = 0; j < 60; j++)
                    {
                        if(j >= rate) //颜色分界处
                        {
                            if(color)
                            {
                                colorLen=strlen("\x1b[92m");  //绿色
                                memcpy(buf+j*2,"\x1b[92m",colorLen);
                                color=0;
                            }
                        }
                        memcpy(buf+j*2+colorLen,"█",2);
                    }
                    color=1,colorLen=0;
                    printf(_YELLOW_BR_("\r[+] %s [%02d%%] "),buf,receive*100/66);
                    if(receive==66)
                        receive=0;

                    XModemProcessByte(rbuf[i]);
                }
                else if(XmodeFlag==2)  //接收
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
                            if(j < rate) //输出num个">"
                                memcpy(buf+j*2,"█",2);
                            else
                                memcpy(buf+j*2,"─",2);
                        }
                        printf(_YELLOW_BR_("\r[+] 空间使用率: %s▏[%02d%%] "),buf,BlockAddress*100/(uint32_t)(RECEIVE_BUF_SIZE-20));
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
            //printf(_RED_BR_("[+] 接收已完毕...\r\n"));
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

	MessageBoxA(0, "变色龙WIN32命令行终端应用，未经发布的测试版本！\r\n详情可访问 https://www.PM3_SE_hub.com", "警告!", MB_OK|MB_ICONWARNING);

    int mcu = MessageBoxA(0, "您使用的是STM32设备吗?\r\n\r\n是: 选择STM32开始\r\n否: 以官方AVR版本开始", "设备选择", MB_YESNO|MB_ICONQUESTION);
    if(mcu == IDYES)
    {
        mcu=stm32;
    }
    else if(mcu == IDNO)
    {
        mcu=avr;
    }

    start:

	initUart(); //初始化串口

	printf(SCREEN_CLEAR); //清屏

	//调用线程读串口
	_beginthread((void *)readUart, 0, NULL);

    //调用线程读串口
	//_beginthread((void *)readUartPM3, 0, NULL);

#else

    printf("警告!变色龙WIN32命令行终端应用，未经发布的测试版本！\r\n详情可访问 https://www.PM3_SE_hub.com");
    char mcu;
    printf("设备选择 - 您使用的是STM32设备吗?\r\n\r\nY: 选择STM32开始\r\nN: 以官方AVR版本开始");
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
    if(sp1 == INVALID_SERIAL_PORT) {
        printf("串口打开失败，重新插拔设备可能会解决问题\n\n");
    }
    else {
        printf("串口打开成功\n");
    }

    printf(SCREEN_CLEAR); //清屏

    //调用线程读串口
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, readUartPM3, NULL);
    if (ret != 0) {
        printf("Failed to create thread.\n");
        return 1;
    }
    printf("Thread created successfully.\n");

    printf("\r\n");

#endif

	//主线程写串口
	while(1)
	{
        #ifdef _WIN32
		if (INVALID_HANDLE_VALUE != hCom)
		{
        #endif
			//printf(_RED_BR_("[+] 等待输入...\r\n"));
			char c ;
			uint8_t len=0;
			while((c=getc(stdin)) !='\n')
			{
				wbuf[len++]=c;
			}
            if(strlen(wbuf)==0)
            {
                printf(_RED_BR_("[!] 无效的数据输入.键入[help]以获取帮助信息.\r\n"));
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
                
				memset(wbuf,0,OUTBUFLEN);
				continue;
            }
            //printf(_RED_BR_("[+] 输入完毕...\r\n"));
			wbuf[len++]='\r';
			wbuf[len++]='\n';
			if(strcasecmp((const char *)wbuf,"cls\r\n")==0)
			{
				printf(SCREEN_CLEAR); //清屏
                #ifdef _WIN32
				WriteFile(hCom, "ping\r\n", strlen("ping\r\n"), &wsize, NULL);
                #else
                uart_send(sp1,"ping\r\n", strlen("ping\r\n"));
                #endif
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
            if(strcasecmp((const char *)wbuf,"make\r\n")==0)
			{
                /* 使用绝对路径------------------------------------------- */
                /*
                char path[PATH_MAX]; //PATH_MAX is defined in limits.h
                char *p; // 存放最后一个斜杠字符的位置
                getcwd(path, sizeof(path)); // 获取当前工作目录
                uint8_t i = 0;
                while(path[i])
                {
                    if(path[i]=='\\')
                        path[i]='/';
                    i++;
                }
                p = strrchr(path, '\\'); // 找到最后一个斜杠字符
                if (p) {
                    *p = '\0'; // 将其替换为\0
                }
                strcat(path,"/Make");
                char pathMakeCMD[PATH_MAX];
                sprintf(pathMakeCMD,"make -C %s size -j8",path);

                system(pathMakeCMD);
                */
                /* 使用相对路径------------------------------------------- */

                system("make -C ../Make size -j8");

                printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			// if(strcasecmp((const char *)wbuf,"reconnect\r\n")==0)
			// {
            //     //deInitUart();
            //     initUart(); //初始化串口
            //     printf(SCREEN_CLEAR); //清屏
            //     //调用线程读串口
            //     _beginthread((void *)readUart, 0, NULL);
			// 	memset(wbuf,0,OUTBUFLEN);
			// 	continue;
			// }
			if(strcasecmp((const char *)wbuf,"mfkey\r\n")==0)
			{
                //logProcessAuth((uint8_t*)XmodeReceiveStr,BlockAddress);
                mfkey();
                //crapto1();
                getc(stdin); //吃掉回车
				printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			if(strcasecmp((const char *)wbuf,"autoMfkey32\r\n")==0)
			{
                printf(_RED_BR_("[!] 执行此任务前请关闭其他应用程序,并确保计算机散热情况良好.\r\n"));
                printf(_YELLOW_BR_("[+] 选择解密方式: \r\n"
                                   "[+] 0 - 快速解 (高并发执行相邻两次的密流计算,适用于大多数情况)\r\n"
                                   "[+] 1 - 可靠解 (并发执行相邻的多条密流计算,当快速解不能满足要求时应选用此方式)\r\n"
                                   "[+] 2 - 完全解 (对密流数据进行完全匹配分析,所得结果更精确,但耗时更长)\r\n"
                                   "[+] "));
                scanf("%d",&decryptionLevel);
                getc(stdin); //吃掉输入的回车
                if(decryptionLevel)
                    autoMfkey32();
                else
                    autoMfkey32Fast();
                printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			if(strcasecmp((const char *)wbuf,"autoMfkey64\r\n")==0)
			{
                autoMfkey64();

                printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
            if(strcasecmp((const char *)wbuf,"upgrade\r\n")==0 && (mcu == stm32))
			{
                memset(wbuf,0,OUTBUFLEN);
                
                printf(_YELLOW_BR_("[+] 选择一个选项以开始更新您的设备\r\n"
                                   "[+] 1 - 从当前目录查找 Chameleon-Mini.elf 以更新设备\r\n"
                                   "[+] 2 - 我有自己的固件程序文件及保存路径\r\n"
                ));

                scanf("%d",&decryptionLevel);
                getc(stdin); //吃掉输入的回车

                #ifdef _WIN32
                WriteFile(hCom, "upgrade\r\n", strlen("upgrade\r\n"), &wsize, NULL);  //发送更新
                #else
                uart_send(sp1,"upgrade\r\n", strlen("upgrade\r\n"));
                #endif

                //uart_close(sp1);//关闭串口
                deInitUart();
                shutdowmread=1; //关闭接收线程

                printf(_GREEN_BR_("[+] STM32加载引导程序...\r\n"));
                printf(_GREEN_BR_("[+] STM32已进入BootLoader\r\n"));

                if(decryptionLevel == 1)
                {
                    char path[PATH_MAX]; //PATH_MAX is defined in limits.h
                    getcwd(path,sizeof(path));
                    strcat(path,"\\固件更新.exe");
                    system(path);

                    printf("\r\n\r\n");
                    uint8_t state[4]={'-','\\','|','/'};
                    static char ing=0;
                    for(int i=0;i<20;i++)
                    {
                        if(ing==4)
                            ing=0;
                        printf(_YELLOW_BR_("\r[%c] 正在更新可用端口...."),state[ing++]);
                        #ifdef _WIN32
                        Sleep(100);
                        #else
                        sleep(1);
                        #endif
                    }
                    goto start;

                    return 0;
                }
                
                //此时已断开连接
				printf(_YELLOW_BR_("[+] 指定固件所在路径和固件名称,禁止包含中文字符,用例 D:/MyFiles/Chameleon-Mini.elf\r\n[+] "));
				upgradeflag=1;
                
				continue;
			}
			if(upgradeflag)
			{
				upgradeflag=0;
				wbuf[len-2]='\0';
				FILE *fp;
				//char XmodeSendStr[1024];
				//判断文件是否打开失败
				if ( (fp = fopen((const char *)wbuf, "rb")) == NULL ) //rt 读txt rb 读bin wb 写bin 
				{
					printf(_RED_BR_("[!] 找不到文件.指定的文件或文件路径不存在.\r\n"));
                    printf(_RED_BR_("[!] %s"),wbuf);
					printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}

                printf("\r\n");
                printf(_YELLOW_BR_("[+] 确认是否擦除用户数据\r\n"
                                   "[+] ----------------------------\r\n"
                                   "[+] 1- 擦除用户存储的卡片及配置信息\r\n"
                                   "[+] 2- 保留用户存储的卡片及配置信息\r\n"
                                   "[+] "
                ));

                scanf("%d",&decryptionLevel);
                getc(stdin); //吃掉输入的回车

                //调用外部接口下载
                char path[PATH_MAX]; //PATH_MAX is defined in limits.h
                getcwd(path,sizeof(path));
                strcat(path,"/FlashDownload/STM32CubeProgrammer/STM32_Programmer_CLI.exe");
                printf("烧录程序路径：%s\r\n",path);

                char pathDownload[PATH_MAX];
                char pathEarse[PATH_MAX];
                char pathStart[PATH_MAX];
                strcpy(pathDownload,path);
                strcpy(pathEarse,path);
                strcpy(pathStart,path);

                
                strcat(pathDownload," -c port=usb1 -w ");
                strcat(pathDownload,wbuf);
                strcat(pathDownload," 0x08000000");
                printf("烧录命令：%s\r\n",pathDownload);

                
                strcat(pathEarse," -c port=usb1 -e all");
                printf("擦除命令：%s\r\n",pathEarse);
                strcat(pathStart," -c port=usb1 -s");
                printf("启动命令：%s\r\n",pathStart);

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
                                  "                   完毕,现在可使用终端软件连接STM32,或安全拔出.                \r\n"
                                  "      -------------------------------------------------------------------\r\n"
                                  "                                                                         \r\n"
                                  "STM32变色龙迷你.版权所有,盗版必究!                                           \r\n"
                ));

                // 重启程序
                // Sleep(1000);
                // execl(argv[0], NULL, NULL);
                printf("\r\n\r\n");
                uint8_t state[4]={'-','\\','|','/'};
                static char ing=0;
                for(int i=0;i<20;i++)
                {
                    if(ing==4)
                        ing=0;
                    printf(_YELLOW_BR_("\r[%c] 正在更新可用端口...."),state[ing++]);
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
                receive=0; /*进度条归零*/
				printf(_YELLOW_BR_("[+] 指定文件路径和文件名,如 D:/我的文档/门卡.dump\r\n[+] "));
				uploadflag=1;
				memset(wbuf,0,OUTBUFLEN);
				continue;
			}
			else if (strcasecmp((const char *)wbuf,"download\r\n")==0 || strcasecmp((const char *)wbuf,"logdownload\r\n"/*"DETECTION?\r\n"*/)==0)
			{
				memset(XmodeReceiveStr,0,RECEIVE_BUF_SIZE);
				if(strcasecmp((const char *)wbuf,"download\r\n")==0)
				{
					printf(_YELLOW_BR_("[+] 指定保存的文件名,如:门卡1.dump\r\n[+] "));
					downloadflag=1;
					downloadtype=1;
				}
				else
				{
					printf(_YELLOW_BR_("[+] 指定保存的文件名,如:log.bin\r\n[+] "));
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
				//判断文件是否打开失败
				if ( (fp = fopen((const char *)wbuf, "rb")) == NULL ) //rt 读txt rb 读bin wb 写bin 
				{
					printf(_RED_BR_("[!] 找不到文件.指定的文件或文件路径不存在.\r\n"));
                    printf(_RED_BR_("[!] %s"),wbuf);
					printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}
				memset(wbuf,0,OUTBUFLEN);

				//获取文件大小
				fseek(fp, 0, SEEK_END);//定位文件指针到文件尾。
				long int size=ftell(fp);//获取文件指针偏移量，即文件大小。
				if(size>1024*8)
				{
					printf(_RED_BR_("[!] 文件大小超出范围:%ld byte 最大 %d byte."),size,MEMORY_SIZE_PER_SETTING);
					printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}
				printf(_YELLOW_BR_("[=] 文件大小:%ld byte.\r\n"),size);

				//循环读取文件的每一行数据
				fseek(fp, 0, SEEK_SET);//定位文件指针到文件头。
				fread(XmodeSendStr, size, 1, fp); // 二进制读 != NULL )
				fclose(fp);
				/*读取整卡数据*****************************************************/
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
				printf(_YELLOW_BR_("[!] 开始上传.\r\n"));
				XmodeFlag=1;
				continue;
			}
			else if(downloadflag)
			{
				wbuf[len-2]='\0';
				FILE *fp;
				if ( (fp = fopen((const char *)wbuf, "w+")) == NULL ) //新建可读可写的文件
				{
					printf(_RED_BR_("[!] 创建文件时出错.文件被占用或只读文件"));
					printf(OPTIONAL_ANSWER_TRAILER);//加上usb->
					memset(wbuf,0,OUTBUFLEN);
					continue;
				}
				printf(_GREEN_BR_("[=] 创建文件:%s\r\n"),wbuf);
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
				Sleep(500);  //等待接收进程处理完最后一则消息
				_beginthread((void *)XModemTickProcess, 0, NULL);
                #else
                sleep(1);  //等待接收进程处理完最后一则消息
                pthread_t thread;
				pthread_create(&thread, NULL, XModemTickProcess, NULL);
                #endif
				printf(_YELLOW_BR_("[!] 开始接收.\r\n"));
				XmodeFlag=2;
				continue;
			}
			//printf(_RED_BR_("[+] 正在发送...\r\n"));
            #ifdef _WIN32
			WriteFile(hCom, wbuf, strlen((const char *)wbuf), &wsize, NULL);
            #else
            uart_send(sp1, wbuf, strlen((const char *)wbuf));
            #endif
			memset(wbuf,0,OUTBUFLEN);
			//printf(_RED_BR_("[+] 发送已完毕...\r\n"));
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
读头验证地址(块):44 秘钥类型:KeyA 截获的秘钥(1) = 111111111111
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
