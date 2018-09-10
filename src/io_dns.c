#include "io_dns.h"

static const char* TAG = "dns";

///////////////////////////////////////////////////////////////////////////////
//
// DNS packet format
//
// https://www2.cs.duke.edu/courses/fall16/compsci356/DNS/DNS-primer.pdf
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// some DNS related definitions
//
///////////////////////////////////////////////////////////////////////////////
#define T_A       1             // A record (IPv4 address)
#define T_NS      2             // nameserver
#define T_CNAME   5             // cname
#define T_PTR     12
#define T_MX      15

//
// FIXME endian!!!
//
struct dns_header
{
  uint16_t      id;

  uint16_t      rd:1;       // recursion
  uint16_t      tc:1;       // truncated
  uint16_t      aa:1;       // authoritative answer
  uint16_t      opcode:4;
  uint16_t      qr:1;       // query/response
  
  uint16_t      rcode:4;    // response code
  uint16_t      cd:1;       // check disabled
  uint16_t      ad:1;       // authenticated data
  uint16_t      z:1;
  uint16_t      ra:1;       // recursion available

  uint16_t      q_count;    // number of questions
  uint16_t      ans_count;  // number of answers
  uint16_t      auth_count; // number of authorities
  uint16_t      add_count;  // number of resource entries
};

struct dns_question
{
  uint16_t    qtype;
  uint16_t    qclass;
};

#pragma pack(push, 1)
struct dns_r_data
{
  uint16_t    type;
  uint16_t    _class;
  uint32_t    ttl;
  uint16_t    data_len;
};
#pragma pack(pop)

struct dns_res_record
{
  unsigned char*        name;
  struct dns_r_data*    resource;
  unsigned char*        rdata;
};

struct dns_query
{
  uint8_t*              name;
  struct dns_question*  question;
};

///////////////////////////////////////////////////////////////////////////////
//
// DNS utilities
//
///////////////////////////////////////////////////////////////////////////////
static inline int
build_request_query(uint8_t* buf)
{
  struct dns_header*    h;

  h = (struct dns_header*)buf;
  h->id           = 0;        // FIXME
  h->qr           = 0;        // query
  h->opcode       = 0;        // standard query
  h->aa           = 0;        // not authoritative
  h->tc           = 0;        // not truncated
  h->rd           = 1;        // recursion desired
  h->ra           = 0;        // recursion not available
  h->z            = 0;
  h->ad           = 0;
  h->cd           = 0;
  h->rcode        = 0;
  h->q_count      = htons(1);
  h->ans_count    = 0;
  h->auth_count   = 0;
  h->add_count    = 0;

  return sizeof(struct dns_header);
}

static inline int
build_qname(uint8_t* buf, const char* host)
{
  uint8_t*    len_ptr;
  uint8_t*    b = buf;

  len_ptr     = b++;
  (*len_ptr)  = 0;

  for(int i = 0; i < strlen(host); i++)
  {
    if(host[i] == '.')
    {
      len_ptr     = b++;
      (*len_ptr)  = 0;
    }
    else
    {
      *b++ = (uint8_t)host[i];
      (*len_ptr)  += 1;
    }
  }
  *b++ = '\0';

  return (int)(b - buf);
}

static inline int
io_dns_build_A_query(uint8_t* buf, const char* host, int qtype)
{
  uint8_t*              qname;
  struct dns_question*  qinfo;
  int                   l = 0;

  l = build_request_query(buf);

  qname = (uint8_t*)&buf[l];
  l += build_qname(qname, host);

  qinfo           = (struct dns_question*)&buf[l];
  qinfo->qtype    = htons(qtype);
  qinfo->qclass   = htons(1);
  l += sizeof(struct dns_question);

  return l;
}

static inline int
parse_qname(uint8_t* buf)
{
  uint8_t*    b = buf;

  while(*b != '\0')
  {
    if(*b == 0xc0)
    {
      // pointer
      LOGI(TAG, "XXX pointer\n");
      b += 2;
      return (int)(b - buf);
    }
    else
    {
      b += (*b + 1);
    }
  }
  b++;

  return (int)(b - buf);
}

static inline int
io_dns_parse_A_response(io_dns_t* d, uint8_t* buf, int len, io_dns_host_addrs_t* addrs)
{
  struct dns_header*    h;
  int                   ans_count;
  uint8_t*              b = buf;
  struct dns_r_data*    r;
  int                   l;

  h = (struct dns_header*)b;
  ans_count = ntohs(h->ans_count);

  LOGI(TAG, "ans_count : %d\n", ans_count);
  if(ans_count < 1)
  {
    LOGE(TAG, "ans_count is %d\n", ans_count);
    return -1;
  }

  addrs->n_addrs = 0;

  b += d->qlen;
  // b += sizeof(struct dns_header);

  for(int i = 0; i < ans_count; i++)
  {
    l = parse_qname(b);
    LOGI(TAG, "qname length is %d\n", l);

    b = b + l;

    r = (struct dns_r_data*)b;
    b += sizeof(struct dns_r_data);

    if(ntohs(r->type) == 0x0001 && ntohs(r->_class) == 0x0001)
    {
      LOGI(TAG, "ttl: %u\n", ntohl(r->ttl));
      if(ntohs(r->data_len) != 4)
      {
        LOGE(TAG, "invalid RDLENGTH %d\n", ntohs(r->data_len));
        return -1;
      }

      // ipv4 address. internet
      if(addrs->n_addrs >= IO_DNS_MAX_HOST_ADDRESSES)
      {
        LOGE(TAG, "%s overflow...\n", __func__);
      }
      else
      {
        addrs->addrs[addrs->n_addrs][0] = b[0];
        addrs->addrs[addrs->n_addrs][1] = b[1];
        addrs->addrs[addrs->n_addrs][2] = b[2];
        addrs->addrs[addrs->n_addrs++][3] = b[3];
      }
    }
    else
    {
      LOGI(TAG, "unhandled type or class %d, %d\n", ntohs(r->type), ntohs(r->_class));
    }

    b += ntohs(r->data_len);
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// callbacks
//
///////////////////////////////////////////////////////////////////////////////
static io_net_return_t
io_dns_rx_callback(io_net_t* n, io_net_event_t* e)
{
  io_dns_t*             d = container_of(n, io_dns_t, n);
  io_dns_host_addrs_t   a;
  io_dns_event_t    ev;

  memset(&ev, 0, sizeof(ev));

  if(io_dns_parse_A_response(d, e->r.buf, e->r.len, &a) != 0)
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
    return -1;
  }

  soft_timer_init_elem(&d->timer);
  d->timer.cb = io_dns_timeout_callback;

  d->cb = cb;
  d->t  = t;

  io_net_set_rx_buf(&d->n, d->rx_buf, IO_DNS_RX_BUF_SIZE);

  req_len = io_dns_build_A_query(d->rx_buf, target, T_A);
  d->qlen = req_len;

  memset(&to, 0, sizeof(to));
  to.sin_family       = AF_INET;
  to.sin_addr.s_addr  = inet_addr(cfg->server);
  to.sin_port         = htons(cfg->port);

  io_net_udp_tx(&d->n, &to, d->rx_buf, req_len);
  io_timer_start(t, &d->timer, cfg->timeout * 1000);

  return 0;
}
