#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "io_pipe.h"

static const char* TAG = "io_pipe";

///////////////////////////////////////////////////////////////////////////////
//
// I/O Driver callbacks
//
///////////////////////////////////////////////////////////////////////////////
static void
io_pipe_rx_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_pipe_t*        p = container_of(w, io_pipe_t, rw);
  io_pipe_event_t   ev;
  int               ret;

  if((e & IO_DRIVER_EVENT_RX) == 0)
  {
    return;
  }

  ret = read(p->pipe_r, p->rx_buf, p->rx_size);
  if(ret <= 0)
  {
    int status;

    LOGI(TAG, "pipe read returns <= 0 %d\n", ret);
    ev.ev = io_pipe_event_closed;

    //
    // XXX
    // assumption here is 
    // a child program is supposed to exit immediately if it closes stdout
    //
    waitpid(p->child, &status, 0);
  }
  else
  {
    ev.ev = io_pipe_event_rx;
    ev.buf = p->rx_buf;
    ev.len = (uint32_t)ret;
  }

  (void)p->cb(p, &ev);
  return;
}

static void
io_pipe_tx_callback(io_driver_watcher_t* w, io_driver_event e)
{
  io_pipe_t*        p = container_of(w, io_pipe_t, tw);
  io_pipe_event_t   ev;

  if((e & IO_DRIVER_EVENT_TX) == 0)
  {
    return;
  }

  LOGI(TAG, "pipe tx event\n");

  ev.ev = io_pipe_event_tx;

  io_driver_no_watch(p->driver, &p->tw, IO_DRIVER_EVENT_TX);

  p->cb(p, &ev);
}

///////////////////////////////////////////////////////////////////////////////
//
// public interfaces
//
///////////////////////////////////////////////////////////////////////////////
int
io_pipe_init(io_driver_t* driver, io_pipe_t* p, io_pipe_callback cb,
    const char* prog, char* const argv[])
{
  pid_t   ret;
  int     pipe_r[2], pipe_w[2];

  if(pipe(pipe_r) != 0)
  {
    return -1;
  }

  if(pipe(pipe_w) != 0)
  {
    close(pipe_r[0]);
    close(pipe_r[1]);
    return 1;
  }

  fcntl(pipe_r[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipe_r[1], F_SETFD, FD_CLOEXEC);

  fcntl(pipe_w[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipe_w[1], F_SETFD, FD_CLOEXEC);

  ret = fork();

  if(ret == -1)
  {
    //
    // error
    //
    close(pipe_r[0]);
    close(pipe_r[1]);
    close(pipe_w[0]);
    close(pipe_w[1]);
    return -1;
  }

  if(ret == 0)
  {
    //
    // child
    //
    close(pipe_r[0]);
    close(pipe_w[1]);

    dup2(pipe_w[0], 0);
    dup2(pipe_r[1], 1);
    dup2(pipe_r[1], 2);

    //
    // exec now
    //
    execv(prog, argv);

    // reaching here means an error
    exit(-1);
  }

  // parent
  p->child = ret;

  p->pipe_r = pipe_r[0];
  close(pipe_r[1]);

  p->pipe_w = pipe_w[1];
  close(pipe_w[0]);

  fcntl(p->pipe_r, F_SETFL, fcntl(p->pipe_r, F_GETFL, 0) | O_NONBLOCK);
  fcntl(p->pipe_w, F_SETFL, fcntl(p->pipe_w, F_GETFL, 0) | O_NONBLOCK);

  p->cb = cb;

  io_driver_watcher_init(&p->rw, p->pipe_r, io_pipe_rx_callback);
  io_driver_watcher_init(&p->tw, p->pipe_w, io_pipe_tx_callback);

  io_driver_watch(driver, &p->rw, IO_DRIVER_EVENT_RX);
  return 0;
}

void
io_pipe_close(io_pipe_t* p)
{
  io_driver_no_watch(p->driver, &p->rw, IO_DRIVER_EVENT_RX);
  io_driver_no_watch(p->driver, &p->tw, IO_DRIVER_EVENT_TX);

  close(p->pipe_r);
  close(p->pipe_w);
}

int
io_pipe_tx(io_pipe_t* p, uint8_t* buf, int len)
{
  int ret;

  ret = write(p->pipe_w, buf, len);
  if(ret <= 0)
  {
    if(!(errno == EWOULDBLOCK || errno == EAGAIN))
    {
      LOGI(TAG, "io_pipe_tx error %d\n", errno);
      return -1;
    }

    LOGI(TAG, "pipe schedling tx event\n");

    io_driver_watch(p->driver, &p->tw, IO_DRIVER_EVENT_TX);
    return 0;
  }
  return ret;
}
