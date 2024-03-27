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
     curdir[NAMSIZE],		/* Current directory */
#if 0
     topdir[NAMSIZE],		/* Top level directory of current default */
     curdev[NAMSIZE],		/* Current device */
#endif
     new_directory[NAMSIZE],	/* Directory of current file */
     newfile[NAMSIZE],		/* VMS format of file name */
     outfile[NAMSIZE],		/* Complete output file specification */
     temp[NAMSIZE],		/* Scratch */
     creation[NAMSIZE],		/* Date as extracted from the TAR file */
     linkname[NAMSIZE];		/* Linked file name  */

#endif /* LOADED_VMSTARP_H */
