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

char buffer[RECSIZE];             /* buffer for a tarfile record */

/* Miscellaneous globals, etc. */

char tarfile[NAMSIZE],  /* Tarfile name  */
    pathname[NAMSIZE],  /* File name as found on tape (UNIX) */
    curdir[NAMSIZE],    /* Current directory */
#if 0 /* unneeded.  */
    topdir[NAMSIZE],    /* Top level directory of current default */
    curdev[NAMSIZE],    /* Current device */
#endif
    new_directory[NAMSIZE],     /* Directory of current file */
    newfile[NAMSIZE],   /* VMS format of file name */
    outfile[NAMSIZE],   /* Complete output file specification */
    temp[NAMSIZE],      /* Scratch */
    creation[NAMSIZE],  /* Date as extracted from the TAR file */
    linkname[NAMSIZE];  /* Linked file name  */
