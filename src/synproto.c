
#include "synproto.h"

void synproto_dump( const char* buf, size_t buf_sz ) {
   size_t i = 0;

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "%ld bytes: ", buf_sz );
   for( i = 0 ; buf_sz > i ; i++ ) {
      osio_printf( NULL, __LINE__, MINPUT_STAT_DEBUG,
         "0x%02x (%c) ",
         buf[i] > 32 /* Is printable? */ ? buf[i] : ' ',
         buf[i] );
   }
}

size_t synproto_vprintf(
   char* buf, size_t buf_sz, const char* fmt, va_list args
) {
   int in_token = 0;
   uint8_t arg_i8 = 0;
   uint16_t arg_i16 = 0;
   uint32_t arg_i32 = 0;
   uint8_t* arg_p = NULL;
   size_t in_pos = 0;
   size_t out_pos = 0;
   char* arg_s = NULL;
   size_t i = 0;

   while( in_pos < strlen( fmt ) ) {
      switch( fmt[in_pos] ) {
      case '%':
         /* Default to 4-byte int token (32-bits). */
         in_token = 4;
         break;

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '0':
         if( in_token ) {
            /* Don't leave token yet! */
            in_token = atoi( &(fmt[in_pos]) );
            break;
         }

         /* Proceed to default if not in token. */

      case 'i':
         if( in_token ) {
            /* Insert a raw (non-ASCII) integer in network order. */
            /* TODO: Handle flexible sizes? */
            switch( in_token ) {
            case 4:
               arg_i32 = va_arg( args, int );
               arg_p = (uint8_t*)&arg_i32;
               break;

            case 2:
               arg_i16 = va_arg( args, int );
               arg_p = (uint8_t*)&arg_i16;
               break;

            case 1:
               arg_i8 = va_arg( args, int );
               arg_p = (uint8_t*)&arg_i8;
               break;

            default:
               osio_printf( __FILE__, __LINE__, MINPUT_STAT_ERROR,
                  "invalid token size (%d)?", in_token );
               goto cleanup;
            }

            /* Reverse the integer as we insert it. */
            for( i = 0 ; in_token > i ; i++ ) {
               buf[out_pos + in_token - i - 1] = arg_p[i];
            }
            out_pos += in_token;

            in_token = 0;
            break;
         }

         /* Proceed to default if not in token. */

      case 's':
         if( in_token ) {
            arg_s = va_arg( args, char* );
            for( i = 0 ; '\0' != arg_s[i]; i++ ) {
               buf[out_pos++] = arg_s[i];
            }
            break;
         }

         /* Proceed to default if not in token. */

      default:
         buf[out_pos++] = fmt[in_pos];
         in_token = 0;
         break;
      }
      in_pos++;
   }

cleanup:

   return out_pos;
}

uint32_t synproto_send( int sockfd, uint8_t force_sz, const char* fmt, ... ) {
   char out_buf[SOCKBUF_SZ + 1];
   uint32_t out_pos = 0;
   va_list args;
   uint32_t* buf_sz_p = (uint32_t*)&(out_buf);
   
   memset( out_buf, 0, SOCKBUF_SZ + 1 );

   va_start( args, fmt );
   /* Reserve 4 bytes for message size. */
   out_pos = synproto_vprintf( &(out_buf[4]), SOCKBUF_SZ - 4, fmt, args );
   va_end( args );

#ifdef DEBUG_SEND
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "sending packet:" );
   synproto_dump( out_buf, out_pos + 4 );
#endif /* DEBUG_SEND */

   if( force_sz ) {  
#ifdef DEBUG_SEND
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "buf_sz_p (%p): %u", buf_sz_p, out_pos );
#endif /* DEBUG_SEND */
      *buf_sz_p = swap_32( (uint32_t)4 );
   } else {
#ifdef DEBUG_SEND
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "buf_sz_p (%p): %u", buf_sz_p, out_pos );
#endif /* DEBUG_SEND */
      *buf_sz_p = swap_32( out_pos );
   }

   send( sockfd, out_buf, out_pos + 4, 0 );

   return out_pos;
}

int synproto_parse_and_reply(
   struct NETIO_CFG* config, const char* pkt_buf, uint32_t pkt_buf_sz
) {
   uint32_t* pkt_type_p = (uint32_t*)&(pkt_buf[4]);
   uint16_t ver_maj = 0,
      ver_min = 0,
      mouse_x = 0,
      mouse_y = 0,
      screen_w = 0,
      screen_h = 0,
      key_id = 0,
      key_mod = 0,
      key_btn = 0;
   int32_t clp_seq = 0;
   int8_t clp_id = 0,
      clp_mark = 0;
   int retval = 0;
   size_t i = 0;

   /* It's not ideal for separation of concerns that we send the replies
    * directly from the switch, below. But the alternatives are more complex
    * than we would like, as our goal here is (relative) simplicity.
    */

   /* More info on the protocol strings is available in the barrier codebase
    * at: src/lib/barrier/protocol_types.cpp
    */

#ifdef DEBUG_PACKETS_IN
   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "------" );

   osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
      "pkt type: %c%c%c%c (0x%08x)",
      pkt_buf[4], pkt_buf[5], pkt_buf[6], pkt_buf[7],
      swap_32( *pkt_type_p ) );
