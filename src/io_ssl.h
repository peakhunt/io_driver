#ifndef __IO_SSL_DEF_H__
#define __IO_SSL_DEF_H__

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_time            time
#define mbedtls_time_t          time_t
#define mbedtls_fprintf         fprintf
#define mbedtls_printf          printf
#define MBEDTLS_EXIT_SUCCESS    EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE    EXIT_FAILURE
#endif /* MBEDTLS_PLATFORM_C */

#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include <string.h>
#include "io_net.h"

#define IO_SSL_RX_BUF_SIZE            512
#define IO_SSL_TX_BUF_SIZE            512

struct __io_ssl_t;
typedef struct __io_ssl_t io_ssl_t;

typedef struct
{
  io_net_event_enum_t       ev;
  union
  {
    io_ssl_t*     n;
    struct
    {
      uint8_t*    buf;
      uint32_t    len;
    } r;
  };
  struct sockaddr_in*   from;
} io_ssl_event_t;

typedef io_net_return_t (*io_ssl_callback)(io_ssl_t* s, io_ssl_event_t* e);

struct __io_ssl_t
{
  int                       sd;
  io_ssl_callback           cb;
  io_driver_watcher_t       watcher;
  io_driver_t*              driver;

  mbedtls_net_context       mbed_fd;
  mbedtls_x509_crt          srvcert;
  mbedtls_ssl_config        conf;
  mbedtls_ssl_context       ssl;
  mbedtls_entropy_context   entropy;
  mbedtls_pk_context        pkey;
  mbedtls_ctr_drbg_context  ctr_drbg;

  uint8_t                   handshaking;

  uint8_t*              rx_buf;
  int                   rx_size;
};

extern int io_ssl_bind(io_driver_t* driver, io_ssl_t* s, int port, io_ssl_callback cb);
extern int io_ssl_tx(io_ssl_t* s, uint8_t* buf, int len);
extern void io_ssl_close(io_ssl_t* s);

static inline void
io_ssl_set_rx_buf(io_ssl_t* s, uint8_t* rx_buf, int rx_size)
{
  s->rx_buf   = rx_buf;
  s->rx_size  = rx_size;
}

#endif /* !__IO_SSL_DEF_H__ */
