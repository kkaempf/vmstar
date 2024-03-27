/*
 *	VMSTARP.H	holds all the declarations for global variables (so
 *			called private parts).
 */
#ifndef LOADED_VMSTARP_H
#define LOADED_VMSTARP_H
#include "vmstar.h"

/* Miscellaneous globals, etc. */

extern struct tarhdr header;

extern
char buffer[BUFFERSIZE];        /* buffer for tarfile and output record */

extern
char pathname[ T_NAM_LEN+ 1],	/* File name as found on tape (UNIX) */
     curdir[ T_NAM_LEN+ 1],	/* Current directory */
     new_dir[ T_NAM_LEN+ 1],	/* Directory of current file */
     newfile[ T_NAM_LEN+ 1],	/* VMS format of file name */
     outfile[ T_NAM_LEN+ 1],	/* Complete output file specification */
     temp[ T_NAM_LEN+ 1],	/* Scratch */
     creation[30],		/* Date as extracted from the TAR file */
     linkname[T_NAM_LEN+ 1];	/* Linked file name  */

extern int linkname_len;
extern int pathname_len;


#endif /* LOADED_VMSTARP_H */
