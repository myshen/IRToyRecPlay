/*
    Written and maintained by the IR TOY Project and http://dangerousprototypes.com
    WIKI page:    http://dangerousprototypes.com/usb-ir-toy-manual/
    Forum page:   http://dangerousprototypes.com/forum/viewforum.php?f=29&sid=cdcf3a3177044bc1382305a921585bca
********************************************************************************************************************

Copyright (C) 2011 Where Labs, LLC (DangerousPrototypes.com/Ian Lesnet)

This work is free: you can redistribute it and/or modify it under the terms of Creative Commons Attribution ShareAlike license v3.0

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the License for more details. You should have received a copy of the License along with this program. If not, see <http://creativecommons.org/licenses/by-sa/3.0/>.

Contact Details: http://www.DangerousPrototypes.com

********************************************************************************************************************* */

/* spanner 2010-10 v0.31
 * some minor tidy up of text, finished implementation of a v minimal verbose display option
 * Added two automated / macro play modes to the original play mode:
 * Three play modes:
 * 1. -p f NAME
 *     This is the unchanged method form the origianl release.
 *     Plays all bin files matching NAME_nnn.bin, where nnn starts at 000 and consists of SEQUENTIAL numbers. Stops at first gap in numbers, or last file.
 *     In this mode user MUST press any key, except 'x', to play next file/command.
 * 2. -p f NAME - a nnn
 *     Plays as above, except does NOT wait for any key to be pressed, instead delays nnn miliseconds between sending each file.
 *     500 is recommended as a starting delay. On old P4 computer, no delay always hangs the IRToy (have to uplug & plug in again to reset).
 * 3. -q -f NAME
 *     Play command files listed in the file indicated in -f parameter (requires -f )
 *
 *         Note the file names can be random, ie the numbered sequential rule does not apply.
 *         Sample file content:
 *         sanyo_000.bin
 *         sanyo_010.bin
 *         sanyo_020.bin
 *         sanyo_500.bin
 *
 *         Files can be in a subdirectory if use the following syntax:
 *         .\\dvd\\sanyo_000.bin
 *         .\\dvd\\sanyo_010.bin
 *         .\\dvd\\sanyo_020.bin
 *         .\\dvd\\sanyo_500.bin
 */

// added: enhanced to support continous stream of data from irtoy.
//        support for conversion to OLS format, via commandline option -o
//

//  version 0.06 added the experimental conversion (untested) for other resolution:

//  ( newbytes*newresolution)=(irtoybytes*21.3333)
//  (prontocodes*prontoresolution)=(irtoycodes*21.3333)
//  prontocodes=(irtoycodes*21.3333)/protoresolution
//
// *** new in v0.8
// added -b buffer parameter for playback, e.g -b 32 , to  finetune playback
// added single playing file when an complete filename with extension .bin is specified
// in recording mode, filename with extension bin is overwritten. (overwrites the same file in single mode)

// 6/4/2011   (fix transmit problem)http://dangerousprototypes.com/forum/viewtopic.php?f=29&t=2363
// 6/6/2011    added fix to other file format.

