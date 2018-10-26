#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "io_pipe.h"
#include "io_timer.h"

#define NUM_PIPE_TESTS      0

typedef struct
{
  io_pipe_t       pipe;
  int             id;
  uint8_t         buf[128];
} pipe_test_work_t;

static io_pipe_return_t pipe_callback(io_pipe_t* p, io_pipe_event_t* e);
static io_pipe_return_t pipe_callback2(io_pipe_t* p, io_pipe_event_t* e);

static const char* TAG = "main";

static io_driver_t        io_driver;
static io_timer_t         io_timer;

static pipe_test_work_t _works[NUM_PIPE_TESTS];

static pipe_test_work_t _work2;

static SoftTimerElem      tx_tmr;

static void
start_pipe_test_work(pipe_test_work_t* work)
{
  char    buf[128];
  char*   argv[4] = 
  {
    "bash",
    "./test/test.sh",
    buf,
    NULL,
  };

  sprintf(buf, "%d", work->id);

  printf("starting pipe work %s\n", buf);

  if(io_pipe_init(&io_driver, &work->pipe, pipe_callback, "/bin/bash", argv) != 0)
  {
    LOGI(TAG, "io_pipe_init failed\n");
  }
  io_pipe_set_rx_buf(&work->pipe, work->buf, 128);
}

static void
tx_to_work2(pipe_test_work_t* work)
{
  char    buf[128];
  static int count = 0;

  sprintf(buf, "%d -> Hello World Count: %d\n", work->id, count);

  count++;

  io_pipe_tx(&work->pipe, (uint8_t*)buf, strlen(buf));
  io_timer_start(&io_timer, &tx_tmr, 1000);
}

static void
start_pipe_test_work2(pipe_test_work_t* work)
{
  char    buf[128];
  char*   argv[4] = 
  {
    "bash",
    "./test/test2.sh",
    buf,
    NULL,
  };

  sprintf(buf, "%d", work->id);

  printf("starting pipe work %s\n", buf);

  if(io_pipe_init(&io_driver, &work->pipe, pipe_callback2, "/bin/bash", argv) != 0)
  {
    LOGI(TAG, "io_pipe_init failed\n");
  }
  io_pipe_set_rx_buf(&work->pipe, work->buf, 128);

  tx_to_work2(work);
}

static io_pipe_return_t
pipe_callback(io_pipe_t* p, io_pipe_event_t* e)
{
  pipe_test_work_t* work = container_of(p, pipe_test_work_t, pipe);

  switch(e->ev)
  {
  case io_pipe_event_rx:
    for(int i = 0; i < e->len; i++)
    {
      printf("%c", (char)e->buf[i]);
    }
    fflush(stdout);
    break;

  case io_pipe_event_tx:
    break;

  case io_pipe_event_closed:
    io_timer_stop(&io_timer, &tx_tmr);
    io_pipe_close(p);
    start_pipe_test_work(work);
    break;
  }

  return io_pipe_return_continue;
}

static io_pipe_return_t
pipe_callback2(io_pipe_t* p, io_pipe_event_t* e)
{
  pipe_test_work_t* work = container_of(p, pipe_test_work_t, pipe);

  switch(e->ev)
  {
  case io_pipe_event_rx:
    for(int i = 0; i < e->len; i++)
    {
      printf("%c", (char)e->buf[i]);
    }
    fflush(stdout);
    break;

  case io_pipe_event_tx:
    break;

  case io_pipe_event_closed:
    io_pipe_close(p);
    start_pipe_test_work2(work);
    break;
  }

  return io_pipe_return_continue;
}

static void
tx_timeout(SoftTimerElem* te)
{
  tx_to_work2(&_work2);
}

static void
init_pipe_test_works(void)
{
  for(int i = 0; i < NUM_PIPE_TESTS; i++)
  {
    _works[i].id = i;
  }

  for(int i = 0; i < NUM_PIPE_TESTS; i++)
  {
    start_pipe_test_work(&_works[i]);
  }

  soft_timer_init_elem(&tx_tmr);
  tx_tmr.cb = tx_timeout;

  _work2.id = 10;
  start_pipe_test_work2(&_work2);
}

int
main(int argc, char** argv)
{
  LOGI(TAG, "starting pipe test\n");

  io_driver_init(&io_driver);
  io_timer_init(&io_driver, &io_timer, 100);

  init_pipe_test_works();

  while(1)
  {
    io_driver_run(&io_driver);
  }
  return 0;
}
