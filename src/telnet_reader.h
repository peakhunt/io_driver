#ifndef __TELNET_READER_DEF_H__
#define __TELNET_READER_DEF_H__

#include "common_def.h"

#define TELNET_READER_BUFFER_SIZE       32

struct __telnet_reader;
typedef struct __telnet_reader telnet_reader_t;

typedef enum
{
  telnet_reader_state_start,
  telnet_reader_state_rx_iac,
  telnet_reader_state_rx_do_dont_will_wont,
  telnet_reader_state_rx_sb,
  telnet_reader_state_wait_for_se,
} telnet_reader_state_t;

//
// return 0 : continue
//       -1 : abort
//
typedef int (*telnet_data_callback)(telnet_reader_t* tr, uint8_t data);
typedef int (*telnet_cmd_callback)(telnet_reader_t* tr);

struct __telnet_reader
{
  uint8_t                 buffer[TELNET_READER_BUFFER_SIZE];
  uint8_t                 buf_ndx;

  telnet_reader_state_t   state;
  uint8_t                 command;
  uint8_t                 opt;

  uint8_t                 finished;

  telnet_data_callback    databack;
  telnet_cmd_callback     cmdback;
};

extern void telnet_reader_init(telnet_reader_t* tr);
extern void telnet_reader_deinit(telnet_reader_t* tr);
extern int telnet_reader_feed(telnet_reader_t* tr, uint8_t c);

#endif /* !__TELNET_READER_DEF_H__ */
