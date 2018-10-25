#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "io_pipe.h"

#define NUM_PIPE_TESTS      1

typedef struct
{
  io_pipe_t       pipe;
  int             id;
  uint8_t         buf[128];
} pipe_test_work_t;

static io_pipe_return_t pipe_callback(io_pipe_t* p, io_pipe_event_t* e);

static const char* TAG = "main";
static io_driver_t        io_driver;

static pipe_test_work_t _works[NUM_PIPE_TESTS];

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
    io_pipe_close(p);
    start_pipe_test_work(work);
    break;
  }

  return io_pipe_return_continue;
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
}

int
main(int argc, char** argv)
{
  LOGI(TAG, "starting pipe test\n");
  io_driver_init(&io_driver);

  init_pipe_test_works();

  while(1)
  {
    io_driver_run(&io_driver);
  }
  return 0;
}
