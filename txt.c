/*
    Written and maintained by the IR TOY Project and http://dangerousprototypes.com
    WIKI page:    http://dangerousprototypes.com/usb-ir-toy-manual/
    Forum page:   http://dangerousprototypes.com/forum/viewforum.php?f=29&sid=cdcf3a3177044bc1382305a921585bca
********************************************************************************************************************

Copyright (C) 2011 Where Labs, LLC (DangerousPrototypes.com/Ian Lesnet)

This work is free: you can redistribute it and/or modify it under the terms of Creative Commons Attribution ShareAlike license v3.0

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the License for more details. You should have received a copy of the License along with this program. If not, see <http://creativecommons.org/licenses/by-sa/3.0/>.

Contact Details: http://www.DangerousPrototypes.com
Where Labs, LLC, 208 Pine Street, Muscatine, IA 52761,USA

********************************************************************************************************************* */

#include "txt.h"


BOOL is_txt_file_extension_specified(char *param_fname)
{
#ifdef _WIN32
  if ((strcmpi(param_fname, ".txt")) <= 0)
#else
  // Linux does not have strcmpi.
  if ((strcasecmp(param_fname, ".txt")) <= 0)
#endif
  {
    return TRUE;
  } else {
    return FALSE;
  }
}

void IR_txt_record( char *param_fname)
{
    int i,flag;
    unsigned long absolute=0;

    unsigned long size=0;

    int fcounter;
    char inkey;
// char *param_delay=NULL;

    char fnameseq[255],fnameseq_txt[255];
    FILE *fp, *fp_txt=NULL;
    int res;
    uint16_t txt_buffer[1];
    char s[4];
    //check filename if exist
    printf(" Entering TXT conversion Mode \n");
    fcounter=0;
    inkey=0;
    while (1)
    {
        sprintf(fnameseq,"%s_%03d.bin",param_fname,fcounter);
        fp=fopen(fnameseq,"rb");
        if (fp==NULL)
        {
            if (fcounter > 0)
                printf(" No more file(s). \n\n");
            else
                printf(" File does not exits. \n");
            break;
        }

        sprintf(fnameseq_txt,"%s_%03d.txt",param_fname,fcounter);
        fp_txt=fopen(fnameseq_txt,"w");
        if (fp_txt==NULL)
        {
            printf(" Error: Cannot create txt file: %s. \n",fnameseq_txt);
            break;
        }

        printf("\n Creating txt file: %s \n\n",fnameseq_txt);
        size=0;
        absolute=0;
        char temp[4];
        flag=0;
        while(!feof(fp))
        {
            if ((res=fread(&txt_buffer,sizeof(uint16_t),sizeof(txt_buffer),fp)) > 0)
            {
                for(i=0; i<res; i++)
                {
                    sprintf(temp,"%04X",(uint16_t) txt_buffer[i]);
                    sprintf(s,"%c%c%c%c",temp[2],temp[3],temp[0],temp[1]);
                    if (verbose==TRUE)
                        printf(" %s",s);
                    fprintf(fp_txt,"%s ", s);
                }
            }

        }

        printf("\n .. Done.\n");
        fclose(fp);
        fclose(fp_txt);
        fcounter++;
    }
}

int play_txt_file(char *fname, int fd) {
  // begin playback for open file

  FILE *fp = fopen(fname, "rt");

  // Start transmission
  // IRs mode IRS_TRANSMIT_unit command
  serial_write(fd, "\x03", 1);

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

  // convert txt file to bin
  char *bin_buffer = (char *)malloc(file_size * sizeof(char));
  unsigned int bin_size = 0;
  char *token;
  char ircode[4];
  char hex[2];
  IRBYTE irbyte;

  token = strtok(file_buffer, " ");
  while (token != NULL) {
      strcpy(ircode, token);

      sprintf(hex, "%c%c", ircode[0], ircode[1]);
      irbyte = (IRBYTE)strtoul(hex, NULL, 16);
      bin_buffer[bin_size++] = irbyte;

      sprintf(hex, "%c%c", ircode[2], ircode[3]);
      irbyte = (IRBYTE)strtoul(hex, NULL, 16);
      bin_buffer[bin_size++] = irbyte;

      if (verbose == TRUE) {
          printf(" %02X%02X", bin_buffer[bin_size - 2], bin_buffer[bin_size - 1]);
      }

      token = strtok(file_buffer, " ");
  }
  free(file_buffer);

  int totalbytes = IRs_tx_buffer(fd, bin_buffer, bin_size);
  free(bin_buffer);

  if (totalbytes != bin_size) {
    fprintf(stderr, "Error playing buffer.\n");
    serial_write(fd, "\xFF\xFF\x00", 3);
  }

  fclose(fp);
}

void IR_txt_play(	char *param_fname,int fd,char *param_delay)
{
  IRs_play(param_fname, fd, param_delay, play_txt_file);
}
