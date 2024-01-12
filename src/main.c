
#define MINPUT_MAIN_C
#include "minput.h"

int minput_loop_iter(
   struct NETIO_CFG* config, char* pkt_buf, uint32_t* p_pkt_buf_sz
) {
   int retval = 0;
   uint32_t time_now = 0;

   /* Ensure we are connected; reconnect if not. */
   if( 0 == config->socket_fd ) {
      retval = netio_connect( config );
      if( 0 != retval ) {
         goto cleanup;
      }
   }

   /* Process packets and handle protocol/input events. */
   retval = minhop_process_packets( config, pkt_buf, p_pkt_buf_sz );
   if( MINHOP_ERR_RECV == retval ) {
      /* Not a show-stopper, but try to reconnect! */
      retval = 0;
      goto cleanup;
   } else if( MINHOP_ERR_PROTOCOL == retval ) {
      /* Protocol error also isn't a show-stopper. */
      retval = 0;
   }

   /* Check how we're doing for keepalives. */
   time_now = osio_get_time();
   if( time_now > config->calv_deadline ) {
      /* Too long since the last keepalive! Restart! */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "timed out (%u past %u), restarting!\n",
         time_now, config->calv_deadline );
      netio_disconnect( config, NETIO_DISCO );
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "successfully finished packet queue!\n" );

cleanup:
   return retval;
}

int minput_main( struct NETIO_CFG* config ) {
   int retval = 0;

   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );

   osio_logging_setup();

   /*
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
      "%s | %s | %d\n",
      config->client_name, config->server_addr, config->server_port );
   */

   if(
      '\0' == config->client_name[0] ||
      '\0' == config->server_addr[0] ||
      0 == config->server_port
   ) {
      /* Wait until inside minput_main to show usage error, as logging is now
       * setup!
       */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "usage: minhop <name> <server> [port]\n" );
      retval = MINHOP_ERR_ARGS;
      goto cleanup;
   }

   retval = osio_ui_setup();
   if( 0 != retval ) {
      goto cleanup;
   }

   /* Get to actual startup! */
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "starting up...\n" );
   
   retval = netio_setup( config );
   if( 0 != retval ) {
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "starting main loop...\n" );

   retval = osio_loop( config );

cleanup:

   if( 0 < config->socket_fd ) {
      netio_disconnect( config, NETIO_DISCO_FORCE );
   }

   netio_cleanup();

   osio_ui_cleanup();

   osio_logging_cleanup();

   return retval; 
}

