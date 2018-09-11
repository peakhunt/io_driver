#ifndef __DNS_UTIL_DEF_H__
#define __DNS_UTIL_DEF_H__

#include "common_def.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define DNS_UTIL_MAX_HOST_ADDRESSES         4

typedef struct
{
  uint8_t     n_addrs;
  in_addr_t   addrs[DNS_UTIL_MAX_HOST_ADDRESSES];
} dns_util_host_addrs_t;

typedef struct
{
  uint8_t*    buf;
  int         buf_len;
  int         write_ndx;
  int         read_ndx;
} dns_util_t;

extern int dns_util_build_A_query(dns_util_t* d, const char* target);
extern int dns_util_parse_A_response(dns_util_t* d, int qlen, dns_util_host_addrs_t* addrs);

static inline void
dns_util_reset(dns_util_t* d, uint8_t* buf, int len)
{
  d->write_ndx  = 0;
  d->read_ndx   = 0;
  d->buf_len    = len;
  d->buf        = buf;
}
  

#endif /* !__DNS_UTIL_DEF_H__ */
