#include "dns_util.h"

static const char* TAG = "dns_util";

///////////////////////////////////////////////////////////////////////////////
//
// DNS packet format
//
// https://www2.cs.duke.edu/courses/fall16/compsci356/DNS/DNS-primer.pdf
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
// basic utilities
//
///////////////////////////////////////////////////////////////////////////////
static inline uint32_t
dns_util_space_left(dns_util_t* d)
{
  return d->buf_len - d->write_ndx;
}

static inline uint32_t
dns_util_data_left(dns_util_t* d)
{
  return d->buf_len - d->read_ndx;
}

static inline uint8_t*
dns_util_current(dns_util_t* d)
{
  return &d->buf[d->write_ndx];
}

static inline uint8_t*
dns_util_write_byte(dns_util_t* d, uint8_t b)
{
  uint8_t* p;
  
  if(dns_util_space_left(d) < 1)
  {
    return NULL;
  }

  p   = &d->buf[d->write_ndx];
  *p  = b;
  d->write_ndx++;

  return p;
}

static inline uint8_t*
dns_util_read(dns_util_t* d, int len)
{
  uint8_t* p;

  if(dns_util_data_left(d) < len)
  {
    return NULL;
  }

  p = &d->buf[d->read_ndx];
  d->read_ndx += len;

  return p;
}

///////////////////////////////////////////////////////////////////////////////
//
// dns message utilities
//
///////////////////////////////////////////////////////////////////////////////
static int
dns_util_build_request_query(dns_util_t* d)
{
  struct dns_header*    h;

  if(dns_util_space_left(d) < sizeof(struct dns_header))
  {
    return -1;
  }

  h = (struct dns_header*)&d->buf[d->write_ndx];;

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

  d->write_ndx += sizeof(struct dns_header);
  return 0;
}

static int
dns_util_build_qname(dns_util_t* d, const char* host)
{
  uint8_t*    len_ptr;

  //
  // start with zero length
  //
  if((len_ptr = dns_util_write_byte(d, 0)) == NULL)
  {
    return -1;
  }

  for(int i = 0; i < strlen(host); i++)
  {
    if(host[i] == '.')
    {
      if((len_ptr = dns_util_write_byte(d, 0)) == NULL)
      {
        return -1;
      }
    }
    else
    {
      if(dns_util_write_byte(d, (uint8_t)host[i]) == NULL)
      {
        return -1;
      }
      (*len_ptr)  += 1;
    }
  }

  if(dns_util_write_byte(d, (uint8_t)'\0') == NULL)
  {
    return -1;
  }

  return 0;
}

static inline int
dns_util_parse_qname(dns_util_t* d)
{
  uint8_t*    b;

  while(1)
  {
    b = dns_util_read(d, 1);
    if(b == NULL)
    {
      return -1;
    }

    if(*b == '\0')
    {
      break;
    }

    if((*b & 0xc0) == 0xc0)
    {
      // pointer
      LOGI(TAG, "XXX pointer\n");
      if(dns_util_read(d, 1) == NULL)
      {
        return -1;
      }
      return 0;
    }
    else
    {
      // read data
      if(dns_util_read(d, *b) == NULL)
      {
        return -1;
      }
    }
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// DNS utilities
//
///////////////////////////////////////////////////////////////////////////////
int
dns_util_build_A_query(dns_util_t* d, const char* target)
{
  struct dns_question*    qinfo;

  if(dns_util_build_request_query(d) != 0)
  {
    return -1;
  }

  if(dns_util_build_qname(d, target) != 0)
  {
    return -1;
  }

  if(dns_util_space_left(d) < sizeof(struct dns_question))
  {
    return -1;
  }

  qinfo = (struct dns_question*)&d->buf[d->write_ndx];
  qinfo->qtype    = htons(T_A);
  qinfo->qclass   = htons(1);

  d->write_ndx += sizeof(struct dns_question);

  return d->write_ndx;
}

int
dns_util_parse_A_response(dns_util_t* d, int qlen, dns_util_host_addrs_t* addrs)
{
  struct dns_header*    h;
  int                   ans_count;
  struct dns_r_data*    r;
  uint32_t*             data;

  if((h = (struct dns_header*)dns_util_read(d, sizeof(struct dns_header))) == NULL)
  {
    return -1;
  }
  ans_count = ntohs(h->ans_count);

  LOGI(TAG, "ans_count : %d\n", ans_count);
  if(ans_count < 1)
  {
    LOGE(TAG, "ans_count is %d\n", ans_count);
    return -1;
  }

  addrs->n_addrs = 0;

  if(dns_util_read(d, qlen - sizeof(struct dns_header)) == NULL)
  {
    return -1;
  }

  for(int i = 0; i < ans_count; i++)
  {
    if(dns_util_parse_qname(d) != 0)
    {
      return -1;
    }

    r = (struct dns_r_data*)dns_util_read(d, sizeof(struct dns_r_data));
    if(r == NULL)
    {
      return -1;
    }

    if(ntohs(r->type) == 0x0001 && ntohs(r->_class) == 0x0001)
    {
      LOGI(TAG, "ttl: %u\n", ntohl(r->ttl));
      if(ntohs(r->data_len) != 4)
      {
        LOGE(TAG, "invalid RDLENGTH %d\n", ntohs(r->data_len));
        return -1;
      }

      data = (uint32_t*)dns_util_read(d, 4);

      if(data == NULL)
      {
        return -1;
      }

      // ipv4 address. internet
      if(addrs->n_addrs >= DNS_UTIL_MAX_HOST_ADDRESSES)
      {
        LOGE(TAG, "%s overflow...\n", __func__);
      }
      else
      {
        addrs->addrs[addrs->n_addrs++] = *(data);
      }
    }
    else
    {
      LOGI(TAG, "unhandled type or class %d, %d\n", ntohs(r->type), ntohs(r->_class));
      if(dns_util_read(d, ntohs(r->data_len)) == NULL)
      {
        return -1;
      }
    }
  }
  return 0;
}
