//
// XXX
//
// DO NOT USE THIS.
// ALL THE TLS CODES ARE INTEGRATED INTO io_net NOW.
// THIS IS PURELY KEPT FOR REFERENCE PURPOSE.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>

#include "io_ssl.h"

#define DEBUG_LEVEL     0

static const char* TAG = "io_ssl";
static const char* pers = "io_ssl_server";

static inline void
sock_util_put_nonblock(int sd)
{
  const int            const_int_1 = 1;

  ioctl(sd, FIONBIO, (char*)&const_int_1);
}

static void my_debug( void *ctx, int level,
                      const char *file, int line,
                      const char *str )
{
  ((void) level);

  mbedtls_fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
  fflush(  (FILE *) ctx  );
}


///////////////////////////////////////////////////////////////////////////////
//
// utilities
//
///////////////////////////////////////////////////////////////////////////////
static int
io_ssl_mbedtls_init_server(io_ssl_t* s)
{
  int ret;

  mbedtls_net_init(&s->mbed_fd);
  mbedtls_ssl_init(&s->ssl);
  mbedtls_ssl_config_init(&s->conf);
  mbedtls_entropy_init(&s->entropy);
  mbedtls_pk_init(&s->pkey);
  mbedtls_x509_crt_init(&s->cacert);
  mbedtls_ctr_drbg_init(&s->ctr_drbg);

  ret = mbedtls_ctr_drbg_seed(&s->ctr_drbg, mbedtls_entropy_func, &s->entropy,
      (const uint8_t*)pers, strlen(pers));
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_ctr_drbg_seed %d\n", ret);
    return -1;
  }

  ret = mbedtls_x509_crt_parse( &s->cacert, (const unsigned char *) mbedtls_test_srv_crt,
      mbedtls_test_srv_crt_len );
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_x509_crt_parse returned %d\n", ret);
    return -1;
  }

  ret = mbedtls_x509_crt_parse(&s->cacert, (const unsigned char *) mbedtls_test_cas_pem,
      mbedtls_test_cas_pem_len );
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_x509_crt_parse returned %d\n", ret);
    return -1;
  }

  ret =  mbedtls_pk_parse_key(&s->pkey, (const unsigned char *) mbedtls_test_srv_key,
      mbedtls_test_srv_key_len, NULL, 0 );
  if(ret != 0)
  {
    LOGE(TAG, "failed!  mbedtls_pk_parse_key returned %d\n", ret);
    return -1;
  }

  ret = mbedtls_ssl_config_defaults(&s->conf,
      MBEDTLS_SSL_IS_SERVER,
      MBEDTLS_SSL_TRANSPORT_STREAM,
      MBEDTLS_SSL_PRESET_DEFAULT);
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_ssl_config_defaults %d\n", ret);
    return -1;
  }

  mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &s->ctr_drbg);
  mbedtls_ssl_conf_dbg(&s->conf, my_debug, stdout);   // FIXME
  mbedtls_ssl_conf_ca_chain(&s->conf, s->cacert.next, NULL);

  ret = mbedtls_ssl_conf_own_cert(&s->conf, &s->cacert, &s->pkey);
  if(ret != 0)
  {
    LOGE(TAG, "failed!  mbedtls_ssl_conf_own_cert returned %d\n", ret);
    return -1;
  }

  ret = mbedtls_ssl_setup(&s->ssl, &s->conf);
  if(ret != 0)
  {
    LOGE(TAG, "mbedtls_ssl_setup failed %d\n", ret);
    return -1;
  }

  s->handshaking = FALSE;

  return 0;
}

