#define module_name	VMS2TAR
#define module_version  "V2.2"
/*
 *	VMS2TAR.C - Handles the create functionality of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#else
#module module_name module_version
#endif

#include "vmstar_cmdline.h"
#include "vmstarP.h"
#include "VMSmunch.h"
#include "vms_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <stat.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <file.h>
#include <errno.h>

#ifdef SYMLINKS
#  include <unistd.h>
#endif /* def SYMLINKS */

#include <unixio.h>
#include <unixlib.h>
#include <ssdef.h>
#include <rms.h>
#include <starlet.h>
#include <lib$routines.h>

/* Work-around for missing modern stat.h macros. */
#ifndef S_ISDIR
# define S_ISDIR(m) (((m)& S_IFMT) == S_IFDIR)
#endif /* ndef S_ISDIR */

#ifndef S_ISLNK
# ifndef S_IFLNK
#  define S_IFLNK 0120000    /* symbolic link */
# endif /* ndef S_IFLNK */
# define S_ISLNK(m) (((m)& S_IFMT) == S_IFLNK)
#endif /* ndef S_ISLNK */

#ifndef S_ISREG
# define S_ISREG(m)  (((m)& S_IFMT) == S_IFREG)
#endif /* ndef S_ISREG */

#define EXPERIMENT 1

/* Globals in this module */

static int mode, uid, gid;    /* current values used by create */
static vt_size_t bytecount;
static int bufferpointer, funnyfile;

/* File characteristics */

static int outfd;
static struct stat sblock;
static unsigned int vmsmrs;             /* maximum record size */
static int vmsrat, vmsorg, vmsrfm;      /* other format (as integers) */
static unsigned long vmstime;


#ifdef EXPERIMENT
int outfile_rms();
vt_size_t get_varfilesize();
char EmptyTrailer[RECSIZE] = {'\0'};
#endif /* EXPERIMENT */


/* strcaseindex - search for string2 in string1, return address pointer */

char *strcaseindex( char *string1, char *string2)
{
char *c1, *c2, *cp;
    for(c1 = string1; *c1 !=0; c1++)
    {
        cp = c1;        /* save the start address */
        for(c2=string2; *c2 !=0 && toupper(*c1) == toupper(*c2); c1++,c2++);
           if(*c2 == 0)
                return cp;
    }
    return NULL;
}


/* cleanup_dire - routine to get rid of rooted directory problems */
/* and any others that turn up */

int cleanup_dire( char *string)
{
    char *ptr;
    int sts;

    /* We need to make sure the directory delimiters are square brackets,
       otherwise we'll get some problems... -- Richard Levitte */
    while ((ptr = strchr( string, '<')) != NULL)
        if (*(ptr- 1) != '^')
            *ptr = '[';
    while ((ptr = strchr( string, '>')) != NULL)
        if (*(ptr- 1) != '^')
            *ptr = ']';

    sts = 0;
    while ((ptr = strcaseindex( string, "][")) != NULL)
    {
        /* Just collapse around the string */
        if (*(ptr- 1) != '^')
        {
            strcpy( ptr, (ptr+ 2));
            sts = 1;
        }
    }

    /* Remove "[000000.". */
    if ((ptr = strcaseindex( string, "[000000.")) != NULL)
    {
        if (*(ptr- 1) != '^')
        {
            strcpy( (ptr+ 1), (ptr+ 8));
            sts = 1;
        }
    }

#if 0
    /* Remove "[000000]". */
    if ((ptr = strcaseindex( string, "[000000]")) != NULL)
    {
        if (*(ptr- 1) != '^')
        {
            strcpy( (ptr+ 1), (ptr+ 7));
            sts = 1;
        }
    }
#endif /* 0 */

    return sts;
}

/* 2009-05-06 SMS.
 * Changed to use unsigned arithmetic for checksum calculations, to
 * agree with the POSIX standard.
 */

/* fill_header - fill the fields of the header */
/* enter with the file name, if the file name is empty, */
/* then this is a trailer, and we should fill it with zeroes. */

int fill_header( int linkflag)
{
unsigned int chksum;
char tem[15];
unsigned char *ptr;

    /* Fill the header with zeros. */
    bzero( &header, RECSIZE);

    if (strlen( pathname) != 0)         /* only fill if there is a file */
    {
        /* File name, truncated if longer than NAMSIZE (100),
         * NUL-terminated (if space allows).
         */
        strncpy( header.title, pathname, NAMSIZE);
        /* GNU says that numeric fields are SP-NUL-terminated, except
         * for size (.count) and mtime (.time), which have no NUL, but
         * GNU tar 1.16.1 does not actually follow this.
         */
        sprintf( header.protection, "%06o ", mode); /* Mode. */
        sprintf( header.uid, "%06o ", uid);         /* UID. */
        sprintf( header.gid, "%06o ", gid);         /* GID. */
#ifdef _LARGEFILE
        if (bytecount >= 0100000000000LL)
        {
            /* Size exceeds max octal value.  Use (GNU) binary. */
            int ndx;
            vt_size_t bc;

            bc = bytecount;
            for (ndx = 11; ndx > 0; ndx--)
            {
                header.count[ ndx] = bc& 0xff;      /* Size (large, binary). */
                bc = bc>> 8;
            }
            header.count[ 0] = 0x80;
        }
        else
#endif /* def _LARGEFILE */
        {
            sprintf( tem, VT_SIZE_011O, bytecount); /* Size (small, octal). */
            strncpy( header.count, tem, 12);
        }

        sprintf( tem, "%011o ", vmstime);       /* Mtime. */
        strncpy( header.time, tem, 12);

        header.linkflag = linkflag;             /* Link flag. */

        /* Link name, truncated if longer than NAMSIZE (100),
         * NUL-terminated (if space allows).
         */
        strncpy( header.linkname, linkname, NAMSIZE);

        /* Calculate and place the checksum. */
        strncpy( header.chksum, "        ", 8); /* All blanks for chksum. */
        chksum = 0;
        for (ptr = (unsigned char *)&header;
         ptr < (unsigned char *) &header.dummy[ 12];
         ptr++)
             chksum += *ptr;
        sprintf( header.chksum, "%06o", chksum);
    }
    return 0;
}


