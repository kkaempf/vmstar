/*
 *	VMSTARP.H	holds all the declarations for global variables (so
 *			called private parts).
 */
#ifndef LOADED_VMSTARP_H
#define LOADED_VMSTARP_H
#include "vmstar.h"

/* Miscellaneous globals, etc. */

extern struct tarhdr header;

extern char buffer[RECSIZE];             /* buffer for a tarfile record */

extern
char pathname[NAMSIZE],		/* File name as found on tape (UNIX) */
     curdir[32767],		/* Current directory */
     new_directory[32767],	/* Directory of current file */
     newfile[32767],		/* VMS format of file name */
     outfile[32767],		/* Complete output file specification */
     temp[32767],		/* Scratch */
     creation[30],		/* Date as extracted from the TAR file */
     linkname[32767];		/* Linked file name  */

#endif /* LOADED_VMSTARP_H */
