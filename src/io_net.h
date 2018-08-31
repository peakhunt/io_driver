#ifndef __IO_NET_DEF_H__
#define __IO_NET_DEF_H__

#include <netinet/in.h>
#include <arpa/inet.h>

#include "io_driver.h"
#include "circ_buffer.h"

#define IO_NET_EVENT_CONNECTED          1
#define IO_NET_EVENT_RX                 2
#define IO_NET_EVENT_CLOSED             3

#define IO_NET_FLAGS_MARK_DELETED       0x01
#define IO_NET_FLAGS_EXECUTING          0x02

struct __io_net_t;
typedef struct __io_net_t io_net_t;

typedef struct
{
  int       ev;
  union
  {
    io_net_t*     n;        // in case of accept
    struct                  // in case of RX 
    {
      uint8_t*    buf;      // rx buffer
      uint32_t    len;      // data length in rx buffer

      struct sockaddr_in*  from;
    } r;
  };
} io_net_event_t;

//
// returns 0: continue
// returns -1: stop
//
typedef void (*io_net_callback)(io_net_t* n, io_net_event_t* e);
typedef void (*io_net_event_exit)(io_net_t* n);

struct __io_net_t
{
  int                   sd;
  io_net_callback       cb;
  io_driver_watcher_t   watcher;
  io_driver_t*          driver;

  io_driver_callback    target;
  io_net_event_exit     exit;

  uint32_t              flags;

  ////////////////////////////////////////////
  // XXX
  // these should be set by user
  //
  ////////////////////////////////////////////
  uint8_t*              rx_buf;
  int                   rx_size;
  circ_buffer_t*        tx_buf;
  void*                 priv;             // for protocols above net
  void*                 user_priv;        // for end users
};

extern void io_net_init(void);

extern io_net_t* io_net_bind(io_driver_t* driver, int port, io_net_callback cb);
extern io_net_t* io_net_connect(io_driver_t* driver, const char* ip_addr, int port, io_net_callback cb);
extern void io_net_close(io_net_t* n);

extern int io_net_tx(io_net_t* n, uint8_t* buf, int len);

extern int io_net_udp(io_driver_t* driver, io_net_t* n, int port, io_net_callback cb);
extern int io_net_udp_tx(io_net_t* n, struct sockaddr_in* to, uint8_t* buf, int len);

static inline void
io_net_set_rx_buf(io_net_t* n, uint8_t* rx_buf, int rx_size)
{
  n->rx_buf   = rx_buf;
  n->rx_size  = rx_size;
}

static inline void
io_net_set_tx_circ_buf(io_net_t* n, circ_buffer_t* cb)
{
  n->tx_buf = cb;
}

static inline uint8_t
io_net_is_mark_deleted(io_net_t* n)
{
  if((n->flags & IO_NET_FLAGS_MARK_DELETED))
  {
    return TRUE;
  }
  return FALSE;
}

static inline uint8_t
io_net_is_in_callback(io_net_t* n)
{
  if((n->flags & IO_NET_FLAGS_EXECUTING))
  {
    return TRUE;
  }
  return FALSE;
}

#endif /* !__IO_NET_DEF_H__ */
