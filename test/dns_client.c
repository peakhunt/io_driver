#include <stdio.h>
#include <stdlib.h>

#include "io_dns.h"

static const char* TAG = "main";
static io_dns_cfg_t   _cfg = 
{
  //"8.8.8.8",
  "127.0.0.53",
  53,
  10,
};

static io_driver_t        io_driver;
static io_timer_t         io_timer;
static io_dns_t           io_dns;

static void 
dns_callback(io_dns_t* d, io_dns_event_t* e)
{
  LOGI(TAG, "dns_callback\n");

  switch(e->evt_type)
  {
  case io_dns_event_got_result:
    LOGI(TAG, "got something\n");
    for(int i = 0; i < e->addrs->n_addrs; i++)
    {
      LOGI(TAG, "result %d: %d.%d.%d.%d\n",
          i,
          e->addrs->addrs[i][0],
          e->addrs->addrs[i][1],
          e->addrs->addrs[i][2],
          e->addrs->addrs[i][3]);
    }
    exit(0);
    break;
  case io_dns_event_error:
    LOGI(TAG, "error\n");
    exit(-1);
    break;

  case io_dns_event_timedout:
    LOGI(TAG, "timeout\n");
    exit(-1);
    break;
  }
}

int
main(int argc, char** argv)
{
  LOGI(TAG, "starting dns client\n");

  if(argc != 2)
  {
    LOGI(TAG, "%s name\n", argv[0]);
    return -1;
  }

  io_driver_init(&io_driver);
  io_timer_init(&io_driver, &io_timer, 100);

  LOGI(TAG, "looking up %s\n", argv[1]);

  io_dns_lookup_ipv4(&io_driver, &io_timer, &io_dns, &_cfg, argv[1], dns_callback);

  while(1)
  {
    io_driver_run(&io_driver);
  }
}
