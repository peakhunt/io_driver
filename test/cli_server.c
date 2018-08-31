#include <stdio.h>
#include <stdlib.h>

#include "io_driver.h"
#include "io_net.h"
#include "io_telnet.h"

#include "generic_list.h"
#include "telnet.h"

#include "cli.h"

typedef struct
{
  struct list_head    le;
  io_net_t*           conn;
  cli_intf_t          cli_if;
} cli_conn_t;

static const char* TAG = "main";
static io_driver_t        io_driver;
static struct list_head   conns;
static io_net_t*          tserver;

static void alloc_cli_connection(io_net_t* n);
static void dealloc_cli_connection(io_net_t* n);

io_driver_t*
cli_io_driver(void)
{
  return &io_driver;
}

static inline void
init_telnet_session(cli_conn_t* c)
{
  static const char iacs_to_send[] =
  {
    IAC, WILL,   TELOPT_SGA,
    IAC, WILL,   TELOPT_ECHO,
  };
  io_telnet_tx(c->conn, (uint8_t*)iacs_to_send, sizeof(iacs_to_send));
}

static void
cli_tx(cli_intf_t* intf, const char* buf, int len)
{
  cli_conn_t*   c = container_of(intf, cli_conn_t, cli_if);

  io_telnet_tx(c->conn, (uint8_t*)buf, len);
}

static void
cli_exit(cli_intf_t* intf)
{
  cli_conn_t*   c = container_of(intf, cli_conn_t, cli_if);

  dealloc_cli_connection(c->conn);
}

static void
alloc_cli_connection(io_net_t* n)
{
  cli_conn_t*   c;

  c = malloc(sizeof(cli_conn_t));
  INIT_LIST_HEAD(&c->le);
  c->conn = n;

  n->user_priv = c;

  list_add_tail(&c->le, &conns);

  init_telnet_session(c);

  c->cli_if.put_tx_data = cli_tx;
  c->cli_if.exit = cli_exit;

  cli_intf_register(&c->cli_if);

  LOGI(TAG, "new connection : %p, %p\n", n, c);
}

static void
dealloc_cli_connection(io_net_t* n)
{
  cli_conn_t*   c = (cli_conn_t*)n->user_priv;

  list_del(&c->le);

  cli_intf_unregister(&c->cli_if);

  free(c);

  io_telnet_close(n);
}

static void
telnet_server_callback(io_net_t* n, io_net_event_t* e)
{
  cli_conn_t* c = (cli_conn_t*)n->user_priv;
  cli_intf_t* intf = &c->cli_if;

  switch(e->ev)
  {
  case IO_NET_EVENT_CONNECTED:
    LOGI(TAG, "new telnet connection\n");
    alloc_cli_connection(e->n);
    break;

  case IO_NET_EVENT_RX:
    if(e->r.buf[0] != 0)
    {
      cli_handle_rx(intf, e->r.buf, e->r.len);
    }
    break;

  case IO_NET_EVENT_CLOSED:
    LOGI(TAG, "Close event : %p, %p\n", n, intf);
    dealloc_cli_connection(n);
    break;
  }
}

int
main()
{

  LOGI(TAG, "starting cli server\n");

  INIT_LIST_HEAD(&conns);

  io_driver_init(&io_driver);

  tserver = io_telnet_bind(&io_driver, 11060, telnet_server_callback);

  cli_init(NULL, 0, 0);

  while(1)
  {
    io_driver_run(&io_driver);
  }
  return 0;
}