/* fill_ln_header - Fill the fields of a long-name header. */

int fill_ln_header( char *name, int name_len, char type)
{
    unsigned int chksum;
    unsigned char *ptr;
    char tem[ 16];

#define LONG_LINK "././@LongLink"

    /* Fill the header with zeros. */
    bzero( &header, RECSIZE);

    /* Fill in the required header fields. */
    strcpy( header.title, LONG_LINK);           /* Fake file name. */
    sprintf( header.protection, "%07o", 0);     /* Fake mode. */
    sprintf( header.uid, "%07o", 0);            /* Fake UID. */
    sprintf( header.gid, "%07o", 0);            /* Fake GID. */
    sprintf( tem, "%011o ", (name_len+ 1));     /* Name length + 1. */
    strncpy( header.count, tem, 12);
    sprintf( tem, "%011o ", 0);                 /* Fake mtime. */
    strncpy( header.time, tem, 12);
    strncpy( header.chksum, "        ", 8);     /* All blanks for chksum. */
    header.linkflag = type;                     /* Linkflag. */

    chksum = 0;
    for (ptr = (unsigned char *)&header;
     ptr <= (unsigned char *)&header.linkflag;
     ptr++)
        chksum += *ptr;                /* make the checksum */
    sprintf( header.chksum, "%06o", chksum);

    return 0;
}


/* 2005-02-04 SMS.
   find_dir().

   Find directory boundaries in an ODS2 or ODS5 file spec.
   Returns length (zero if no directory, negative if error),
   and sets "start" argument to first character (typically "[") location.

   No one will care about the details, but the return values are:

       0  No dir.
      -2  [, no end.              -3  <, no end.
      -4  [, multiple start.      -5  <, multiple start.
      -8  ], no start.            -9  >, no start.
     -16  ], wrong end.          -17  >, wrong end.
     -32  ], multiple end.       -33  >, multiple end.

   Note that the current scheme handles only simple EFN cases, but it
   could be made more complicated.
*/
int find_dir( char *file_spec, char **start)
{
  char *cp;
  char chr;

  char *end_tmp = NULL;
  char *start_tmp = NULL;
  int lenth = 0;

  for (cp = file_spec; cp < file_spec+ strlen( file_spec); cp++)
  {
    chr = *cp;
    if (chr == '^')
    {
      /* Skip ODS5 extended name escaped characters. */
      cp++;
      /* If escaped char is a hex digit, skip the second hex digit, too. */
      if (char_prop[ (unsigned char) *cp]& 64)
        cp++;
    }
    else if (chr == '[')
    {
      /* Found start. */
      if (start_tmp == NULL)
      {
        /* First time.  Record start location. */
        start_tmp = cp;
        /* Error if no end. */
        lenth = -2;
      }
      else
      {
        /* Multiple start characters.  */
        lenth = -4;
        break;
      }
    }
    else if (chr == '<')
    {
      /* Found start. */
      if (start_tmp == NULL)
      {
        /* First time.  Record start location. */
        start_tmp = cp;
        /* Error if no end. */
        lenth = -3;
      }
      else
      {
        /* Multiple start characters.  */
        lenth = -5;
        break;
      }
    }
    else if (chr == ']')
    {
      /* Found end. */
      if (end_tmp == NULL)
      {
        /* First time. */
        if (lenth == 0)
        {
          /* End without start. */
          lenth = -8;
          break;
        }
        else if (lenth != -2)
        {
          /* Wrong kind of end. */
          lenth = -16;
          break;
        }
        /* End ok.  Record end location. */
        end_tmp = cp;
        lenth = end_tmp+ 1- start_tmp;
        /* Could break here, ignoring excessive end characters. */
      }
      else
      {
        /* Multiple end characters. */
        lenth = -32;
        break;
      }
    }
    else if (chr == '>')
    {
      /* Found end. */
      if (end_tmp == NULL)
      {
        /* First time. */
        if (lenth == 0)
        {
          /* End without start. */
          lenth = -9;
          break;
        }
        else if (lenth != -3)
        {
          /* Wrong kind of end. */
          lenth = -17;
          break;
        }
        /* End ok.  Record end location. */
        end_tmp = cp;
        lenth = end_tmp+ 1- start_tmp;
        /* Could break here, ignoring excessive end characters. */
      }
      else
      {
        /* Multiple end characters. */
        lenth = -33;
        break;
      }
    }
  }

  /* If both start and end were found,
     then set result pointer where safe.
  */
  if (lenth > 0)
  {
    if (start != NULL)
    {
      *start = start_tmp;
    }
  }
  return lenth;
}


/* flushout - write a fixed size block in output tarfile */

void flushout( int fdout)
{
    if (write(fdout,buffer,RECSIZE) != RECSIZE)
    {
        fprintf( stderr, "tar: error writing tarfile.\n");
        fprintf( stderr, " %s\n", strerror( errno));
        exit( vaxc$errno);
    }
    bufferpointer += RECSIZE;
}


/* initsearch - parse input file name */
/* To start looking for file names to satisfy the requested input,
 * use the sys$parse routine to create a wild-card name block. When
 * it returns, we can then use the resultant FAB and NAM blocks on
 * successive calls to sys$search() until there are no more files
 * that match.
 */

struct FAB fblock;              /* File attribute block */
struct NAMX nblock;             /* Name attribute block for rms */

