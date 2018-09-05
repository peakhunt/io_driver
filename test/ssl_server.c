#include <stdio.h>
#include <stdlib.h>

#include "io_driver.h"
#include "io_ssl.h"
#include "circ_buffer.h"

#define HTTP_RESPONSE \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>mbed TLS Test Server</h2>\r\n" \
    "<p>Successful connection using: %s</p>\r\n"


typedef struct
{
  struct list_head    le;
  io_ssl_t            sconn;
  uint8_t             rx_buf[128];
  circ_buffer_t       txcb;
} ssl_conn_t;

static io_net_return_t ssl_server_callback(io_ssl_t* t, io_ssl_event_t* e);
static ssl_conn_t* alloc_ssl_connection(void);
static void dealloc_ssl_connection(ssl_conn_t* c);

static const char* TAG = "main";
static io_driver_t        io_driver;
static struct list_head   conns;
static io_ssl_t        sserver;

static ssl_conn_t* 
alloc_ssl_connection(void)
{
  ssl_conn_t*   c;

  c = malloc(sizeof(ssl_conn_t));
  INIT_LIST_HEAD(&c->le);

  list_add_tail(&c->le, &conns);

  circ_buffer_init(&c->txcb, 512);

  LOGI(TAG, "new connection :\n");
  return c;
}

static void
dealloc_ssl_connection(ssl_conn_t* c)
{
  io_ssl_close(&c->sconn);
  circ_buffer_deinit(&c->txcb);
  list_del(&c->le);
  free(c);
}

static void
ssl_tx_resume(ssl_conn_t* c)
{
  uint8_t   buffer[128];
  int       data_size,
            ret,
            len;

  LOGI(TAG, "cli_tx_resume\n");

  while(1)
  {
    if(circ_buffer_is_empty(&c->txcb))
    {
      return;
    }

    data_size = circ_buffer_get_data_size(&c->txcb);
    len = data_size < 128 ? data_size : 128;
    circ_buffer_peek(&c->txcb, buffer, len);

    ret = io_ssl_tx(&c->sconn, buffer, len);
    if(ret == 0)
    {
      LOGE(TAG, "0 in cli_tx_resume\n");
      return;
    }
    else if(ret == -1)
    {
      LOGE(TAG, "ERROR -1 in cli_tx_resume\n");
      return;
    }
    circ_buffer_advance(&c->txcb, ret);
  }
}

static void
ssl_tx(ssl_conn_t* c, uint8_t* buf, int len)
{
  int           ret,
                nwritten = 0;

  if(circ_buffer_is_empty(&c->txcb) == FALSE)
  {
    if(circ_buffer_put(&c->txcb, (uint8_t*)buf, len) != 0)
    {
      LOGE(TAG, "cli_tx. 1-circ_buffer_put failed\n");
      return;
    }
  }

  while(nwritten < len)
  {
    ret = io_ssl_tx(&c->sconn, (uint8_t*)buf, len);
    if(ret == 0)
    {
      ret = circ_buffer_put(&c->txcb, (uint8_t*)&buf[nwritten], len - nwritten);
      if(ret != 0)
      {
        LOGE(TAG, "cli_tx circ_buffer_put error\n");
        return;
      }
      LOGE(TAG, "cli_tx paused\n");
      return;
    }
    else if(ret == -1)
    {
      LOGE(TAG, "cli_tx error\n");
      return;
    }
    nwritten += ret;
  }
}

static io_net_return_t
ssl_server_callback(io_ssl_t* s, io_ssl_event_t* e)
{
  ssl_conn_t* c;

  switch(e->ev)
  {
  case io_net_event_enum_alloc_connection:
    LOGI(TAG, "new ssl connection\n");
    c = alloc_ssl_connection();
    if(c == NULL)
    {
      return io_net_return_stop;
    }
    e->n = &c->sconn;
    return io_net_return_continue;

  case io_net_event_enum_connected:
    LOGI(TAG, "new ssl connected\n");
    c = container_of(s, ssl_conn_t, sconn); 
    io_ssl_set_rx_buf(s, c->rx_buf, 128);
    return io_net_return_continue;

  case io_net_event_enum_rx:
    LOGI(TAG, "RX: %d bytes\n", e->r.len);
    c = container_of(s, ssl_conn_t, sconn); 
    ssl_tx(c, (uint8_t*)HTTP_RESPONSE, strlen(HTTP_RESPONSE));
    //dealloc_ssl_connection(c);
    //return io_net_return_stop;
    return io_net_return_continue;

  case io_net_event_enum_closed:
    c = container_of(s, ssl_conn_t, sconn); 
    LOGI(TAG, "Close event :\n");
    dealloc_ssl_connection(c);
    return io_net_return_stop;

  case io_net_event_enum_tx:
    c = container_of(s, ssl_conn_t, sconn); 
    LOGI(TAG, "TX event :\n");
    ssl_tx_resume(c);
    break;

  default:
    break;
  }
  return io_net_return_continue;
}

int
main()
{
  LOGI(TAG, "starting ssl server\n");

  INIT_LIST_HEAD(&conns);

  io_driver_init(&io_driver);
  io_ssl_bind(&io_driver, &sserver, 11070, ssl_server_callback);

  while(1)
  {
    io_driver_run(&io_driver);
  }

  return 0;
}
