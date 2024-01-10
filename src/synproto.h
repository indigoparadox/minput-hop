
#ifndef SYNPROTO_H
#define SYNPROTO_H

/**
 * \file synproto.h
 * \brief Tools for dealing with the Synergy protocol.
 */

#include "intplat.h"

#ifdef MINPUT_OS_WIN32
#  include <winsock.h>
#  include <windows.h>
#elif defined MINPUT_OS_WIN16
#  include <winsock.h>
#  include <windows.h>
#else
#  include <stddef.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <errno.h>
#endif /* MINPUT_OS_WIN */

#include "minhop.h"

#include <stdarg.h>

#ifndef SOCKBUF_SZ
/*! \brief Size of the receive protocol buffer in bytes. */
#  define SOCKBUF_SZ 4096
#endif /* !SOCKBUF_SZ */

#ifndef SYNPROTO_TIMEOUT_MS
/*! \brief Timeout to wait between keepalives before disconnecting. */
#  define SYNPROTO_TIMEOUT_MS 5000
#endif /* !SYNPROTO_TIMEOUT_MS */

#ifdef SYNPROTO_ENDIAN
#  define swap_32( num ) (num)
#  define swap_16( num ) (num)
#else

/**
 * \brief Swap byte order in a 32-bit integer from network to native.
 */
#define swap_32( num ) ((((num)>>24)&0xff) | (((num)<<8)&0xff0000) | (((num)>>8)&0xff00) | (((num)<<24)&0xff000000))

/**
 * \brief Swap byte order in a 16-bit integer from network to native.
 */
#define swap_16( num ) (((num)>>8) | ((num)<<8))

#endif

/**
 * \brief Perform endianness swap on a 32-bit integer from a buffer if needed.
 * \param buffer Buffer to pull integer out of.
 * \param offset Offset of the buffer (in bytes) of the integer to pull.
 */
#define synproto_exval_32( buffer, offset ) \
   (swap_32( *((uint32_t*)&(buffer[offset])) ))

/**
 * \brief Perform endianness swap on a 16-bit integer from a buffer if needed.
 * \param buffer Buffer to pull integer out of.
 * \param offset Offset of the buffer (in bytes) of the integer to pull.
 */
#define synproto_exval_16( buffer, offset ) \
   (swap_16( *((uint16_t*)&(buffer[offset])) ))

/**
 * \brief Given a buffer with a string from the Synergy server, dump it into
 *        a more human-readable form.
 */
void synproto_dump( const char* buf, size_t buf_sz );

/**
 * \brief Given a format string and tokens, write those tokens into a buffer
 *        using specific tokens listed here.
 * \param buf Buffer to write the formatted response to.
 * \param buf_sz Size of buf in bytes.
 * \param fmt Format string using the tokens below.
 * \param vargs A va_list of tokens already initialized with va_start().
 * \return The size of the string written to buf in bytes.
 *
 * | Token | Specifies                                                  |
 * |-------|------------------------------------------------------------|
 * | %%ni  | Where n is a single-digit number, an integer n-bytes wide. |
 * | %%s   | A null-terminated string.                                  |
 *
 */
size_t synproto_vprintf(
   char* buf, size_t buf_sz, const char* fmt, va_list vargs );

/**
 * \brief Send a formatted string to the Synergy server with tokens as
 *        specified for synproto_vprintf().
 * \param sockfd The socket identifier/handle on which the Synergy server is
 *               connected.
 * \param force_sz Force the preliminary size of the response packet to this
 *                 number. If 0, the number is calculated automatically.
 * \param fmt Format string using the tokens from synproto_vprintf().
 */
uint32_t synproto_send( int sockfd, uint8_t force_sz, const char* fmt, ... );

/**
 * \brief Given a buffer with a string from the Synergy server, parse it
 *        and respond or simulate OS activity as directed.
 * \param pkt_buf The buffer in which the server transmission is stored.
 * \param pkt_buf_sz The size of pkt_buf.
 */
int synproto_parse_and_reply(
   struct MINHOP_CFG* config, const char* pkt_buf, size_t pkt_buf_sz );

#endif /* SYNPROTO_H */

