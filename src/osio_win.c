
#include "osio.h"

#include "minhopr.h"

#ifdef MINPUT_OS_WIN16

/* These are setup in WinMain below, if needed. */
FARPROC WINAPI g_mouse_event_proc = 0;
FARPROC WINAPI g_keybd_event_proc = 0;

#define MOUSEEVENTF_MOVE         0x00000001L
#define MOUSEEVENTF_LEFTDOWN     0x00000002L
#define MOUSEEVENTF_LEFTUP       0x00000004L
#define MOUSEEVENTF_RIGHTDOWN    0x00000008L
#define MOUSEEVENTF_RIGHTUP      0x00000010L
#define MOUSEEVENTF_MIDDLEDOWN   0x00000020L
#define MOUSEEVENTF_MIDDLEUP     0x00000040L

#define KEYEVENTF_EXTENDEDKEY    0x00000001L
#define KEYEVENTF_KEYUP          0x00000002L

/* Huge thanks to @cvtsi2sd@hachyderm.io for pointing out this way to call
 * undocumented mouse_event in Win16!
 */

void mouse_event(
   WORD flags, WORD dx_, WORD dy_, WORD data, DWORD extra_info
) {
   WORD exinfo_hi = HIWORD( extra_info );
   WORD exinfo_lo = LOWORD( extra_info );

   /* Note that pusha below needs a 286+, but so does Windows 3.1, so... */
   __asm {
      pusha
      mov ax, flags
      mov bx, dx_
      mov cx, dy_
      mov dx, data
      mov si, exinfo_hi
      mov di, exinfo_lo
      call g_mouse_event_proc
      popa
   }
}

void keybd_event(
   BYTE vk, BYTE scan, DWORD flags, DWORD extra_info
) {
   WORD exinfo_hi = HIWORD( extra_info );
   WORD exinfo_lo = LOWORD( extra_info );
   BYTE flags_hi = 0;

   /* TODO: Extended key flag. */

   /* TODO Test this. */
   if( KEYEVENTF_KEYUP == (KEYEVENTF_KEYUP & flags) ) {
      flags_hi = 0x80;
   }

   /* Note that pusha below needs a 286+, but so does Windows 3.1, so... */
   __asm {
      pusha
      mov al, vk
      mov ah, flags_hi
      mov bl, scan
      mov bh, flags_hi
      mov si, exinfo_hi
      mov di, exinfo_lo
      call g_keybd_event_proc
      popa
   }
}

#endif /* MINPUT_OS_WIN16 */

UINT WM_TASKBARCREATED = 0;

HWND g_window = NULL;
HWND g_status_label_h = NULL;
HINSTANCE g_instance = NULL;
int g_cmd_show = 0;
char* g_pkt_buf = NULL;
uint32_t g_pkt_buf_sz = 0;
struct NETIO_CFG g_config;

#ifdef MINPUT_OS_WIN32
NOTIFYICONDATA g_notify_icon_data;
#endif /* MINPUT_OS_WIN32 */

static void osio_win_quit( HWND window_h ) {
   /* Don't try to reconnect! */
   KillTimer( window_h, ID_TIMER_LOOP );
   netio_disconnect( &g_config, NETIO_DISCO_FORCE );
   PostQuitMessage( 0 );
}

static HWND osio_win_add_field(
   HWND parent_h, HANDLE instance_h,
   const char* label, uint16_t y, uint16_t id
) {
   HWND out_h = (HWND)NULL;

   CreateWindow(
      "static", label, WS_CHILD | WS_VISIBLE,
      10, y, 110, 25, parent_h, NULL,
      instance_h, NULL );
   out_h = CreateWindow(
      "edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER,
      120, y, 160, 25, parent_h, (HMENU)id,
      instance_h, NULL );

   return out_h;
}

