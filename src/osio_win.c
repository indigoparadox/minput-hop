
#include "osio.h"

#include "minhopr.h"

UINT WM_TASKBARCREATED = 0;

HWND g_window = NULL;
HINSTANCE g_instance = NULL;
int g_cmd_show = 0;
MSG g_message;

#ifdef MINPUT_OS_WIN32
NOTIFYICONDATA g_notify_icon_data;
#endif /* MINPUT_OS_WIN32 */

LRESULT CALLBACK WndProc(
   HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam
) {

   return DefWindowProc( hWnd, message, wParam, lParam );
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
      /* TODO: MessageBox */
      goto cleanup;
   }

   g_window = CreateWindowEx(
      0,
      "MinhopWindowClass",
      "Minput Hop",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      100,
      100,
      NULL,
      NULL,
      g_instance,
      NULL
   );

   if( !g_window ) {
      /* TODO: MessageBox */
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

int osio_ui_loop() {
   int retval = 0;

   retval = GetMessage( &g_message, NULL, 0, 0 );
   if( retval ) {
      TranslateMessage( &g_message );
      DispatchMessage( &g_message );
   }

   return retval;
}

void osio_printf( const char* file, int line, const char* fmt, ... ) {
   va_list args;
   char buffer[OSIO_PRINTF_BUFFER_SZ + 1];

   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );

   va_start( args, fmt );
   /* XXX: Print to window.
   vsnprintf( buffer, OSIO_PRINTF_BUFFER_SZ, fmt, args );
   */
   va_end( args );

   /* XXX: Print to window.
   printf( "%s", buffer );

   fprintf( g_dbg, "%s", buffer );
   */
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
   /* XXX
   g_dbg = fopen( "dbg.txt", "w" );
   assert( NULL != g_dbg );
   */
}

void osio_logging_cleanup() {
   /* XXX
   if( NULL != g_dbg && stdout != g_dbg ) {
      fclose( g_dbg );
   }
   */
}

int WINAPI WinMain(
   HINSTANCE instance, HINSTANCE prev_instance, LPSTR args, int cmd_show
) {
   int retval = 0;
   struct NETIO_CFG config;

   g_instance = instance;
   g_cmd_show = cmd_show;

#ifdef MINPUT_OS_WIN32
   memset( &config, '\0', sizeof( struct NETIO_CFG ) );
   retval = minhop_parse_args( __argc, __argv, &config );
   if( 0 != retval ) {
      goto cleanup;
   }
#endif /* MINPUT_OS_WIN32 */

   retval = minput_main( &config );

#ifdef MINPUT_OS_WIN32
cleanup:
#endif /* MINPUT_OS_WIN32 */
   return retval;
}

