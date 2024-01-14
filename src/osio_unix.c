
#include "osio.h"

#ifdef DEBUG
static FILE* g_dbg = NULL;
#endif /* DEBUG */

void osio_parse_args( int argc, char* argv[], struct NETIO_CFG* config ) {
   /* Very simple arg parser. */

   /* Default port. */
   config->server_port = 24800;

   if( 3 <= argc ) {
      if( 4 <= argc ) {
         config->server_port = atoi( argv[2] );
      }

      strncpy( config->server_addr, argv[2], SERVER_ADDR_SZ_MAX );

      strncpy( config->client_name, argv[1], CLIENT_NAME_SZ_MAX );
      
   }
}

int osio_ui_setup() {
   return 0;
}

void osio_ui_cleanup() {

}

int osio_loop( struct NETIO_CFG* config ) {
   int retval = 0;
   char pkt_buf[SOCKBUF_SZ + 1];
   uint32_t pkt_buf_sz = 0;

   /* Run each the loop iterator directly. */
   do {
      retval = minput_loop_iter( config );
   } while( 0 == retval );

   return retval;
}

void osio_printf( const char* file, int line, int status, const char* fmt, ... ) {
   va_list args;
   char buffer[OSIO_PRINTF_BUFFER_SZ + 1];
   char prefix[OSIO_PRINTF_PREFIX_SZ + 1];

   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );

   va_start( args, fmt );
   vsnprintf( buffer, OSIO_PRINTF_BUFFER_SZ, fmt, args );
   va_end( args );

   if( NULL != file ) {
      /* Only produce a prefix on a new line. */
      memset( prefix, '\0', OSIO_PRINTF_PREFIX_SZ + 1 );
      snprintf( prefix, OSIO_PRINTF_PREFIX_SZ,
         "(%d) %s: %d: ", status, file, line );

      printf( "\n%s: %s", prefix, buffer );
#ifdef DEBUG
      if( NULL != g_dbg ) {
         /* Print the prefix if available. */
         fprintf( g_dbg, "\n%s: %s", prefix, buffer );
         fflush( g_dbg );
      }
#endif /* DEBUG */
   } else {
      printf( "%s", buffer );
#ifdef DEBUG
      if( NULL != g_dbg ) {
         fprintf( g_dbg, "%s", buffer );
         fflush( g_dbg );
      }
#endif /* DEBUG */
   }
}

uint32_t osio_get_time() {
   struct timeval tv;
   
   gettimeofday( &tv, NULL );

   return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void osio_screen_get_w_h( uint16_t* screen_w_p, uint16_t* screen_h_p ) {
   /* TODO: Get actual screen coords? This is just for testing... */
   *screen_w_p = 640;
   *screen_h_p = 480;
}

void osio_mouse_move( uint16_t mouse_x, uint16_t mouse_y ) {
}

void osio_mouse_down( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn ) {
}

void osio_mouse_up( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn ) {
}

void osio_key_down( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "id: (%c) %d, mod: 0x%04x, btn: %d", key_id, key_id, key_mod, key_btn );
}

void osio_key_up( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "id: (%c) %d, mod: 0x%04x, btn: %d", key_id, key_id, key_mod, key_btn );
}

void osio_key_rpt( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
}

void osio_logging_setup() {
#ifdef DEBUG
   g_dbg = fopen( "dbg.txt", "w" );
   assert( NULL != g_dbg );
#endif /* DEBUG */
}

void osio_logging_cleanup() {
#ifdef DEBUG
   if( NULL != g_dbg && stdout != g_dbg ) {
      fclose( g_dbg );
   }
#endif /* DEBUG */
}

int main( int argc, char* argv[] ) {
   int retval = 0;
   struct NETIO_CFG config;

   memset( &config, '\0', sizeof( struct NETIO_CFG ) );
   osio_parse_args( argc, argv, &config );

   retval = minput_main( &config );

   return retval;
}

