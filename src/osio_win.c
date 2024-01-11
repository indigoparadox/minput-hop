
#include "osio.h"

#include "minhopr.h"

UINT WM_TASKBARCREATED = 0;

HWND g_window = NULL;
HINSTANCE g_instance = NULL;
int g_cmd_show = 0;
char g_pkt_buf[SOCKBUF_SZ + 1];
uint32_t g_pkt_buf_sz = 0;
struct NETIO_CFG g_config;

#ifdef MINPUT_OS_WIN32
NOTIFYICONDATA g_notify_icon_data;
#endif /* MINPUT_OS_WIN32 */

LRESULT CALLBACK WndProc(
   HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam
) {

   switch( message ) {
   case WM_TIMER:
      minput_loop_iter( &g_config, g_pkt_buf, &g_pkt_buf_sz );
      /* TODO: Exit on bad retval? */
      break;
   }

   return DefWindowProc( hWnd, message, wParam, lParam );
}

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
   int retval = 0;
   WNDCLASS wc = { 0 };

   WM_TASKBARCREATED = RegisterWindowMessage( "TaskbarCreated" );

   /* Create window class. */
   wc.lpfnWndProc = (WNDPROC)&WndProc;
   wc.hInstance = g_instance;
   wc.hIcon = LoadIcon( g_instance, MAKEINTRESOURCE( ID_MINHOP_ICO ) );
   wc.hCursor = LoadCursor( 0, IDC_ARROW );
   wc.hbrBackground = (HBRUSH)( COLOR_BTNFACE + 1 );
   wc.lpszClassName = "MinhopWindowClass";

   if( !RegisterClass( &wc ) ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not register window class!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

   g_window = CreateWindowEx(
      0,
      "MinhopWindowClass",
      "Minput Hop",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      300,
      200,
      NULL,
      NULL,
      g_instance,
      NULL
   );

   if( !g_window ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not create window!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

   if( !SetTimer( g_window, ID_TIMER_LOOP, 10, NULL ) ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not setup timer!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

#ifdef MINPUT_OS_WIN32

   /* Setup and display tray notification icon. */

   memset( &g_notify_icon_data, '\0', sizeof( NOTIFYICONDATA ) );

   g_notify_icon_data.cbSize = sizeof( NOTIFYICONDATA );
   g_notify_icon_data.hWnd = g_window;
   g_notify_icon_data.uID = ID_NOTIFY_ICON;
   g_notify_icon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
   g_notify_icon_data.hIcon = LoadIcon(
      g_instance, MAKEINTRESOURCE( ID_MINHOP_ICO ) );
   strcpy( g_notify_icon_data.szTip, "Minput Hop" );

   Shell_NotifyIcon( NIM_ADD, &g_notify_icon_data );

#endif /* MINPUT_OS_WIN32 */

   ShowWindow( g_window, g_cmd_show );

cleanup:

   return retval;
}

void osio_ui_cleanup() {
#ifdef MINPUT_OS_WIN32
   Shell_NotifyIcon( NIM_DELETE, &g_notify_icon_data );
#endif /* MINPUT_OS_WIN32 */
}

int osio_loop( struct NETIO_CFG* config ) {
   int retval = 0;
   MSG msg;

   do {
      retval = GetMessage( &msg, NULL, 0, 0 );
      TranslateMessage( &msg );
      DispatchMessage( &msg );
   } while( 0 < retval );

   if( 0 < retval ) {
      retval = 0;
   }

   return retval;
}

void osio_printf(
   const char* file, int line, int status, const char* fmt, ...
) {

#ifndef MINPUT_NO_PRINTF
   va_list args;
   char buffer[OSIO_PRINTF_BUFFER_SZ + 1];

   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );

   va_start( args, fmt );
   vsnprintf( buffer, OSIO_PRINTF_BUFFER_SZ, fmt, args );
   va_end( args );

   /* XXX: Print to window.
   printf( "%s", buffer );
   */

   fprintf( g_dbg, "%s", buffer );

   if( MINPUT_STAT_ERROR == status ) {
      MessageBox( g_window, buffer, "MInput Hop", MB_ICONSTOP );
   }
#else
   if( MINPUT_STAT_ERROR == status ) {
      MessageBox( g_window, "Error", "MInput Hop", MB_ICONSTOP );
   }
#endif /* !MINPUT_NO_PRINTF */
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

void osio_logging_setup() {
   g_dbg = fopen( "dbg.txt", "w" );
   assert( NULL != g_dbg );
}

void osio_logging_cleanup() {
   if( NULL != g_dbg && stdout != g_dbg ) {
      fclose( g_dbg );
   }
}

#ifdef MINPUT_OS_WIN32
int WINAPI WinMain(
   HINSTANCE instance, HINSTANCE prev_instance, LPSTR args, int cmd_show
) {
#else
int PASCAL WinMain(
   HINSTANCE instance, HINSTANCE prev_instance, LPSTR args, int cmd_show
) {
#endif /* MINPUT_OS_WIN32 */
   int retval = 0;

   g_instance = instance;
   g_cmd_show = cmd_show;

   memset( &g_config, '\0', sizeof( struct NETIO_CFG ) );
   osio_parse_args( __argc, __argv, &g_config );

   retval = minput_main( &g_config );

   if( retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "quitting: %d\n", retval );
   }

   return retval;
}