int initsearch( char *string)
{
int status;
static char searchname[ NAMX_MAXRSS+ 1];
static char default_name[] = DEFAULT_NAME;

    if (strcaseindex( string, "::") != NULL)
    {
        fprintf( stderr, "tar: DECnet file access is not supported.\n");
        return -1;
    }
    nblock = CC_RMS_NAMX;
    fblock = cc$rms_fab;
    fblock.FAB_L_NAMX = &nblock;

#ifdef NAML$C_MAXRSS
    fblock.fab$l_dna = (char *) -1;     /* Using NAML for default name. */
    fblock.fab$l_fna = (char *) -1;     /* Using NAML for file name. */
#endif /* def NAML$C_MAXRSS */

    FAB_OR_NAML(fblock, nblock).FAB_OR_NAML_DNA = default_name;
    FAB_OR_NAML(fblock, nblock).FAB_OR_NAML_DNS = strlen(default_name);

    FAB_OR_NAML(fblock, nblock).FAB_OR_NAML_FNA = string;
    FAB_OR_NAML(fblock, nblock).FAB_OR_NAML_FNS = strlen(string);

    nblock.NAMX_L_ESA = searchname;
    nblock.NAMX_B_ESS = sizeof(searchname)- 1;

#ifdef DEBUG
    fprintf( stderr, "searching on: %s\n", string);
#endif
    status = sys$parse(&fblock);
    if(status != RMS$_NORMAL)
    {
        if(status == RMS$_DNF)
            fprintf( stderr, "tar: directory not found %s\n" ,searchname);
        else
            fprintf( stderr, "tar: error in SYS$PARSE.\n");
        return -1;
    }
    searchname[ nblock.NAMX_B_ESL] = '\0';      /* NUL-terminate the string. */

    /* Now reset for searching, pointing to the parsed name block */

    fblock = cc$rms_fab;
    fblock.FAB_L_NAMX = &nblock;
    return nblock.NAMX_B_ESL;   /* return the length of the string  */
}


/* lowercase - function to change a string to lower case */

int lowercase( char *string)
{
int i;
        for(i=0;string[i]=tolower(string[i]);i++);
        return (--i);           /* return string length */
}


/* scan_name - decode a file name */

/* Decode a file name into the directory, and the name, and convert
* to a valid UNIX pathname. Return  a value to indicate if this is
* a directory name, or another file.
* We return the extracted directory string in "dire", and the
* filename (if it exists) in "fname". The full title is in "line"
* at input.
*/

int scan_name( char *line, char *dire, char *fname, int absolute, int dev_len)
{
char *cp1,*cp2;
int len,len2,i;

/* The format will be VMS at input, so we have to scan for the
 * VMS device separator ':', and also the VMS directory separators
 * '[' and ']'.
 * If the name ends with '.dir;1' then it is actually a directory name.
 * The outputs dire and fname will be a complete file spec, based on the
 * default directory.
 * It may be a rooted directory, in which case there will be a "][" string,
 * remove it.
 * Strip out colons from the right, in case there is a node name (should
 * not be!) If the filename ends in a trailing '.', then suppress it,
 * unless the "d" option is set.
 */

    int ftype;

    ftype = ERROR1;
    if (STAT( line, &sblock))
    {
        fprintf( stderr, "tar: can't stat() %s\n", line);
        fprintf( stderr, "tar: %s\n", strerror( errno));
        funnyfile = -1;
    }
    else if (S_ISDIR( sblock.st_mode))
    {
        ftype = ISDIRE;
    }
    else if (S_ISLNK( sblock.st_mode))
    {
        ftype = ISSLNK;
    }
    else if (S_ISREG( sblock.st_mode))
    {
        ftype = ISFILE;
    }

    /* Mystery file type. */
    if (ftype == ERROR1)
    {
        vmstime = 0;            /* Avoid junk in output. */

        return ftype;
    }

    /* Now get the file attributes.  We don't use them all. */
    /* 2007-06-06 SMS.
     * Previously, this part was in get_attributes().
     */
    vmsrat = sblock.st_fab_rat;
    vmsmrs = sblock.st_fab_mrs;
    vmsrfm = sblock.st_fab_rfm;
    vmstime = sblock.st_mtime;
    bytecount = sblock.st_size;
    /* UID/GID fix. */
    uid = sblock.st_uid & 0xffff;
    gid = sblock.st_gid;
#ifndef CONSTANT_MODE
    mode = sblock.st_mode& 0777;        /* Actual UNIX-like mode bits. */
#endif /* ndef CONSTANT_MODE */

    /* Save everything past the device name. */
    strcpy( temp, (line+ dev_len));

/* If relative path, get rid of default directory part of the name  */

    if (absolute == 0)
    {
#if 0
        /* 2007-06-06 SMS.
         * "./" prefix is redundant and nonstandard.
         * "./fred/" and "fred/" are equivalent.
         */
        strcpy( dire, "./");
#endif /* 0 */
        *dire = '\0';

        for (cp1 = temp, cp2 = curdir;
         *cp2 && (toupper(*cp1) == toupper(*cp2));
         cp1++, cp2++);

        if(*cp2 == 0)
            *cp1 = 0;              /* Perfect match, no directory spec  */
        else
        {
            switch(*cp1)
            {
            case '.':
            case '[':               /* Legal beginnings or ends  */
                break;
            default:            /* We are above the default, use full name */
                cp1 = strchr(temp,'[');    /* Fixed this from 1.5  */
                /* *dire = '\0'; Obsolete. */
                break;
            }
            cp1++;                 /* Get past the starting . or [ */
        }
        strcat( dire, cp1);
    }
    else
    {
        /* Absolute path.  Add (or arrange to add) "/" prefix. */
        if (strncmp( temp, "[000000]", 8) == 0)
        {
            /* Discard "[000000" (effectively, "/000000"). */
            strcpy( dire, (temp+ 7));
        }
        else
        {
            strcpy( dire, "/");
            /* Use the whole directory spec. */
            strcpy( (dire+ 1), (temp+ 1));
        }
    }

    cp1 = dire - 1;                /* change trailing directory mark */
    while ((cp1 = strchr(cp1 + 1, ']')) != NULL)
        if (*(cp1 - 1) != '^') {
            *cp1++ = '/';
            *cp1 = '\0';           /* and terminate string (no file name) */
            break;
            }

    cp1 = line - 1;
    while ((cp1 = strchr(cp1 + 1, ']')) != NULL)
        if (*(cp1 - 1) != '^')
            break;

    strcpy(temp,cp1+1);

    if (ftype == ISDIRE)
    {
        /* Slash-terminate the directory name.  "x.dir;1" -> "x/". */
        if ((cp1 = strcaseindex( temp, ".DIR;1")) != 0)
        {
            *cp1++ = '/';
            *cp1 = '\0';
        }
        strcat( dire, temp);
        /* For a directory, fname is null. */
        *fname = '\0';
    }
    else
    {
        /* Not a directory.  Adjust fname.  Strip off version number.
         * Down-case ODS2 name.  Remove caret escapes from ODS5 name.
         */
        strcpy( fname, temp);
        *strchr( fname, ';') = '\0';            /* Truncate version nr. */
        if (acp_type != DVI$C_ACP_F11V5)
            lowercase( fname);                  /* ODS2: Map to lowercase. */
        else
        {
            eat_carets( fname);                 /* ODS5: Remove caret esc's. */
            if (ods2)                           /* If requested, then */
                lowercase( fname);              /* down-case the name. */
        }
    }

    /* Now, adjust the directory name.
     * Convert (unescaped) "." separators to "/".
     * Down-case ODS2 name.  Remove caret escapes from ODS5 name.
     */
    for (cp1 = dire + 1; *cp1; ++cp1)           /* Change '.' to '/'  */
        if ((*cp1 == '.') && (*(cp1 - 1) != '^'))
            *cp1 = '/';

    if (acp_type != DVI$C_ACP_F11V5)
        lowercase( dire);                       /* ODS2: down-case. */
    else
    {
        eat_carets( dire);                      /* ODS5: Remove caret esc's. */
        if (ods2)                               /* If requested, then */
            lowercase( dire);                   /* down-case the directory. */
    }

    /* Suppress a trailing dot on a file/link name, if requested. */
    if ((ftype != ISDIRE) && (dot == 0))
    {
        /* Don't destroy name ".".  (Require strlen( fname) > 1.)
         * (Although "." may not be much better than nothing on a
         * non-VMS system.  Convert to "?"?  See Info-ZIP Zip.)
         */
        if ((len = strlen( fname)- 1) > 0)
            if (fname[ len] == '.')
                fname[ len] = '\0';
    }

    return ftype;
}


