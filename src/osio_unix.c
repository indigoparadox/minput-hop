
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

unsigned long osio_time() {
   struct timeval tv;
   double tv_ms = 0;
   
   gettimeofday( &tv, NULL );

   return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void osio_screen_get_w_h( unsigned short* screen_w, unsigned short* screen_h ) {
   *screen_w = 640;
   *screen_h = 480;
}

void osio_mouse_move( int mouse_x, int mouse_y ) {
}

void osio_mouse_down( int mouse_x, int mouse_y, int mouse_btn ) {
}

void osio_mouse_up( int mouse_x, int mouse_y, int mouse_btn ) {
}

