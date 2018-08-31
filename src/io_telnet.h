#ifndef __IO_TELNET_DEF_H__
#define __IO_TELNET_DEF_H__

#include "io_net.h"

extern void io_telnet_init(void);

extern io_net_t* io_telnet_bind(io_driver_t* driver, int port, io_net_callback cb);
extern io_net_t* io_telnet_connect(io_driver_t* driver, const char* ip_addr, int port, io_net_callback cb);
extern void io_telnet_close(io_net_t* n);
extern int io_telnet_tx(io_net_t* n, uint8_t* buf, int len);

#endif /* !__IO_TELNET_DEF_H__ */
