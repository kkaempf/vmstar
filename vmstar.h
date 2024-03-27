/*
 *      VMSTAR.H - holds all the definitions needed to handle a tar file.
 */
#ifndef LOADED_VMSTAR_H
#define LOADED_VMSTAR_H

#include <descrip.h>
#include <dvidef.h>

#ifndef DVI$C_ACP_F11V5
# define DVI$C_ACP_F11V5 11             /* FILES-11 STRUCTURE LEVEL 5 */
#endif

#if !defined(__DECC) || __VMS_VER < 70000000
# define bzero(__x,__y) memset(__x, 0, __y)
#endif

#if !defined(__VAX) && defined(__CRTL_VER) && __CRTL_VER >= 70301000
# ifndef NO_SYMLINKS
#   define SYMLINKS
# endif
#endif

#if !defined(__VAX) && __CRTL_VER >= 70301000
# define STAT lstat
#else
# define STAT stat
#endif


#define ERROR1          -1
#define ISDIRE          1
#define ISFILE          0
#define ISSLNK          2
#define NAMSIZE         100
#define BUFFERFACTOR    32      /* Buffer size multiplier */
#define RECSIZE         512     /* Data block size */
#define BUFFERSIZE      (RECSIZE * BUFFERFACTOR)

#define T_NAM_LEN 32767         /* Miscellaneous name storage size. */

#define DEFAULT_NAME    "*.*;"

/* BADCHARS defines which characters are not permitted in a VMS file name.
   TRANSLATE defines what character should be used instead.
   Beware! The position of the character in TRANSLATE must correspond to
   the position of the bad character in BADCHARS.
*/

#define BADCHARS_ODS2   "!@#%^&*()+=|~`[]{}':;<>,? \\\""
#define TRANSLATE_ODS2  "__$______X____________________"

#define BADCHARS_ODS5   "*|:<>?\\\""
#define TRANSLATE_ODS5  "__________"

/* POSIX typeflag (our linkflag) values. */
#define AREGTYPE '\0'           /* regular file */
#define REGTYPE  '0'            /* regular file */
#define LNKTYPE  '1'            /* link */
#define SYMTYPE  '2'            /* reserved */
#define CHRTYPE  '3'            /* character special */
#define BLKTYPE  '4'            /* block special */
#define DIRTYPE  '5'            /* directory */
#define FIFOTYPE '6'            /* FIFO special */
#define CONTTYPE '7'            /* reserved */

#define XHDTYPE  'x'            /* Extended header referring to the
                                     next file in the archive */
#define XGLTYPE  'g'            /* Global extended header */


struct tarhdr                   /* A tar header */
{
    char title[NAMSIZE];        /*   0  Member name (if <= 100 c's).  (name) */
    char protection[8];         /* 100  Mode.  (mode) */
    char uid[8];                /* 108  User id. */
    char gid[8];                /* 116  Group id. */
    char count[12];             /* 124  (size) */
    char time[12];              /* 136  UNIX format date-time.  (mtime) */
    char chksum[8];             /* 148  Header checksum. */
    char linkflag;              /* 156  (typeflag) */
    char linkname[NAMSIZE];     /* 157  Link name. */
/*
 *  Posix header items:
 */
    char magic[6];              /* 257  Magic ID string ("ustar\0"). */
    char version[2];            /* 263  USTAR Version. */
    char uname[32];             /* 265 */
    char gname[32];             /* 297 */
    char devmajor[8];           /* 329 */
    char devminor[8];           /* 337 */
    char prefix[155];           /* 345 */
    char dummy[12];             /* 500  (Pad to 512 (record size).) */
};

/* GNU tar extract. */

/* The linkflag defines the type of file.  */
#define LF_OLDNORMAL    '\0'    /* normal disk file, Unix compat */
#define LF_NORMAL       '0'     /* normal disk file */
#define LF_LINK         '1'     /* link to previously dumped file */
#define LF_SYMLINK      '2'     /* symbolic link */
#define LF_CHR          '3'     /* character special file */
#define LF_BLK          '4'     /* block special file */
#define LF_DIR          '5'     /* directory */
#define LF_FIFO         '6'     /* FIFO special file */
#define LF_CONTIG       '7'     /* contiguous file */
/* Further link types may be defined later.  */

/* Note that the standards committee allows only capital A through
 * capital Z for user-defined expansion.  This means that defining
 * something as, say '8' is a *bad* idea.
 */

#define LF_ACL          'A'     /* Solaris Access Control List. */
#define LF_DUMPDIR      'D'     /* GNU dump dir. */
#define LF_EXTATTR      'E'     /* Solaris Extended Attribute File. */
#define LF_META         'I'     /* Inode (metadata only, no file). */
#define LF_LONGLINK     'K'     /* Long linkname for next file. */
#define LF_LONGNAME     'L'     /* Long name for next file. */
#define LF_MULTIVOL     'M'     /* Continuation file (begun on other vol.). */
#define LF_NAMES        'N'     /* OLD GNU for names > 100 characters. */
#define LF_VMS          'P'     /* VMS attributes. */
#define LF_SPARSE       'S'     /* Sparse file descriptor(s). */
#define LF_VOLHDR       'V'     /* Tape/volume header.  (Ignore on extract.) */
#define LF_VU_XHDR      'X'     /* POSIX.1-2001 xtended (Sun VU version) */

#define LF_XHD          'x'     /* Extended header. */
#define LF_XGL          'g'     /* Global extended header. */


/* Common function prototypes. */

int cleanup_dire( char *string);

void tar2vms( int argc, char **argv);

void vms2tar( int argc, char **argv);


#endif /* LOADED_VMSTAR_H */
