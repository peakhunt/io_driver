#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#include "io_timer.h"

///////////////////////////////////////////////////////////////////////////////
//
// I/O driver callbacks
//
///////////////////////////////////////////////////////////////////////////////
static void
io_timer_tick_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_timer_t*     t = container_of(w, io_timer_t, watcher);
  uint64_t        v;

  read(t->timerfd, &v, sizeof(v));
  (void)v;

  soft_timer_drive(&t->st);
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
void
io_timer_init(io_driver_t* driver, io_timer_t* t, int tickrate)
{
  struct itimerspec ts;

  t->timerfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
  if(t->timerfd < 0)
  {
    LOGE("io_timer", "%s failed to timerfd_create\n", __func__);
    return;
  }

  ts.it_interval.tv_sec     = 0;
  ts.it_interval.tv_nsec    = tickrate * 1000000;   // ms
  ts.it_value               = ts.it_interval;

  timerfd_settime(t->timerfd, 0, &ts, NULL);

  t->driver = driver;
  soft_timer_init(&t->st, tickrate);

  io_driver_watcher_init(&t->watcher, t->timerfd, io_timer_tick_callback);
  io_driver_watch(driver, &t->watcher, IO_DRIVER_EVENT_RX);
}

void
io_timer_deinit(io_timer_t* t)
{
  io_driver_no_watch(t->driver, &t->watcher, IO_DRIVER_EVENT_RX);
  close(t->timerfd);
}