/* search - get the next file name that matches the namblock */
/* that was set up by the sys$search() function. */

int search( char *buff, int maxlen, int *dev_len)
{
int status;

    nblock.NAMX_L_RSA = buff;
    nblock.NAMX_B_RSS = maxlen;
    nblock.NAMX_B_RSL = 0;      /* For next time around  */
    while ((status = sys$search(&fblock)) != RMS$_NMF)
    {
        /* NUL-terminate the resultant name. */
        buff[ nblock.NAMX_B_RSL] = '\0';

        /* We know the device name length now, so there's no good
         * reason to go searching for colons later.
         */
        *dev_len = nblock.NAMX_B_DEV;

        if(status == RMS$_NORMAL)
        {
            return nblock.NAMX_B_RSL;
        }
        else if (status == RMS$_PRV)
        {
            fprintf( stderr, "tar: no privilege to access %s\n", buff);
        }
        else if (status == RMS$_FNF)
        {
            fprintf( stderr, "tar: file not found %s\n", buff);
        }
        else
        {
            fprintf( stderr, "tar: error in SYS$SEARCH for %s\n", buff);
        }
    }
    return 0;
}


/* write_header - copy the header to the output file  */

int write_header( int fd)
{
int n;
    if((n=write(fd,&header,RECSIZE))!=RECSIZE)
    {
        fprintf( stderr, "tar: error writing header in tarfile.\n");
        fprintf( stderr, " %s\n", strerror( errno));
        exit( vaxc$errno);
    }
    bufferpointer += RECSIZE;
    return n;
}


/* write_trailer - write the two blank blocks on the output file */
/* pad the output to a full blocksize if needed. */

write_trailer( int fdout)
{
    memset( &header, 0, 512);
    write_header(fdout);
    write_header(fdout);

    /*
    ** This is to satisfy Tar on NT, that works only with block sizes
    ** of 1024.
    */

#ifdef DEBUG
#endif
    if (!padding)
      return 1;

    bufferpointer = bufferpointer% block_size;
    while (bufferpointer < block_size)
        write_header(fdout);
    return 1;
}


/* out_file - write out the file.
* move nbytes of data from "fdin" to "fdout";
* Always pad the output to a full RECSIZE
* If it a VMS text file, it may be various formats, so we will
* read the file twice in case of a text file
* so that we get the correct byte count.
* We set the funnyfile=-1 if this is funny file.
*/

