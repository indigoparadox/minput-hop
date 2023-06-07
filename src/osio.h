
#ifndef OSIO_H
#define OSIO_H

#define OSIO_MOUSE_LEFT 0x01
#define OSIO_MOUSE_RIGHT 0x03

void osio_mouse_move( int mouse_x, int mouse_y );

void osio_mouse_down( int mouse_x, int mouse_y, int mouse_btn );
 
void osio_mouse_up( int mouse_x, int mouse_y, int mouse_btn );

#endif /* !OSIO_H */

