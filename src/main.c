
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef int ssize_t;
#else
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>
#endif

#include <assert.h>
#include <stdio.h>

#define swap_32( num ) (((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000))

#define swap_16( num ) ((num>>8) | (num<<8));

#define SCREEN_NAME mintest
#define SERVER_IP "192.168.250.166"
#define SERVER_PORT 24800

#define SOCKBUF_SZ 4096

void dump_pkt( const char* buf, size_t buf_sz ) {
   size_t i = 0;

   printf( "%ld bytes\n", buf_sz );
   for( i = 0 ; buf_sz > i ; i++ ) {
      printf( "0x%02x (%c) ", buf[i], buf[i] );
   }
   printf( " (%s)\n", buf );
}

#define SYN_INT_SZ 2
#define SYN_INT_TYPE uint16_t

size_t syn_printf( char* buf, size_t buf_sz, const char* fmt, va_list args ) {
   int in_token = 0;
   uint8_t arg_i8 = 0;
   uint16_t arg_i16 = 0;
   uint32_t arg_i32 = 0;
   uint8_t* arg_p = NULL;
   size_t in_pos = 0;
   size_t out_pos = 0;
   char* arg_s = NULL;
   size_t i = 0;

   while( in_pos < strlen( fmt ) ) {
      switch( fmt[in_pos] ) {
      case '%':
         /* Default to 4-byte int token (32-bits). */
         in_token = 4;
         break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '0':
         if( in_token ) {
            /* Don't leave token yet! */
            in_token = atoi( &(fmt[in_pos]) );
            break;
         }

         /* Proceed to default if not in token. */

      case 'i':
         if( in_token ) {
            /* Insert a raw (non-ASCII) integer in network order. */
            /* TODO: Handle flexible sizes? */
            switch( in_token ) {
            case 4:
               arg_i32 = va_arg( args, int );
               arg_p = (uint8_t*)&arg_i32;
               break;

            case 2:
               arg_i16 = va_arg( args, int );
               arg_p = (uint8_t*)&arg_i16;
               break;

            case 1:
               arg_i8 = va_arg( args, int );
               arg_p = (uint8_t*)&arg_i8;
               break;

            default:
               printf( "invalid token size (%d)?\n", in_token );
               goto cleanup;
            }

            /* Reverse the integer as we insert it. */
            for( i = 0 ; in_token > i ; i++ ) {
               buf[out_pos + in_token - i - 1] = arg_p[i];
            }
            out_pos += in_token;

            in_token = 0;
            break;
         }

         /* Proceed to default if not in token. */

      case 's':
         if( in_token ) {
            arg_s = va_arg( args, char* );
            printf( "str: %s\n", arg_s );
            for( i = 0 ; '\0' != arg_s[i]; i++ ) {
               buf[out_pos++] = arg_s[i];
            }
            break;
         }

         /* Proceed to default if not in token. */

      default:
         buf[out_pos++] = fmt[in_pos];
         in_token = 0;
         break;
      }
      in_pos++;
   }

cleanup:

   return out_pos;
}

uint32_t send_syn_msg( int sockfd, uint8_t force_sz, const char* fmt, ... ) {
   char out_buf[SOCKBUF_SZ + 1];
   uint32_t out_pos = 0;
   va_list args;
   uint32_t* buf_sz_p = (uint32_t*)&(out_buf);
   
   memset( out_buf, 0, SOCKBUF_SZ + 1 );

   va_start( args, fmt );
   /* Reserve 4 bytes for message size. */
   out_pos = syn_printf( &(out_buf[4]), SOCKBUF_SZ - 4, fmt, args );
   va_end( args );

   printf( "sending packet:\n" );
   dump_pkt( out_buf, out_pos + 4 );

   if( force_sz ) {  
      printf( "buf_sz_p (%p): %u\n", buf_sz_p, out_pos );
      *buf_sz_p = swap_32( 4 );
   } else {
      printf( "buf_sz_p (%p): %u\n", buf_sz_p, out_pos );
      *buf_sz_p = swap_32( out_pos );
   }

   send( sockfd, out_buf, out_pos + 4, 0 );

   return out_pos;
}