int out_file( char *filename, vt_size_t nbytes, int fdout)
{
int i, n, pos;
char *bufp, *linep;
FILE *filein;
static char dbuffer[RECSIZE];
#ifdef EXPERIMENT
int r_value;
#endif /* EXPERIMENT */

    funnyfile = 0;

    if(vmsrfm == FAB$C_FIX || vmsrfm == FAB$C_STM ||
       vmsrfm == FAB$C_STMLF)
    {
        if ((filein = fopen( filename, FOPR)) == NULL)
        {
            fprintf( stderr, "tar: error opening input file %s\n", filename);
            return -1;
        }

#ifndef _LARGEFILE
        /* 2007-06-02 SMS.
         * This would be a reasonable place to add a test for a file
         * which is too large for us to get an accurate size in a
         * small-file vt_size_t.  I don't know of a nice way to do this
         * using an unsigned int for vt_size_t.
         */
#endif /* ndef _LARGEFILE */

        fill_header( LF_NORMAL);        /* We have all of the information */
        write_header(outfd);            /* So write to the output */

        while (nbytes > 0)
        {
            n = fread( buffer, 1,
             (nbytes > RECSIZE) ? RECSIZE : nbytes, filein);
            if (n == 0)
            {
                fclose(filein);
                fprintf( stderr, "tar: error reading input file %s\n",
                 filename);
                return -1;
            }

            /* If writing a partial block, clear the unset part. */
            if (nbytes < RECSIZE)
                bzero( (buffer+ nbytes), (RECSIZE- nbytes));

            nbytes -= n;
            flushout(fdout);
        }
        fclose(filein);
        return 0;
    }
    else
    {
        if(vmsrat == 0)			/* No record attributes? */
        {				/* Might be a text file anyway */
					/* So let's check two records. */

#if 0
	    /* Unfortunatelly, it won't work, because I seem to get RECSIZE
	       byte blocks, and I can't find a way to get just one record.
	       Thus, I can't reliably find out if the file is text or not.
	       Pity.  -- Richard Levitte */
	    if((filein = fopen(filename,"r","rfm=fix","mrs=512","ctx=rec")) == NULL)
	    {
                fprintf( stderr, "tar: error opening input file %s\n",
                 filename);
                return -1;
            }
	    if((nbytes = fread(dbuffer,1,RECSIZE,filein)) != 0)
	    {
		if(dbuffer[nbytes-2] != '\r' || dbuffer[nbytes-1] != '\n')
		{
		    fclose(filein);
		    funnyfile = -1;
		    return 0;
		}
	    }
	    else if (!feof(filein))
	    {
		fclose(filein);
		fprintf( stderr, "tar: error reading input file %s\n",
                 filename);
		fprintf( stderr,
                 "     record too large or incorrect RMS attributes\n");
		return -1;
	    }
	    if((nbytes = fread(dbuffer,1,RECSIZE,filein)) != 0)
	    {
		if(dbuffer[nbytes-2] != '\r' || dbuffer[nbytes-1] != '\n')
		{
		    fclose(filein);
		    funnyfile = -1;
		    return 0;
		}
	    }
	    else if (!feof(filein))
	    {
		fclose(filein);
		fprintf( stderr, "tar: error reading input file %s\n",
                 filename);
		fprintf( stderr,
                 "     record too large or incorrect RMS attributes\n");
		return -1;
	    }
	    fclose(filein); /* rewind(filein); */
	    /* exit(1); */
#else /* 0 */
# ifdef EXPERIMENT
            /*
            ** OK, the regular VMSTAR skips them, but these may be Schlumberger
            ** file formats (PDS, DLIS etc.) so let us take another shot
            ** at them by using Stream_LF or RMS routines.
            */

	    /*
	    ** The file turned out to be a variable length file, so we
	    ** need the exact size without the bytes used for indicating size
	    ** of each record. I could not find a way of accurately knowing
	    ** the size of such files without parsing them first !!!
	    */
	    bytecount = 0;
	    funnyfile = 0;
            nbytes = get_varfilesize(filename, !force);
	    if ((nbytes == (vt_size_t)(-1)) || funnyfile < 0)
/* 2007-06-02 SMS.  "return nbytes"?  It's not an int. */
/* 		return nbytes;                         */
                return -1;

	    bytecount = nbytes;

            /* r_value = outfile_stream(filename, nbytes, fdout); */
            r_value = outfile_rms(filename, nbytes, fdout, !force);

            return r_value;
# else /* EXPERIMENT */
	    funnyfile = -1;
	    return 0;
# endif /* EXPERIMENT [else] */
#endif /* 0 [else] */
        }

        if ((filein = fopen( filename, FOPR)) == NULL)
	{
	    fprintf(stderr, "tar: error opening input file %s\n", filename);
	    return -1;
	}
	/* compute "Unix" file size */
	nbytes = 0;
	while(fgets(dbuffer,RECSIZE,filein) !=  NULL)
	    nbytes += strlen(dbuffer);
	if (!feof(filein))
	{
	    fclose(filein);
	    fprintf( stderr, "tar: error reading input file %s\n", filename);
	    fprintf( stderr,
             "     record too large or incorrect RMS attributes\n");
	    return -1;
	}
	rewind(filein);                 /* Back to the beginning  */
	bytecount = nbytes;             /* Use the real count  */
	fill_header( LF_NORMAL);        /* Compute the header */
	write_header(outfd);            /* Write it  */
	bufp = buffer;

	if (nbytes != 0)		    /* Check for empty file */
	{
	    while(fgets(dbuffer,RECSIZE,filein) !=  NULL)
	    {                           /* Now copy to the output */
		linep = dbuffer;
		while (*linep != '\0')  /* read source file line by line */
		{
		    if (bufp >= &buffer[RECSIZE])
		    {
			bufp = buffer;
			flushout(fdout); /* if buffer full, flush it */
		    } /* copy in fixed size output buffer */
		    *bufp++ = *linep++;
		}
	    }
	    flushout(fdout);
	}
	fclose(filein);
    }
    return 0;
}


/* addfile - copy the vms file to the output file */

int addfile( char *vmsname, char *unixname)
{
    int ind;

    if (funnyfile < 0)          /* We don't output unsupported files. */
        return 0;

    if ((ind = out_file( vmsname, bytecount, outfd)) < 0)
    {
        /* 2010-11-20 SMS.
         * We don't output unsupported files if out_file() decides that
         * they're funny, either.  (Not checking here was causing fatal
         * "error copying" errors instead of "skipped".)
         */
        if (funnyfile < 0)
            return 0;

        return ind;
    }

    bufferpointer = bufferpointer% block_size;
    return 1;
}


/*--------------------------------------------------------------------*/

/* vms2tar -- handles create function */

