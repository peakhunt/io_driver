#include <stdio.h>
#include <stdlib.h>

#include "io_dns.h"

static void dns_callback(io_dns_t* d, io_dns_event_t* e);

static const char* TAG = "main";
static io_dns_cfg_t   _cfg = 
{
  "8.8.8.8",
  53,
  10,
};

static io_driver_t        io_driver;
static io_timer_t         io_timer;
static io_dns_t           io_dns;
static int                count = 0;
static char*              target;

static void 
dns_callback(io_dns_t* d, io_dns_event_t* e)
{
  LOGI(TAG, "============== %d ========\n", count);
  LOGI(TAG, "dns_callback\n");

  switch(e->evt_type)
  {
  case io_dns_event_got_result:
    LOGI(TAG, "got something\n");
    for(int i = 0; i < e->addrs->n_addrs; i++)
    {
      struct in_addr    in;

      in.s_addr = e->addrs->addrs[i];
      LOGI(TAG, "result %d: %s\n",
          i,
          inet_ntoa(in));
    }
    break;
  case io_dns_event_error:
    LOGI(TAG, "error\n");
    break;

  case io_dns_event_timedout:
    LOGI(TAG, "timeout\n");
    break;
  }

  count++;
  if(count >= 5)
  {
    exit(0);
  }

  io_dns_lookup_ipv4(&io_driver, &io_timer, &io_dns, &_cfg, target, dns_callback);
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

  target = argv[1];

  io_driver_init(&io_driver);
  io_timer_init(&io_driver, &io_timer, 100);

  LOGI(TAG, "looking up %s\n", argv[1]);

  io_dns_lookup_ipv4(&io_driver, &io_timer, &io_dns, &_cfg, target, dns_callback);

  while(1)
  {
    io_driver_run(&io_driver);
  }
}
