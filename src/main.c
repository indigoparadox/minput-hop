
#define MINPUT_MAIN_C
#include "minput.h"

int minput_main( struct NETIO_CFG* config ) {
   int retval = 0;
   uint32_t time_now = 0;
   char pkt_buf[SOCKBUF_SZ + 1];
   uint32_t pkt_buf_sz = 0;

   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );

   osio_logging_setup();

   /* retval = minhop_gui_setup();
   if( 0 != retval ) {
      goto cleanup;
   } */

   /* Get to actual startup! */
   osio_printf( __FILE__, __LINE__, "starting up...\n" );
   
   retval = netio_setup( config );
   if( 0 != retval ) {
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, "starting main loop...\n" );

   do {

      /* Ensure we are connected; reconnect if not. */
      if( 0 == config->socket_fd ) {
         retval = netio_connect( config );
         if( 0 != retval ) {
            goto cleanup;
         }
      }

      /* Process packets and handle protocol/input events. */
      retval = minhop_process_packets( config, pkt_buf, &pkt_buf_sz );
      if( MINHOP_ERR_RECV == retval ) {
         /* Not a show-stopper, but try to reconnect! */
         retval = 0;
         continue;
      } else if( MINHOP_ERR_PROTOCOL == retval ) {
         /* Protocol error also isn't a show-stopper. */
         retval = 0;
      }

      /* Check how we're doing for keepalives. */
      time_now = osio_get_time();
      if( time_now > config->calv_deadline ) {
         /* Too long since the last keepalive! Restart! */
         osio_printf( __FILE__, __LINE__,
            "timed out (%u past %u), restarting!\n",
            time_now, config->calv_deadline );
         netio_disconnect( config );
      }

      osio_printf( __FILE__, __LINE__,
         "successfully finished packet queue!\n" );

   } while( 0 == retval );

cleanup:

   if( 0 < config->socket_fd ) {
      netio_disconnect( config );
   }

   /* osio_ui_cleanup(); */

   osio_logging_cleanup();

   return retval; 
}

