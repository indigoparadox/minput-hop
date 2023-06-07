
#include "synproto.h"

#include <stdio.h>
#include <string.h> /* memset */
#include <stdlib.h> /* atoi */

#include "osio.h"

extern FILE* g_dbg;

void synproto_dump( const char* buf, size_t buf_sz ) {
   size_t i = 0;

   fprintf( g_dbg, "%ld bytes\n", buf_sz );
   for( i = 0 ; buf_sz > i ; i++ ) {
      fprintf( g_dbg, "0x%02x (%c) ", buf[i], buf[i] );
   }
   fprintf( g_dbg, " (%s)\n", buf );
}

#define SYN_INT_SZ 2
#define SYN_INT_TYPE uint16_t

size_t synproto_printf(
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
               fprintf( g_dbg, "invalid token size (%d)?\n", in_token );
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
            fprintf( g_dbg, "str: %s\n", arg_s );
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
   out_pos = synproto_printf( &(out_buf[4]), SOCKBUF_SZ - 4, fmt, args );
   va_end( args );

   fprintf( g_dbg, "sending packet:\n" );
   synproto_dump( out_buf, out_pos + 4 );

   if( force_sz ) {  
      fprintf( g_dbg, "buf_sz_p (%p): %u\n", buf_sz_p, out_pos );
      *buf_sz_p = swap_32( 4 );
   } else {
      fprintf( g_dbg, "buf_sz_p (%p): %u\n", buf_sz_p, out_pos );
      *buf_sz_p = swap_32( out_pos );
   }

   send( sockfd, out_buf, out_pos + 4, 0 );

   return out_pos;
}

void synproto_parse( int sockfd, const char* pkt_buf, size_t pkt_buf_sz ) {
   uint32_t pkt_sz = 0,
      pkt_type = 0;
   uint16_t ver_maj = 0,
      ver_min = 0,
      mouse_x = 0,
      mouse_y = 0;

   /* src/lib/barrier/protocol_types.cpp */

   memcpy( &pkt_sz, pkt_buf, 4 );
   pkt_sz = swap_32( pkt_sz );
   
   memcpy( &pkt_type, &(pkt_buf[4]), 4 );
   pkt_type = swap_32( pkt_type );

   fprintf( g_dbg, "pkt type: %d\n", pkt_type );

   switch( pkt_type ) {

   case 1113682546: /* "Barr" */

      /* Grab the version out of the server packet. */
      memcpy( &ver_maj, &(pkt_buf[11]), 2 );
      ver_maj = swap_16( ver_maj );

      memcpy( &ver_min, &(pkt_buf[13]), 2 );
      ver_min = swap_16( ver_min );

      fprintf( g_dbg, "%u barrier %u.%u found\n", pkt_sz, ver_maj, ver_min );

      /* Send an acknowledgement and our name. */
      synproto_send(
         sockfd, 0, "Barrier%2i%2i%4i%s", ver_maj, ver_min, 7, "mintest" );
      
      break;

   case 1363758662: /* QINF */
      
      synproto_send(
         sockfd, 0, "DINF%2i%2i%2i%2i%2i%2i%2i",
         0, 0, 640, 480, 0, 0, 0 );

      break;

   case 1128874315: /* CIAK */
      fprintf( g_dbg, "CIAK?\n" );
      break;

   case 1129467728: /* CROP */
      fprintf( g_dbg, "CROP?\n" );
      break;

   case 1128352854: /* "CALV" */
      fprintf( g_dbg, "Ping... Pong!\n" );
      synproto_send( sockfd, 4, "CALV%4iCNOP", 4 );
      break;

   case 1128877646: /* "CINN" */
      fprintf( g_dbg, "in!\n" );
      break;

   case 1145261136: /* DCLP */
      fprintf( g_dbg, "clip in!\n" );
      break;

   case 1145916758: /* DMMV */

      memcpy( &mouse_x, &(pkt_buf[8]), 2 );
      mouse_x = swap_16( mouse_x );

      memcpy( &mouse_y, &(pkt_buf[10]), 2 );
      mouse_y = swap_16( mouse_y );

      fprintf( g_dbg, "mmv: %u, %u\n", mouse_x, mouse_y );

      osio_mouse_move( mouse_x, mouse_y );
      break;

   case 1145914446: /* DMDN */
      osio_mouse_down( mouse_x, mouse_y, pkt_buf[8] );
      break;

   case 1145918800: /* DMUP */
      osio_mouse_up( mouse_x, mouse_y, pkt_buf[8] );
      break;

   case 1129272660: /* COUT */
      fprintf( g_dbg, "out!\n" );
      break;

   case 1161969988: /* "EBAD" */

   default:
      /* Not found? Then what *did* we get? */
      fprintf( g_dbg, "invalid packet:\n" );
      synproto_dump( pkt_buf, pkt_buf_sz );

      /* goto cleanup; */
   }

}

