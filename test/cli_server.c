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
  io_telnet_t         tconn;
  cli_intf_t          cli_if;
} cli_conn_t;

static io_net_return_t telnet_server_callback(io_telnet_t* t, io_telnet_event_t* e);
static cli_conn_t* alloc_cli_connection(void);
static void dealloc_cli_connection(cli_conn_t* c);

static const char* TAG = "main";
static io_driver_t        io_driver;
static struct list_head   conns;
static io_telnet_t        tserver;


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
  io_telnet_tx(&c->tconn, (uint8_t*)iacs_to_send, sizeof(iacs_to_send));
}

static void
cli_tx(cli_intf_t* intf, const char* buf, int len)
{
  cli_conn_t*   c = container_of(intf, cli_conn_t, cli_if);

  io_telnet_tx(&c->tconn, (uint8_t*)buf, len);
}

static void
cli_exit(cli_intf_t* intf)
{
  cli_conn_t*   c = container_of(intf, cli_conn_t, cli_if);

  dealloc_cli_connection(c);
  LOGI(TAG, "cli exit\n");
}

static cli_conn_t* 
alloc_cli_connection(void)
{
  cli_conn_t*   c;

  c = malloc(sizeof(cli_conn_t));
  INIT_LIST_HEAD(&c->le);

  list_add_tail(&c->le, &conns);

  c->cli_if.put_tx_data = cli_tx;
  c->cli_if.exit = cli_exit;

  cli_intf_register(&c->cli_if);

  LOGI(TAG, "new connection :\n");
  return c;
}

static void
dealloc_cli_connection(cli_conn_t* c)
{
  io_telnet_close(&c->tconn);

  list_del(&c->le);
  cli_intf_unregister(&c->cli_if);
  free(c);
}

static io_net_return_t
telnet_server_callback(io_telnet_t* t, io_telnet_event_t* e)
{
  cli_conn_t* c;

  switch(e->ev)
  {
  case io_net_event_enum_alloc_connection:
    LOGI(TAG, "new telnet connection\n");
    c = alloc_cli_connection();
    if(c == NULL)
    {
      return io_net_return_stop;
    }
    e->n = &c->tconn;
    return io_net_return_continue;

  case io_net_event_enum_connected:
    c = container_of(t, cli_conn_t, tconn); 
    init_telnet_session(c);
    return io_net_return_continue;

  case io_net_event_enum_rx:
    c = container_of(t, cli_conn_t, tconn); 
    if(e->r.buf[0] != 0)
    {
      if(cli_handle_rx(&c->cli_if, e->r.buf, e->r.len) == -1)
      {
        LOGI(TAG, "returning stop\n");
        return io_net_return_stop;
      }
    }
    return io_net_return_continue;

  case io_net_event_enum_closed:
    c = container_of(t, cli_conn_t, tconn); 
    LOGI(TAG, "Close event :\n");
    dealloc_cli_connection(c);
    return io_net_return_stop;
  }
  return io_net_return_continue;
}

int
main()
{

  LOGI(TAG, "starting cli server\n");

  INIT_LIST_HEAD(&conns);

  io_driver_init(&io_driver);
  io_telnet_bind(&io_driver, &tserver, 11060, telnet_server_callback);
  cli_init(NULL, 0, 0);

  while(1)
  {
    io_driver_run(&io_driver);
  }
  return 0;
}