static int
io_ssl_mbedtls_init_client(io_ssl_t* s)
{
  int ret;

  mbedtls_net_init(&s->mbed_fd);
  mbedtls_ssl_init(&s->ssl);
  mbedtls_ssl_config_init(&s->conf);
  mbedtls_entropy_init(&s->entropy);
  mbedtls_pk_init(&s->pkey);
  mbedtls_x509_crt_init(&s->cacert);
  mbedtls_ctr_drbg_init(&s->ctr_drbg);

  ret = mbedtls_ctr_drbg_seed(&s->ctr_drbg, mbedtls_entropy_func, &s->entropy,
      (const uint8_t*)pers, strlen(pers));
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_ctr_drbg_seed %d\n", ret);
    return -1;
  }

  ret = mbedtls_x509_crt_parse(&s->cacert, (const unsigned char *) mbedtls_test_cas_pem,
      mbedtls_test_cas_pem_len );
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_x509_crt_parse returned %d\n", ret);
    return -1;
  }

  ret = mbedtls_ssl_config_defaults(&s->conf,
      MBEDTLS_SSL_IS_CLIENT,
      MBEDTLS_SSL_TRANSPORT_STREAM,
      MBEDTLS_SSL_PRESET_DEFAULT);
  if(ret != 0)
  {
    LOGE(TAG, "failed! mbedtls_ssl_config_defaults %d\n", ret);
    return -1;
  }

  mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_ca_chain(&s->conf, &s->cacert, NULL );
  mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &s->ctr_drbg );

  ret = mbedtls_ssl_setup(&s->ssl, &s->conf);
  if(ret != 0)
  {
    LOGE(TAG, "mbedtls_ssl_setup failed %d\n", ret);
    return -1;
  }

  mbedtls_ssl_set_bio(&s->ssl, &s->mbed_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

  s->handshaking = FALSE;
  return 0;
}

static int
io_ssl_mbedtls_init_accepted(io_ssl_t* l, io_ssl_t* s)
{
  int ret;

  mbedtls_net_init(&s->mbed_fd);
  mbedtls_ssl_init(&s->ssl);
  mbedtls_ssl_config_init(&s->conf);
  mbedtls_entropy_init(&s->entropy);
  mbedtls_pk_init(&s->pkey);
  mbedtls_x509_crt_init(&s->cacert);
  mbedtls_ctr_drbg_init(&s->ctr_drbg);

  s->mbed_fd.fd = s->sd;

  ret = mbedtls_ctr_drbg_seed(&s->ctr_drbg, mbedtls_entropy_func, &s->entropy,
      (const uint8_t*)pers, strlen(pers));
  if(ret != 0)
  {
    LOGE(TAG, "mbedtls_ctr_drbg_reseed failed %d\n", ret);
    return -1;
  }

  ret = mbedtls_ssl_setup(&s->ssl, &l->conf);
  if(ret != 0)
  {
    LOGE(TAG, "mbedtls_ssl_setup failed %d\n", ret);
    return -1;
  }

  mbedtls_ssl_set_bio(&s->ssl, &s->mbed_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

  s->handshaking = FALSE;
  return 0;
}

static void
io_ssl_mbedtls_deinit(io_ssl_t* s)
{
  mbedtls_net_free(&s->mbed_fd);
  mbedtls_x509_crt_free(&s->cacert);
  mbedtls_pk_free(&s->pkey);
  mbedtls_ssl_free(&s->ssl);
  mbedtls_ssl_config_free(&s->conf);
  mbedtls_ctr_drbg_free(&s->ctr_drbg);
  mbedtls_entropy_free(&s->entropy);
}

///////////////////////////////////////////////////////////////////////////////
//
// callbacks
//
///////////////////////////////////////////////////////////////////////////////
static void
io_ssl_generic_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_ssl_t*       s = container_of(w, io_ssl_t, watcher);
  int             ret;
  io_ssl_event_t  ev;

  if((e & IO_DRIVER_EVENT_RX))
  {
    ret = mbedtls_ssl_read(&s->ssl, s->rx_buf, s->rx_size);
    if(ret <= 0)
    {
      switch(ret)
      {
      case MBEDTLS_ERR_SSL_WANT_READ:
        break;

      case MBEDTLS_ERR_SSL_WANT_WRITE:
        LOGI(TAG, "%s activating TX event\n", __func__);
        io_driver_watch(s->driver, &s->watcher, IO_DRIVER_EVENT_TX);
        return;

      case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
      case MBEDTLS_ERR_NET_CONN_RESET:
      default:
        LOGE(TAG, "ssl connection error %x\n", -ret);
        ev.ev = io_net_event_enum_closed;
        s->cb(s, &ev);
        return;
      }
    }
    else
    {
      ev.ev = io_net_event_enum_rx;
      ev.r.buf = s->rx_buf;
      ev.r.len = (uint32_t)ret;
      if(s->cb(s, &ev) == io_net_return_stop)
      {
        return;
      }
    }
  }

  if((e & IO_DRIVER_EVENT_TX))
  {
    //
    // XXX
    // what if mbedtls wants to tx? How should I notify mbedtls of tx availability???
    //
    LOGI(TAG, "%s deactivating TX event\n", __func__);
    io_driver_no_watch(s->driver, &s->watcher, IO_DRIVER_EVENT_TX);

    ev.ev = io_net_event_enum_tx;
    s->cb(s, &ev);
  }
}

