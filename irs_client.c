#include "irs_client.h"

extern int verbose;
extern BOOL useHandshake;
extern BOOL countreq;
extern BOOL completereq;

#undef RETRIES
#define RETRIES 2

#define IRS_READ_BUFSIZE 512

#define IRS_NO_PACKET_SIZE 0

int IRs_begin(int fd, char **ret_version_buffer)
{
    unsigned int cnt = 0;
    char irs_version_buffer[4] = {0};
    int nrbytes;
    unsigned int i;

    fprintf(stderr, "Entering IRs sample mode...");

    while (1) {
        fprintf(stderr, "(%d/%d)...", cnt, RETRIES);
        serial_write(fd, "S", 1);
        fprintf(stderr, "wrote S...");

        // get IRs sampler version
        nrbytes = serial_read(fd, irs_version_buffer, sizeof(irs_version_buffer) - 1, READ_RETRY);
        if (nrbytes >= 3) {
            if (irs_version_buffer[0] == 'S') {
                fprintf(stderr, "\nIRs sampler version: ");
                for (i=0; i<3; i++) {
                    fprintf(stderr, "%c", irs_version_buffer[i]);
                }
                fprintf(stderr, "\n");

                break;
            } else {
                //got garbage, retry again
                for (i=0; i < sizeof(irs_version_buffer); i++) {
                    irs_version_buffer[i] = '\0';
                }
            }
        } else {
            cnt++;
            if (cnt > RETRIES) {
                fprintf(stderr, "no response. Abort.\n");
                return -E_IR_NO_RESPONSE;
            }
        }
    }

    *ret_version_buffer = strdup(irs_version_buffer);
    return nrbytes;
}

int IRs_get_firmware_version(int fd, char **ret_version_buffer)
{
    fprintf(stderr, "Asking for firmware version...");
    unsigned int cnt = 0;
    char version_buffer[5] = {0};
    int nrbytes;

    while (1) {
        fprintf(stderr, "(%d/%d)...", cnt, RETRIES);
        serial_write(fd, "v", 1);
        fprintf(stderr, "wrote v...");
        // get IR toy firmware version
        nrbytes = serial_read(fd, version_buffer, sizeof(version_buffer) - 1, READ_RETRY);
        if (nrbytes > 0) {
            fprintf(stderr, "\nFirmware version: %s\n", version_buffer);
            break;
        } else {
            cnt++;
            if (cnt > RETRIES) {
                fprintf(stderr, "no response. Abort.\n");
                return -E_IR_NO_RESPONSE;
            }
        }
    }
    *ret_version_buffer = strdup(version_buffer);
    return nrbytes;
}

unsigned int IRs_get_next_packet_size(int fd, BOOL use_handshake) {
  // Return the expected packet size.
  // IRs will return the expected packet size if it is placed into transmit
  // mode with handshake mode on.
  // Default: 62
  // If handshake mode is on and no response is given, return
  // IRS_NO_PACKET_SIZE
  //
  unsigned int retries = 0;
  char buf[1];
  unsigned int bytes = IRS_NO_PACKET_SIZE;
  int nrbytes = 0;

  fprintf(stderr, "fetching next packet size...");

  fprintf(stderr, "handshake mode ");
  if (use_handshake) {
    fprintf(stderr, "ON. ");
    nrbytes = serial_read(fd, buf, sizeof(buf), READ_RETRY);
    if (nrbytes > 0) {
      bytes = buf[0];
      fprintf(stderr, "requested %d bytes.\n", bytes);
    } else {
      fprintf(stderr, "no response.\n");
    }
  } else {
    fprintf(stderr, "OFF. ");

    // use default packet size
    bytes = 62;

    fprintf(stderr, "defaulting to %d bytes.\n", bytes);
  }
  return bytes;
}

void IRs_fprint(FILE *stream, char *command, unsigned int len)
{
  unsigned int i = 0;
  for (; i < len; i += 2) {
    fprintf(stream, "%02X%02X ", command[i], command[i + 1]);
  }
}

