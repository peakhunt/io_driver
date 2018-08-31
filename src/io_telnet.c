#include <stdlib.h>
#include "io_telnet.h"
#include "telnet_reader.h"

///////////////////////////////////////////////////////////////////////////////
//
// internal definitions
//
///////////////////////////////////////////////////////////////////////////////
#define IO_TELNET_RX_BUF_SIZE       128
#define IO_TELNET_TX_BUF_SIZE       256

typedef struct
{
  io_net_callback     cb;
  telnet_reader_t     treader;
  uint8_t             rx_buf[IO_TELNET_RX_BUF_SIZE];
  uint8_t             tx_buf[IO_TELNET_TX_BUF_SIZE];
  circ_buffer_t       txcb;
  io_net_t*           n;
} io_telnet_t;

static void io_telnet_data_back(telnet_reader_t* tr, uint8_t data);
static void io_telnet_cmd_back(telnet_reader_t* tr);

static const char* TAG = "telnet";

///////////////////////////////////////////////////////////////////////////////
//
// io_telnet memory allocator
//
///////////////////////////////////////////////////////////////////////////////
static inline io_telnet_t*
io_telnet_alloc(void)
{
  return calloc(1, sizeof(io_telnet_t));
}

static inline void
io_telnet_free(io_telnet_t* n)
{
  free(n);
}
 

///////////////////////////////////////////////////////////////////////////////
//
// utilities
//
///////////////////////////////////////////////////////////////////////////////
static io_telnet_t*
io_telnet_connection_init(io_net_t* n, io_net_callback cb)
{
  io_telnet_t*    tn;

  tn = io_telnet_alloc();
  if(tn == NULL)
  {
    return NULL;
  }

  circ_buffer_init_with_mem(&tn->txcb, tn->tx_buf, IO_TELNET_TX_BUF_SIZE);
  io_net_set_rx_buf(n, tn->rx_buf, IO_TELNET_RX_BUF_SIZE);
  io_net_set_tx_circ_buf(n, &tn->txcb);

  tn->treader.databack = io_telnet_data_back;
  tn->treader.cmdback  = io_telnet_cmd_back;
  telnet_reader_init(&tn->treader);

  n->priv = tn;
  tn->n   = n;
  tn->cb  = cb;

  return tn;
}

///////////////////////////////////////////////////////////////////////////////
//
// telnet reader callback
//
///////////////////////////////////////////////////////////////////////////////
static void
io_telnet_data_back(telnet_reader_t* tr, uint8_t data)
{
  io_telnet_t*    tn = container_of(tr, io_telnet_t, treader);
  io_net_event_t  e;

  e.ev    = IO_NET_EVENT_RX;
  e.r.buf = &data;
  e.r.len = 1;

  tn->cb(tn->n, &e);
}

static void
io_telnet_cmd_back(telnet_reader_t* tr)
{
  // XXX IGNORED
}

///////////////////////////////////////////////////////////////////////////////
//
// io_net callbacks
//
///////////////////////////////////////////////////////////////////////////////
static void
io_telnet_server_callback(io_net_t* n, io_net_event_t* e)
{
  io_net_t*         nn;
  io_telnet_t*      tn = (io_telnet_t*)n->priv;
  io_telnet_t*      ntn;

  switch(e->ev)
  {
  case IO_NET_EVENT_CONNECTED:
    nn = e->n;

    ntn = io_telnet_connection_init(nn, tn->cb);
    if(ntn == NULL)
    {
      LOGE(TAG, "%s io_telnet_alloc failed\n", __func__);
      io_net_close(nn);
      return;
    }

    tn->cb(n, e);
    break;

  case IO_NET_EVENT_RX:
    for(int i = 0; i < e->r.len && io_net_is_mark_deleted(n) == FALSE; i++)
    {
      telnet_reader_feed(&tn->treader, e->r.buf[i]);
    }
    break;

  case IO_NET_EVENT_CLOSED:
    tn->cb(n, e);
    break;
  }
}

static void
io_telnet_client_callback(io_net_t* n, io_net_event_t* e)
{
  io_telnet_t*      tn = (io_telnet_t*)n->priv;

  switch(e->ev)
  {
  case IO_NET_EVENT_CONNECTED:
    tn->cb(n, e);
    break;

  case IO_NET_EVENT_RX:
    for(int i = 0; i < e->r.len && io_net_is_mark_deleted(n) == FALSE; i++)
    {
      telnet_reader_feed(&tn->treader, e->r.buf[i]);
    }
    break;

  case IO_NET_EVENT_CLOSED:
    tn->cb(n, e);
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
void
io_telnet_init(void)
{
  // module initializer
}

io_net_t*
io_telnet_bind(io_driver_t* driver, int port, io_net_callback cb)
{
  io_net_t*       n;
  io_telnet_t*    tn;

  n = io_net_bind(driver, port, io_telnet_server_callback);
  if(n == NULL)
  {
    return NULL;
  }

  tn = io_telnet_connection_init(n, cb);
  if(tn == NULL)
  {
    LOGE(TAG, "%s io_telnet_connection_init failed\n", __func__);
    io_net_close(n);
    return NULL;
  }

  return n;
}

io_net_t*
io_telnet_connect(io_driver_t* driver, const char* ip_addr, int port, io_net_callback cb)
{
  io_net_t*       n;
  io_telnet_t*    tn;

  n = io_net_connect(driver, ip_addr, port, io_telnet_client_callback);
  if(n == NULL)
  {
    return NULL;
  }

  tn = io_telnet_connection_init(n, cb);
  if(tn == NULL)
  {
    LOGE(TAG, "%s io_telnet_connection_init failed\n", __func__);
    io_net_close(n);
    return NULL;
  }

  return n;
}

static void
__io_telnet_close(io_net_t* n)
{
  LOGI(TAG, "%s executing delete\n", __func__);
  io_telnet_free(n->priv);
  io_net_close(n);
}

void
io_telnet_close(io_net_t* n)
{
  if(io_net_is_in_callback(n))
  {
    LOGI(TAG, "%s scheduling delete\n", __func__);
    n->flags |= IO_NET_FLAGS_MARK_DELETED;
    n->exit = __io_telnet_close;
  }
  else
  {
    __io_telnet_close(n);
  }
}

int
io_telnet_tx(io_net_t* n, uint8_t* buf, int len)
{
  return io_net_tx(n, buf, len);
}
