#ifndef __IO_TIMER_DEF_H__
#define __IO_TIMER_DEF_H__

#include "io_driver.h"
#include "soft_timer.h"

typedef struct
{
  io_driver_t*        driver;
  io_driver_watcher_t watcher;
  SoftTimer           st;
  int                 timerfd;
} io_timer_t;

extern void io_timer_init(io_driver_t* driver, io_timer_t* t, int tickrate);
extern void io_timer_deinit(io_timer_t* t);

static inline void
io_timer_start(io_timer_t* t, SoftTimerElem* e, int expires)
{
  soft_timer_add(&t->st, e, expires);
}

static inline void
io_timer_stop(io_timer_t* t, SoftTimerElem* e)
{
  soft_timer_del(&t->st, e);
}

static inline void
io_timer_restart(io_timer_t* t, SoftTimerElem* e, int expires)
{
  io_timer_stop(t, e);
  io_timer_start(t, e, expires);
}

#endif /* !__IO_TIMER_DEF_H__ */