int IRs_rx(int fd, float resolution, IRBYTE **command) {
  unsigned int size = 0;
  char buffer[IRS_READ_BUFSIZE] = {0};
  IRBYTE *currcommand = *command;
  IRCODE IRCode;
  double NewIRcode;
  int nrbytes, c;
  BOOL finished_reading = FALSE;

  *command = (IRBYTE *)(realloc(*command, size * sizeof(IRBYTE)));
  currcommand = *command;

  while ((nrbytes = serial_read(fd, buffer, sizeof(buffer), READ_RETRY)) > 0) {
    // comment out to avoid exiting.
    // if (nrbytes % 2)
    //     exit(-1); // JTR IRTOY should ALWAYS return even number of BYTE values

    size += nrbytes;
    *command = (IRBYTE *)(realloc(*command, size * sizeof(IRBYTE)));
    currcommand = &(*command)[size - nrbytes];

    fprintf(stderr, "got %d bytes", nrbytes);

    // run through what was read by the toy
    for (c = 0; c < nrbytes; c += 2) {
      IRCode = (IRBYTE) buffer[c];
      IRCode |= (buffer[c + 1] << 8);

      if (IRCode == 0xFFFF) {
        currcommand[c] = 0xFF;
        currcommand[c + 1] = 0xFF;

        size = size - nrbytes + (c + 1) + 1;
        *command = (IRBYTE *)(realloc(*command, size * sizeof(IRBYTE)));

        finished_reading = TRUE;
      } else {
        NewIRcode = ((double)IRCode * 21.3333) / resolution;
        currcommand[c] = (IRBYTE)NewIRcode;
        currcommand[c + 1] = (IRBYTE)((NewIRcode) / 256) & 0xFF;
      }
      fprintf(stderr, "%04X ", ((IRCODE *)currcommand)[c / 2]);

      if (finished_reading == TRUE) {
        break;
      }
    }

    if (finished_reading == TRUE) {
      fprintf(stderr, "\n");
      fprintf(stderr, "Captured command.\n");
      break;
    }
  }

  return size;
}

int IRs_tx(int fd, char *buffer, unsigned int buflen) {
  // TX a buffer of arbitrary length
  // Takes into account handshaking, splitting the buffer into chunks
  // handleable by the IR Toy and does completion checks.

  unsigned int i;
  unsigned int bytes_to_tx;
  int bytes_txd;
  int ttl_bytes_txd = 0;
  int irtoy_ttl_bytes_txd;
  char *bufoffset = buffer;
  char *bufmax = buffer + sizeof(char) * buflen;

  int nrbytes;
  char buffer_checks[3];

  fprintf(stderr, "tx %d bytes\n", buflen);

  while (bufoffset != bufmax) {
    bytes_to_tx = IRs_get_next_packet_size(fd, useHandshake);
    if (bytes_to_tx == IRS_NO_PACKET_SIZE) {
      fprintf(stderr, "Did not receive packet size. Waiting a bit to try again...\n");
      sleep_(0.1);
    } else {
      if (bufmax - bufoffset < bytes_to_tx) {
        fprintf(stderr, 
             "bytes_to_tx (%u) is greater than number of bytes left (%ld). Setting to %ld.\n",
             bytes_to_tx, bufmax - bufoffset, bufmax - bufoffset);
        bytes_to_tx = bufmax - bufoffset;
      }
      if (verbose) {
        fprintf(stderr, "Sending %u bytes to IRToy:\n", bytes_to_tx);
        IRs_fprint(stderr, bufoffset, bytes_to_tx);
        fprintf(stderr, "\n");
      }

      bytes_txd = serial_write(fd, bufoffset, bytes_to_tx);

      fprintf(stderr, "Checking # bytes sent....");
      if (bytes_txd != bytes_to_tx) {
        fprintf(stderr, "# comms error bytes sent %d <> bytes supposed to send %d\n", bytes_txd, bytes_to_tx);
        return -1;
      } else {
        fprintf(stderr, "%i bytes...ok.\n", bytes_txd);
        ttl_bytes_txd += bytes_txd;
        bufoffset += sizeof(char) * bytes_to_tx;
      }
    }
  }

  // Exit transmit unit if there was nothing to send or the last two bytes of
  // buffer are not 0xFF.
  if (buflen == 0 || 
      (buffer[buflen - 1] != 0xFF && buffer[buflen - 2] != 0xFF)) {
    serial_write(fd, "\xFF\xFF", 2);
  }

  // If handshake mode is on, read the last expected bytes from the toy,
  // otherwise it will be left on the serial and mess things up later
  bytes_to_tx = IRs_get_next_packet_size(fd, useHandshake);

  if (countreq) {
    // check number of bytes sent
    nrbytes = serial_read(fd, buffer_checks, 3, READ_RETRY);
    if (nrbytes == 3) {
      if (buffer_checks[0] == 't') {
        irtoy_ttl_bytes_txd = (buffer_checks[1] << 8) + (IRBYTE)buffer_checks[2];
        fprintf(stderr, "buffer had %d bytes, txd %d bytes...", buflen,
                irtoy_ttl_bytes_txd);
        if (buflen == irtoy_ttl_bytes_txd) {
          fprintf(stderr, "ok");
        } else {
          fprintf(stderr, "failed");
        }
      } else {
        fprintf(stderr, "Bad sent count: ");
        for (i = 0; i < 3; i++) {
          fprintf(stderr, " %02X ", (IRBYTE)buffer_checks[i]);
        }
      }
    } else {
      fprintf(stderr, "IR Toy did not return count bytes. Got %d bytes.", nrbytes);
    }
    fprintf(stderr, "\n");
  }
  if (completereq) {
    // check completion status
    nrbytes = serial_read(fd, buffer_checks, 1, READ_RETRY);
    if (nrbytes == 1) {
      if (buffer_checks[0] == 'C') {
        fprintf(stderr, "Transmit was successful and glitch free!");
      } else if (buffer_checks[0] == 'F') {
        fprintf(stderr, "Error, transmit was interrupted at some point!!!");
      } else {
        fprintf(stderr, "Bad completion status: ");
        fprintf(stderr, "%02X", (IRBYTE)buffer_checks[i]);
      }
      fprintf(stderr, "\n");
    } else {
      fprintf(stderr, "IR Toy did not return completion status. Got %d bytes.\n", nrbytes);
    }
  }

  return ttl_bytes_txd;
}

