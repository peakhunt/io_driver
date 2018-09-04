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
// utilities
//
///////////////////////////////////////////////////////////////////////////////
static io_net_return_t
io_net_handle_data_rx_event(io_net_t* n)
{
  int             ret;
  io_net_event_t  ev;

  ret = read(n->sd, n->rx_buf, n->rx_size);
  if(ret <= 0)
  {
    ev.ev = io_net_event_enum_closed;
  }
  else
  {
    ev.ev = io_net_event_enum_rx;
    ev.r.buf = n->rx_buf;
    ev.r.len = (uint32_t)ret;
  }

  return n->cb(n, &ev);
}

static void
io_net_handle_data_tx_event(io_net_t* n)
{
  uint8_t   buffer[128];
  int       data_size,
            ret,
            len;

  if(circ_buffer_is_empty(n->tx_buf))
  {
    LOGI(TAG, "disabling TX event. circ buffer empty now");
    io_driver_no_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);
    return;
  }

  // XXX
  // looping would be more efficient but
  // on the other hand, this has better time sharing feature
  //
  data_size = circ_buffer_get_data_size(n->tx_buf);
  len = data_size < 128 ? data_size : 128;
  circ_buffer_peek(n->tx_buf, buffer, len);

  ret = write(n->sd, buffer, len);
  if(ret <= 0)
  {
    // definitely stream got into a trouble
    io_driver_no_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);

    LOGI(TAG, "XXXXXXXX this should not happen");
    CRASH();
    return;
  }

  circ_buffer_advance(n->tx_buf, ret);

  if(circ_buffer_is_empty(n->tx_buf))
  {
    io_driver_no_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);
  }

  return;
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

  if((e & IO_DRIVER_EVENT_RX))
  {
    if(io_net_handle_data_rx_event(n) == io_net_return_stop)
    {
      LOGI(TAG, "%s catching stop\n", __func__);
      return;
    }
  }

  if((e & IO_DRIVER_EVENT_TX))
  {
    if(n->tx_buf)
    {
      io_net_handle_data_tx_event(n);
    }
    else
    {
      io_net_event_t    ev;

      ev.ev = io_net_event_enum_tx;
      n->cb(n, &ev);
    }
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

  io_driver_watcher_init(&n->watcher, newsd, io_net_generic_callback);
  io_driver_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_RX);

  ev.ev = io_net_event_enum_connected;
  ev.n  = NULL;
  ev.from = &from;

  n->cb(n, &ev);
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
    ev.ev = io_net_event_enum_closed;
  }
  else
  {
    // connect success
    io_driver_watcher_set_cb(&n->watcher, io_net_generic_callback);

    ev.ev = io_net_event_enum_connected;
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

    ev.ev     = io_net_event_enum_rx;
    ev.r.buf  = n->rx_buf;
    ev.r.len  = (uint32_t)ret;
    ev.from   = &from;

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
io_net_bind(io_driver_t* driver, io_net_t* n, int port, io_net_callback cb)
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
  addr.sin_family       = AF_INET;
  addr.sin_addr.s_addr  = INADDR_ANY;
  addr.sin_port         = htons(port);

  if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
  {
    LOGE(TAG, "%s failed to bind %d\n", __func__, port);
    goto bind_failed;
  }

  listen(sd, 5);

  n->sd       = sd;
  n->cb       = cb;
  n->driver   = driver;

  io_driver_watcher_init(&n->watcher, sd, io_net_accept_callback);
  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_RX);

  return 0;

bind_failed:
  close(sd);

socket_failed:
  return -1;
}

int
io_net_connect(io_driver_t* driver, io_net_t* n, const char* ip_addr, int port, io_net_callback cb)
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
  to.sin_family       = AF_INET;
  to.sin_addr.s_addr  = inet_addr(ip_addr);
  to.sin_port         = htons(port);

  n->sd       = sd;
  n->cb       = cb;
  n->driver   = driver;

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
  int ret;

  if(n->tx_buf == NULL)
  {
#if 0
    int nwritten = 0;

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
      else
      {
        nwritten += ret;
      }
    }
    return nwritten;
#else
    return write(n->sd, buf, len);
#endif
  }

  // use tx buffer mode

  if(circ_buffer_is_empty(n->tx_buf) == FALSE)
  {
    // circular buffer is not empty. to maintain message order
    // can't send on the socket.
    return circ_buffer_put(n->tx_buf, buf, len) == 0 ? len : -1;
  }
  else
  {
    // nothing is tx buffer
    // try to send
    ret = write(n->sd, buf, len);
    if(ret == len)
    {
      return len;
    }

    if(ret < 0)
    {
      if(!(errno == EWOULDBLOCK || errno == EAGAIN))
      {
        return -1;
      }
    }
  }

  //
  // message partially sent. put the rest in circular buffer
  //
  if(circ_buffer_put(n->tx_buf, &buf[ret], len - ret) == FALSE)
  {
    return -1;
  }
  
  io_driver_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);

  return len;
}

int
io_net_udp(io_driver_t* driver, io_net_t* n, int port, io_net_callback cb)
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
  mine.sin_family       = AF_INET;
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

  io_driver_watcher_init(&n->watcher, sd, io_net_udp_callback);
  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_RX);

  return 0;

bind_failed:
  close(sd);

socket_failed:

  return -1;
}

int
io_net_udp_tx(io_net_t* n, struct sockaddr_in* to, uint8_t* buf, int len)
{
  ssize_t ret;

  ret = sendto(n->sd, buf, len, 0, (struct sockaddr*)to, sizeof(struct sockaddr_in));
  if(ret == len)
  {
    return 0;
  }

  return -1;
}
