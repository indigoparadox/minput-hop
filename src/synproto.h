
#ifndef SYNPROTO_H
#define SYNPROTO_H

#ifdef MINPUT_OS_WIN
typedef unsigned long int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef int ssize_t;
#  include <winsock2.h>
#  include <windows.h>
#else
#  include <stdint.h>
#  include <stddef.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <arpa/inet.h>
#endif /* MINPUT_OS_WIN */

#include <stdarg.h>

#ifndef SOCKBUF_SZ
#  define SOCKBUF_SZ 4096
#endif /* !SOCKBUF_SZ */

#define swap_32( num ) (((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000))

#define swap_16( num ) ((num>>8) | (num<<8));

void synproto_dump( const char* buf, size_t buf_sz );

size_t synproto_printf(
   char* buf, size_t buf_sz, const char* fmt, va_list args );

uint32_t synproto_send( int sockfd, uint8_t force_sz, const char* fmt, ... );

void synproto_parse( int sockfd, const char* pkt_buf, size_t pkt_buf_sz );

#endif /* SYNPROTO_H */

