
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

#ifdef MINPUT_NO_ULTOA

/* Needed for borland. */
#define _ultoa( a, b, c ) ltoa( a, b, c )

#endif /* MINPUT_NO_ULTOA */

static UINT WM_TASKBARCREATED = 0;
static HWND g_window = NULL;
static HWND g_status_label_h = NULL;
#ifdef DEBUG
/* Used for keycode debugging in osio_key_down() below. */
static HWND g_key_label_h = NULL;
#endif /* DEBUG */
static HINSTANCE g_instance = NULL;
static int g_cmd_show = 0;
static struct NETIO_CFG g_config;
static int g_running = 1;
#ifdef MINPUT_OS_WIN32
static NOTIFYICONDATA g_notify_icon_data;
#endif /* MINPUT_OS_WIN32 */
static FILE* g_dbg = NULL;

static void osio_win_quit( HWND window_h, int retval ) {
   
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

   /* Create a label on the left and a text field on the right. */

   CreateWindow(
      "static", label, WS_CHILD | WS_VISIBLE,
      10, y, 110, 20, parent_h, NULL,
      instance_h, NULL );
#ifdef MINPUT_OS_WIN16
   out_h = CreateWindow(
#else
   out_h = CreateWindowEx(
      /* Use 3D look in 32-bit Windows. */
      WS_EX_CLIENTEDGE,
#endif /* MINPUT_OS_WIN16 */
      "edit", NULL,
      WS_CHILD | WS_VISIBLE | WS_BORDER,
      120, y - 5, 160, 25, parent_h, (HMENU)id,
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

#define osio_assert_control( ctl_h ) \
   if( NULL == ctl_h ) { \
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR, \
         "could not allocate: " #ctl_h ); \
      osio_win_quit( window_h, MINHOP_ERR_OS ); \
      goto cleanup; \
   }

LRESULT CALLBACK WndProc(
   HWND window_h, UINT message, WPARAM wParam, LPARAM lParam
) {
   static HWND client_name_h = (HWND)NULL;
   static HWND server_addr_h = (HWND)NULL;
   static HWND server_port_h = (HWND)NULL;
   static HWND save_h = (HWND)NULL;
#ifdef MINPUT_OS_WIN32
   static HMENU popup_menu = (HMENU)NULL;
   POINT cursor_pt;
   UINT popup_menu_clicked = 0;
#endif /* MINPUT_OS_WIN32 */
   HANDLE instance_h = (HANDLE)NULL;
   char num_buffer[OSIO_NUM_BUFFER_SZ + 1];

   /* Handle window messages. */

   switch( message ) {
   case WM_CREATE:
      /* Create text fields. */
      client_name_h = osio_win_add_field(
         window_h, instance_h, "Client name", 10, ID_WIN_CLIENT_NAME );
      osio_assert_control( client_name_h );
      SetWindowText( client_name_h, g_config.client_name );
      
      server_addr_h = osio_win_add_field(
         window_h, instance_h, "Server address", 40, ID_WIN_SERVER_ADDR );
      osio_assert_control( server_addr_h );
      SetWindowText( server_addr_h, g_config.server_addr );
      
      server_port_h = osio_win_add_field(
         window_h, instance_h, "Server port", 70, ID_WIN_SERVER_ADDR );
      osio_assert_control( server_port_h );
      memset( num_buffer, '\0', OSIO_NUM_BUFFER_SZ + 1 );
      _ultoa( g_config.server_port, num_buffer, 10 );
      SetWindowText( server_port_h, num_buffer );

      save_h = CreateWindow(
         "button", "&Save", WS_CHILD | WS_VISIBLE | WS_BORDER,
         10, 100, 60, 30, window_h, (HMENU)ID_WIN_SAVE,
         instance_h, NULL );

#ifdef DEBUG
      /* This is the field used to display key debug info... only in
       * DEBUG mode!
       */
      g_key_label_h = CreateWindow(
         "static", "", WS_CHILD | WS_VISIBLE,
         80, 95, 200, 45, window_h, NULL,
         instance_h, NULL );
#endif /* DEBUG */

      /* Status notification label at the bottom. */
      g_status_label_h = CreateWindow(
         "static", "", WS_CHILD | WS_VISIBLE,
         10, 140, 270, 20, window_h, NULL,
         instance_h, NULL );

#ifdef MINPUT_OS_WIN32
      /* Create the notification area menu. */
      popup_menu = CreatePopupMenu();
      AppendMenu( popup_menu, MF_STRING, ID_TRAY_CONTEXT_EXIT, "Exit" );
#endif /* MINPUT_OS_WIN32 */

      break;

   case WM_TIMER:
      if( ID_TIMER_LOOP == wParam ) {
         minput_loop_iter( &g_config );
         /* TODO: Exit on bad retval? */
      }
      break;

#ifdef MINPUT_OS_WIN32
   case WM_DESTROY:
      /* Cleanup the notification area popup menu. */
      DestroyMenu( popup_menu );
      break;
#endif /* MINPUT_OS_WIN32 */

#ifdef MINPUT_OS_WIN32
   case WM_TRAYICON:
      switch( lParam ) {
      case WM_LBUTTONDBLCLK:
         /* Restore the window, as the notification area icon was clicked! */
         ShowWindow( window_h, SW_SHOW );
         break;

      case WM_RBUTTONDOWN:
         GetCursorPos( &cursor_pt );
         SetForegroundWindow( window_h ); 
         popup_menu_clicked = TrackPopupMenu(
            popup_menu,
            TPM_RETURNCMD | TPM_NONOTIFY,
            cursor_pt.x,
            cursor_pt.y,
            0,
            window_h,
            NULL
         );
         if( ID_TRAY_CONTEXT_EXIT == popup_menu_clicked ) {
            osio_win_quit( window_h, 0 );
         }
         break;
      }
      break;
#endif /* MINPUT_OS_WIN32 */

   case WM_COMMAND:
      switch( wParam ) {
      case ID_WIN_MENU_FILE_EXIT:
         osio_win_quit( window_h, 0 );
         break;

      case ID_WIN_SAVE:
         osio_win_save_fields( client_name_h, server_addr_h, server_port_h );
         break;
      }
      break;

   case WM_CLOSE:
#ifdef MINPUT_OS_WIN32
      /* We can restore from the notification area icon. */
      ShowWindow( window_h, SW_HIDE );
#else
      /* Quit program on main window close. */
      osio_win_quit( window_h, 0 );
#endif /* MINPUT_OS_WIN32 */
      break;

   case WM_SYSCOMMAND:
      switch( wParam & 0xfff0 ) {
#ifdef MINPUT_OS_WIN32
         case SC_MINIMIZE:
         case SC_CLOSE:
            /* We can restore from the notification area icon. */
            ShowWindow( window_h, SW_HIDE );
            break;
#endif /* MINPUT_OS_WIN32 */

         default:
            return DefWindowProc( window_h, message, wParam, lParam );
      }
      break;
      
   default:
      return DefWindowProc( window_h, message, wParam, lParam );
   }

cleanup:
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
   WNDCLASS wc;

   memset( &wc, '\0', sizeof( WNDCLASS ) );

#ifdef MINPUT_OS_WIN32
   WM_TASKBARCREATED = RegisterWindowMessage( "TaskbarCreated" );
#endif /* MINPUT_OS_WIN32 */

   /* Create window class. */

   wc.lpfnWndProc = (WNDPROC)&WndProc;
   wc.hInstance = g_instance;
   wc.hIcon = LoadIcon( g_instance, MAKEINTRESOURCE( ID_MINHOP_ICO ) );
   wc.hCursor = LoadCursor( 0, IDC_ARROW );
#ifdef MINPUT_OS_WIN32
   wc.hbrBackground = (HBRUSH)( COLOR_WINDOW );
#elif defined( MINPUT_OS_WIN16 )
   wc.hbrBackground = (HBRUSH)( COLOR_WINDOW + 1 );
#endif /* MINPUT_OS_WIN */
   wc.lpszMenuName = "MinhopWindowMenu";
   wc.lpszClassName = "MinhopWindowClass";

   if( !RegisterClass( &wc ) ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not register window class!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

   /* Create the window. */

   g_window = CreateWindow(
      "MinhopWindowClass",
      "Minput Hop",
      WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
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

   if( !SetTimer( g_window, ID_TIMER_LOOP, 100, NULL ) ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "could not set timer!\n" );
      retval = MINHOP_ERR_OS;
      goto cleanup;
   }

#ifdef MINPUT_OS_WIN32

   /* Setup and display tray notification area icon. */

   memset( &g_notify_icon_data, '\0', sizeof( NOTIFYICONDATA ) );

   g_notify_icon_data.cbSize = sizeof( NOTIFYICONDATA );
   g_notify_icon_data.hWnd = g_window;
   g_notify_icon_data.uID = ID_NOTIFY_ICON;
   g_notify_icon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
   g_notify_icon_data.hIcon = LoadIcon(
      g_instance, MAKEINTRESOURCE( ID_MINHOP_ICO ) );
   strcpy( g_notify_icon_data.szTip,
      "Minput Hop (double-click to restore)" );
   g_notify_icon_data.uCallbackMessage = WM_TRAYICON;

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
#ifdef DEBUG_FLOW
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "processing windows message queue...\n" );
#endif /* DEBUG_FLOW */
      retval = GetMessage( &msg, NULL, 0, 0 );
      TranslateMessage( &msg );
      DispatchMessage( &msg );

      if( WM_QUIT == msg.message ) {
         if( g_running ) {
            osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
               "found quit message while still running!\n" );
         }
         retval = msg.wParam;
         g_running = 0;
      }
   } while( g_running && 0 < retval );

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_INFO,
      "leaving message loop...\n" );

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
   char itoa_buf[OSIO_NUM_BUFFER_SZ + 1];
   char c;

   va_start( args, fmt );

   memset( prefix, '\0', OSIO_PRINTF_PREFIX_SZ + 1 );
   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );
   memset( itoa_buf, '\0', OSIO_NUM_BUFFER_SZ + 1 );

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
         memset( itoa_buf, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );
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

