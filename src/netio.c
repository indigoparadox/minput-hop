
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
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
         "could not connect socket" );
      netio_disconnect( config );
      retval = MINHOP_ERR_CONNECT;
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO, "connected!" );

cleanup:

   /* Set this whether we succeed or fail... we're not doing keepalives yet!
    */
   synproto_reset_calv_deadline( config );

   return retval;
}

int netio_fetch_packets( struct NETIO_CFG* config ) {
   int retval = 0;
   int32_t recv_sz = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   char* new_pkt_buf = NULL;
#ifdef DEBUG_PACKETS_IN_DUMP
   uint32_t j = 0;
#endif /* DEBUG_PACKETS_IN_DUMP */

   /* Receive more data from the socket. */
   recv_sz = recv( config->socket_fd, sockbuf, SOCKBUF_SZ, 0 );
   if( 0 >= recv_sz ) {
      /* Connection died. Restart loop so we can try to reconnect. */
      retval = MINHOP_ERR_RECV;
      goto cleanup;
   }

#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "recv sz: %lu", recv_sz );
#endif /* DEBUG_PACKETS_IN */

   /* Make sure the packet buffer has enough space for received data. */
   if( config->pkt_buf_sz + recv_sz >= SOCKBUF_SZ ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "packet won't fit in buffer! (%lu bytes) reallocating...",
         config->pkt_buf_sz + recv_sz );

      new_pkt_buf = realloc(
         config->pkt_buf, config->pkt_buf_sz + recv_sz + 100 );
      if( NULL == new_pkt_buf ) {
         osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
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

#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "copied packet(s) to pkt buffer; new pkt buffer sz: %lu",
      config->pkt_buf_sz );
#endif /* DEBUG_PACKETS_IN */

#ifdef DEBUG_PACKETS_IN_DUMP
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "new pkt buffer: " );
   for( j = 0 ; recv_sz > j ; j++ ) {
      osio_printf( NULL, __LINE__, MINPUT_STAT_DEBUG,
         "0x%02x ('%c') ", config->pkt_buf[j] );
   }
#endif /* DEBUG_PACKETS_IN_DUMP */

cleanup:
   return retval;
}

int netio_process_packets( struct NETIO_CFG* config ) {
   uint32_t pkt_claim_sz;
   uint32_t j = 0;
   int retval = 0;

   if( 0 == config->pkt_buf_sz ) {
      /* Nothing to do! */
      goto cleanup;
   }

   /* How big does the packet claim to be? */
   pkt_claim_sz = swap_32( *((uint32_t*)(config->pkt_buf)) ) + 4;

#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "recv announced size: %lu (0x%08lx)",
      pkt_claim_sz, pkt_claim_sz );
#endif /* DEBUG_PACKETS_IN */

   if( config->pkt_buf_sz < pkt_claim_sz ) {
      /* The packet is too small! Let's wait until we've fetched the rest! */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "packet (%lu bytes) too small for buffer (%lu bytes)! waiting...",
         pkt_claim_sz, config->pkt_buf_sz );
      goto cleanup;
   }

   /* Parse this packet, taking its claim at face value. */
   retval = synproto_parse_and_reply(
      config, config->pkt_buf, pkt_claim_sz );

   /* Remove the packet size from the packet buffer size. */
   config->pkt_buf_sz -= pkt_claim_sz;
#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "removed packet; new pkt buffer sz: %lu",
      config->pkt_buf_sz );
#endif /* DEBUG_PACKETS_IN */

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

   /* Reset packet buffer. */
   config->pkt_buf_sz = 0;

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

