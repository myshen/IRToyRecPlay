#ifndef IRS_CLIENT_H_INCLUDED
#define IRS_CLIENT_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32

#include <conio.h>
#include <windef.h>

#else

#include <sys/select.h>
#include <stdbool.h>
#include <stdint.h>

#endif

#include "config.h"
#include "serial.h"
#include "kbhit.h"

extern int verbose;
extern BOOL useHandshake;
extern char countreq;
extern char completereq;

typedef uint16_t IRCODE;
typedef uint8_t IRBYTE;

#define E_IRBIN_OUTPUT 1
#define E_IRBIN_NO_MEM 2
#define E_IRBIN_XMIT 3
#define E_IRBIN_NO_PACKET_SIZE 4
#define E_IR_NO_RESPONSE 5

// Enter IRs sample mode and fetch returned sample mode version.
int IRs_begin(int fd, char **ret_version_buffer);

// Fetch IR toy firmware version
int IRs_get_firmware_version(int fd, char **ret_version_buffer);

unsigned int IRs_get_next_packet_size(int fd, BOOL use_handshake);

int IRs_tx_buffer(int fd, char *buffer, unsigned int buflen);


void IRs_play(char *param_fname, int fd, char *param_delay, int (* play_file)(char *, int));

#endif // IRS_CLIENT_H_INCLUDED
