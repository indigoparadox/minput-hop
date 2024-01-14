
#ifndef MINHOP_H
#define MINHOP_H

/**
 * \file netio.h
 * \brief Contains network-transported-related functions and structs.
 */

#ifndef SOCKBUF_SZ
/*! \brief Size of the receive protocol buffer in bytes. */
#  define SOCKBUF_SZ 2048
#endif /* !SOCKBUF_SZ */

#include "minput.h"

int netio_setup( struct NETIO_CFG* config );

int netio_connect( struct NETIO_CFG* config );

int netio_process_packets( struct NETIO_CFG* config );

void netio_disconnect( struct NETIO_CFG* config );

void netio_cleanup();

#endif /* !MINHOP_H */

