
#if !defined( MINPUT_OS_WIN32 ) && !defined( MINPUT_OS_WIN16 )
#include <stdlib.h>
#include <unistd.h> /* close */
#include <errno.h>
#endif /* !MINPUT_OS_WIN */

#include <string.h> /* memset */
#include <assert.h>
#include <stdio.h> /* fopen, fclose */

#include "synproto.h"
#include "osio.h"

#define SCREEN_NAME mintest
#define SERVER_IP "192.168.250.166"
#define SERVER_PORT 24800

extern FILE* g_dbg;

int main() {
   int sockfd = 0;
   struct sockaddr_in servaddr;
   int retval = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   ssize_t i = 0,
      recv_sz = 0;
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   WSADATA wsa_data;
#endif /* MINPUT_OS_WIN */
   size_t sockbuf_offset = 0;
   uint32_t calv_deadline = 0,
      time_now = 0;

   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );

   g_dbg = fopen( "dbg.txt", "w" );
   assert( NULL != g_dbg );

   osio_printf( __FILE__, __LINE__, "starting up...\n" );
#ifdef MINPUT_OS_WIN32
   retval = WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );
   if( 0 != retval ) {
      osio_printf( __FILE__, __LINE__, "error at WSAStartup()\n" );
      goto cleanup;
   }
#elif defined( MINPUT_OS_WIN16 )
   retval = WSAStartup( 1, &wsa_data );
   if( 0 != retval ) {
      osio_printf( __FILE__, __LINE__, "error at WSAStartup()\n" );
      goto cleanup;
   }
#endif /* MINPUT_OS_WIN */
   
   /* Resolve server address. */
   memset( &servaddr, 0, sizeof( struct sockaddr_in ) );
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons( SERVER_PORT );
   servaddr.sin_addr.s_addr = inet_addr( SERVER_IP );

   do {

      if( 0 == sockfd ) {
         /* Open socket. */
         osio_printf( __FILE__, __LINE__,
            "connecting to 0x%08x...\n", servaddr.sin_addr.s_addr );
         sockfd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
         if( -1 == sockfd ) {
            perror( "socket" );
            goto cleanup;
         }

         /* Connect socket. */
         retval = connect(
            sockfd, (struct sockaddr*)&servaddr, sizeof( struct sockaddr ) );
         if( retval ) {
            perror( "connect" );
            goto cleanup;
         } else {
            osio_printf( __FILE__, __LINE__, "connected\n" );
         }

         calv_deadline = osio_get_time() + SYNPROTO_TIMEOUT_MS;
      }

      /* Receive handshake opener. */
      recv_sz = recv(
         sockfd, &(sockbuf[sockbuf_offset]), SOCKBUF_SZ - sockbuf_offset, 0 );
      if( 0 >= recv_sz ) {
         /* Connection died. */
         continue;
      } else if( 4 == recv_sz ) {
         /* The size was split off the rest of the packet? Wait for the rest!
          */
         sockbuf_offset += recv_sz;
         continue;
      }

      retval = synproto_parse_and_reply(
         sockfd, sockbuf, recv_sz, &calv_deadline );

      memset( sockbuf, '\0', SOCKBUF_SZ + 1 );
      sockbuf_offset = 0;

      time_now = osio_get_time();
      if( time_now > calv_deadline ) {
         /* Too long since the last keepalive! Restart! */

         osio_printf( __FILE__, __LINE__,
            "timed out (%u past %u), restarting!\n",
            time_now, calv_deadline );

#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
         closesocket( sockfd );
#else
         close( sockfd );
#endif /* MINPUT_OS_WIN */
         sockfd = 0;
      }

   } while( 0 <= recv_sz );

cleanup:

   if( 0 < sockfd ) {
      osio_printf( __FILE__, __LINE__, "closing socket...\n" );
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
      closesocket( sockfd );
#else
      close( sockfd );
#endif /* MINPUT_OS_WIN */
   }

   if( NULL != g_dbg && stdout != g_dbg ) {
      fclose( g_dbg );
   }

   return retval; 
}

