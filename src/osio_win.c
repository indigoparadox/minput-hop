
#include "osio.h"

#include "minhopr.h"

#ifdef MINPUT_OS_WIN16

/* These are setup in WinMain below, if needed. */
FARPROC g_mouse_event_proc = 0;
FARPROC g_keybd_event_proc = 0;

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
      call dword ptr [g_mouse_event_proc]
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
      call dword ptr [g_keybd_event_proc]
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
int g_running = 1;

#ifdef MINPUT_OS_WIN32
NOTIFYICONDATA g_notify_icon_data;
#endif /* MINPUT_OS_WIN32 */

static void osio_win_quit( HWND window_h ) {
   
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "quit command received from UI...\n" );

   /* Don't try to reconnect! */
   KillTimer( window_h, ID_TIMER_LOOP );
   if( 0 != g_config.socket_fd ) {
      netio_disconnect( &g_config );
   }
   PostQuitMessage( 0 );

   g_running = 0;
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

static void osio_win_save_fields(
   HWND client_name_h, HWND server_addr_h, HWND server_port_h
) {
   int retval = 0;
   char buffer[SERVER_ADDR_SZ_MAX + 1];

   /* Automatically try to connect with new info later. */
   netio_disconnect( &g_config );

   /* Parse the fields into config. */

   memset( buffer, '\0', SERVER_ADDR_SZ_MAX + 1 );
   retval = GetWindowText( client_name_h, buffer, SERVER_ADDR_SZ_MAX );
   strncpy( g_config.client_name, buffer, SERVER_ADDR_SZ_MAX );
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "set client name to: %s\n", g_config.client_name );

   memset( buffer, '\0', SERVER_ADDR_SZ_MAX + 1 );
   retval = GetWindowText( server_addr_h, buffer, SERVER_ADDR_SZ_MAX );
   strncpy( g_config.server_addr, buffer, SERVER_ADDR_SZ_MAX );
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "set server address to: %s\n", g_config.server_addr );

   memset( buffer, '\0', SERVER_ADDR_SZ_MAX + 1 );
   retval = GetWindowText( server_port_h, buffer, SERVER_ADDR_SZ_MAX );
   g_config.server_port = atoi( buffer );
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "set server port to: %u\n", g_config.server_port );

   netio_connect( &g_config );
}

#define NUM_BUFFER_SZ 50

LRESULT CALLBACK WndProc(
   HWND window_h, UINT message, WPARAM wParam, LPARAM lParam
) {
   static HWND client_name_h = (HWND)NULL;
   static HWND server_addr_h = (HWND)NULL;
   static HWND server_port_h = (HWND)NULL;
   static HWND save_h = (HWND)NULL;
   HANDLE instance_h = (HANDLE)NULL;
   char num_buffer[NUM_BUFFER_SZ + 1];

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
      memset( num_buffer, '\0', NUM_BUFFER_SZ + 1 );
      _ultoa( g_config.server_port, num_buffer, 10 );
      SetWindowText( server_port_h, num_buffer );

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

      case ID_WIN_SAVE:
         osio_win_save_fields( client_name_h, server_addr_h, server_port_h );
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
   wc.hbrBackground = (HBRUSH)( COLOR_WINDOW );
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
         "could not set timer!\n" );
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
#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "processing windows message queue...\n" );
#endif
      retval = GetMessage( &msg, NULL, 0, 0 );
      TranslateMessage( &msg );
      DispatchMessage( &msg );

      if( WM_QUIT == msg.message ) {
#ifdef DEBUG
         if( g_running ) {
            osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
               "found quit message while still running!\n" );
         }
#endif
         g_running = 0;
      }
   } while( g_running && 0 < retval );

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
      "leaving message loop...\n" );

   if( 0 < retval ) {
      retval = 0;
   }

   return retval;
}

#ifdef MINPUT_NO_PRINTF
union FMT_SPEC {
   long d;
   unsigned long u;
   char c;
   void* p;
   char* s;
};

#define SPEC_FMT_LONG 1
#define SPEC_FMT_UNSIGNED 2
#endif /* MINPUT_NO_PRINTF */

void osio_printf(
   const char* file, int line, int status, const char* fmt, ...
) {
   va_list args;
   char buffer[OSIO_PRINTF_BUFFER_SZ + 1];
   char prefix[OSIO_PRINTF_PREFIX_SZ + 1];
   static char last_char = '\n';

#ifndef MINPUT_NO_PRINTF
   memset( prefix, '\0', OSIO_PRINTF_PREFIX_SZ + 1 );
   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );

   if( '\n' == last_char ) {
      /* Only produce a prefix on a new line. */
      snprintf( prefix, OSIO_PRINTF_PREFIX_SZ,
         "(%d) %s: %d: ", status, file, line );
   }

   va_start( args, fmt );
   vsnprintf( buffer, OSIO_PRINTF_BUFFER_SZ, fmt, args );
   va_end( args );

   assert( 0 < strlen( buffer ) );
   last_char = buffer[strlen( buffer ) - 1];
