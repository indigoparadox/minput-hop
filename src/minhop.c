
#include "minhop.h"

#include <string.h> /* memset, strncpy */
#include <stdlib.h> /* atoi */
#include <stdio.h> /* perror */

int minhop_parse_args( int argc, char* argv[], struct MINHOP_CFG* config ) {
   /* Very simple arg parser. */

   /* Default port. */
   config->server_port = 24800;

   if( 3 <= argc ) {
      if( 4 <= argc ) {
         config->server_port = atoi( argv[2] );
         osio_printf( __FILE__, __LINE__, "server port: %d\n",
            config->server_port );
      }

      strncpy( config->server_addr, argv[2], SERVER_ADDR_SZ_MAX );
      osio_printf( __FILE__, __LINE__, "server address: %s\n",
         config->server_addr );

      strncpy( config->client_name, argv[1], CLIENT_NAME_SZ_MAX );
      osio_printf( __FILE__, __LINE__, "client name: %s\n",
         config->client_name );
      
      return 0;

   } else {
      osio_printf( __FILE__, __LINE__,
         "usage: minhop <name> <server> [port]\n" );
      return 1;
   }

}

int minhop_network_setup( struct MINHOP_CFG* config ) {
   int retval = 0;
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   WSADATA wsa_data;
#endif /* MINPUT_OS_WIN */

   osio_printf( __FILE__, __LINE__, "setting up network...\n" );

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
   
cleanup:

   return retval;
}

int minhop_network_connect( struct MINHOP_CFG* config ) {
   struct sockaddr_in servaddr;
   int retval = 0;

   /* Resolve server address. */
   memset( &servaddr, 0, sizeof( struct sockaddr_in ) );
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons( config->server_port );
   servaddr.sin_addr.s_addr = inet_addr( config->server_addr );

   /* Open socket. */
   osio_printf( __FILE__, __LINE__,
      "connecting to 0x%08x...\n", servaddr.sin_addr.s_addr );
   config->socket_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
   if( -1 == config->socket_fd ) {
      perror( "socket" );
      goto cleanup;
   }

   /* Connect socket. */
   retval = connect(
      config->socket_fd,
      (struct sockaddr*)&servaddr, sizeof( struct sockaddr ) );
   if( retval ) {
      perror( "connect" );
      goto cleanup;
   } else {
      osio_printf( __FILE__, __LINE__, "connected\n" );
   }

   config->calv_deadline = osio_get_time() + SYNPROTO_TIMEOUT_MS;

cleanup:

   return retval;
}