void vms2tar( int argc, char **argv)
{
    char file_spec[ NAMX_MAXRSS+ 1];
    char string[ NAMX_MAXRSS+ 1];
    char *cp;
    char *dp;
    char *wp;
    char answ0;
    int absolute;
    int argi;
    int dev_len;
    int dir_len;
    int file_type;
    int process;
    vt_size_t bytecount_orig;

    unsigned short res_length;
    static char res_string[ NAMX_MAXRSS+ 1];
    $DESCRIPTOR( dev_descr, tarfile);
    $DESCRIPTOR( res_descr, res_string);

    bufferpointer = 0;

/* Open the output file */

#ifdef __DECC /* Suggested by Martin Stiftinger <stifting@iue.tuwien.ac.at> */
    dev_descr.dsc$w_length = strlen(tarfile);
    if (lib$getdvi(&DVI$_ACPTYPE,0,&dev_descr,0,&res_descr,&res_length) & 1 &&
     strncmp(res_descr.dsc$a_pointer,"UNKNOWN",res_length) == 0)
        /* The following should only happen with tapes. */
        outfd = open(tarfile,O_WRONLY,0600,"rfm=fix","mrs=512");
    else
#endif /* 1 */
        outfd = creat( tarfile, 0600, CREA);

    if (outfd < 0)
    {
        fprintf( stderr, "tar: error opening output tarfile %s", tarfile);
        fprintf( stderr, " %s\n", strerror( errno));
        exit( vaxc$errno);
    }

    /* Loop through the (remaining) command-line file spec arguments. */
    answ0 = '\0';               /* Clear the quit flag/answer character. */
    for (argi = 0; argi < argc; ++argi)
    {
        if (answ0 == 'q')       /* Quit early on user request. */
            break;

        strncpy( string, argv[ argi], NAMX_MAXRSS);
        string[ NAMX_MAXRSS] = '\0';    /* NUL-terminate, just in case. */
        absolute = 0;

        /* Look for an explicit device name. */
        cp = NULL;
        for (dp = string; *dp != '\0'; dp++)
        {
            char chr;

            chr = *dp;
            if (chr == '^')
            {
                /* Skip ODS5 extended name escaped characters. */
                dp++;
                /* If escaped char is a hex digit, then
                 * skip the second hex digit, too.
                 */
                if (char_prop[ (unsigned char) *dp]& 64)
                {
                    dp++;
                }
            }
            else if (chr == ':')
            {
                /* Found device-name colon. */
                cp = dp;
                break;
            }
            else if ((chr == '[') || (chr == ']') || (chr == '.'))
            {
                /* Found some non-device-name character. */
                break;
            }
        }

        if (cp == NULL)
        {
            /* No explicit device name.  Proceed using whole string. */
            cp = string;
        }
        else
        {
            /* Explicit device name requires ODS2/5 test. */

            /* 2007-06-06 SMS.
             * Old logic set "absolute" here, too, but a user _could_
             * specify "dev:[.dir]file.type", which, I claim, should be
             * processed the same as "[.dir]file.type" (so far as the
             * archive naming is concerned, anyway).
             */

            /* I don't think that it's necessary to lop off the stuff
             * after the device specification, but it's easy and should
             * be harmless.
             */
            curdevdesc.dsc$a_pointer = string;
            curdevdesc.dsc$w_length = cp - string + 1;
            lib$getdvi(&DVI$_ACPTYPE, 0, &curdevdesc, &acp_type, 0, 0);
                                    /* update device EFS/ODS5 status */
        }

        /* Attempt to find a directory spec in the file spec. */
        dir_len = find_dir( cp, &dp);

        if (dir_len == 0)
        {
            /* Device but no directory.  User must want absolute paths. */
            absolute = 1;
        }
        else
        {
            /* Found a directory.  Set "absolute" if not "[." or "[-". */
            if ((*(dp+ 1) != '.') && (*(dp+ 1) != '-'))
            {
                absolute = 1;
            }
        }

        if (initsearch( string) <= 0)
            fprintf( stderr, "tar: no files matching %s\n", string);
        else
        {
            answ0 = '\0';       /* Clear the quit flag/answer character. */
            while (search( newfile, ((sizeof newfile)- 1), &dev_len) != 0)
            {
                if (answ0 == 'q')       /* Quit early on user request. */
                    break;

                funnyfile = 0;
                strcpy( file_spec, newfile);
                cleanup_dire(newfile);
                file_type =
                 scan_name( newfile, new_dir, outfile, absolute, dev_len);
                strcpy( pathname, new_dir);
                strcat( pathname, outfile);
                pathname_len = strlen( pathname);
                *linkname = '\0';
                linkname_len = 0;
                process = 1;
                if (the_wait)
                {
                    answ0 = '\0';   /* Clear the quit flag/answer character. */
                    while (answ0 == '\0')
                    {
                        fprintf( stderr,
                         "%s : Archive (Yes/No/Quit/All) [N]? ",
                         file_spec);
                        fflush( stderr);

                        if (fgets( temp, sizeof( temp), stdin) == NULL)
                        {
                            /* Treat EOF or read error as "q". */
                            fprintf( stderr,
 "(EOF or read error.  Treating as \"Quit\" ...)\n");
                            strcpy( temp, "q");
                        }

                        /* Strip off a trailing newline, to avoid corrupt
                         * complaints when displaying the answer.
                         */
                        if (temp[ strlen( temp)- 1] == '\n')
                            temp[ strlen( temp)- 1] = '\0';

                        /* Interpret response. */
                        answ0 = tolower( temp[ 0]);
                        switch (answ0)
                        {
                            case 'a':       /* "All".  Yes, and stop asking. */
                            the_wait = 0;
                            case 'y':       /* "Yes".  Yes. */
                                process = 1;
                                break;
                            case '\0':      /* Null answer.  Treat as "No". */
                                answ0 = 'n';    /* (Don't try again.) */
                            case 'n':       /* "No".  No. */
                                process = 0;
                                break;
                            case 'q':       /* "Quit". */
                                process = 0;
                                break;
                            default:        /* Invalid response. */
                                fprintf( stderr, "Invalid response [%.1s]\n",
                                 temp);
                            answ0 = '\0';   /* Try again. */
                        }
                    }
                }

                if (process)
                {
                    if (file_type == ISDIRE)
                    {
                        if (pathname_len > NAMSIZE)
                        {
                            char *bufp;

                            bytecount =
                             (pathname_len+ RECSIZE)/ RECSIZE* RECSIZE;
                            fill_ln_header( pathname, pathname_len,
                             LF_LONGNAME);
                            write_header( outfd);
                            strcpy( buffer, pathname);
                            bzero( (buffer+ pathname_len+ 1),
                             bytecount- (pathname_len+ 1));

                            bufp = buffer;
                            while (bytecount > 0)
                            {
                                if (write( outfd, bufp, RECSIZE) != RECSIZE)
                                {
                                    fprintf( stderr,
                                     "tar: error writing tarfile.\n");
                                    fprintf( stderr, "%s\n", strerror( errno));
                                    exit( vaxc$errno);
                                }
                                bytecount -= RECSIZE;
                            }
                        }
                        bytecount =  0;
#ifdef CONSTANT_MODE
                        mode = 0755;
#endif /* def CONSTANT_MODE */
                        fill_header( LF_NORMAL);
                        write_header(outfd);
                    }
                    if (file_type == ISFILE)
                    {
                        if (pathname_len > NAMSIZE)
                        {
                            char *bufp;

                            bytecount_orig = bytecount;
                            bytecount =
                             (pathname_len+ RECSIZE)/ RECSIZE* RECSIZE;
                            fill_ln_header( pathname, pathname_len,
                             LF_LONGNAME);
                            write_header( outfd);
                            strcpy( buffer, pathname);
                            bzero( (buffer+ pathname_len+ 1),
                             bytecount- (pathname_len+ 1));

                            bufp = buffer;
                            while (bytecount > 0)
                            {
                                if (write( outfd, bufp, RECSIZE) != RECSIZE)
                                {
                                    fprintf( stderr,
                                     "tar: error writing tarfile.\n");
                                    fprintf( stderr, "%s\n", strerror( errno));
                                    exit( vaxc$errno);
                                }
                                bytecount -= RECSIZE;
                            }
                            bytecount = bytecount_orig;
                        }
#ifdef CONSTANT_MODE
                        mode = 0644;
#endif /* def CONSTANT_MODE */
                        if (addfile( newfile, pathname) < 0)
                        {
                            fprintf( stderr, "tar: error copying %s\n",
                             file_spec);
                            exit( vaxc$errno);
                        }
                    }

#ifdef SYMLINKS

                    if (file_type == ISSLNK)
                    {
                        /* Process path name. */
                        if (pathname_len > NAMSIZE)
                        {
                            char *bufp;

                            bytecount_orig = bytecount;
                            bytecount =
                             (pathname_len+ RECSIZE)/ RECSIZE* RECSIZE;
                            fill_ln_header( pathname, pathname_len,
                             LF_LONGNAME);
                            write_header( outfd);
                            strcpy( buffer, pathname);
                            bzero( (buffer+ pathname_len+ 1),
                             bytecount- (pathname_len+ 1));

                            bufp = buffer;
                            while (bytecount > 0)
                            {
                                if (write( outfd, bufp, RECSIZE) != RECSIZE)
                                {
                                    fprintf( stderr,
                                     "tar: error writing tarfile.\n");
                                    fprintf( stderr, "%s\n", strerror( errno));
                                    exit( vaxc$errno);
                                }
                                bytecount -= RECSIZE;
                            }
                            bytecount = bytecount_orig;
                        }

                        /* Process link (target) name. */
                        linkname_len =
                         readlink( file_spec, linkname, (sizeof linkname- 1));

                        if (linkname_len > NAMSIZE)
                        {
                            char *bufp;

                            bytecount_orig = bytecount;
                            bytecount =
                             (linkname_len+ RECSIZE)/ RECSIZE* RECSIZE;
                            fill_ln_header( linkname, linkname_len,
                             LF_LONGLINK);
                            write_header( outfd);
                            strcpy( buffer, linkname);
                            bzero( (buffer+ linkname_len+ 1),
                             bytecount- (linkname_len+ 1));

                            bufp = buffer;
                            while (bytecount > 0)
                            {
                                if (write( outfd, bufp, RECSIZE) != RECSIZE)
                                {
                                    fprintf( stderr,
                                     "tar: error writing tarfile.\n");
                                    fprintf( stderr, "%s\n", strerror( errno));
                                    exit( vaxc$errno);
                                }
                                bytecount -= RECSIZE;
                            }
                            bytecount = bytecount_orig;
                        }

                        bytecount = 0;
#ifdef CONSTANT_MODE
                        mode = 0644;
#endif /* def CONSTANT_MODE */
                        fill_header( LF_SYMLINK);
                        write_header(outfd);
                    }

#endif /* def SYMLINKS */

                    if (funnyfile < 0 && file_type == ISFILE)
                    {
                        fprintf( stderr,
                         "Skipping (unsupported format): %s\n", file_spec);
                    }
                    if (verbose && (funnyfile >= 0 || file_type == ISDIRE))
                    {
                         strcpy(creation,ctime(&vmstime)); /* work on this */
                         creation[24]=0;
                         fprintf( stderr, "%s %s %s\n",
                          creation + 4,
                          fofft( bytecount, "8", VT_SIZE_DU),
                          pathname);
                    }
                }
            } /* end while (file search) */
        } /* end if (no match to argument) */
    } /* end for (argument list) */
    write_trailer(outfd);
    close(outfd);
}



