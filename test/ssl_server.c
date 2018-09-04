#include <stdio.h>
#include <stdlib.h>

#include "io_driver.h"
#include "io_ssl.h"

#define HTTP_RESPONSE \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>mbed TLS Test Server</h2>\r\n" \
    "<p>Successful connection using: %s</p>\r\n"


typedef struct
{
  struct list_head    le;
  io_ssl_t            sconn;
  uint8_t             rx_buf[128];
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

  LOGI(TAG, "new connection :\n");
  return c;
}

static void
dealloc_ssl_connection(ssl_conn_t* c)
{
  io_ssl_close(&c->sconn);

  list_del(&c->le);
  free(c);
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
    io_ssl_tx(s, (uint8_t*)HTTP_RESPONSE, strlen(HTTP_RESPONSE));
    dealloc_ssl_connection(c);
    return io_net_return_stop;

  case io_net_event_enum_closed:
    c = container_of(s, ssl_conn_t, sconn); 
    LOGI(TAG, "Close event :\n");
    dealloc_ssl_connection(c);
    return io_net_return_stop;

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
