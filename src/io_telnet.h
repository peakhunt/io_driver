#ifndef __IO_TELNET_DEF_H__
#define __IO_TELNET_DEF_H__

#include "io_net.h"
#include "telnet_reader.h"

#define IO_TELNET_RX_BUF_SIZE       128
#define IO_TELNET_TX_BUF_SIZE       256

struct __io_telnet_t;
typedef struct __io_telnet_t io_telnet_t;

typedef struct
{
  io_net_event_enum_t     ev;
  union
  {
    io_telnet_t*     n;     // in case of accept. n should be set by user
    struct                  // in case of RX 
    {
      uint8_t*    buf;      // rx buffer
      uint32_t    len;      // data length in rx buffer
    } r;
  };
  struct sockaddr_in*  from;
} io_telnet_event_t;

typedef io_net_return_t (*io_telnet_callback)(io_telnet_t* n, io_telnet_event_t* e);

struct __io_telnet_t
{
  io_telnet_callback  cb;
  io_net_t            n;
  telnet_reader_t     treader;
  uint8_t             rx_buf[IO_TELNET_RX_BUF_SIZE];
  uint8_t             tx_buf[IO_TELNET_TX_BUF_SIZE];
  circ_buffer_t       txcb;
};


extern int io_telnet_bind(io_driver_t* driver, io_telnet_t* t, int port, io_telnet_callback cb);
extern void io_telnet_close(io_telnet_t* t);
extern int io_telnet_tx(io_telnet_t* t, uint8_t* buf, int len);
extern int io_telnet_connect(io_driver_t* driver, io_telnet_t* t, const char* ip_addr, int port, io_telnet_callback cb);

#endif /* !__IO_TELNET_DEF_H__ */
