#ifndef __IO_NET_DEF_H__
#define __IO_NET_DEF_H__

#include <netinet/in.h>
#include <arpa/inet.h>

#include "io_driver.h"

#define IO_NET_EVENT_CONNECTED          1
#define IO_NET_EVENT_RX                 2
#define IO_NET_EVENT_CLOSED             3

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

typedef void (*io_net_callback)(io_net_t* n, io_net_event_t* e);
typedef io_net_t* (*io_net_alloc)(io_net_t* n);

struct __io_net_t
{
  int                   sd;
  io_net_callback       cb;

  io_net_alloc        alloc;
  uint8_t*            rx_buf;
  int                 rx_size;

  io_driver_watcher_t   watcher;
  io_driver_t*          driver;
};

extern int io_net_bind(io_driver_t* driver, io_net_t* n, int port, io_net_callback cb, io_net_alloc alloc,
    uint8_t* rx_buf, int rx_size);
extern int io_net_connect(io_driver_t* driver, io_net_t* n, const char* ip_addr, int port, io_net_callback cb,
    uint8_t* rx_buf, int rx_size);
extern void io_net_close(io_net_t* n);
extern int io_net_tx(io_net_t* n, uint8_t* buf, int len);

extern int io_net_udp(io_driver_t* driver, io_net_t* n, int port, uint8_t* rx_buf, int rx_size, io_net_callback cb);
extern int io_net_tx_udp(io_net_t* n, struct sockaddr_in* to, uint8_t* buf, int len);

#endif /* !__IO_NET_DEF_H__ */