#else
   size_t i = 0,
      i_out = 0;
   char last = '\0';
   int spec_fmt = 0;
   union FMT_SPEC spec;
   char itoa_buf[50];
   char c;

   va_start( args, fmt );

   memset( prefix, '\0', OSIO_PRINTF_PREFIX_SZ + 1 );
   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );
   memset( itoa_buf, '\0', 50 );

   if( '\n' == last_char ) {
      /* Only produce a prefix on a new line. */
      snprintf( prefix, OSIO_PRINTF_PREFIX_SZ,
         "(%d) %s: %d: ", status, file, line );
   }

   /* Roughly adapted from uprintf for Visual C++ */

   for( i = 0 ; '\0' != fmt[i] ; i++ ) {
      c = fmt[i]; /* Separate so we can play tricks below. */

      if( i_out >= OSIO_PRINTF_BUFFER_SZ ) {
         break;
      }
 
      if( '%' == last ) {
         /* Conversion specifier encountered. */
         switch( fmt[i] ) {
            case 'l':
               spec_fmt |= SPEC_FMT_LONG;
               /* Skip resetting the last char. */
               continue;

            case 's':
               spec.s = va_arg( args, char* );

               /* Print string. */
               i_out += strlen( spec.s );
               strcat( buffer, spec.s );
               break;

            case 'u':
               spec_fmt |= SPEC_FMT_UNSIGNED;
            case 'd':
               if( SPEC_FMT_LONG == (SPEC_FMT_LONG & spec_fmt) ) {
                  if( SPEC_FMT_UNSIGNED == (SPEC_FMT_UNSIGNED & spec_fmt) ) {
                     spec.u = va_arg( args, unsigned long );
                  } else {
                     spec.d = va_arg( args, long );
                  }
               } else {
                  if( SPEC_FMT_UNSIGNED == (SPEC_FMT_UNSIGNED & spec_fmt) ) {
                     spec.u = va_arg( args, unsigned );
                  } else {
                     spec.d = va_arg( args, int );
                  }
               }
               
               if( SPEC_FMT_UNSIGNED == (SPEC_FMT_UNSIGNED & spec_fmt) ) {
                  _ultoa( spec.u, itoa_buf, 10 );
               } else {
                  _ltoa( spec.d, itoa_buf, 10 );
               }
               i_out += strlen( itoa_buf );
               strcat( buffer, itoa_buf );
               break;

            case 'x':
               if( SPEC_FMT_LONG == (SPEC_FMT_LONG & spec_fmt) ) {
                  spec.u = va_arg( args, unsigned long );
               } else {
                  spec.u = va_arg( args, unsigned int );
               }

               /* TODO */

               _ultoa( spec.u, itoa_buf, 16 );
               i_out += strlen( itoa_buf );
               strcat( buffer, itoa_buf );
               break;

            case 'c':
               spec.c = va_arg( args, int );

               buffer[i_out++] = spec.c;
               break;

            case '%':
               /* Literal % */
               last = '\0';
               buffer[i_out++] = '%';
               break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
               c = '%';
               break;
         }
      } else if( '%' != c ) {
         spec_fmt = 0;
         buffer[i_out++] = c;
         memset( itoa_buf, '\0', 50 );
      }
      last = c;
   }

   va_end( args );
#endif /* !MINPUT_NO_PRINTF */

   if( MINPUT_STAT_ERROR == status ) {
      MessageBox( g_window, buffer, "MInput Hop", MB_ICONSTOP );
   } else if( MINPUT_STAT_INFO == status ) {
      SetWindowText( g_status_label_h, buffer );
   }

   fprintf( g_dbg, "%s: %s", prefix, buffer );
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
#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "found USER.EXE at: 0x%04x\n", user_h );
#endif /* DEBUG */

   g_mouse_event_proc = GetProcAddress( user_h, "mouse_event" ); /* 507a */
   if( NULL == g_mouse_event_proc ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not find mouse_event proc!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }
#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "found mouse_event at: 0x%08lx\n", g_mouse_event_proc );
#endif /* DEBUG */

   g_keybd_event_proc = GetProcAddress( user_h, "keybd_event" ); /* 4b59 */
   if( NULL == g_keybd_event_proc ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not find keybd_event proc!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }
#ifdef DEBUG
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "found keybd_event at: 0x%08lx\n", g_keybd_event_proc );
#endif /* DEBUG */

#endif /* MINPUT_OS_WIN16 */

   /* Setup packet buffer. */
   g_pkt_buf = calloc( SOCKBUF_SZ + 1, 1 );
   if( NULL == g_pkt_buf ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not allocate packet buffer!\n" );
      retval = MINHOP_ERR_ALLOC;
      goto cleanup;
   }

#ifdef MINPUT_NO_ARGS
   strcpy( g_config.client_name, MINPUT_S_CLIENT_NAME );
   strcpy( g_config.server_addr, MINPUT_S_SERVER_ADDR );
   g_config.server_port = 24800;
#else
   /* Setup network config. */
   memset( &g_config, '\0', sizeof( struct NETIO_CFG ) );
   osio_parse_args( __argc, __argv, &g_config );
#endif /* MINPUT_NO_ARGS */

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

