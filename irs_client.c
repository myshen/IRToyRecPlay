#include "irs_client.h"

#undef RETRIES
#define RETRIES 2

#define IRS_NO_PACKET_SIZE -1

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

int IRs_tx_buffer(int fd, char *buffer, unsigned int buflen) {
  // TX a buffer of arbitrary length
  // Takes into account handshaking, splitting the buffer into chunks
  // handleable by the IR Toy and does completion checks.

  unsigned int i;
  int bytes_to_tx;
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
      fprintf(stderr, "Did not receive packet size. Unable to transmit. Abort.\n");
      return -1;
    } else {
      if (bytes_to_tx < 0) {
        fprintf(stderr, "Bad number of bytes to send. Waiting.\n");
        sleep_(0.1);
        continue;
      }
      if (bufmax - bufoffset < bytes_to_tx) {
        bytes_to_tx = bufmax - bufoffset;
      }
      if (verbose) {
        fprintf(stderr, "Sending %d bytes to IRToy:\n", bytes_to_tx);
        for (i = 0; i < bytes_to_tx; i++) {
          fprintf(stderr, " %02X ", (IRBYTE)bufoffset[i]);
        }
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

  if (buflen == 0) {
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
        fprintf(stderr, "Bad count reply: ");
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
        fprintf(stderr, "Bad completion reply:");
        for (i = 0; i < 1; i++) {
          fprintf(stderr, " %02X ", (IRBYTE)buffer_checks[i]);
        }
      }
      fprintf(stderr, "\n");
    } else {
      fprintf(stderr, "IR Toy did not return completion status. Got %d bytes.\n", nrbytes);
    }
  }

  return ttl_bytes_txd;
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

  useHandshake = TRUE;

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

  while (1) {
    // If file exists then single play mode, otherwise assume there are
    // multiple bin files and play them
    
    if (!file_exists(param_fname)) {
      // playing a sequence
      sprintf(fnameseq, "%s_%03d.bin", param_fname, fcounter);
      if (!file_exists(fnameseq)) {
        if (fcounter > 0) {
          printf("No more file(s).\n");
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
