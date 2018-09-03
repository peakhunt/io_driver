#include <stdio.h>
#include <stdlib.h>

#include "io_driver.h"
#include "io_net.h"
#include "io_telnet.h"
#include "io_timer.h"

static const char* TAG = "main";
static io_driver_t        io_driver;
static io_telnet_t        tclient;

static io_timer_t         io_timer;

static char*              ipaddr;
static int                port;

static io_net_return_t telnet_client_callback(io_telnet_t* n, io_telnet_event_t* e);

static SoftTimerElem      reconn_tmr;
static SoftTimerElem      conn_tmr;
static SoftTimerElem      close_tmr;

static void
start_connect(void)
{
  if(io_telnet_connect(&io_driver, &tclient, ipaddr, port, telnet_client_callback) != 0)
  {
    LOGE(TAG, "io_telnet_connect returned NULL....\n");
    return;
  }

  LOGI(TAG, "starting connect timer: 5000\n");
  io_timer_start(&io_timer, &conn_tmr, 5000);
}

static void
reconnect_timeout(SoftTimerElem* te)
{
  LOGI(TAG, "reconnect_timeout\n");
  start_connect();
}

static void
connect_timeout(SoftTimerElem* te)
{
  LOGI(TAG, "connect_timeout\n");
  LOGI(TAG, "AAAAAAAAA\n");
  io_telnet_close(&tclient);
  start_connect();
}

static void
close_timeout(SoftTimerElem* te)
{
  LOGI(TAG, "close_timeout\n");
  LOGI(TAG, "BBBBBBBBBBBBBB\n");
  io_telnet_close(&tclient);
  start_connect();
}

static io_net_return_t
telnet_client_callback(io_telnet_t* n, io_telnet_event_t* e)
{
  switch(e->ev)
  {
  case io_net_event_enum_connected:
    LOGI(TAG, "connected to %s:%d\n", ipaddr, port);
    io_timer_stop(&io_timer, &conn_tmr);

    LOGI(TAG, "Starting Close Timer: 3000\n");
    io_timer_start(&io_timer, &close_tmr, 3000);
    break;

  case io_net_event_enum_rx:
    LOGI(TAG, "RX %d bytes\n", e->r.len);
    break;

  case io_net_event_enum_closed:
    io_telnet_close(&tclient);
    io_timer_stop(&io_timer, &conn_tmr);
    io_timer_stop(&io_timer, &close_tmr);
    LOGI(TAG, "Starting Reconnect Timer: 1000\n");
    io_timer_start(&io_timer, &reconn_tmr, 1000);
    return io_net_return_stop;

  default:
    break;
  }
  return io_net_return_continue;
}

int
main(int argc, char** argv)
{
  LOGI(TAG, "starting cli server\n");

  if(argc != 3)
  {
    LOGI(TAG, "%s ip-addr port\n", argv[0]);
  }

  ipaddr = argv[1];
  port = atoi(argv[2]);

  io_driver_init(&io_driver);
  io_timer_init(&io_driver, &io_timer, 100);

  soft_timer_init_elem(&reconn_tmr);
  reconn_tmr.cb = reconnect_timeout;

  soft_timer_init_elem(&conn_tmr);
  conn_tmr.cb = connect_timeout;

  soft_timer_init_elem(&close_tmr);
  close_tmr.cb = close_timeout;

  start_connect();

  while(1)
  {
    io_driver_run(&io_driver);
  }

  return 0;
}
