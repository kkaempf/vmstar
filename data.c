#define module_name	DATA
#define module_version  "V1.0"
/*
 *	DATA.C - Holds all the global data of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#else
#module module_name module_version
#endif

#include "vmstar.h"

struct tarhdr header;           /* A tar header */

char buffer[BUFFERSIZE];	/* buffer for tarfile and output record */

/* Miscellaneous globals, etc. */

char tarfile[32767],		/* Tarfile name  */
    pathname[NAMSIZE+1],	/* File name as found on tape (UNIX) */
    curdir[32767],		/* Current directory */
    new_directory[32767],	/* Directory of current file */
    newfile[32767],		/* VMS format of file name */
    outfile[32767],		/* Complete output file specification */
    temp[32767],		/* Scratch */
    creation[30],		/* Date as extracted from the TAR file */
    linkname[32767];		/* Linked file name  */
