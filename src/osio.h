
#ifndef OSIO_H
#define OSIO_H

#include <stdarg.h>

#define OSIO_MOUSE_LEFT 0x01
#define OSIO_MOUSE_RIGHT 0x03

#ifndef OSIO_PRINTF_BUFFER_SZ
#  define OSIO_PRINTF_BUFFER_SZ 2048
#endif /* !OSIO_PRINTF_BUFFER_SZ */

void osio_printf( const char* file, int line, const char* fmt, ... );

unsigned long osio_time();

void osio_screen_get_w_h( unsigned short* screen_w, unsigned short* screen_h );

void osio_mouse_move( int mouse_x, int mouse_y );

void osio_mouse_down( int mouse_x, int mouse_y, int mouse_btn );
 
void osio_mouse_up( int mouse_x, int mouse_y, int mouse_btn );

#endif /* !OSIO_H */