int main() {
   int sockfd = 0;
   uint32_t pkt_sz = 0,
      pkt_type = 0;
   uint16_t ver_maj = 0,
      ver_min = 0;
   struct sockaddr_in servaddr;
   int retval = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   ssize_t i = 0,
      recv_sz = 0;
#ifdef WIN32
   WSADATA wsa_data;
#endif /* WIN32 */

   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );

#ifdef WIN32
   printf( "starting up...\n" );
   retval = WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );
   if( NO_ERROR != retval ) {
      printf( "error at WSAStartup()\n" );
      goto cleanup;
   }
#endif /* WIN32 */
   
   /* Resolve server address. */
   memset( &servaddr, 0, sizeof( struct sockaddr_in ) );
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons( SERVER_PORT );
   servaddr.sin_addr.s_addr = inet_addr( SERVER_IP );

   /* Open socket and connect. */
   printf( "connecting to 0x%08x...\n", servaddr.sin_addr.s_addr );
   sockfd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
   if( -1 == sockfd ) {
      perror( "socket" );
      goto cleanup;
   }

   retval = connect(
      sockfd, (struct sockaddr*)&servaddr, sizeof( struct sockaddr ) );
   if( retval ) {
      perror( "connect" );
      goto cleanup;
   } else {
      printf( "connected\n" );
   }

   do {

      /* Receive handshake opener. */
      memset( sockbuf, '\0', SOCKBUF_SZ + 1 );
      recv_sz = recv( sockfd, sockbuf, SOCKBUF_SZ, 0 );
      if( 0 >= recv_sz ) {
         continue;
      }

      /* src/lib/barrier/protocol_types.cpp */

      memcpy( &pkt_sz, sockbuf, 4 );
      pkt_sz = swap_32( pkt_sz );
      
      memcpy( &pkt_type, &(sockbuf[4]), 4 );
      pkt_type = swap_32( pkt_type );

      printf( "pkt type: %d\n", pkt_type );

      switch( pkt_type ) {

      case 1113682546: /* "Barr" */

         /* Grab the version out of the server packet. */
         memcpy( &ver_maj, &(sockbuf[11]), 2 );
         ver_maj = swap_16( ver_maj );

         memcpy( &ver_min, &(sockbuf[13]), 2 );
         ver_min = swap_16( ver_min );

         printf( "%u barrier %u.%u found\n", pkt_sz, ver_maj, ver_min );

         /* Send an acknowledgement and our name. */
         send_syn_msg(
            sockfd, 0, "Barrier%2i%2i%4i%s", ver_maj, ver_min, 7, "mintest" );
         
         break;

      case 1363758662: /* QINF */
         
         send_syn_msg(
            sockfd, 0, "DINF%2i%2i%2i%2i%2i%2i%2i",
            0, 0, 640, 480, 0, 0, 0 );

         break;

      case 1128874315: /* CIAK */
         printf( "CIAK?\n" );
         break;

      case 1129467728: /* CROP */
         printf( "CROP?\n" );
         break;

      case 1128352854: /* "CALV" */
         printf( "Ping... Pong!\n" );
         send_syn_msg( sockfd, 4, "CALV%4iCNOP", 4 );
         break;

      case 1128877646: /* "CINN" */
         printf( "in!\n" );
         break;

      case 1145261136: /* DCLP */
         printf( "clip in!\n" );
         break;

      case 1145916758: /* DMMV */
         printf( "mmv\n" );
         break;

      case 1129272660: /* COUT */
         printf( "out!\n" );
         break;

      case 1161969988: /* "EBAD" */

      default:
         /* Not found? Then what *did* we get? */
         printf( "invalid packet:\n" );
         dump_pkt( sockbuf, recv_sz );

         /* goto cleanup; */
      }

   } while( 0 <= recv_sz );

cleanup:

   if( 0 < sockfd ) {
      printf( "closing socket...\n" );
#ifdef WIN32
      closesocket( sockfd );
#else
      close( sockfd );
#endif
   }

   return retval; 
}

