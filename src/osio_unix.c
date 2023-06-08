
#include "osio.h"

#include <stdio.h> /* vprintf, FILE */
#include <string.h> /* memset */
#include <sys/time.h>

FILE* g_dbg = NULL;

void osio_printf( const char* file, int line, const char* fmt, ... ) {
   va_list args;
   char buffer[OSIO_PRINTF_BUFFER_SZ + 1];

   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );

   va_start( args, fmt );
   vsnprintf( buffer, OSIO_PRINTF_BUFFER_SZ, fmt, args );
   va_end( args );

   printf( "%s", buffer );

   fprintf( g_dbg, "%s", buffer );
}

uint32_t osio_get_time() {
   struct timeval tv;
   double tv_ms = 0;
   
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
   osio_printf( __FILE__, __LINE__, 
      "id: (%c) %d, mod: 0x%04x, btn: %d\n", key_id, key_id, key_mod, key_btn );
}

void osio_key_up( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
   osio_printf( __FILE__, __LINE__, 
      "id: (%c) %d, mod: 0x%04x, btn: %d\n", key_id, key_id, key_mod, key_btn );
}

void osio_key_rpt( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
}