LRESULT CALLBACK WndProc(
   HWND window_h, UINT message, WPARAM wParam, LPARAM lParam
) {
   static HWND client_name_h = (HWND)NULL;
   static HWND server_addr_h = (HWND)NULL;
   static HWND server_port_h = (HWND)NULL;
   static HWND save_h = (HWND)NULL;
   HANDLE instance_h = (HANDLE)NULL;

   switch( message ) {
   case WM_CREATE:
      /* Create text fields. */
      client_name_h = osio_win_add_field(
         window_h, instance_h, "Client name", 10, ID_WIN_CLIENT_NAME );
      SetWindowText( client_name_h, g_config.client_name );
      server_addr_h = osio_win_add_field(
         window_h, instance_h, "Server address", 40, ID_WIN_SERVER_ADDR );
      SetWindowText( server_addr_h, g_config.server_addr );
      server_port_h = osio_win_add_field(
         window_h, instance_h, "Server port", 70, ID_WIN_SERVER_ADDR );

      save_h = CreateWindow(
         "button", "&Save", WS_CHILD | WS_VISIBLE | WS_BORDER,
         10, 100, 60, 30, window_h, (HMENU)ID_WIN_SAVE,
         instance_h, NULL );

      g_status_label_h = CreateWindow(
         "static", "", WS_CHILD | WS_VISIBLE,
         10, 140, 270, 20, window_h, NULL,
         instance_h, NULL );
      break;

   case WM_TIMER:
      if( ID_TIMER_LOOP == wParam ) {
         minput_loop_iter( &g_config, g_pkt_buf, &g_pkt_buf_sz );
         /* TODO: Exit on bad retval? */
      }
      break;

   case WM_COMMAND:
      switch( wParam ) {
      case ID_WIN_MENU_FILE_EXIT:
         osio_win_quit( window_h );
         break;
      }
      break;

   case WM_CLOSE:
      osio_win_quit( window_h );
      break;
      
   default:
      return DefWindowProc( window_h, message, wParam, lParam );
   }

   return 0;
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

#ifdef MINPUT_OS_WIN32
   WM_TASKBARCREATED = RegisterWindowMessage( "TaskbarCreated" );
#endif /* MINPUT_OS_WIN32 */

   /* Create window class. */
   wc.lpfnWndProc = (WNDPROC)&WndProc;
   wc.hInstance = g_instance;
   wc.hIcon = LoadIcon( g_instance, MAKEINTRESOURCE( ID_MINHOP_ICO ) );
   wc.hCursor = LoadCursor( 0, IDC_ARROW );
   wc.hbrBackground = (HBRUSH)( COLOR_BTNFACE + 1 );
   wc.lpszMenuName = "MinhopWindowMenu";
   wc.lpszClassName = "MinhopWindowClass";

   if( !RegisterClass( &wc ) ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not register window class!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

   /* Create the window. */

   g_window = CreateWindowEx(
      0,
      "MinhopWindowClass",
      "Minput Hop",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      300,
      210,
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

   /* Setup timer to call the packet processing iterator. */

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

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
      "leaving message loop...\n" );

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

#else
   char* buffer = "Placeholder";
#endif /* !MINPUT_NO_PRINTF */

   if( MINPUT_STAT_ERROR == status ) {
      MessageBox( g_window, buffer, "MInput Hop", MB_ICONSTOP );
   } else if( MINPUT_STAT_INFO == status ) {
      SetWindowText( g_status_label_h, buffer );
   }
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
   mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN,
      mouse_x, mouse_y, 0, 0 );
}

void osio_mouse_up( uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn ) {
   mouse_event(
      OSIO_MOUSE_LEFT == mouse_btn ?
         MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP,
      mouse_x, mouse_y, 0, 0 );
}

static uint16_t osio_win_key( uint16_t key_id ) {
   /* TODO: Raw key_id is not... quite... right... */
   if( key_id >= 97 && key_id <= 122 ) {
      /* Translate uppercase to lowercase. */
      key_id -= 32;
   }
   return key_id;
}

void osio_key_down( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
   key_id = osio_win_key( key_id );
   keybd_event( key_id, 0, 0, 0 );
}

void osio_key_up( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
   key_id = osio_win_key( key_id );
   keybd_event( key_id, 0, KEYEVENTF_KEYUP, 0 );
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
   HMODULE user_h = NULL;

   /* Stow windows-specific vars for later. */
   g_instance = instance;
   g_cmd_show = cmd_show;

#ifdef MINPUT_OS_WIN16

   /* Setup undocumented input event procs for Windows 3.x. */

   user_h = GetModuleHandle( "USER" );
   if( NULL == user_h ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not find USER module!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

   g_mouse_event_proc = GetProcAddress( user_h, "mouse_event" );
   if( NULL == g_mouse_event_proc ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not find mouse_event proc!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

   g_keybd_event_proc = GetProcAddress( user_h, "keybd_event" );
   if( NULL == g_keybd_event_proc ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not find keybd_event proc!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

#endif /* MINPUT_OS_WIN16 */

   /* Setup packet buffer. */
   g_pkt_buf = calloc( SOCKBUF_SZ + 1, 1 );
   if( NULL == g_pkt_buf ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not allocate packet buffer!\n" );
      retval = MINHOP_ERR_ALLOC;
      goto cleanup;
   }

   /* Setup network config. */
   memset( &g_config, '\0', sizeof( struct NETIO_CFG ) );
   osio_parse_args( __argc, __argv, &g_config );

   retval = minput_main( &g_config );

cleanup:

   if( retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "quitting: %d\n", retval );
   }

   if( NULL != g_pkt_buf ) {
      free( g_pkt_buf );
   }

   return retval;
}