static void
io_ssl_handshake_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_ssl_t*       s = container_of(w, io_ssl_t, watcher);
  int             ret;
  io_ssl_event_t  ev;

  // blindly disable TX that might have been set
  io_driver_no_watch(s->driver, &s->watcher, IO_DRIVER_EVENT_TX);

  ret = mbedtls_ssl_handshake(&s->ssl);
  switch(ret)
  {
  case 0:   // handshake done
    LOGI(TAG, "handshake done\n");
    s->handshaking = FALSE;

    io_driver_watcher_set_cb(&s->watcher, io_ssl_generic_callback);

    ev.ev = io_net_event_enum_handshaken;
    if(s->cb(s, &ev) != io_net_return_stop)
    {
      io_ssl_generic_callback(w, e);
    }
    break;

  case MBEDTLS_ERR_SSL_WANT_READ:
    // RX watch is always enabled
    break;

  case MBEDTLS_ERR_SSL_WANT_WRITE:
    LOGI(TAG, "%s activating TX event\n", __func__);
    io_driver_watch(s->driver, &s->watcher, IO_DRIVER_EVENT_TX);
    break;

  default:  // error
    LOGE(TAG, "handshake failed %x\n", -ret);
    ev.ev = io_net_event_enum_closed;
    s->cb(s, &ev);
    break;
  }
}

static void
io_ssl_accept_callback(io_driver_watcher_t* w, io_driver_event e)
{
  int                     newsd;
  io_ssl_t*               l = container_of(w, io_ssl_t, watcher);
  io_ssl_t*               n;
  struct sockaddr_in      from;
  socklen_t               from_len;
  io_ssl_event_t          ev;

  if(e != IO_DRIVER_EVENT_RX)
  {
    LOGE(TAG, "%s spurious event %d\n", __func__, e);
    return;
  }

  from_len = sizeof(from);

  newsd = accept(l->sd, (struct sockaddr*)&from, &from_len);
  if(newsd <0)
  {
    LOGE(TAG, "%s accept failed\n", __func__);
    return;
  }

  ev.ev = io_net_event_enum_alloc_connection;
  ev.n  = NULL;
  ev.from = &from;

  if(l->cb(l, &ev) ==  io_net_return_stop)
  {
    LOGE(TAG, "alloc cancelled\n");
    close(newsd);
    return;
  }

  sock_util_put_nonblock(newsd);

  n = ev.n;

  n->sd       = newsd;
  n->cb       = l->cb;
  n->driver   = l->driver;

  io_ssl_mbedtls_init_accepted(l, n);
  n->handshaking = TRUE;

  io_driver_watcher_init(&n->watcher, newsd, io_ssl_handshake_callback);
  io_driver_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_RX);

  ev.ev = io_net_event_enum_connected;
  ev.n  = NULL;
  ev.from = &from;

  n->cb(n, &ev);
}

