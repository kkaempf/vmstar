/*
 *	VMSTAR.H - holds all the definitions needed to handle a tar file.
 */
#ifndef LOADED_VMSTAR_H
#define LOADED_VMSTAR_H

#define ERROR1		-1
#define ISDIRE		1
#define ISFILE		0
#define NAMSIZE		100
#define BLKSIZE		10240	/* Block size */
#define RECSIZE 	512	/* Data block size */

#define DEFAULT_NAME	"*.*;"

/* BADCHARS defines which characters are not permitted in a VMS file name.
   TRANSLATE defines what character should be used instead.
   Beware! The position of the character in TRANSLATE must correspond to
   the position of the bad character in BADCHARS.
*/
#define BADCHARS	"!@#%^&*()+=|~`[]{}':;<>,?\\\""
#define TRANSLATE	"__$______X___________________"

struct tarhdr                   /* A tar header */
{
    char title[NAMSIZE];
    char protection[8];
    char uid[8];                /* this is the user id */
    char gid[8];                /* this is the group id */
    char count[12];             /* was 11 in error */
    char time[12];              /* UNIX format date  */
    char chksum[8];             /* header checksum */
    char linkcount;             /* hope this is right */
    char linkname[NAMSIZE];     /* Space for the name of the link */
    char dummy[255];            /* and the rest */
};

#endif /* LOADED_VMSTAR_H */
