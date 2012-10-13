/*
Written and maintained by the IR TOY Project and http://dangerousprototypes.com
WIKI page:    http://dangerousprototypes.com/usb-ir-toy-manual/
Forum page:   http://dangerousprototypes.com/forum/viewforum.php?f=29&sid=cdcf3a3177044bc1382305a921585bca

*******************************************************************************

Copyright (C) 2011 Where Labs, LLC (DangerousPrototypes.com/Ian Lesnet)

This work is free: you can redistribute it and/or modify it under the terms of
Creative Commons Attribution ShareAlike license v3.0

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the License for more details. You should have received
a copy of the License along with this program. If not, see
<http://creativecommons.org/licenses/by-sa/3.0/>.

Contact Details: http://www.DangerousPrototypes.com
Where Labs, LLC, 208 Pine Street, Muscatine, IA 52761,USA

*******************************************************************************
*/

#include "bin.h"


BOOL is_bin_file_extension_specified(char *param_fname)
{
#ifdef _WIN32
  if ((strcmpi(param_fname, ".bin")) <= 0)
#else
  // Linux does not have strcmpi.
  if ((strcasecmp(param_fname, ".bin")) <= 0)
#endif
  {
    return TRUE;
  } else {
    return FALSE;
  }
}

int IR_bin_record(char *param_fname,int fd,float resolution,char *param_buff)
{
  int fcounter = 0;
  char fnameseq[FILENAME_MAX];

  FILE *fp = NULL;

  if (is_bin_file_extension_specified(param_fname)) {
    fprintf(stderr, "File to record in already has bin extension. Abort.\n");
    return E_IRBIN_OUTPUT;
  }

  fprintf(stderr, "Recording started.\n");

  fprintf(stderr, "Press a button on the remote or any key to exit... \n");

  // Read one full IR command at a time, dump them to separate files.
  while (1) {
    IRBYTE *command = NULL;
    int len = IRs_rx(fd, resolution, &command);

    if (len > 0) {
      do {
        sprintf(fnameseq, "%s_%03d.bin", param_fname, fcounter);
        fcounter++;
      } while (file_exists(fnameseq));

      fprintf(stderr, "Creating file: %s\n", fnameseq);

      fp = fopen(fnameseq, "wb");
      if (fp == NULL) {
        fprintf(stderr, "Cannot open output file: %s\n",param_fname);
        return E_IRBIN_OUTPUT;
      }

      fwrite(command, len, sizeof(IRBYTE), fp);
      fclose(fp);
      fprintf(stderr, "Press a button on the remote or any key to exit... \n");
    }

    FREE(command);

    if (kbhit()) {
      break;
    }
  }

  fprintf(stderr, "Recording finished.\n");
  return 0;
}

int play_bin_file(char *fname, int fd) {
  // begin playback for open file

  FILE *fp = fopen(fname, "rb");

  fprintf(stderr, "Playing %s...\n", fname);

  fseek(fp, 0, SEEK_END);
  long int file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *file_buffer = (char *)malloc(file_size * sizeof(char));
  int rbytes = fread(&file_buffer[0], sizeof(char), file_size, fp);
  if (rbytes != file_size) {
    fprintf(stderr, "Error occurred while reading file. rbytes: %d, file_size: %ld\n", rbytes, file_size);
    fprintf(stderr, "ferror: %d\n", ferror(fp));
    return -1;
  }

  int totalbytes = IRs_tx(fd, file_buffer, file_size);
  free(file_buffer);

  if (totalbytes != file_size) {
    fprintf(stderr, "Error playing buffer.\n");
    serial_write(fd, "\xFF\xFF\x00", 3);
  }

  fclose(fp);
  return totalbytes;
}

void IR_bin_play(char *param_fname, int fd, char *param_delay, char *param_buff)
{
  IRs_set_sample_options(fd);
  IRs_play(param_fname, fd, param_delay, play_bin_file);
}
