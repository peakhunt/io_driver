#include "telnet.h"
#include "telnet_reader.h"

static inline void
telnet_reader_move_state(telnet_reader_t* tr, telnet_reader_state_t s)
{
  tr->state   = s;
}

static bool
telnet_reader_handle_command(telnet_reader_t* tr, uint8_t d)
{
  tr->command = d;
  switch(d)
  {
  case DO:
  case DONT:
  case WILL:
  case WONT:
    telnet_reader_move_state(tr, telnet_reader_state_rx_do_dont_will_wont);
    break;

  case SB:
    tr->buf_ndx = 0;
    telnet_reader_move_state(tr, telnet_reader_state_rx_sb);
    break;

  default:
    // invalid syntax
    return FALSE;
  }
  return TRUE;
}

static bool
telnet_reader_handle_subcommand(telnet_reader_t* tr, uint8_t d)
{
  switch(d)
  {
  case IAC:
    // could be buggy but who cares
    telnet_reader_move_state(tr, telnet_reader_state_wait_for_se);
    break;

  default:
    if(tr->buf_ndx < TELNET_READER_BUFFER_SIZE)
    {
      tr->buffer[tr->buf_ndx++] = d;
    }
    else
    {
      return FALSE;
    }
    break;
  }
  return TRUE;
}

static int
telnet_reader_state_machine(telnet_reader_t* tr, uint8_t d)
{
  int ret = 0;

  switch(tr->state)
  {
  case telnet_reader_state_start:
    if(d == IAC)
    {
      telnet_reader_move_state(tr, telnet_reader_state_rx_iac);
    }
    else
    {
      ret = tr->databack(tr, d);
    }
    break;

  case telnet_reader_state_rx_iac:
    if(d == IAC)
    {
      telnet_reader_move_state(tr, telnet_reader_state_start);
      ret = tr->databack(tr, IAC);
    }
    else
    {
      if(telnet_reader_handle_command(tr, d) == FALSE)
      {
        // discard
        telnet_reader_move_state(tr, telnet_reader_state_start);
      }
    }
    break;

  case telnet_reader_state_rx_do_dont_will_wont:
    tr->opt = d;
    ret = tr->cmdback(tr);
    telnet_reader_move_state(tr, telnet_reader_state_start);
    break;

  case telnet_reader_state_rx_sb:
    if(telnet_reader_handle_subcommand(tr, d) == FALSE)
    {
      telnet_reader_move_state(tr, telnet_reader_state_start);
    }
    break;

  case telnet_reader_state_wait_for_se:
    if(d == SE)
    {
      ret = tr->cmdback(tr);
    }
    telnet_reader_move_state(tr, telnet_reader_state_start);
    break;
  }
  return ret;
}

void
telnet_reader_init(telnet_reader_t* tr)
{
  tr->state     = telnet_reader_state_start;
  tr->buf_ndx   = 0;
}

void
telnet_reader_deinit(telnet_reader_t* tr)
{
}

int
telnet_reader_feed(telnet_reader_t* tr, uint8_t c)
{
  return telnet_reader_state_machine(tr, c);
}
