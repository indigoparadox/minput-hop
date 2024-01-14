
#include "netio.h"

#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
int g_wsastart_success = 0;
#endif /* MINPUT_OS_WIN */

int netio_setup( struct NETIO_CFG* config ) {
   int retval = 0;
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   WSADATA wsa_data;
#endif /* MINPUT_OS_WIN */

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
      "setting up network..." );

#ifdef MINPUT_OS_WIN32
   retval = WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );
   if( 0 != retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "error at WSAStartup()" );
      goto cleanup;
   }
#elif defined( MINPUT_OS_WIN16 )
   retval = WSAStartup( MAKEWORD( 1, 1 ), &wsa_data );
   if( 0 != retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "error at WSAStartup()" );
      goto cleanup;
   }
#endif /* MINPUT_OS_WIN */

#if defined( MINPUT_OS_WIN32 ) || defined( MINPUT_OS_WIN16 )
   /* If we got this far, WSAStartup worked! */
   g_wsastart_success = 0;
   
cleanup:
#endif /* MINPUT_OS_WIN32 */

   return retval;
}

int netio_connect( struct NETIO_CFG* config ) {
   struct sockaddr_in servaddr;
   int retval = 0;
#if defined( MINPUT_OS_WIN32 ) || defined( MINPUT_OS_WIN16 )
   uint32_t timeout = 100; /* 100 ms = 0.1sec */
#else
   struct timeval timeout;

   memset( &timeout, '\0', sizeof( struct timeval ) );
   timeout.tv_sec = 1;
#endif /* MINPUT_OS_WIN32 || MINPUT_OS_WIN16 */

   /* Resolve server address. */
   memset( &servaddr, 0, sizeof( struct sockaddr_in ) );
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons( config->server_port );
   servaddr.sin_addr.s_addr = inet_addr( config->server_addr );

   if( 0 == config->socket_fd ) {
      /* Open socket. */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "connecting to 0x%08lx...", servaddr.sin_addr.s_addr );
      config->socket_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
      if( -1 == config->socket_fd ) {
         /* perror( "socket" ); */
         retval = MINHOP_ERR_CONNECT;
         /* Note: This is not STAT_ERROR because that might produce too many
          * dialogs, overwhelming Win16! */
         osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
            "could not open socket" );
         config->socket_fd = 0;
         goto cleanup;
      }
   }

   setsockopt( config->socket_fd, (int)SOL_SOCKET, SO_RCVTIMEO,
      (const char*)&timeout, sizeof( timeout ) );

   /* Connect socket. */
   retval = connect(
      config->socket_fd,
      (struct sockaddr*)&servaddr, sizeof( struct sockaddr ) );
   if( retval ) {
      /* perror( "connect" ); */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not connect socket" );
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO, "connected!" );

   config->calv_deadline = osio_get_time() + SYNPROTO_TIMEOUT_MS;

cleanup:

   return retval;
}

int netio_process_packets( struct NETIO_CFG* config ) {
   uint32_t pkt_claim_sz;
   int32_t recv_sz = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   uint32_t j = 0;
   int retval = 0;
   char* new_pkt_buf = NULL;

   /* Receive more data from the socket. */
   recv_sz = recv( config->socket_fd, sockbuf, SOCKBUF_SZ, 0 );
   if( 0 >= recv_sz ) {
      /* Connection died. Restart loop so we can try to reconnect. */
      /*osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "connection died!" );*/
      retval = MINHOP_ERR_RECV;
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "recv sz: %lu", recv_sz );

   /* Dump the received data into the packet buffer. */
   if( config->pkt_buf_sz + recv_sz >= SOCKBUF_SZ ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "packet won't fit in buffer! (%lu bytes) reallocating...",
         config->pkt_buf_sz + recv_sz );

      new_pkt_buf = realloc(
         config->pkt_buf, config->pkt_buf_sz + recv_sz + 100 );
      if( NULL == new_pkt_buf ) {
         osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
            "could not reallocate packet buffer! (%lu bytes)",
            config->pkt_buf_sz + recv_sz + 100 );
         retval = MINHOP_ERR_ALLOC;
         goto cleanup;
      }
      config->pkt_buf = new_pkt_buf;
      config->pkt_buf_sz_max = config->pkt_buf_sz + recv_sz + 100;
   }

   /* Append the received data to the packet buffer. */
   memcpy( &(config->pkt_buf[config->pkt_buf_sz]), sockbuf, recv_sz );
   config->pkt_buf_sz += recv_sz;
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "copied packet(s) to pkt buffer; new pkt buffer sz: %lu",
      config->pkt_buf_sz );

#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, "new pkt buffer: " );
   for( j = 0 ; recv_sz > j ; j++ ) {
      osio_printf( NULL, __LINE__, MINPUT_STAT_DEBUG,
         "0x%02x ('%c') ", pkt_buf[j] );
   }
#endif /* DEBUG_PACKETS_IN */

   /* Process packets in the packet buffer until we run out. */
   do {
      /* How big does the packet claim to be? */
      pkt_claim_sz = swap_32( *((uint32_t*)(config->pkt_buf)) ) + 4;
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "recv announced size: %lu (0x%08lx)",
         pkt_claim_sz, pkt_claim_sz );

      if( config->pkt_buf_sz < pkt_claim_sz ) {
         /* The packet is too small! Let's try again to fetch the rest! */
         break;
      }

      /* Parse this packet, taking its claim at face value. */
      retval = synproto_parse_and_reply(
         config, config->pkt_buf, pkt_claim_sz );

      /* Remove the packet size from the packet buffer size. */
      config->pkt_buf_sz -= pkt_claim_sz;
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "removed packet; new pkt buffer sz: %lu",
         config->pkt_buf_sz );

      /* Move the remaining contents of packet buffer down to the start. */
      for( j = 0 ; config->pkt_buf_sz > j ; j++ ) {
         if( j + pkt_claim_sz >= config->pkt_buf_sz_max ) {
            osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
               "pkt offset too large!" );
            retval = MINHOP_ERR_OVERFLOW;
            goto cleanup;
         }
         config->pkt_buf[j] = config->pkt_buf[j + pkt_claim_sz];
      }

      /* Keep going while we have valid packets left to process. */
   } while( config->pkt_buf_sz > 0 );

cleanup:

   return retval;
}

void netio_disconnect( struct NETIO_CFG* config ) {
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
      "closing socket..." );
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   closesocket( config->socket_fd );
#else
   close( config->socket_fd );
#endif /* MINPUT_OS_WIN */
   config->socket_fd = 0;

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
      "disconnected!" );
}

void netio_cleanup() {
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   if( g_wsastart_success ) {
      WSACleanup();
   }
#endif /* MINPUT_OS_WIN */
}

