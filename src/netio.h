
#ifndef MINHOP_H
#define MINHOP_H

/**
 * \file minhop.h
 * \brief Contains network-transported-related functions and structs.
 */

#define SERVER_ADDR_SZ_MAX 64
#define CLIENT_NAME_SZ_MAX 64

#include "minerr.h"

#include "intplat.h"

struct MINHOP_CFG {
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
};

#include "osio.h"
#include "synproto.h"

int minhop_parse_args( int argc, char* argv[], struct MINHOP_CFG* config );

int netio_setup( struct MINHOP_CFG* config );

int netio_connect( struct MINHOP_CFG* config );

int minhop_process_packets(
   struct MINHOP_CFG* config, char* pkt_buf, uint32_t* p_pkt_buf_sz );

void netio_disconnect( struct MINHOP_CFG* config );

#endif /* !MINHOP_H */
