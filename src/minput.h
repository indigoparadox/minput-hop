
#ifndef MINPUT_H
#define MINPUT_H

/**
 * \addtogroup minput_error_codes Error Codes
 * \{
 */

#define MINHOP_ERR_RECV 2
#define MINHOP_ERR_PROTOCOL 4
#define MINHOP_ERR_OVERFLOW 8
#define MINHOP_ERR_OS 16
#define MINHOP_ERR_ARGS 32
#define MINHOP_ERR_ALLOC 64
#define MINHOP_ERR_CONNECT 128

#define MINPUT_STAT_DEBUG 1
#define MINPUT_STAT_ERROR 2
#define MINPUT_STAT_INFO  3

/*! \} */ /* minput_error_codes */

/**
 * \addtogroup minput_constants Constants
 * \{
 */
#define SERVER_ADDR_SZ_MAX 64
#define CLIENT_NAME_SZ_MAX SERVER_ADDR_SZ_MAX

/* \} */ /* minput_constants */

#if defined( MINPUT_OS_WIN16 )
#  include <winsock.h>
#  include <windows.h>
#  include <mmsystem.h>
#  include <stdio.h> /* FILE */
#  include <string.h> /* strlen, memset */
#  include <stdlib.h> /* atoi */
#  include <stdarg.h>
#  define assert( cond )
#elif defined( MINPUT_OS_WIN32 )
#  include <winsock.h>
#  include <windows.h>
#  include <mmsystem.h>
#  include <shellapi.h>
#  ifdef DEBUG
#     include <assert.h>
#  endif /* DEBUG */
#  include <stdio.h> /* FILE */
#  include <string.h> /* strlen, memset */
#  include <stdlib.h> /* atoi */
#  include <stdarg.h>
#else
#  include <stddef.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <errno.h>
#  include <stdarg.h>
#  include <stdio.h>
#  include <string.h>
#  include <stdlib.h>
#  include <sys/time.h>
#  ifdef DEBUG
#     include <assert.h>
#  endif /* DEBUG */
#  include <unistd.h>
#endif

#include "intplat.h"

struct NETIO_CFG {
   char server_addr[SERVER_ADDR_SZ_MAX];
/*! \brief The name of this client while connecting to the server. */
   char client_name[CLIENT_NAME_SZ_MAX];
   int server_port;
/**
 * \brief The socket identifier/handle on which the Synergy server is
 *        connected.
 */
   int socket_fd;
/**
 * \brief The next osio_get_time() in ms by which a keepalive must be
 *        received from the server in order for us not to disconnect.
 */
   uint32_t calv_deadline;
   char* pkt_buf;
   uint32_t pkt_buf_sz;
   uint32_t pkt_buf_sz_max;
};

int minput_loop_iter( struct NETIO_CFG* config );

#include "netio.h"
#include "osio.h"
#include "synproto.h"

#endif /* !MINPUT_H */

