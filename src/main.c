
#define MINPUT_MAIN_C
#include "minput.h"

int minput_loop_iter(
   struct NETIO_CFG* config, char* pkt_buf, uint32_t* p_pkt_buf_sz
) {
   int retval = 0;
   uint32_t time_now = 0;

#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "beginning iteration...\n" );
#endif /* DEBUG */

   /* Ensure we are connected; reconnect if not. */
   if( 0 == config->socket_fd ) {
      retval = netio_connect( config );
      if( 0 != retval ) {
         goto cleanup;
      }
   }

#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "processing packets...\n" );
#endif /* DEBUG */

   /* Process packets and handle protocol/input events. */
   retval = minhop_process_packets( config, pkt_buf, p_pkt_buf_sz );
   if( MINHOP_ERR_RECV == retval ) {
      /* Not a show-stopper, but try to reconnect! */
      retval = 0;
#ifdef DEBUG
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "no data received!\n" );
#endif /* DEBUG */
   
   } else if( MINHOP_ERR_PROTOCOL == retval ) {
      /* Protocol error also isn't a show-stopper. */
#ifdef DEBUG
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "protocol error!\n" );
#endif /* DEBUG */
      retval = 0;

   } else if( MINHOP_ERR_OVERFLOW == retval ) {
      /* Dump the packet buffer and start from scratch! */
      *p_pkt_buf_sz = 0;
      netio_disconnect( config );
      retval = 0;
      goto cleanup;
   }

#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "checking keepalives...\n" );
#endif /* DEBUG */

   /* Check how we're doing for keepalives. */
   time_now = osio_get_time();
   if( time_now > config->calv_deadline ) {
      /* Too long since the last keepalive! Restart! */
      /* Note: This is not STAT_ERROR because that might produce too many
       * dialogs, overwhelming Win16! */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
         "timed out (%u past %u), restarting!\n",
         time_now, config->calv_deadline );
      if( 0 != config->socket_fd ) {
         netio_disconnect( config );
      }
   }

#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "(%lu) successfully finished iteration!\n", time_now );
#endif /* DEBUG */

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
      netio_disconnect( config );
   }

   netio_cleanup();

   osio_ui_cleanup();

   osio_logging_cleanup();

   return retval; 
}

