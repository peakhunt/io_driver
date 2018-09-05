#ifndef __IO_NET_DEF_H__
#define __IO_NET_DEF_H__

#include <netinet/in.h>
#include <arpa/inet.h>

#include "io_driver.h"

typedef enum
{
  io_net_event_enum_alloc_connection,
  io_net_event_enum_connected,
  io_net_event_enum_rx,
  io_net_event_enum_tx,
  io_net_event_enum_closed,
} io_net_event_enum_t;

struct __io_net_t;
typedef struct __io_net_t io_net_t;

typedef struct
{
  io_net_event_enum_t     ev;
  union
  {
    io_net_t*     n;        // in case of accept. n should be set by user
    struct                  // in case of RX 
    {
      uint8_t*    buf;      // rx buffer
      uint32_t    len;      // data length in rx buffer
    } r;
  };
  struct sockaddr_in*  from;
} io_net_event_t;

typedef enum
{
  io_net_return_continue,
  io_net_return_stop,
} io_net_return_t;

typedef io_net_return_t (*io_net_callback)(io_net_t* n, io_net_event_t* e);

struct __io_net_t
{
  int                   sd;
  io_net_callback       cb;
  io_driver_watcher_t   watcher;
  io_driver_t*          driver;

  ////////////////////////////////////////////
  // XXX
  // these should be set by user
  //
  ////////////////////////////////////////////
  uint8_t*              rx_buf;
  int                   rx_size;
};

extern int io_net_bind(io_driver_t* driver, io_net_t* n, int port, io_net_callback cb);
extern int io_net_connect(io_driver_t* driver, io_net_t* n, const char* ip_addr, int port, io_net_callback cb);
extern void io_net_close(io_net_t* n);

extern int io_net_tx(io_net_t* n, uint8_t* buf, int len);

static inline void
io_net_set_rx_buf(io_net_t* n, uint8_t* rx_buf, int rx_size)
{
  n->rx_buf   = rx_buf;
  n->rx_size  = rx_size;
}

#endif /* !__IO_NET_DEF_H__ */
