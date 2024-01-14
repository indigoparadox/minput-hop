
#ifndef OLDC_H
#define OLDC_H

#ifdef MINPUT_NO_PRINTF

union FMT_SPEC {
   long d;
   unsigned long u;
   char c;
   void* p;
   char* s;
};

static void vsnprintf(
   const char* buffer, size_t buffer_sz, const char* fmt, va_list args
) {
   size_t i = 0,
      i_out = 0;
   char last = '\0';
   int spec_fmt = 0;
   union FMT_SPEC spec;
   char itoa_buf[OSIO_NUM_BUFFER_SZ + 1];
   char c;

#  define SPEC_FMT_LONG 1
#  define SPEC_FMT_UNSIGNED 2

   memset( itoa_buf, '\0', OSIO_NUM_BUFFER_SZ + 1 );

   /* Roughly adapted from uprintf from maug. */

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
}

#endif /* MINPUT_NO_PRINTF */

#endif /* !OLDC_H */

