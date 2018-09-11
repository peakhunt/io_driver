#include "io_dns.h"

static const char* TAG = "dns";

///////////////////////////////////////////////////////////////////////////////
//
// callbacks
//
///////////////////////////////////////////////////////////////////////////////
static io_net_return_t
io_dns_rx_callback(io_net_t* n, io_net_event_t* e)
{
  io_dns_t*             d = container_of(n, io_dns_t, n);
  dns_util_host_addrs_t a;
  io_dns_event_t        ev;

  memset(&ev, 0, sizeof(ev));

  LOGI(TAG, "read %d bytes\n", e->r.len);
  dns_util_reset(&d->du, e->r.buf, e->r.len);
  if(dns_util_parse_A_response(&d->du, d->qlen, &a) != 0)
  {
    ev.evt_type = io_dns_event_error;
  }
  else
  {
    ev.evt_type = io_dns_event_got_result;
    ev.addrs = &a;
  }

  io_net_close(&d->n);
  io_timer_stop(d->t, &d->timer);

  d->cb(d, &ev);

  return io_net_return_continue;
}

static void
io_dns_timeout_callback(SoftTimerElem* te)
{
  io_dns_t*         d = container_of(te, io_dns_t, timer);
  io_dns_event_t    ev;

  io_net_close(&d->n);

  memset(&ev, 0, sizeof(ev));
  ev.evt_type = io_dns_event_timedout;

  d->cb(d, &ev);
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
int
io_dns_lookup_ipv4(io_driver_t* driver, io_timer_t* t, io_dns_t* d, const io_dns_cfg_t* cfg,
    const char* target, io_dns_callback cb)
{
  int                   req_len;
  struct sockaddr_in    to;

  if(io_net_udp(driver, &d->n, 0, io_dns_rx_callback) != 0)
  {
    LOGE(TAG, "io_net_udp failed\n");
    return -1;
  }

  soft_timer_init_elem(&d->timer);
  d->timer.cb = io_dns_timeout_callback;

  d->cb = cb;
  d->t  = t;

  io_net_set_rx_buf(&d->n, d->rx_buf, IO_DNS_RX_BUF_SIZE);

  dns_util_reset(&d->du, d->rx_buf, IO_DNS_RX_BUF_SIZE);
  req_len = dns_util_build_A_query(&d->du, target);
  if(req_len < 0)
  {
    LOGE(TAG, "dns_util_build_A_query failed\n");
    io_net_close(&d->n);
    return -1;
  }
  LOGI(TAG, "requesting. %d bytes\n", req_len);

  d->qlen = req_len;

  memset(&to, 0, sizeof(to));
  to.sin_family       = AF_INET;
  to.sin_addr.s_addr  = inet_addr(cfg->server);
  to.sin_port         = htons(cfg->port);

  io_net_udp_tx(&d->n, &to, d->rx_buf, req_len);
  io_timer_start(t, &d->timer, cfg->timeout * 1000);

  return 0;
}
