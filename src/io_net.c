#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>

#include "io_net.h"

static const char* TAG  = "io_net";

///////////////////////////////////////////////////////////////////////////////
//
// socket related utilities
//
///////////////////////////////////////////////////////////////////////////////
static inline void
sock_util_put_nonblock(int sd)
{
  const int            const_int_1 = 1;

  ioctl(sd, FIONBIO, (char*)&const_int_1);
}

///////////////////////////////////////////////////////////////////////////////
//
// I/O driver callbacks
//
///////////////////////////////////////////////////////////////////////////////
static void
io_net_generic_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_net_t*       n = container_of(w, io_net_t, watcher);
  int             ret;
  io_net_event_t  ev;

  switch(e)
  {
  case IO_DRIVER_EVENT_RX:
    ret = read(n->sd, n->rx_buf, n->rx_size);
    if(ret <= 0)
    {
      ev.ev = IO_NET_EVENT_CLOSED;
    }
    else
    {
      ev.ev = IO_NET_EVENT_RX;
      ev.r.buf = n->rx_buf;
      ev.r.len = (uint32_t)ret;
    }
    n->cb(n, &ev);
    break;

  case IO_DRIVER_EVENT_TX:
  case IO_DRIVER_EVENT_EX:
    LOGE(TAG, "%s spurious event %d\n", __func__, e);
    break;
  }
}

static void
io_net_accept_callback(io_driver_watcher_t* w, io_driver_event e)
{
  int                     newsd;
  io_net_t*               l = container_of(w, io_net_t, watcher);
  io_net_t*               n;
  struct sockaddr_in      from;
  socklen_t               from_len;
  io_net_event_t          ev;

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

  n = l->alloc(l);
  if(n == NULL)
  {
    LOGE(TAG, "%s io_net_alloc failed\n", __func__);
    return;
  }

  sock_util_put_nonblock(newsd);

  n->sd       = newsd;
  n->cb       = l->cb;
  n->driver   = l->driver;
  n->rx_buf   = l->rx_buf;
  n->rx_size  = l->rx_size;

  io_driver_watcher_init(&n->watcher, newsd, io_net_generic_callback);

  io_driver_watch(l->driver, &n->watcher, IO_DRIVER_EVENT_RX);

  ev.ev = IO_NET_EVENT_CONNECTED;
  ev.n  = n;

  l->cb(l, &ev);
}

static void
io_net_connect_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_net_t*               n = container_of(w, io_net_t, watcher);
  int                     err;
  socklen_t               len = sizeof(err);
  io_net_event_t          ev;

  if(e != IO_DRIVER_EVENT_TX)
  {
    LOGE(TAG, "%s spurious event %d\n", __func__, e);
    return;
  }

  getsockopt(n->sd, SOL_SOCKET, SO_ERROR, &err, &len);

  if(err != 0)
  {
    // connect failed
    ev.ev = IO_NET_EVENT_CLOSED;
  }
  else
  {
    // connect success
    ev.ev = IO_NET_EVENT_CONNECTED;
    io_driver_no_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);
    io_driver_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_RX);
  }
  ev.n    = n;
  n->cb(n, &ev);
}

static void
io_net_udp_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_net_t*       n = container_of(w, io_net_t, watcher);
  int             ret;
  io_net_event_t  ev;
  socklen_t       from_len = sizeof(struct sockaddr_in);
  struct sockaddr_in from;

  switch(e)
  {
  case IO_DRIVER_EVENT_RX:
    ret = recvfrom(n->sd, n->rx_buf, n->rx_size, 0, (struct sockaddr*)&from, &from_len);
    if(ret <= 0)
    {
      LOGE(TAG, "%s recvfrom failed\n", __func__);
      return;
    }

    ev.ev     = IO_NET_EVENT_RX;
    ev.r.buf  = n->rx_buf;
    ev.r.len  = (uint32_t)ret;
    ev.r.from = &from;
    n->cb(n, &ev);
    break;

  case IO_DRIVER_EVENT_TX:
  case IO_DRIVER_EVENT_EX:
    LOGE(TAG, "%s spurious event %d\n", __func__, e);
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
int
io_net_bind(io_driver_t* driver, io_net_t* n, int port, io_net_callback cb, io_net_alloc alloc,
    uint8_t* rx_buf, int rx_size)
{
  int                   sd;
  const int             on = 1;
  struct sockaddr_in    addr;

  sd = socket(AF_INET, SOCK_STREAM, 0);
  if(sd < 0)
  {
    LOGE(TAG, "%s socket failed\n", __func__);
    goto socket_failed;
  }

  sock_util_put_nonblock(sd);
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  memset(&addr, 0, sizeof(addr));
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
  {
    LOGE(TAG, "%s failed to bind %d\n", __func__, port);
    goto bind_failed;
  }

  listen(sd, 5);

  n->sd       = sd;
  n->cb       = cb;
  n->alloc    = alloc;
  n->driver   = driver;
  n->rx_buf   = rx_buf;
  n->rx_size  = rx_size;

  io_driver_watcher_init(&n->watcher, sd, io_net_accept_callback);

  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_RX);

  return 0;

bind_failed:
  close(sd);

socket_failed:
  return -1;
}