#endif /* DEBUG_PACKETS_IN */

   switch( swap_32( *pkt_type_p ) ) {

   case 0x42617272: /* "Barr" */

      /* Grab the version out of the server packet. */
      ver_maj = synproto_exval_16( pkt_buf, 11 );
      ver_min = synproto_exval_16( pkt_buf, 13 );

      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "barrier %u.%u found", ver_maj, ver_min );

      /* Send an acknowledgement and our name. */
      synproto_send(
         config->socket_fd, 0, "Barrier%2i%2i%4i%s",
         ver_maj, ver_min, 7, config->client_name );

      config->calv_deadline = osio_get_time() + SYNPROTO_TIMEOUT_MS;
      
      break;

   case 0x51494e46: /* QINF */

      osio_screen_get_w_h( &screen_w, &screen_h );

#ifdef DEBUG
      osio_printf(
         __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "QINF: sw: %u, sh: %u", screen_w, screen_h );
#endif /* DEBUG */

      synproto_send(
         config->socket_fd, 0, "DINF%2i%2i%2i%2i%2i%2i%2i",
         0, 0, screen_w, screen_h, 0, 0, 0 );

      break;

   case 0x4349414b: /* CIAK */
      /* TODO: Handle this? */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "CIAK?" );
      break;

   case 0x43524f50: /* CROP */
      /* TODO: Handle this? */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "CROP?" );
      break;

   case 0x43414c56: /* "CALV" */
#ifdef DEBUG_CALV
      /* Keepalive received, so respond! */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "Ping... Pong!" );
#endif /* DEBUG_CALV */
      synproto_send( config->socket_fd, 4, "CALV%4iCNOP", 4 );
      config->calv_deadline = osio_get_time() + SYNPROTO_TIMEOUT_MS;
      break;

   case 0x43494e4e: /* "CINN" */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "in!" );
      break;

   case 0x44434c50: /* DCLP */
      /* TODO: Transfer clipboard data. */
      #define DCLP_BUF_START 29
      clp_id = synproto_exval_8( pkt_buf, 8 );
      clp_seq = synproto_exval_16( pkt_buf, 9 );
      clp_mark = synproto_exval_8( pkt_buf, 13 );

      if( 0 != clp_id ) {
         /* Ignore all non-0 clip IDs (not from server?) */
         break;
      }

#ifdef DEBUG_PROTO_CLIP
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "DCLP: id: %02x, seq: %08lx, mark: %02x, sz: %lu bytes: ",
         clp_id, clp_seq, clp_mark, pkt_buf_sz );
      if( 2 == clp_mark ) {
         for( i = DCLP_BUF_START ; pkt_buf_sz > i ; i++ ) {
            osio_printf( NULL, __LINE__, MINPUT_STAT_DEBUG,
               "%c (%02x) ",
               pkt_buf[i] > 32 /* Is printable? */ ? pkt_buf[i] : ' ',
               pkt_buf[i] );
         }
      }
#endif /* DEBUG_PROTO_CLIP */

      osio_set_clipboard(
         &(pkt_buf[DCLP_BUF_START]), pkt_buf_sz - DCLP_BUF_START );
      break;

   case 0x444d4d56: /* DMMV */
      /* OS-specific mouse movement. */
      mouse_x = synproto_exval_16( pkt_buf, 8 );
      mouse_y = synproto_exval_16( pkt_buf, 10 );
#ifdef DEBUG_PROTO_MOUSE
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "DMMV: x: %u, y: %u", mouse_x, mouse_y );
#endif /* DEBUG_PROTO_MOUSE */
      osio_mouse_move( mouse_x, mouse_y );
      break;

   case 0x444d444e: /* DMDN */
      /* OS-specific mouse click. */
      /* XXX: Don't get mouse_x? */
      osio_mouse_down( mouse_x, mouse_y, pkt_buf[8] );
      break;

   case 0x444d5550: /* DMUP */
      /* OS-specific mouse click. */
      /* XXX: Don't get mouse_x? */
      osio_mouse_up( mouse_x, mouse_y, pkt_buf[8] );
      break;

   case 0x444b444e: /* DKDN */
      /* OS-specific key press. */
      key_id = synproto_exval_16( pkt_buf, 8 );
      key_mod = synproto_exval_16( pkt_buf, 10 );
      key_btn = synproto_exval_16( pkt_buf, 12 );
      osio_key_down( key_id, key_mod, key_btn );
      break;

   case 0x444b5550: /* DKUP */
      /* OS-specific key press. */
      key_id = synproto_exval_16( pkt_buf, 8 );
      key_mod = synproto_exval_16( pkt_buf, 10 );
      key_btn = synproto_exval_16( pkt_buf, 12 );
      osio_key_up( key_id, key_mod, key_btn );
      break;

   case 0x444b5250: /* DKRP */
      /* OS-specific key press. */
      key_id = synproto_exval_16( pkt_buf, 8 );
      key_mod = synproto_exval_16( pkt_buf, 10 );
      key_btn = synproto_exval_16( pkt_buf, 12 );
      osio_key_rpt( key_id, key_mod, key_btn );
      break;

   case 0x434f5554: /* COUT */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG, "out!" );
      break;

   case 0x44534f50:
      /* TODO: Handle options. */
      break;

   case 0x45424144: /* "EBAD" */
      /* Protocol error! */

   default:
      /* Not found? Then what *did* we get? */
      osio_printf( __FILE__, __LINE__, MINPUT_STAT_DEBUG,
         "invalid packet:" );
      synproto_dump( pkt_buf, pkt_buf_sz );

      retval = MINHOP_ERR_PROTOCOL;

      /* goto cleanup; */
   }

   return retval;
}