static void
io_ssl_connect_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_ssl_t*               n = container_of(w, io_ssl_t, watcher);
  int                     err;
  socklen_t               len = sizeof(err);
  io_ssl_event_t          ev;

  if(e != IO_DRIVER_EVENT_TX)
  {
    LOGE(TAG, "%s spurious event %d\n", __func__, e);
    return;
  }

  getsockopt(n->sd, SOL_SOCKET, SO_ERROR, &err, &len);

  if(err != 0)
  {
    // connect failed
    ev.ev = io_net_event_enum_closed;
  }
  else
  {
    // connect success
    io_driver_watcher_set_cb(&n->watcher, io_ssl_generic_callback);

    ev.ev = io_net_event_enum_connected;
    io_driver_no_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);
    io_driver_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_RX);
  }

  n->handshaking = TRUE;
  io_driver_watcher_set_cb(&n->watcher, io_ssl_handshake_callback);

  ev.n    = n;
  n->cb(n, &ev);

  // initiate handshake
  io_ssl_handshake_callback(w, 0);
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
int
io_ssl_bind(io_driver_t* driver, io_ssl_t* s, int port, io_ssl_callback cb)
{
  int                   sd;
  const int             on = 1;
  struct sockaddr_in    addr;

  if(io_ssl_mbedtls_init_server(s) != 0)
  {
    return -1;
  }

  sd = socket(AF_INET, SOCK_STREAM, 0);
  if(sd < 0)
  {
    LOGE(TAG, "%s socket failed\n", __func__);
    goto socket_failed;
  }

  sock_util_put_nonblock(sd);
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family       = AF_INET;
  addr.sin_addr.s_addr  = INADDR_ANY;
  addr.sin_port         = htons(port);

  if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
  {
    LOGE(TAG, "%s failed to bind %d\n", __func__, port);
    goto bind_failed;
  }

  listen(sd, 5);

  s->sd     = sd;
  s->cb     = cb;
  s->driver = driver;

  io_driver_watcher_init(&s->watcher, sd, io_ssl_accept_callback);
  io_driver_watch(driver, &s->watcher, IO_DRIVER_EVENT_RX);

  return 0;

bind_failed:
  close(sd);

socket_failed:
  io_ssl_mbedtls_deinit(s);
  return -1;
}

int
io_ssl_connect(io_driver_t* driver, io_ssl_t* s, const char* ip_addr, int port, io_ssl_callback cb)
{
  int                 sd;
  struct sockaddr_in  to;

  if(io_ssl_mbedtls_init_client(s) != 0)
  {
    return -1;
  }

  sd = socket(AF_INET, SOCK_STREAM, 0);
  if(sd < 0)
  {
    LOGE(TAG, "%s socket failed\n", __func__);
    goto socket_failed;
  }

  sock_util_put_nonblock(sd);

  memset(&to, 0, sizeof(to));
  to.sin_family       = AF_INET;
  to.sin_addr.s_addr  = inet_addr(ip_addr);
  to.sin_port         = htons(port);

  s->sd       = sd;
  s->cb       = cb;
  s->driver   = driver;

  io_driver_watcher_init(&s->watcher, sd, io_ssl_connect_callback);
  io_driver_watch(driver, &s->watcher, IO_DRIVER_EVENT_TX);

  s->mbed_fd.fd = s->sd;

  //
  // don't care about return value here
  // anyway any error will be detected at the next loop
  //
  connect(sd, (struct sockaddr*)&to, sizeof(to));

  return 0;

socket_failed:
  io_ssl_mbedtls_deinit(s);
  return -1;
}

int
io_ssl_tx(io_ssl_t* s, uint8_t* buf, int len)
{
  int ret;

  if(s->handshaking)
  {
    LOGE(TAG,"%s called during handshaking\n", __func__);
    return -1;
  }

  ret = mbedtls_ssl_write(&s->ssl, buf, len);
  if(ret <= 0)
  {
    if(ret != MBEDTLS_ERR_SSL_WANT_WRITE)
    {
      return -1;
    }
    LOGI(TAG, "%s activating TX event\n", __func__);
    io_driver_watch(s->driver, &s->watcher, IO_DRIVER_EVENT_TX);
  }
  return ret;
}

void
io_ssl_close(io_ssl_t* s)
{
  io_ssl_mbedtls_deinit(s);
  io_driver_no_watch(s->driver,
      &s->watcher,
      IO_DRIVER_EVENT_RX | IO_DRIVER_EVENT_TX | IO_DRIVER_EVENT_EX);
  close(s->sd);
}
