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

static int io_telnet_data_back(telnet_reader_t* tr, uint8_t data);
static int io_telnet_cmd_back(telnet_reader_t* tr);

static const char* TAG = "telnet";

///////////////////////////////////////////////////////////////////////////////
//
// utilities
//
///////////////////////////////////////////////////////////////////////////////
static void
io_telnet_connection_init(io_telnet_t* t)
{
  circ_buffer_init_with_mem(&t->txcb, t->tx_buf, IO_TELNET_TX_BUF_SIZE);
  io_net_set_rx_buf(&t->n, t->rx_buf, IO_TELNET_RX_BUF_SIZE);
  io_net_set_tx_circ_buf(&t->n, &t->txcb);

  t->treader.databack = io_telnet_data_back;
  t->treader.cmdback  = io_telnet_cmd_back;
  telnet_reader_init(&t->treader);
}

///////////////////////////////////////////////////////////////////////////////
//
// telnet reader callback
//
///////////////////////////////////////////////////////////////////////////////
static int
io_telnet_data_back(telnet_reader_t* tr, uint8_t data)
{
  io_telnet_t*    tn = container_of(tr, io_telnet_t, treader);
  io_telnet_event_t   e;

  e.ev    = io_net_event_enum_rx;
  e.r.buf = &data;
  e.r.len = 1;

  if(tn->cb(tn, &e) == io_net_return_stop)
  {
    return -1;
  }

  return 0;
}

static int
io_telnet_cmd_back(telnet_reader_t* tr)
{
  // XXX IGNORED
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// io_net callbacks
//
///////////////////////////////////////////////////////////////////////////////
static io_net_return_t
io_telnet_server_callback(io_net_t* n, io_net_event_t* e)
{
  io_telnet_event_t ev;
  io_telnet_t*      t = container_of(n, io_telnet_t, n);
  io_telnet_t*      nt;

  switch(e->ev)
  {
  case io_net_event_enum_alloc_connection:
    ev.ev   = io_net_event_enum_alloc_connection;
    ev.n    = NULL;
    ev.from = e->from;

    if(t->cb(t, &ev) == io_net_return_stop)
    {
      return io_net_return_stop;
    }

    nt = ev.n;
    nt->cb = t->cb;
    e->n = &nt->n;
    return io_net_return_continue;

  case io_net_event_enum_connected:
    ev.ev   = io_net_event_enum_connected;
    ev.n    = NULL;
    ev.from = e->from;

    io_telnet_connection_init(t);

    return t->cb(t, &ev);

  case io_net_event_enum_rx:
    for(int i = 0; i < e->r.len; i++)
    {
      if(telnet_reader_feed(&t->treader, e->r.buf[i]) != 0)
      {
        LOGI(TAG, "%s returning stop\n", __func__);
        return io_net_return_stop;
      }
    }
    return io_net_return_continue;

  case io_net_event_enum_closed:
    ev.ev = io_net_event_enum_closed;
    ev.n  = NULL;
    return t->cb(t, &ev);
  }

  return io_net_return_continue;
}

static io_net_return_t
io_telnet_client_callback(io_net_t* n, io_net_event_t* e)
{
  io_telnet_event_t ev;
  io_telnet_t*      t = container_of(n, io_telnet_t, n);

  switch(e->ev)
  {
  case io_net_event_enum_connected:
    ev.ev   = io_net_event_enum_connected;
    ev.n    = NULL;
    return t->cb(t, &ev);

  case io_net_event_enum_rx:
    for(int i = 0; i < e->r.len; i++)
    {
      if(telnet_reader_feed(&t->treader, e->r.buf[i]) != 0)
      {
        return io_net_return_stop;
      }
    }
    return io_net_return_continue;

  case io_net_event_enum_closed:
    ev.ev = io_net_event_enum_closed;
    ev.n  = NULL;
    return t->cb(t, &ev);

  default:
    break;
  }
  return io_net_return_continue;
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
int
io_telnet_bind(io_driver_t* driver, io_telnet_t* t, int port, io_telnet_callback cb)
{
  if(io_net_bind(driver, &t->n, port, io_telnet_server_callback) != 0)
  {
    return -1;
  }

  io_telnet_connection_init(t);
  t->cb = cb;
  return 0;
}

int
io_telnet_connect(io_driver_t* driver, io_telnet_t* t, const char* ip_addr, int port, io_telnet_callback cb)
{
  if(io_net_connect(driver, &t->n, ip_addr, port, io_telnet_client_callback) != 0)
  {
    return -1;
  }

  io_telnet_connection_init(t);
  t->cb = cb;

  return 0;
}

void
io_telnet_close(io_telnet_t* t)
{
  io_net_close(&t->n);
}

int
io_telnet_tx(io_telnet_t* t, uint8_t* buf, int len)
{
  return io_net_tx(&t->n, buf, len);
}
