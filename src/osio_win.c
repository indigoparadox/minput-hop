
#include <windows.h>
#include <mmsystem.h>

#include "osio.h"

#include <stdio.h> /* vprintf, FILE */
#include <string.h> /* memset */

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
   return timeGetTime();
}

void osio_screen_get_w_h( uint16_t* screen_w_p, uint16_t* screen_h_p ) {
   *screen_w_p = GetSystemMetrics( SM_CXSCREEN );
   *screen_h_p = GetSystemMetrics( SM_CYSCREEN );
}

void osio_mouse_move( uint16_t mouse_x, uint16_t mouse_y ) {
   POINT new_pos;
   HWND cur_hwnd;
   UINT code = 0;

   SetCursorPos( mouse_x, mouse_y );
}

void osio_mouse_down( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn ) {
#ifdef MINPUT_OS_WIN32
   mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN,
      mouse_x, mouse_y, 0, 0 );
#endif /* MINPUT_OS_WIN32 */
}

void osio_mouse_up( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn ) {
#ifdef MINPUT_OS_WIN32
   mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP,
      mouse_x, mouse_y, 0, 0 );
#endif /* MINPUT_OS_WIN32 */
}

void osio_key_down( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
#ifdef MINPUT_OS_WIN32
   /* TODO: Raw key_id is not... quite... right... */

   if( key_id >= 97 && key_id <= 122 ) {
      /* Translate uppercase to lowercase. */
      key_id -= 32;
   }

   keybd_event( key_id, 0, 0, 0 );
#endif /* MINPUT_OS_WIN32 */
}

void osio_key_up( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
}

void osio_key_rpt( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
}

