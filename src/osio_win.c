
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

unsigned long osio_time() {
   return timeGetTime();
}

void osio_screen_get_w_h( unsigned short* screen_w, unsigned short* screen_h ) {
   *screen_w = GetSystemMetrics( SM_CXSCREEN );
   *screen_h = GetSystemMetrics( SM_CYSCREEN );
}

void osio_mouse_move( int mouse_x, int mouse_y ) {
   POINT new_pos;
   HWND cur_hwnd;
   UINT code = 0;

   SetCursorPos( mouse_x, mouse_y );
   
   /*
   GetCursorPos( &new_pos );
   cur_hwnd = WindowFromPoint( new_pos );

   code = SendMessage( cur_hwnd, WM_NCHITTEST, 
      0, MAKELPARAM( new_pos.x, new_pos.y ) );
   
   SendMessage( cur_hwnd, WM_SETCURSOR,
      (WPARAM)cur_hwnd, MAKELPARAM( code, WM_MOUSEMOVE ) );
   */

   /* mouse_event(
      MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE,
      mouse_x,
      mouse_y,
      0, 0 ); */

}

void osio_mouse_down( int mouse_x, int mouse_y, int mouse_btn ) {
   /* mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN,
      mouse_x, mouse_y, 0, 0 ); */
}

void osio_mouse_up( int mouse_x, int mouse_y, int mouse_btn ) {
   /* mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP,
      mouse_x, mouse_y, 0, 0 ); */
}

