#ifndef __IO_PIPE_DEF_H__
#define __IO_PIPE_DEF_H__

#include "io_driver.h"

typedef enum
{
  io_pipe_event_rx,
  io_pipe_event_tx,
  io_pipe_event_closed,
} io_pipe_event_enum_t;

struct __io_pipe_t;
typedef struct __io_pipe_t io_pipe_t;

typedef struct
{
  io_pipe_event_enum_t  ev;
  io_pipe_t*            p;
  uint8_t*              buf;
  uint32_t              len;
} io_pipe_event_t;

typedef enum
{
  io_pipe_return_continue,
  io_pipe_return_stop,
} io_pipe_return_t;

typedef io_pipe_return_t (*io_pipe_callback)(io_pipe_t* p, io_pipe_event_t* e);

struct __io_pipe_t
{
  int                 pipe_r;
  int                 pipe_w;

  io_pipe_callback    cb;

  io_driver_watcher_t rw;
  io_driver_watcher_t tw;

  io_driver_t*        driver;

  uint8_t*            rx_buf;
  int                 rx_size;

  pid_t               child;
};

extern int io_pipe_init(io_driver_t* driver, io_pipe_t* p, 
    io_pipe_callback cb,
    const char* prog,
    char* const argv[]);

extern void io_pipe_close(io_pipe_t* p);
extern int io_pipe_tx(io_pipe_t* p, uint8_t* buf, int len);

static inline void
io_pipe_set_rx_buf(io_pipe_t* p, uint8_t* rx_buf, int rx_size)
{
  p->rx_buf   = rx_buf;
  p->rx_size  = rx_size;
}

#endif /* !__IO_PIPE_DEF_H__ */