/* CDC commands
 * 0   - SUMP clear
 * 1   - SUMP run
 * 2   - SUMP ID
 * r|R - IRMAN decoder
 * s|S - IRs sampling mode
 * u|U - uart mode
 * p|P - IR widget mode
 * t|T - self test
 * v|V - version
 * $ - bootloader jump
 *
 * SUMP, IRMAN decoder, and IRs sample modes remain active until exited with 0x00
 *
 * See Firmware-main/IRs.c for IRs commands
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32

#include <conio.h>
#include <windef.h>
#include <windows.h>

#else

#include <sys/select.h>
#include <ncurses.h>
#include <stdbool.h>
#include <unistd.h>

#endif

#include "config.h"
#include "serial.h"
#include "txt.h"
#include "ols.h"
#include "bin.h"
#include "queue.h"

#undef RETRIES
#define RETRIES 2

// IRToy Recorder/Player error codes
#define E_IRTOY_NORESPONSE 1
#define E_IRTOY_OPTS 2

//int modem = FALSE;   //set this to TRUE if testing a MODEM
int verbose = 0;
BOOL useHandshake = TRUE;
char completereq = 1;
char countreq = 1;


unsigned int sleep_(float seconds) {
#ifdef _WIN32
  seconds *= 1000;
  if (seconds < 0) {
    seconds = -seconds;
  }
  Sleep(seconds);
  return 0;
#else
  seconds *= 1000000;
  return usleep((unsigned int)seconds);
#endif
}

int get_char() {
#ifdef _WIN32
  return getch();
# else
  return get_ch();
  //return getchar_unlocked();
# endif
}

BOOL file_exists(const char * filename)
{
  FILE *file = fopen(filename, "r");
  if (file != NULL)
  {
    fclose(file);
    return true;
  }
  return false;
}

void print_usage(char * appname) {
  FILE *f = stderr;
    fprintf(f, "IRToy version: %s\n", IRTOY_VERSION);
    fprintf(f, "Usage:\n");
    fprintf(f, "%s -d device [-s speed]\n", appname);
    fprintf(f, "\n");
    fprintf(f, "Options:\n");
    fprintf(f, "\t-d device is port e.g. ");

#ifdef _WIN32
    fprintf(f, "COM1\n");
#elif IS_DARWIN
    fprintf(f, "/dev/cu.usbmodem000001, /dev/cu.usbmodem631\n");
#else
    fprintf(f, "/dev/ttyS0, /dev/ttyACM0\n");
#endif

    fprintf(f, "\t-s Speed is port Speed (default: 115200)\n");
    fprintf(f, "\t-f Output/input file is a base filename for recording/playing");
    fprintf(f, "\t   pulses  \n");
    fprintf(f, "\t-r Record into a file indicated in -f parameter (requires -f) \n");
    fprintf(f, "\t-p Play the file/sequence of file indicated in -f parameter");
    fprintf(f, "\t   (requires -f \n");
    fprintf(f, "\t-n Output and convert codes to other resolutions  (default is 21.33us)\n");
    fprintf(f, "\t   (requires -f \n");
    fprintf(f, "\t-q Play command files listed in the file indicated in -f");
    fprintf(f, "\t   parameter (requires -f )\n");
    fprintf(f, "\t-a Optional automatic play (does not wait for keypress). \n");
    fprintf(f, "\t   You must specify delay in milliseconds between sending \n");
    fprintf(f, "\t   each command.\n");
    fprintf(f, "\t-v Display verbose output, have to specify level 0, 1 etc,\n");
    fprintf(f, "\t   although at present it is only on or off :).\n");
    fprintf(f, "\t-o Create OLS file based on the filename format \n");
    fprintf(f, "\t   ext. \"ols\" (Requires -f)  \n");
    fprintf(f, "\t-t Create or Play text files based on the filename format\n");
    fprintf(f, "\t   ext. \"txt\" (Requires -f)  \n");
    fprintf(f, "\t-h [on] [off] Specify Handshake to be used between player packets\n");
    fprintf(f, "\t-e Specify End of transmision to be returned when player has transmitted last value\n");
    fprintf(f, "\t-c Specify Count of bytes received by player\n");
    fprintf(f, "\t-x Reset\n");
    fprintf(f, "\n");
    fprintf(f, "Example usage:\n");
    fprintf(f, "  %s -d COM1 -s speed -f outputfile  -r -p \n", appname);
    fprintf(f, "  %s -d COM1 -s speed -f outputfile  -r -p -t -o\n", appname);
    fprintf(f, "To record and play a text file test_000.txt, use \n");
    fprintf(f, "  %s -d COM1 -f test -p -t -r\n\n", appname);
    fprintf(f, "\n");
    fprintf(f, "NOTE: Except for -q command, use only the base name of the\n");
    fprintf(f, "\toutput/input file, without the numeric sequence:\n");
    fprintf(f, "\tuse -f test instead of -f test_000.bin \n");
    fprintf(f, "\t_000 to _999 will be supplied by this utility. \n");
    fprintf(f, "\tYou may also edit the resulting text file and replace \n");
    fprintf(f, "\tit with your own values, and should end with FFFF.\n");
    fprintf(f, "-------------------------------------------------------------------------\n");
}


int main(int argc, char** argv)
{
    int retstatus = 0;
    int cnt, i,flag;
    int opt;
    char *version_firmware;
    char *version_irs;

    int fd;
    int res,c;
    char *param_port = NULL;
    char *param_speed = NULL;
    char *param_handshake = NULL;
    char *param_fname = NULL;
    char *param_delay = NULL;
    char *param_timer = NULL;
    char *param_buffin = NULL;
    float resolution;
    BOOL record = FALSE, play = FALSE, queue = FALSE, OLS = FALSE, textfile = FALSE, RESET = FALSE;
    char dummy[3];
    int firmware_version = 0;

    printf("+-----------------------------------------+\n");
    printf("|IR Toy Recorder/Player utility %s (CC-0)|\n", IRTOY_VERSION);
    printf("|http://dangerousprototypes.com           |\n");
    printf("+-----------------------------------------+\n");

    if (argc <= 1)
    {
        print_usage(argv[0]);
        return E_IRTOY_OPTS;
    }

    while ((opt = getopt(argc, argv, "torpqsvcexn:a:d:f:b:h:")) != -1)
    {
        // printf("%c  \n",opt);
        switch (opt)
        {

        case 'v':
            verbose=1;
            break;
        case 'e':
            completereq=1;
            break;
        case 'c':
            countreq = 1;
            break;
        case 'h':
            param_handshake = strdup(optarg);
            if (!strcmp((param_handshake), ("off")))
                useHandshake= FALSE;
            if (!strcmp((param_handshake), ("on")))
                useHandshake= TRUE;
            break;
        case 'a':  // delay in miliseconds
            if ( param_delay != NULL)
            {
                printf(" delay error!\n");
                return E_IRTOY_OPTS;
            }
            param_delay = strdup(optarg);
            // printf("delay %s - %d \n", param_delay, atoi(param_delay));
            break;
        case 'd':  // device   eg. com1 com12 etc
            if ( param_port != NULL)
            {
                printf(" Device/PORT error!\n");
                return E_IRTOY_OPTS;
            }
            param_port = strdup(optarg);
            break;
        case 's':
            if (param_speed != NULL)
            {
                printf(" Speed should be set: eg  115200 \n");
                return E_IRTOY_OPTS;
            }
            param_speed = strdup(optarg);

            break;
        case 'f':  // device   eg. com1 com12 etc
            if ( param_fname != NULL)
            {
                printf(" Error: File Name parameter error!\n");
                return E_IRTOY_OPTS;
            }
            param_fname = strdup(optarg);

            break;
        case 'b':
            //buffer receive size, default is 512 - note starting v21, play
            //buffer is 62  bytes only, while read can be adjusted

            if ( param_buffin != NULL)
            {
                printf(" Error: Buffer-In parameter error!\n");
                return E_IRTOY_OPTS;
            }
            param_buffin = strdup(optarg);

            break;
        case 'n':    //use to change sample timer: default is 21.3333us
            if (param_timer!=NULL)
            {
                printf("Sample timer error!\n");
                return E_IRTOY_OPTS;
            }
            param_timer=strdup(optarg);
            break;
        case 'p':    //play
            play =TRUE;
            break;
        case 'q':    //play command queue from file (ie macro command list)
            queue =TRUE;
            break;
        case 'r':    //record
            record =TRUE;
            break;
        case 't':    //text file: record or play text file
            textfile =TRUE;
            break;
        case 'o':    //write to OLS format file
            OLS =TRUE;
            break;
        case 'x':    // reset the IR toy
            RESET = TRUE;
            break;
        default:
            fprintf(stderr, "Invalid argument %c\n", opt);
            print_usage(argv[0]);
            //return E_IRTOY_OPTS;
            break;
        }
    }

//defaults here --------------
    if (param_delay==NULL)
        param_delay=strdup("-1");
    if (param_handshake==NULL)
        useHandshake= TRUE;

    if (param_port==NULL)
    {
        printf(" No serial port set\n");
        print_usage(argv[0]);
        return E_IRTOY_OPTS;
    }
    if (param_timer==NULL)
    {
        param_timer=strdup("21.3333");
    }

    if (param_buffin==NULL)
    {
        param_buffin=strdup("512");  //default buffer size for recieving
    }
    if (param_speed==NULL) {
        param_speed=strdup("115200");    //default port speed
        // printf("Speed: %s\n",param_speed);
    }
    if (record==TRUE)
    {
        // either 'r' or 'p' or both should be used
        // If no filename is given, just display it the received bytes
        if ((param_fname==NULL) && (record==TRUE))
        {
            printf(" Error: -f Parameter is required\n");
            print_usage(argv[0]);
            return E_IRTOY_OPTS;
        }
    }
    if (OLS==TRUE)    // -f must be specified + r or p  or both
    {
        if (param_fname==NULL)
        {
            printf(" Error: -f Parameter is required, ignoring OLS writing.\n");
            OLS=FALSE;
        }
    }
    resolution = atof(param_timer);

    // open serial
    fprintf(stderr, "Opening IR Toy on %s at %sbps...\n", param_port, param_speed);

    fd = serial_open(param_port);
    FREE(param_port);
    if (fd < 0) {
        fprintf(stderr, "Error opening serial port: %d\n", fd);
        return -1;
    }

    fprintf(stderr, "Setting up serial port...\n");

#ifdef _WIN32
    serial_setup(fd, (unsigned long) param_speed);
#else
    serial_setup(fd, strtoul(param_speed,NULL,10));
#endif
    FREE(param_speed);

    fprintf(stderr, "Initializing IR Toy...\n");
    serial_write(fd, "\x00", 1);

    if (RESET == TRUE) {
      fprintf(stderr, "Resetting IR toy.\n");

      serial_write(fd, "\xFF", 1);
      fprintf(stderr, ".");
      serial_write(fd, "\xFF", 1);
      fprintf(stderr, ".");

      for (i = 0; i < 3; i++) {
        serial_write(fd, "\x00", 1);
        fprintf(stderr, ".");
      }
      return 0;
    }

    res = IRs_get_firmware_version(fd, &version_firmware);

    // strict checking is needed because sometimes there are garbages.
    if (res == 4) {
      cnt=0;
      // skip first two bytes of version (those are hardware verson)
      for (i = 2; i < res; i++) {
        dummy[cnt++] = version_firmware[i];
      }
      dummy[cnt] = '\0';
      firmware_version = atoi(dummy);
    } else {
      printf("\n");
      printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      printf("Found Garbage data when reading the firmware version. \n");
      printf("Please re-start this utility again.\n");
      printf("You may need to replug the IR toy.\n");
      printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      printf("\n");
      return -1;
    }

    // restrict firmware to version >= 20
    if (firmware_version < 20) {
      printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      printf("This utility is for firmware version 20 and up.\n");
      printf("Please update your IR TOY firmware version %i to the latest firmware.\n",firmware_version);
      printf("See documentation and firmware update procedures at \n");
      printf("http://dangerousprototypes.com/docs/USB_Infrared_Toy#Firmware\n");
      printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
      return -1;
    }

    FREE(version_firmware);

    res = IRs_begin(fd, &version_irs);
    FREE(version_irs);

    fprintf(stderr, "Current sample timer resolution: %sus\n", param_timer);

    cnt = 0;
    c = 0;
    flag = 0;
    if (textfile == FALSE) {
      fprintf(stderr, "binary mode\n");
      if (record==TRUE) {
        fprintf(stderr, "Recording at Resolution= %fus\n", resolution);

        if (IR_bin_record(param_fname, fd, resolution, param_buffin) == -1) {
          FREE(param_buffin);
          FREE(param_fname);
          return -1;
        }
      }

      if (play==TRUE) {
        IR_bin_play(param_fname,fd,param_delay,param_buffin);
      }
    } else {
        fprintf(stderr, "text mode\n");
        if (record==TRUE) {
            IR_txt_record(param_fname);
        }

        if (play==TRUE) {
            IR_txt_play(param_fname,fd,param_delay);
        }
    }
    FREE(param_buffin);

    if (OLS==TRUE) {
        create_ols(param_fname);
    }

    if (queue==TRUE) {
        IRqueue(param_fname,fd);
    }

    FREE(param_fname);

    // wait a bit for the serial port to clear
    fprintf(stderr, "Waiting for serial port to clear...\n");
    sleep_(0.1);
    serial_close(fd);

    return 0;
}