#ifdef EXPERIMENT
int outfile_rms(filename,nbytes,fdout,check_text)
char filename[];
vt_size_t nbytes;
int fdout;
int check_text;
{
    int n, rms_status;
    struct FAB myfab;
    struct XABFHC myxab;
    struct RAB myrab;
    unsigned short int b_size;
    char *UserBuffer, *TransBuffer;
    int BytesRead, TransBufSize, i, TotalBytes;

#if 0
    BytesRead = TotalBytes = 0;
    b_size = vmsmrs;
    UserBuffer = (char*) malloc(b_size * sizeof(char));
    TransBuffer = (char*) malloc((b_size+512) * sizeof(char));
#endif

    myfab = cc$rms_fab;
    myfab.fab$l_fna = filename;
    myfab.fab$b_fns = strlen(filename);
    myfab.fab$b_rat = vmsrat;
    myfab.fab$w_mrs = vmsmrs;
    myfab.fab$b_org = FAB$C_SEQ;
    myfab.fab$b_rfm = vmsrfm;

    myxab = cc$rms_xabfhc;
    myfab.fab$l_xab = (char*)&myxab;

    rms_status = sys$open(&myfab);
    if (rms_status & 1)
    {
#if 1
	BytesRead = TotalBytes = 0;
	b_size = myxab.xab$w_lrl;
	UserBuffer = malloc(b_size * sizeof(char));
        if (UserBuffer == NULL)
        {
            fprintf( stderr,
             "tar: memory allocation failed (loc = 101)\n");
            fprintf( stderr, " %s\n", strerror( errno));
            exit( vaxc$errno);
        }

	TransBuffer = malloc((b_size+512) * sizeof(char));
        if (TransBuffer == NULL)
        {
            fprintf( stderr,
             "tar: memory allocation failed (loc = 102)\n");
            fprintf( stderr, " %s\n", strerror( errno));
            exit( vaxc$errno);
        }
#endif
        myrab = cc$rms_rab;
        myrab.rab$w_usz = b_size;
#if 0
        if (b_size > 512)
#endif
            myrab.rab$l_ubf = UserBuffer;
#if 0
        else
            myrab.rab$l_ubf = buffer;
#endif
        myrab.rab$l_fab = &myfab;
        rms_status = sys$connect(&myrab);
    }
    if ( !(rms_status & 1) )
    {
        fprintf( stderr, " Unable to define RMS structures ...\n");
        fprintf( stderr, " Skipping file %s \n", filename);
        return -1;
    }

    fill_header( LF_NORMAL);        /* We have all of the information */
    write_header(fdout);            /* So write to the output */

#ifdef DEBUG
    fprintf( stderr, " Starting to transfer " VT_SIZE_D " bytes\n", nbytes);
#endif
#if 0
    if (b_size > 512)
    {
#endif
        /*
        ** OK, this is the tricky case, and output to tar file
        ** will need to be buffered/divided to fit 512 blocks.
        */
        TransBufSize = 0;
        rms_status = sys$get(&myrab);
        while( rms_status != RMS$_EOF && rms_status == RMS$_NORMAL)
        {
            BytesRead = myrab.rab$w_rsz;
	    if (check_text)
		if (UserBuffer[BytesRead-2] == '\r'
		    && UserBuffer[BytesRead-1] == '\n')
		{
		    UserBuffer[(--BytesRead)-1] = '\n';
		}
            nbytes -= BytesRead;
            memcpy( (TransBuffer + TransBufSize), UserBuffer, BytesRead);
            TransBufSize += BytesRead;
            if (TransBufSize >= 512)
            {
                /*
                ** OK, time to flush out the Transfer Buffer
                */
                i = 0;
                while (TransBufSize >= 512)
                {
                    memcpy(buffer, (TransBuffer + i), RECSIZE);
                    flushout(fdout);
                    i += RECSIZE;
                    TransBufSize -= RECSIZE;
                }
                memcpy(TransBuffer, (TransBuffer + i), TransBufSize);
            }
            rms_status = sys$get(&myrab);
        }
        if (TransBufSize != 0 )
        {
            /*
            ** Flush the buffer one last time
            */
            memcpy(buffer, EmptyTrailer, RECSIZE);
            memcpy(buffer, TransBuffer, TransBufSize);
            flushout(fdout);
        }
#if 0				/* This part should NOT be needed */
    }
    else
    {
        /*
        ** This is the simple case of PDS files where max. record length
        ** is 512, so no splitting is required.
        ** Infact even with this case some records could be less than
        ** 512 bytes, and so the output to tar file will need to be
        ** "buffered" - i.e. it will be safe to use the previous option
        ** for all cases.
        */
        rms_status = sys$get(&myrab);
        while( rms_status != RMS$_EOF && rms_status == RMS$_NORMAL)
        {
            BytesRead = myrab.rab$w_rsz;
            nbytes -= BytesRead;
            flushout(fdout);
            rms_status = sys$get(&myrab);
        }
    }
#endif
    sys$close(&myfab);
    return 0;
}


