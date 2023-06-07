
#ifndef MINPUT_OS_WIN
#include <stdlib.h>
#include <unistd.h> /* close */
#include <errno.h>
#include <string.h>
#endif /* !MINPUT_OS_WIN */

#include <assert.h>
#include <stdio.h>

#include "synproto.h"

#define SCREEN_NAME mintest
#define SERVER_IP "192.168.250.166"
#define SERVER_PORT 24800

FILE* g_dbg = NULL;


int main() {
   int sockfd = 0;
   struct sockaddr_in servaddr;
   int retval = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   ssize_t i = 0,
      recv_sz = 0;
#ifdef MINPUT_OS_WIN
   WSADATA wsa_data;
#endif /* MINPUT_OS_WIN */

   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );

   /*g_dbg = fopen( "dbg.txt", "w" );*/
   g_dbg = stdout;
   assert( NULL != g_dbg );

#ifdef MINPUT_OS_WIN
   fprintf( g_dbg, "starting up...\n" );
   retval = WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );
   if( NO_ERROR != retval ) {
      fprintf( g_dbg, "error at WSAStartup()\n" );
      goto cleanup;
   }
#endif /* MINPUT_OS_WIN */
   
   /* Resolve server address. */
   memset( &servaddr, 0, sizeof( struct sockaddr_in ) );
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons( SERVER_PORT );
   servaddr.sin_addr.s_addr = inet_addr( SERVER_IP );

   /* Open socket and connect. */
   fprintf( g_dbg, "connecting to 0x%08x...\n", servaddr.sin_addr.s_addr );
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
      fprintf( g_dbg, "connected\n" );
   }

   do {

      /* Receive handshake opener. */
      memset( sockbuf, '\0', SOCKBUF_SZ + 1 );
      recv_sz = recv( sockfd, sockbuf, SOCKBUF_SZ, 0 );
      if( 0 >= recv_sz ) {
         continue;
      }

      synproto_parse( sockfd, sockbuf, recv_sz );

   } while( 0 <= recv_sz );

cleanup:

   if( 0 < sockfd ) {
      fprintf( g_dbg, "closing socket...\n" );
#ifdef MINPUT_OS_WIN
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