int
io_net_connect(io_driver_t* driver, io_net_t* n, const char* ip_addr, int port, io_net_callback cb,
    uint8_t* rx_buf, int rx_size)
{
  int                 sd;
  struct sockaddr_in  to;

  sd = socket(AF_INET, SOCK_STREAM, 0);
  if(sd < 0)
  {
    LOGE(TAG, "%s socket failed\n", __func__);
    goto socket_failed;
  }

  sock_util_put_nonblock(sd);

  memset(&to, 0, sizeof(to));
  to.sin_addr.s_addr = inet_addr(ip_addr);
  to.sin_port = htons(port);

  n->sd       = sd;
  n->cb       = cb;
  n->driver   = driver;
  n->rx_buf   = rx_buf;
  n->rx_size  = rx_size;

  io_driver_watcher_init(&n->watcher, sd, io_net_connect_callback);

  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_TX);

  //
  // don't care about return value here
  // anyway any error will be detected at the next loop
  //
  connect(sd, (struct sockaddr*)&to, sizeof(to));

  return 0;

socket_failed:
  return -1;
}

void
io_net_close(io_net_t* n)
{
  io_driver_no_watch(n->driver,
      &n->watcher,
      IO_DRIVER_EVENT_RX | IO_DRIVER_EVENT_TX | IO_DRIVER_EVENT_EX);
  close(n->sd);
}

int
io_net_tx(io_net_t* n, uint8_t* buf, int len)
{
  //
  // FIXME
  // this should be changed to  event/buffer based TX
  //
  int nwritten = 0,
      ret;

  while(nwritten < len)
  {
    ret = write(n->sd, &buf[nwritten], len - nwritten);
    if(ret <= 0)
    {
      if(!(errno == EWOULDBLOCK || errno == EAGAIN))
      {
        return -1;
      }
    }
  }
  return nwritten;
}

int
io_net_udp(io_driver_t* driver, io_net_t* n, int port, uint8_t* rx_buf, int rx_size, io_net_callback cb)
{
  int                 sd;
  struct sockaddr_in  mine;

  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0)
  {
    LOGE(TAG, "%s socket failed\n", __func__);
    goto socket_failed;
  }

  sock_util_put_nonblock(sd);

  memset(&mine, 0, sizeof(mine));
  mine.sin_addr.s_addr  = INADDR_ANY;
  mine.sin_port         = htons(port);

  if(bind(sd, (struct sockaddr*)&mine, sizeof(mine)) != 0)
  {
    LOGE(TAG, "%s failed to bind %d\n", __func__, port);
    goto bind_failed;
  }

  n->sd     = sd;
  n->cb     = cb;
  n->driver = driver;

  n->rx_buf   = rx_buf;
  n->rx_size  = rx_size;

  io_driver_watcher_init(&n->watcher, sd, io_net_udp_callback);
  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_RX);

  return 0;

bind_failed:
  close(sd);

socket_failed:

  return -1;
}

int
io_net_tx_udp(io_net_t* n, struct sockaddr_in* to, uint8_t* buf, int len)
{
  ssize_t ret;

  ret = sendto(n->sd, buf, len, 0, (struct sockaddr*)to, sizeof(struct sockaddr_in));
  if(ret == len)
  {
    return 0;
  }

  return -1;
}
