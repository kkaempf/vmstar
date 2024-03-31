#define module_name	DATA
#define module_version  "V1.1"
/*
 *	DATA.C - Holds all the global data of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#endif

#include "vmstarp.h"

struct tarhdr header;           /* A tar header */

char buffer[BUFFERSIZE];	/* buffer for tarfile and output record */

/* Miscellaneous globals, etc. */

char pathname[ T_NAM_LEN+ 1],   /* File name as found on tape (UNIX) */
     curdir[ T_NAM_LEN+ 1],     /* Current directory */
     new_dir[ T_NAM_LEN+ 1],    /* Directory of current file */
     newfile[ T_NAM_LEN+ 1],    /* VMS format of file name */
     outfile[ T_NAM_LEN+ 1],    /* Complete output file specification */
     temp[ T_NAM_LEN+ 1],       /* Scratch */
     creation[30],              /* Date as extracted from the TAR file */
     linkname[T_NAM_LEN+ 1];    /* Linked file name  */

int linkname_len;
int pathname_len;

