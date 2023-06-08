
#ifndef OSIO_H
#define OSIO_H

/**
 * \file osio.h
 * \brief OS-specific routines for controlling the current platform.
 */

#include "intplat.h"

#include <stdarg.h>

#define OSIO_MOUSE_LEFT 0x01
#define OSIO_MOUSE_RIGHT 0x03

#ifndef OSIO_PRINTF_BUFFER_SZ
#  define OSIO_PRINTF_BUFFER_SZ 2048
#endif /* !OSIO_PRINTF_BUFFER_SZ */

void osio_printf( const char* file, int line, const char* fmt, ... );

/**
 * \brief Get the current system time in milliseconds.
 * \return The current system time, or a monotonically-increasing relative
 *         time, at least.
 */
uint32_t osio_get_time();

/**
 * \brief Get the system screen width/height in pixels.
 */
void osio_screen_get_w_h( uint16_t* screen_w_p, uint16_t* screen_h_p );

void osio_mouse_move( uint16_t mouse_x, uint16_t mouse_y );

void osio_mouse_down( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn );
 
void osio_mouse_up( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn );

void osio_key_down( uint16_t key_id, uint16_t key_mod, uint16_t key_btn );

void osio_key_up( uint16_t key_id, uint16_t key_mod, uint16_t key_btn );

void osio_key_rpt( uint16_t key_id, uint16_t key_mod, uint16_t key_btn );

#endif /* !OSIO_H */

