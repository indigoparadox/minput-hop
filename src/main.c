
#if defined( MINPUT_OS_WIN32 ) || defined( MINPUT_OS_WIN16 )
#  include <windows.h>
#else
#  include <stdlib.h>
#  include <unistd.h>
#endif /* !MINPUT_OS_WIN */

#include <string.h> /* memset, strncpy */
#include <assert.h>
#include <stdio.h> /* fopen, fclose */

#include "minhop.h"

#if defined( MINPUT_OS_WIN32 )
#include "guiwin32.h"
#endif /* MINPUT_OS_WIN32 */

extern FILE* g_dbg;

int main( int argc, char* argv[] ) {
   int retval = 0;
   char sockbuf[SOCKBUF_SZ + 1];
   char pkt_buf[SOCKBUF_SZ + 1];
   uint32_t pkt_buf_sz = 0,
      pkt_claim_sz = 0;
   ssize_t i = 0,
      j = 0,
      recv_sz = 0;
   uint32_t time_now = 0;
   struct MINHOP_CFG config;

   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );

   g_dbg = fopen( "dbg.txt", "w" );
   assert( NULL != g_dbg );

   memset( &config, '\0', sizeof( struct MINHOP_CFG ) );
   retval = minhop_parse_args( argc, argv, &config );
   if( 0 != retval ) {
      goto cleanup;
   }

   /* retval = minhop_gui_setup();
   if( 0 != retval ) {
      goto cleanup;
   } */

   /* Get to actual startup! */
   osio_printf( __FILE__, __LINE__, "starting up...\n" );
   
   retval = minhop_network_setup( &config );
   if( 0 != retval ) {
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, "starting main loop...\n" );

   do {

      if( 0 == config.socket_fd ) {
         retval = minhop_network_connect( &config );
         if( 0 != retval ) {
            goto cleanup;
         }
      }

      /* Receive more data from the socket. */
      recv_sz = recv( config.socket_fd, sockbuf, SOCKBUF_SZ, 0 );
      if( 0 >= recv_sz ) {
         /* Connection died. Restart loop so we can try to reconnect. */
         continue;
      }

      osio_printf( __FILE__, __LINE__, "recv sz: %lu\n",
         recv_sz );

      /* Dump the received data into the packet buffer. */
      if( pkt_buf_sz + recv_sz >= SOCKBUF_SZ ) {
         osio_printf( __FILE__, __LINE__, "packet too large! (%lu bytes)\n",
            pkt_buf_sz + recv_sz );
         retval = 1;
         goto cleanup;
      }

      /* Append the received data to the packet buffer. */
      memcpy( &(pkt_buf[pkt_buf_sz]), sockbuf, recv_sz );
      pkt_buf_sz += recv_sz;
      osio_printf( __FILE__, __LINE__,
         "copied packet(s) to pkt buffer; new pkt buffer sz: %lu\n",
         pkt_buf_sz );

#ifdef DEBUG_PACKETS_IN
      osio_printf( __FILE__, __LINE__, "new pkt buffer: " );
      for( j = 0 ; recv_sz > j ; j++ ) {
         osio_printf( __FILE__, __LINE__, "0x%02x ('%c') ", pkt_buf[j] );
      }
      osio_printf( __FILE__, __LINE__, "\n" );
#endif /* DEBUG_PACKETS_IN */

      /* Process packets in the packet buffer until we run out. */
      do {
         /* How big does the packet claim to be? */
         pkt_claim_sz = swap_32( *((uint32_t*)pkt_buf) ) + 4;
         osio_printf( __FILE__, __LINE__,
            "recv announced size: %lu (0x%08x)\n",
            pkt_claim_sz, pkt_claim_sz );

         if( pkt_buf_sz < pkt_claim_sz ) {
            /* The packet is too small! Let's try again to fetch the rest! */
            break;
         }

         /* Parse this packet, taking its claim at face value. */
         retval = synproto_parse_and_reply( &config, pkt_buf, pkt_claim_sz );

         /* Remove the packet size from the packet buffer size. */
         pkt_buf_sz -= pkt_claim_sz;
         osio_printf( __FILE__, __LINE__,
            "removed packet; new pkt buffer sz: %lu\n",
            pkt_buf_sz );

         /* Move the remaining contents of packet buffer down to the start. */
         for( j = 0 ; pkt_buf_sz > j ; j++ ) {
            if( j + pkt_claim_sz >= SOCKBUF_SZ ) {
               osio_printf( __FILE__, __LINE__, "pkt offset too large!\n" );
               goto cleanup;
            }
            pkt_buf[j] = pkt_buf[j + pkt_claim_sz];
         }

         /* Keep going while we have valid packets left to process. */
      } while( pkt_buf_sz > 0 );

      /* Check how we're doing for keepalives. */
      time_now = osio_get_time();
      if( time_now > config.calv_deadline ) {
         /* Too long since the last keepalive! Restart! */

         osio_printf( __FILE__, __LINE__,
            "timed out (%u past %u), restarting!\n",
            time_now, config.calv_deadline );

#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
         closesocket( config.socket_fd );
#else
         close( config.socket_fd );
#endif /* MINPUT_OS_WIN */
         config.socket_fd = 0;
      }

   } while( 0 <= recv_sz );

cleanup:

   if( 0 < config.socket_fd ) {
      osio_printf( __FILE__, __LINE__, "closing socket...\n" );
#if defined( MINPUT_OS_WIN16 ) || defined( MINPUT_OS_WIN32 )
      closesocket( config.socket_fd );
#else
      close( config.socket_fd );
#endif /* MINPUT_OS_WIN */
   }

   if( NULL != g_dbg && stdout != g_dbg ) {
      fclose( g_dbg );
   }

   /* minhop_gui_cleanup(); */

   return retval; 
}

