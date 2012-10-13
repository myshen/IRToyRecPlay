// Configuration for IRtoy

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#ifdef _WIN32

#include <windows.h>

#else

#include <unistd.h>
typedef bool BOOL;
#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#endif

#define FREE(x) if(x) free(x)

#ifdef READ_RETRY
#undef READ_RETRY
#endif
#define READ_RETRY 1

#define IRTOY_VERSION "v21"

#undef RETRIES
#define RETRIES 2

unsigned int sleep_(float);
int get_char(void);
BOOL file_exists(const char * filename);

#endif // CONFIG_H_INCLUDED