#ifdef DEBUG

   /* Append buffer to the log file. */

   if( NULL != g_dbg ) {
      fprintf( g_dbg, "%s: %s", prefix, buffer );
      fflush( g_dbg );
   }

#endif /* DEBUG */
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

void osio_mouse_down(
   uint16_t mouse_x, uint16_t mouse_y, uint16_t mouse_btn
) {

   /* TODO: Detect if the minput hop window is under the mouse and, if so,
    *       automatically follow up with a mouse_up.
    */

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

static void osio_win_key(
   uint16_t* key_id, uint16_t key_mod, uint16_t* key_btn
) {
   uint32_t key_id_vk = 0;

   /* This is ugly and basically brute force... the translations below were
    * derived by sending key presses to a Win9x client and observing what
    * synergy produced for a key ID for each, and then matching them to
    * keycodes from Spy++.
    *
    * It's also probably *highly* specific to your keyboard layout... this
    * is a standard US keyboard, but please feel free to submit pull requests
    * for other keyboards if you like!
    */

   /* Get scancode by subtracting 8 (no, I don't know why!) */
   *key_btn -= 8;

   if( *key_id >= 0x61 && *key_id <= 0x7a ) {
      /* Translate uppercase to lowercase by ASCII offset. */
      *key_id -= 32;
   } else {
      /* Figure out virtual code by brute force. */
      switch( *key_id ) {
         case 0x60: *key_id = 0xc0; break; /* ` */
         case 0x7e: *key_id = 0xc0; break; /* ~ */
         case 0x31: *key_id = '1'; break;
         case 0x21: *key_id = '1'; break;
         case 0x32: *key_id = '2'; break;
         case 0x40: *key_id = '2'; break;
         case 0x23: *key_id = '3'; break;
         case 0x33: *key_id = '3'; break;
         case 0x24: *key_id = '4'; break;
         case 0x34: *key_id = '4'; break;
         case 0x25: *key_id = '5'; break;
         case 0x35: *key_id = '5'; break;
         case 0x36: *key_id = '6'; break;
         case 0x5e: *key_id = '6'; break;
         case 0x26: *key_id = '7'; break;
         case 0x37: *key_id = '7'; break;
         case 0x2a: *key_id = '8'; break;
         case 0x38: *key_id = '8'; break;
         case 0x28: *key_id = '9'; break;
         case 0x39: *key_id = '9'; break;
         case 0x29: *key_id = '0'; break;
         case 0x30: *key_id = '0'; break;
         case 0x2d: *key_id = 0xbd; break; /* - */
         case 0x5f: *key_id = 0xbd; break; /* _ */
         case 0x2b: *key_id = 0xbb; break; /* + */
         case 0x3d: *key_id = 0xbb; break; /* = */
         case 0x2c: *key_id = 0xbc; break; /* , */
         case 0x3c: *key_id = 0xbc; break; /* < */
         case 0x2e: *key_id = 0xbe; break; /* . */
         case 0x3e: *key_id = 0xbe; break; /* > */
         case 0x2f: *key_id = 0xbf; break; /* / */
         case 0x3f: *key_id = 0xbf; break; /* ? */
         case 0x3a: *key_id = 0xba; break; /* : */
         case 0x3b: *key_id = 0xba; break; /* ; */
         case 0x22: *key_id = 0xde; break; /* " */
         case 0x27: *key_id = 0xde; break; /* ' */
         case 0x5b: *key_id = 0xdb; break; /* [ */
         case 0x7b: *key_id = 0xdb; break; /* [ */
         case 0x5d: *key_id = 0xdd; break; /* ] */
         case 0x7d: *key_id = 0xdd; break; /* ] */
         case 0x5c: *key_id = 0xdc; break; /* \\ */
         case 0x7c: *key_id = 0xdc; break; /* | */
         /*
          * Blank template for copy/pasting:
         case 0x: *key_id = ; break;
         */
         case 0xef08: *key_id = ((VK_BACK) & 0xffff); break;
         case 0xef09: *key_id = ((VK_TAB) & 0xffff); break;
         case 0xef0d: *key_id = ((VK_RETURN) & 0xffff); break;
         case 0xefe1: *key_id = ((VK_SHIFT) & 0xffff); break;
         case 0xefe2: *key_id = ((VK_SHIFT) & 0xffff); break;
         case 0xefe3: *key_id = ((VK_CONTROL) & 0xffff); break;
         case 0xefe4: *key_id = ((VK_CONTROL) & 0xffff); break;
         case 0xefe9: *key_id = ((VK_MENU) & 0xffff); break;
         case 0xef13: *key_id = ((VK_PAUSE) & 0xffff); break;
         case 0xef1b: *key_id = ((VK_ESCAPE) & 0xffff); break;
         case 0x0020: *key_id = ((VK_SPACE) & 0xffff); break;
         case 0xef55: *key_id = ((VK_PRIOR) & 0xffff); break;
         case 0xef56: *key_id = ((VK_NEXT) & 0xffff); break;
         case 0xef57: *key_id = ((VK_END) & 0xffff); break;
         case 0xef50: *key_id = ((VK_HOME) & 0xffff); break;
         case 0xef51: *key_id = ((VK_LEFT) & 0xffff); break;
         case 0xef52: *key_id = ((VK_UP) & 0xffff); break;
         case 0xef53: *key_id = ((VK_RIGHT) & 0xffff); break;
         case 0xef54: *key_id = ((VK_DOWN) & 0xffff); break;
         case 0xef61: *key_id = ((VK_SNAPSHOT) & 0xffff); break;
         case 0xef63: *key_id = ((VK_INSERT) & 0xffff); break;
         case 0xefff: *key_id = ((VK_DELETE) & 0xffff); break;
         case 0xefb0: *key_id = ((VK_NUMPAD0) & 0xffff); break;
         case 0xefb1: *key_id = ((VK_NUMPAD1) & 0xffff); break;
         case 0xefb2: *key_id = ((VK_NUMPAD2) & 0xffff); break;
         case 0xefb3: *key_id = ((VK_NUMPAD3) & 0xffff); break;
         case 0xefb4: *key_id = ((VK_NUMPAD4) & 0xffff); break;
         case 0xefb5: *key_id = ((VK_NUMPAD5) & 0xffff); break;
         case 0xefb6: *key_id = ((VK_NUMPAD6) & 0xffff); break;
         case 0xefb7: *key_id = ((VK_NUMPAD7) & 0xffff); break;
         case 0xefb8: *key_id = ((VK_NUMPAD8) & 0xffff); break;
         case 0xefb9: *key_id = ((VK_NUMPAD9) & 0xffff); break;
         case 0xefaa: *key_id = ((VK_MULTIPLY) & 0xffff); break;
         case 0xefab: *key_id = ((VK_ADD) & 0xffff); break;
         case 0xefad: *key_id = ((VK_SUBTRACT) & 0xffff); break;
         case 0xefae: *key_id = ((VK_DECIMAL) & 0xffff); break;
         case 0xefaf: *key_id = ((VK_DIVIDE) & 0xffff); break;
         case 0xefbe: *key_id = ((VK_F1) & 0xffff); break;
         case 0xefbf: *key_id = ((VK_F2) & 0xffff); break;
         case 0xefc0: *key_id = ((VK_F3) & 0xffff); break;
         case 0xefc1: *key_id = ((VK_F4) & 0xffff); break;
         case 0xefc2: *key_id = ((VK_F5) & 0xffff); break;
         case 0xefc3: *key_id = ((VK_F6) & 0xffff); break;
         case 0xefc4: *key_id = ((VK_F7) & 0xffff); break;
         case 0xefc5: *key_id = ((VK_F8) & 0xffff); break;
         case 0xefc6: *key_id = ((VK_F9) & 0xffff); break;
         case 0xefc7: *key_id = ((VK_F10) & 0xffff); break;
         case 0xefc8: *key_id = ((VK_F11) & 0xffff); break;
         case 0xefc9: *key_id = ((VK_F12) & 0xffff); break;
         case 0xef7f: *key_id = ((VK_NUMLOCK) & 0xffff); break;
         /*
         Don't have these keys, apparently? So couldn't test for them!
         case : key_id_vk |= ((VK_CANCEL) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_CLEAR) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_CAPITAL) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_SELECT) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_PRINT) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_EXECUTE) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_HELP) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_SEPARATOR) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F13) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F14) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F15) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F16) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F17) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F18) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F19) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F20) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F21) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F22) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F23) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_F24) & 0xffff); break;
         case 0x: key_id_vk |= ((VK_SCROLL) & 0xffff); break;
         */
      }
   }
}

void osio_key_down( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
#ifdef DEBUG
   char buffer[OSIO_PRINTF_BUFFER_SZ + 1];
#endif /* DEBUG */

#ifdef DEBUG
   /* In DEBUG builds, print details on keypresses in the main window. */
   memset( buffer, '\0', OSIO_PRINTF_BUFFER_SZ + 1 );
   sprintf( buffer, /* Use sprintf for VC 1.5. */
      "id: %c (0x%04x) mod: 0x%04x btn: 0x%04x ",
      key_id, key_id, key_mod, key_btn );
#endif /* DEBUG */

   osio_win_key( &key_id, key_mod, &key_btn );

#ifdef DEBUG
   /* Append what's changed after transformation function. */
   sprintf( &(buffer[strlen( buffer )]), /* Use sprintf for VC 1.5. */
      "tlv: 0x%04x tls: 0x%04x",
      key_id, key_btn );
   SetWindowText( g_key_label_h, buffer );
#endif /* DEBUG */

   keybd_event( key_id, key_btn, 0, 0 );
}

void osio_key_up( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
   osio_win_key( &key_id, key_mod, &key_btn );
   keybd_event( key_id, key_btn, KEYEVENTF_KEYUP, 0 );
}

void osio_key_rpt( uint16_t key_id, uint16_t key_mod, uint16_t key_btn ) {
}

void osio_logging_setup() {
#ifdef DEBUG
   g_dbg = fopen( "dbg.txt", "w" );
   assert( NULL != g_dbg );
#endif /* DEBUG */
}

void osio_logging_cleanup() {
#ifdef DEBUG
   if( NULL != g_dbg && stdout != g_dbg ) {
      fclose( g_dbg );
   }
#endif /* DEBUG */
}

#define _osio_win_undoc_proc_ld( proc_name, module ) \
   g_ ## proc_name ## _proc = GetProcAddress( user_h, #proc_name ); \
   if( NULL == g_ ## proc_name ## _proc ) { \
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR, \
         "could not find " #proc_name " proc!\n" ); \
      retval = MINHOP_ERR_OS; \
      goto cleanup; \
   }

#ifdef DEBUG
/* In debug mode, also write handle to the log! */
#  define osio_win_undoc_proc( proc_name, module ) \
      _osio_win_undoc_proc_ld( proc_name, module ) \
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, \
      "found " #proc_name " at: 0x%08lx\n", g_ ## proc_name ## _proc );
#else
#  define osio_win_undoc_proc( proc_name, module ) \
      _osio_win_undoc_proc_ld( proc_name, module )
#endif /* DEBUG */

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

   osio_win_undoc_proc( mouse_event, user_h ); /* 507a in WFWG311 */
   osio_win_undoc_proc( keybd_event, user_h ); /* 4b59 in WFWG311 */

#endif /* MINPUT_OS_WIN16 */

#ifdef MINPUT_NO_ARGS
   /* Hardcode network config. */
   strcpy( g_config.client_name, MINPUT_S_CLIENT_NAME );
   strcpy( g_config.server_addr, MINPUT_S_SERVER_ADDR );
   g_config.server_port = 24800;
#else
   /* Setup network config. */
   memset( &g_config, '\0', sizeof( struct NETIO_CFG ) );
   osio_parse_args( __argc, __argv, &g_config );
#endif /* MINPUT_NO_ARGS */

   retval = minput_main( &g_config );

#ifdef MINPUT_OS_WIN16
cleanup:
#endif /* MINPUT_OS_WIN16 */

   if( retval ) {
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
         "quitting: %d\n", retval );
   }

   return retval;
}

