
#ifndef MINHOP_H
#define MINHOP_H

/**
 * \file minhop.h
 * \brief Contains network-transported-related functions and structs.
 */

#ifndef SOCKBUF_SZ
/*! \brief Size of the receive protocol buffer in bytes. */
#  define SOCKBUF_SZ 4096
#endif /* !SOCKBUF_SZ */

#include "minput.h"

int minhop_parse_args( int argc, char* argv[], struct NETIO_CFG* config );

int netio_setup( struct NETIO_CFG* config );

int netio_connect( struct NETIO_CFG* config );

int minhop_process_packets(
   struct NETIO_CFG* config, char* pkt_buf, uint32_t* p_pkt_buf_sz );

void netio_disconnect( struct NETIO_CFG* config );

#endif /* !MINHOP_H */

