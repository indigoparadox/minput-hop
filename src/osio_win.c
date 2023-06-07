
#include <windows.h>

#include "osio.h"

void osio_mouse_move( int mouse_x, int mouse_y ) {
   SetCursorPos( mouse_x, mouse_y ); /*
   mouse_event(
      MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE,
      mouse_x,
      mouse_y,
      0, 0 ); */

}

void osio_mouse_down( int mouse_x, int mouse_y, int mouse_btn ) {
   mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN,
      mouse_x, mouse_y, 0, 0 );
}

void osio_mouse_up( int mouse_x, int mouse_y, int mouse_btn ) {
   mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP,
      mouse_x, mouse_y, 0, 0 );
}