void IRs_enter_transmit_unit(int fd)
{
  // IRs mode IRS_TRANSMIT_unit command
  serial_write(fd, "\x03", 1);
}

void IRs_set_sample_options(int fd)
{
  if (useHandshake) {
    fprintf(stderr, "Enabling handshaking.\n");
    // cause packet handshake to be sent
    serial_write(fd, "\x26", 1);
  }
  if (countreq) {
    fprintf(stderr, "Asking for transmit count reporting.\n");
    // cause transmit count to be returned
    serial_write(fd, "\x24", 1);
  }
  if (completereq) {
    fprintf(stderr, "Asking for completion reporting.\n");
    // cause completion status to be returned
    serial_write(fd, "\x25", 1);
  }
}

void IRs_play(char *param_fname, int fd, char *param_delay, int (* play_file)(char *, int))
{
  // IR toy is already in IRs mode

  BOOL soloplay = FALSE;
  int fcounter = 0;
  char fnameseq[255];

  int delay = atoi(param_delay) / 1000;
  int play_status;

  fprintf(stderr, "Playing back files...\n");

  while (1) {
    // If file exists then single play mode, otherwise assume there are
    // multiple bin files and play them
    
    if (!file_exists(param_fname)) {
      // playing a sequence
      sprintf(fnameseq, "%s_%03d.bin", param_fname, fcounter);
      if (!file_exists(fnameseq)) {
        if (fcounter > 0) {
          printf("%s does not exist. No more file(s).\n", fnameseq);
        } else {
          printf("Bin file does not exist.\n");
        }
        break;
      }
    } else {
      // play one file
      strcpy(fnameseq, param_fname);
      printf("Playing single file %s\n",fnameseq);
      soloplay = TRUE;
    }

    if (delay < 0) {
      printf("Press a key to start playing %s or X to exit...\n", fnameseq);

      while (1) {
        if (kbhit()) {
          int input = get_char();
          if ((input == 'x') || (input == 'X')) {
            serial_write(fd, "\x00\x00", 2);
            return;
          }
          break;
        }
      }
    } else if (delay > 0) {
      fprintf(stderr, "Auto playing %s with %d seconds delay.\n", fnameseq, delay);
      sleep_(delay);
    }

    IRs_enter_transmit_unit(fd);
    play_status = play_file(fnameseq, fd);

    if (soloplay==TRUE) {
      break;
    } else if (play_status != E_IRBIN_XMIT) {
      fcounter++;
    } else {
      fprintf(stderr, "Retrying file %s\n", fnameseq);
    }
  }
}
