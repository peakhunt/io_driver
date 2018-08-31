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

static void io_net_callback_enter(io_driver_watcher_t* w, io_driver_event e);

///////////////////////////////////////////////////////////////////////////////
//
// io_net memory allocator
//
///////////////////////////////////////////////////////////////////////////////
static inline io_net_t*
io_net_alloc(void)
{
  return calloc(1, sizeof(io_net_t));
}

static inline void
io_net_free(io_net_t* n)
{
  free(n);
}

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
static inline void
io_net_exec_callback(io_net_t* n, io_net_event_t* e)
{
  n->cb(n, e);
}

static void
io_net_handle_data_rx_event(io_net_t* n)
{
  int             ret;
  io_net_event_t  ev;

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

  io_net_exec_callback(n, &ev);
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
    io_net_handle_data_rx_event(n);
  }

  if((e & IO_DRIVER_EVENT_TX) && !(n->flags & IO_NET_FLAGS_MARK_DELETED))
  {
    io_net_handle_data_tx_event(n);
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

  n = io_net_alloc();
  if(n == NULL)
  {
    LOGE(TAG, "%s io_net_alloc failed\n", __func__);
    return;
  }

  sock_util_put_nonblock(newsd);

  n->sd       = newsd;
  n->cb       = l->cb;
  n->driver   = l->driver;

  n->target = io_net_generic_callback;
  io_driver_watcher_init(&n->watcher, newsd, io_net_callback_enter);

  io_driver_watch(l->driver, &n->watcher, IO_DRIVER_EVENT_RX);

  ev.ev = IO_NET_EVENT_CONNECTED;
  ev.n  = n;

  io_net_exec_callback(l, &ev);
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
    n->target = io_net_generic_callback;

    ev.ev = IO_NET_EVENT_CONNECTED;
    io_driver_no_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_TX);
    io_driver_watch(n->driver, &n->watcher, IO_DRIVER_EVENT_RX);
  }
  ev.n    = n;
  io_net_exec_callback(n, &ev);
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

    io_net_exec_callback(n, &ev);
    break;

  case IO_DRIVER_EVENT_TX:
  case IO_DRIVER_EVENT_EX:
    LOGE(TAG, "%s spurious event %d\n", __func__, e);
    break;
  }
}

static void
io_net_callback_enter(io_driver_watcher_t* w, io_driver_event e)
{
  io_net_t*       n = container_of(w, io_net_t, watcher);

  n->flags |= IO_NET_FLAGS_EXECUTING;
  n->target(w, e);
  n->flags &= ~IO_NET_FLAGS_EXECUTING;

  if(n->exit)
  {
    n->exit(n);
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
void
io_net_init(void)
{
  // module initializaer
}

io_net_t*
io_net_bind(io_driver_t* driver, int port, io_net_callback cb)
{
  int                   sd;
  const int             on = 1;
  struct sockaddr_in    addr;
  io_net_t*             n;

  n = io_net_alloc();
  if(n == NULL)
  {
    LOGE(TAG, "%s io_net_alloc failed\n", __func__);
    goto alloc_failed;
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

  n->sd       = sd;
  n->cb       = cb;
  n->driver   = driver;

  n->target = io_net_accept_callback;
  io_driver_watcher_init(&n->watcher, sd, io_net_callback_enter);

  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_RX);

  return n;

bind_failed:
  close(sd);

socket_failed:
  io_net_free(n);

alloc_failed:
  return NULL;
}

io_net_t*
io_net_connect(io_driver_t* driver, const char* ip_addr, int port, io_net_callback cb)
{
  int                 sd;
  struct sockaddr_in  to;
  io_net_t*           n;

  n = io_net_alloc();
  if(n == NULL)
  {
    LOGE(TAG, "%s io_net_alloc failed\n", __func__);
    goto alloc_failed;
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

  n->sd       = sd;
  n->cb       = cb;
  n->driver   = driver;

  n->target = io_net_connect_callback;
  io_driver_watcher_init(&n->watcher, sd, io_net_callback_enter);

  io_driver_watch(driver, &n->watcher, IO_DRIVER_EVENT_TX);

  //
  // don't care about return value here
  // anyway any error will be detected at the next loop
  //
  connect(sd, (struct sockaddr*)&to, sizeof(to));

  return n;

socket_failed:
  io_net_free(n);

alloc_failed:
  return NULL;
}

static void
__io_net_close(io_net_t* n)
{
  LOGI(TAG, "%s executing delete\n", __func__);
  io_driver_no_watch(n->driver,
      &n->watcher,
      IO_DRIVER_EVENT_RX | IO_DRIVER_EVENT_TX | IO_DRIVER_EVENT_EX);
  close(n->sd);
  io_net_free(n);
}

void
io_net_close(io_net_t* n)
{
  if(io_net_is_in_callback(n))
  {
    LOGI(TAG, "%s scheduling delete\n", __func__);
    n->flags |= IO_NET_FLAGS_MARK_DELETED;
    n->exit = __io_net_close;
  }
  else
  {
    __io_net_close(n);
  }
}

int
io_net_tx(io_net_t* n, uint8_t* buf, int len)
{
  int ret;

  if(n->tx_buf == NULL)
  {
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
        return nwritten;
      }
    }
    return nwritten;
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

  n->target = io_net_udp_callback;
  io_driver_watcher_init(&n->watcher, sd, io_net_callback_enter);
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
