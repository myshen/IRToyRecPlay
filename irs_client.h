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

// Fetch the IR toy's next requested packet size
// This applies when the toy is in IRs transmit mode
unsigned int IRs_get_next_packet_size(int fd, BOOL use_handshake);

// Print the given command sequence to the stream
void IRs_fprint(FILE *stream, char *command, unsigned int len);

// Receive an IR command from the IR toy into the space pointed to by command.
// Returns the number of bytes received.
// This command does not block until it receives something. Call it in a loop
// if you need to make sure to read a command.
int IRs_rx(int fd, float resolution, IRBYTE **command);

// Transmit the given buffer using the IR toy while taking into account the IR
// toy's packet size.
int IRs_tx(int fd, char *buffer, unsigned int buflen);

void IRs_enter_transmit_unit(int fd);

// Set sample mode options
// Used to ask for handshaking and reporting for TX.
void IRs_set_sample_options(int fd);

// Generic playback function
// Modify how it plays files by passing the custom file player
void IRs_play(char *param_fname, int fd, char *param_delay, int (* play_file)(char *, int));

#endif // IRS_CLIENT_H_INCLUDED
