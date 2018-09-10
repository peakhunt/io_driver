#ifndef __IO_DNS_DEF_H__
#define __IO_DNS_DEF_H__

#include "io_net.h"
#include "io_timer.h"

#define IO_DNS_RX_BUF_SIZE                256
#define IO_DNS_MAX_HOST_ADDRESSES         4

struct __io_dns_t;
typedef struct __io_dns_t io_dns_t;

typedef enum
{
  io_dns_event_got_result,
  io_dns_event_error,
  io_dns_event_timedout,
} io_dns_event_enum_t;

typedef struct
{
  uint8_t     n_addrs;
  uint8_t     addrs[IO_DNS_MAX_HOST_ADDRESSES][4];
} io_dns_host_addrs_t;

typedef struct
{
  io_dns_event_enum_t   evt_type;
  union
  {
    io_dns_host_addrs_t*  addrs;
  };
} io_dns_event_t;

typedef struct
{
  char*     server;
  int       port;
  int       timeout;      // in second
} io_dns_cfg_t;

typedef void (*io_dns_callback)(io_dns_t* d, io_dns_event_t* e);

struct __io_dns_t
{
  io_dns_callback   cb;
  io_timer_t*       t;
  io_net_t          n;
  SoftTimerElem     timer;
  uint8_t           rx_buf[IO_DNS_RX_BUF_SIZE];

  int               qlen;
};

extern int io_dns_lookup_ipv4(io_driver_t* driver, io_timer_t* t, io_dns_t* d, const io_dns_cfg_t* cfg,
    const char* target, io_dns_callback cb);

#endif /* !__IO_DNS_DEF_H__ */
