#include "ff.h"

char *ff_strerror(FRESULT r);
FRESULT scan_files(char *path);   /* Start node to be scanned (***also used as work area***) */
int xmodem_rx(char *filename);

