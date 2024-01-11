
#include "netio.h"

int netio_setup( struct NETIO_CFG* config ) {
   int retval = 0;
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   WSADATA wsa_data;
#endif /* MINPUT_OS_WIN */

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "setting up network...\n" );

#ifdef MINPUT_OS_WIN32
   retval = WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );
   if( 0 != retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "error at WSAStartup()\n" );
      goto cleanup;
   }
#elif defined( MINPUT_OS_WIN16 )
   retval = WSAStartup( 1, &wsa_data );
   if( 0 != retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "error at WSAStartup()\n" );
      goto cleanup;
   }
#endif /* MINPUT_OS_WIN */
   
#if defined( MINPUT_OS_WIN32 ) || defined( MINPUT_OS_WIN16 )
cleanup:
#endif /* MINPUT_OS_WIN32 */

   return retval;
}

int netio_connect( struct NETIO_CFG* config ) {
   struct sockaddr_in servaddr;
   int retval = 0;

   /* Resolve server address. */
   memset( &servaddr, 0, sizeof( struct sockaddr_in ) );
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons( config->server_port );
   servaddr.sin_addr.s_addr = inet_addr( config->server_addr );

   /* Open socket. */
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "connecting to 0x%08x...\n", servaddr.sin_addr.s_addr );
   config->socket_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
   if( -1 == config->socket_fd ) {
      /* perror( "socket" ); */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not open socket\n" );
      goto cleanup;
   }

   /* Connect socket. */
   retval = connect(
      config->socket_fd,
      (struct sockaddr*)&servaddr, sizeof( struct sockaddr ) );
   if( retval ) {
      /* perror( "connect" ); */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not connect socket\n" );
      goto cleanup;
   } else {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "connected\n" );
   }

   config->calv_deadline = osio_get_time() + SYNPROTO_TIMEOUT_MS;

cleanup:

   return retval;
}

int minhop_process_packets(
   struct NETIO_CFG* config, char* pkt_buf, uint32_t* p_pkt_buf_sz
) {
   uint32_t pkt_claim_sz;
   int32_t recv_sz = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   uint32_t j = 0;
   int retval = 0;

   /* Receive more data from the socket. */
   recv_sz = recv( config->socket_fd, sockbuf, SOCKBUF_SZ, 0 );
   if( 0 >= recv_sz ) {
      /* Connection died. Restart loop so we can try to reconnect. */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "connection died!\n" );
      retval = MINHOP_ERR_RECV;
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "recv sz: %lu\n", recv_sz );

   /* Dump the received data into the packet buffer. */
   if( *p_pkt_buf_sz + recv_sz >= SOCKBUF_SZ ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "packet too large! (%lu bytes)\n",
         *p_pkt_buf_sz + recv_sz );
      retval = MINHOP_ERR_PROTOCOL;
      goto cleanup;
   }

   /* Append the received data to the packet buffer. */
   memcpy( &(pkt_buf[*p_pkt_buf_sz]), sockbuf, recv_sz );
   *p_pkt_buf_sz += recv_sz;
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "copied packet(s) to pkt buffer; new pkt buffer sz: %lu\n",
      *p_pkt_buf_sz );

#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, "new pkt buffer: " );
   for( j = 0 ; recv_sz > j ; j++ ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "0x%02x ('%c') ", pkt_buf[j] );
   }
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "\n" );
#endif /* DEBUG_PACKETS_IN */

   /* Process packets in the packet buffer until we run out. */
   do {
      /* How big does the packet claim to be? */
      pkt_claim_sz = swap_32( *((uint32_t*)pkt_buf) ) + 4;
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "recv announced size: %lu (0x%08x)\n",
         pkt_claim_sz, pkt_claim_sz );

      if( *p_pkt_buf_sz < pkt_claim_sz ) {
         /* The packet is too small! Let's try again to fetch the rest! */
         break;
      }

      /* Parse this packet, taking its claim at face value. */
      retval = synproto_parse_and_reply( config, pkt_buf, pkt_claim_sz );

      /* Remove the packet size from the packet buffer size. */
      *p_pkt_buf_sz -= pkt_claim_sz;
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "removed packet; new pkt buffer sz: %lu\n",
         *p_pkt_buf_sz );

      /* Move the remaining contents of packet buffer down to the start. */
      for( j = 0 ; *p_pkt_buf_sz > j ; j++ ) {
         if( j + pkt_claim_sz >= SOCKBUF_SZ ) {
            osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
               "pkt offset too large!\n" );
            retval = MINHOP_ERR_OVERFLOW;
            goto cleanup;
         }
         pkt_buf[j] = pkt_buf[j + pkt_claim_sz];
      }

      /* Keep going while we have valid packets left to process. */
   } while( *p_pkt_buf_sz > 0 );

cleanup:

   return retval;
}

void netio_disconnect( struct NETIO_CFG* config ) {
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "closing socket...\n" );
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
   closesocket( config->socket_fd );
#else
   close( config->socket_fd );
#endif /* MINPUT_OS_WIN */
   config->socket_fd = 0;
}