vt_size_t
get_varfilesize(filename,check_text)
char *filename;
int check_text;			/* Boolean: Check if text file on true. */
{
int n, rms_status;
struct FAB myfab;
struct XABFHC myxab;
struct RAB myrab;
unsigned short int b_size;
char *UserBuffer;
int BytesRead;
vt_size_t TotalBytes;

#if 0
    BytesRead = TotalBytes = 0;
    b_size = vmsmrs;
    UserBuffer = (char*) malloc(b_size * sizeof(char));
#endif

    myfab = cc$rms_fab;
    myfab.fab$l_fna = filename;
    myfab.fab$b_fns = strlen(filename);
    myfab.fab$b_rat = vmsrat;
    myfab.fab$w_mrs = vmsmrs;
    myfab.fab$b_org = FAB$C_SEQ;
    myfab.fab$b_rfm = vmsrfm;

    myxab = cc$rms_xabfhc;
    myfab.fab$l_xab = (char*)&myxab;

    rms_status = sys$open(&myfab);
    if (rms_status & 1)
    {
#if 1
	BytesRead = TotalBytes = 0;
	b_size = myxab.xab$w_lrl;
	UserBuffer = malloc(b_size * sizeof(char));
        if (UserBuffer == NULL)
        {
            fprintf( stderr,
             "tar: memory allocation failed (loc = 111)\n");
            fprintf( stderr, " %s\n", strerror( errno));
            exit( vaxc$errno);
        }
#endif
        myrab = cc$rms_rab;
        myrab.rab$w_usz = b_size;
        myrab.rab$l_ubf = UserBuffer;
        myrab.rab$l_fab = &myfab;
        rms_status = sys$connect(&myrab);
    }
    if ( !(rms_status & 1) )
    {
        fprintf( stderr, " Unable to define RMS structures ...\n");
        return -1;
    }

    rms_status = sys$get(&myrab);
    if (rms_status != RMS$_NORMAL)
    {
	sys$close(&myfab);
	funnyfile = -1;
	return 0;
    }
    while( rms_status != RMS$_EOF && rms_status == RMS$_NORMAL)
    {
        BytesRead = myrab.rab$w_rsz;
	if (check_text)
	    if (UserBuffer[BytesRead-2] != '\r'
		|| UserBuffer[BytesRead-1] != '\n')
	    {
		sys$close(&myfab);
		funnyfile = -1;
		return 0;
	    }
	    else
		BytesRead--; /* The \r will be thrown out the window */

        TotalBytes += BytesRead;
        rms_status = sys$get(&myrab);
    }
    sys$close(&myfab);
    return TotalBytes;
}
#endif /* EXPERIMENT */
        
