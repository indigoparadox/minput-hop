
#define MINPUT_MAIN_C
#include "minput.h"

int minput_loop_iter( struct NETIO_CFG* config ) {
   int retval = 0;
   uint32_t time_now = 0;
   int pkt_proc_iter = 0;

#ifdef DEBUG_FLOW
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "beginning iteration..." );
#endif /* DEBUG_FLOW */

   /* Ensure we are connected; reconnect if not. */
   if( 0 == config->socket_fd ) {
      config->pkt_buf_sz = 0; /* Invalidate packet buffer. */
      retval = netio_connect( config );
      if( 0 != retval ) {
         retval = 0;
         goto cleanup;
      }
   }

#ifdef DEBUG_FLOW
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "fetching data from server..." );
#endif /* DEBUG_FLOW */

   /* Fetch packets to process below. */
   retval = netio_fetch_packets( config );
   if( MINHOP_ERR_RECV == retval ) {
      /* Not a show-stopper, but try to reconnect! */
      retval = 0;
#ifdef DEBUG_FLOW
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "no data received!" );
#endif /* DEBUG_FLOW */
   } else if( MINHOP_ERR_ALLOC == retval ) {
      /* Allocation error is a show-stopper! */
      goto cleanup;
   }

#ifdef DEBUG_FLOW
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "processing packets..." );
#endif /* DEBUG_FLOW */
   
   do {
#ifdef DEBUG_FLOW
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "processing packet %d...", pkt_proc_iter );
#endif /* DEBUG_FLOW */

      /* Process up to PKT_PROC_PER_LOOP packets and handle protocol/input
       * events. We'll catch packets in excess of PKT_PROC_PER_LOOP on the
       * next go-around (hopefully we won't fetch too many more), but we
       * gotta keep things moving or the UI might freeze!
       */
      retval = netio_process_packets( config );
      if( MINHOP_ERR_PROTOCOL == retval ) {
         /* Protocol error isn't a show-stopper. */
         osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, /* No dialog! */
            "protocol error!" );
         retval = 0;

      } else if( MINHOP_ERR_OVERFLOW == retval ) {
         /* Overflow is a show-stopped! (But probably shouldn't happen?) */
         goto cleanup;
      }

      pkt_proc_iter++;
   } while( 0 < config->pkt_buf_sz && pkt_proc_iter < PKT_PROC_PER_LOOP );

#ifdef DEBUG_FLOW
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "checking keepalives..." );
#endif /* DEBUG_FLOW */

   /* Check how we're doing for keepalives. */
   time_now = osio_get_time();
   if( time_now > config->calv_deadline ) {
      /* Too long since the last keepalive! Restart! */
      /* Note: This is not STAT_ERROR because that might produce too many
       * dialogs, overwhelming Win16! */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
         "timed out (%lu past %lu), restarting!",
         time_now, config->calv_deadline );
      if( 0 != config->socket_fd ) {
         netio_disconnect( config );
      }
   }

#ifdef DEBUG_FLOW
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "(%lu) successfully finished iteration!", time_now );
#endif /* DEBUG_FLOW */

cleanup:
   return retval;
}

int minput_main( struct NETIO_CFG* config ) {
   int retval = 0;

#ifdef DEBUG
   assert( 4 == sizeof( uint32_t ) );
   assert( 2 == sizeof( uint16_t ) );
   assert( 1 == sizeof( uint8_t ) );
#endif /* DEBUG */

   osio_logging_setup();

   /*
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
      "%s | %s | %d",
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
         "usage: minhop <name> <server> [port]" );
      retval = MINHOP_ERR_ARGS;
      goto cleanup;
   }

   config->pkt_buf_sz_max = SOCKBUF_SZ;
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "setting up packet buffer with initial size of %lu...",
      config->pkt_buf_sz_max );
   config->pkt_buf_sz = 0;
   config->pkt_buf = calloc( config->pkt_buf_sz_max, 1 );
   if( NULL == config->pkt_buf ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not allocate packet buffer!" );
      retval = MINHOP_ERR_ALLOC;
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "starting up UI..." );

   retval = osio_ui_setup();
   if( 0 != retval ) {
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "starting up network..." );
   
   retval = netio_setup( config );
   if( 0 != retval ) {
      goto cleanup;
   }

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "starting main loop..." );

   retval = osio_loop( config );

cleanup:

   if( 0 < config->socket_fd ) {
      netio_disconnect( config );
   }

   netio_cleanup();

   osio_ui_cleanup();

   if( NULL != config->pkt_buf ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "freeing packet buffer..." );
      free( config->pkt_buf );
   }

   osio_printf( NULL, __LINE__, MINPUT_STAT_DEBUG, "\n" );

   osio_logging_cleanup();

   return retval; 
}

