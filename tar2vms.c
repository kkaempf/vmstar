#define module_name	TAR2VMS
#define module_version  "V2.3"
/*
 *	TAR2VMS.C - Handles the extract and list functionality of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <unixio.h>
#include <unixlib.h>

#include <fab.h>
#include <libdef.h>
#include <lib$routines.h>
#include <math.h>
#include <rab.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <starlet.h>
#include <strdef.h>
#include <str$routines.h>
#include <stsdef.h>
#include <xabfhcdef.h>

#include "vmstarP.h"
#include "vmstar_cmdline.h"
#include "VMSmunch.h"
#include "vms_io.h"

#ifdef SYMLINKS
# include <unistd.h>
#endif /* def SYMLINKS */

#ifndef __MODE_T                        /* VAX C. */
# define mode_t unsigned short
#endif /* ndef __MODE_T */

/*
 *  ISO POSIX-1 definitions.  (VAX C lacks these.)
 */
#ifndef S_IRUSR
# define S_IRUSR 0000400        /* read permission: owner */
# define S_IWUSR 0000200        /* write permission: owner */
# define S_IXUSR 0000100        /* execute/search permission: owner */
#endif /* ndef S_IRUSR */


/* Globals in this module */

static mode_t mode;
static unsigned int uid;
static unsigned int gid;
static vt_size_t bytecount;
static FILE *tarfp;		/* The input file pointer */

/* File characteristics */

static char linkflag;

/* Misc. */

#define ABSASCTIM_MAX 23
char vmscreation[ ABSASCTIM_MAX+ 1];

/* Directory permission/protection post-processing.
 * A read-only directory can't be populated.  Therefore, at extraction
 * time, we only record directory permissions in a linked list, and
 * defer the actual permission setting until after all the files have
 * been extracted (and symlinks created).
 */

typedef struct dir_ll_t
{
    struct dir_ll_t *next;
    char *name;
    mode_t mode;
} dir_ll_t;

dir_ll_t *dir_ll_head = NULL;
dir_ll_t *dir_ll_p;


#ifdef SYMLINKS

/* Symlink post-processing.
 *
 * Creating symlinks on the fly opens a security hole, because a
 * maliciously constructed archive could use a symlink to redirect
 * otherwise harmless-looking files to any place in the file system.
 * Therefore, at extraction time, we only record symlinks in a linked
 * list, and defer their actual creation until after all the files have
 * been extracted.
 */

typedef struct symlink_ll_t
{
    struct symlink_ll_t *next;
    char *name;
    char *text;
    mode_t mode;
} symlink_ll_t;

symlink_ll_t *symlink_ll_head = NULL;
symlink_ll_t *symlink_ll_p;

#endif /* def SYMLINKS */


static void
rwx (short unsigned int bits, char *chars)
{
  chars[0] = (bits & S_IRUSR) ? 'r' : '-';
  chars[1] = (bits & S_IWUSR) ? 'w' : '-';
  chars[2] = (bits & S_IXUSR) ? 'x' : '-';
}


static void
setst (short unsigned int bits, char *chars)
{
#ifdef S_ISUID
  if (bits & S_ISUID)
    {
      if (chars[3] != 'x')
        /* Set-uid, but not executable by owner.  */
        chars[3] = 'S';
      else
        chars[3] = 's';
    }
#endif
#ifdef S_ISGID
  if (bits & S_ISGID)
    {
      if (chars[6] != 'x')
        /* Set-gid, but not executable by group.  */
        chars[6] = 'S';
      else
        chars[6] = 's';
    }
#endif
#ifdef S_ISVTX
  if (bits & S_ISVTX)
    {
      if (chars[9] != 'x')
        /* Sticky, but not executable by others.  */
        chars[9] = 'T';
      else
        chars[9] = 't';
    }
#endif
}


/* compute_modestring - fill in string STR with an ls-style ASCII
   representation of the st_mode field of file stats block STATP.
   10 characters are stored in STR; no terminating null is added.
   The characters stored in STR are:

   0    File type.  'd' for directory, 'l' for symbolic link,
        '-' for regular

   1    'r' if the owner may read, '-' otherwise.

   2    'w' if the owner may write, '-' otherwise.

   3    'x' if the owner may execute, 's' if the file is
        set-user-id, '-' otherwise.
        'S' if the file is set-user-id, but the execute
        bit isn't set.

   4    'r' if group members may read, '-' otherwise.

   5    'w' if group members may write, '-' otherwise.

   6    'x' if group members may execute, 's' if the file is
        set-group-id, '-' otherwise.
        'S' if it is set-group-id but not executable.

   7    'r' if any user may read, '-' otherwise.

   8    'w' if any user may write, '-' otherwise.

   9    'x' if any user may execute, 't' if the file is "sticky"
        (will be retained in swap space after execution), '-'
        otherwise.
        'T' if the file is sticky but not executable.  */

void
compute_modestring (mode_t mode, char *str)
{
  switch (linkflag)
    {
    case LF_LINK:
    case LF_SYMLINK:	str[0] = 'l'; break;	/* symbolic or hard link */
    case LF_DIR:	str[0] = 'd'; break;	/* directory */
    default:		str[0] = '-'; break;	/* regular */
    }

  rwx ((mode & 0700) << 0, &str[1]);
  rwx ((mode & 0070) << 3, &str[4]);
  rwx ((mode & 0007) << 6, &str[7]);
  setst (mode, str);
}


/* Get the next chunk of data belonging to the current file being extracted */

int data_read( char *buffer, vt_size_t bytes_to_read)
{
    double block_count;

    /* Be sure to not read past the number of bytes remaining to be
     * read, and handle the case where the number of bytes to read is a
     * RECSIZE multiple.
     */
    if (bytes_to_read>=BUFFERSIZE)
        block_count = BUFFERFACTOR;
    else
        if (modf((double)bytes_to_read/RECSIZE, &block_count)) block_count++;

    return fread( buffer, 1, (size_t)block_count* RECSIZE, tarfp);
}


/* Skip over data to get to the desired position.
 * Commonly used when listing.  Used for unrecognized types during
 * extraction.
 */

/* 2020-12-23 SMS. Without _LARGEFILE (typically on VAX), vt_size_t is
 * unsigned int, and a negative value of the argument here was seen as
 * positive, causing tarskip() to skip the whole of the remaining
 * archive, emit a spurious error, "EOF hit while skipping.".  (And any
 * archive members after the one with the problem were not processed.)
 * The caller is responsible for avoiding this situation.
 */

int tarskip( vt_size_t bytes)
{
    size_t i = 0;

    while (bytes > 0)
    {
        if ((i = fread( buffer, 1, RECSIZE, tarfp)) == 0)
        {
            fprintf( stderr, "tar: EOF hit while skipping.\n");
            return -1;
        }
        if (bytes > i)
            bytes -= i;
        else
            bytes = 0;
    }
    return 0;
}


/* Copy archive data to the output file, no conversion. */

int copyfile( char *outfile, vt_size_t nbytes)
{
    int fil, s, i, ctlchars, eightbitchars, nchars;
    int sts;
    int inbytes;

    register unsigned char c;
    struct VMStimbuf vtb;
    int binfile;		/* "extract as binary" flag */
    vt_size_t bytes = nbytes;
    struct FAB fil_fab;
    struct RAB fil_rab;
    struct XABFHC fil_xab;
#ifdef NAML$C_MAXRSS
    struct NAMX fil_nam;
#endif /* def NAML$C_MAXRSS */

    /* Set up FAB, NAML (if available), and RAB for new file. */

    fil_fab = cc$rms_fab;
    fil_fab.fab$b_fac = FAB$M_BIO;
    fil_fab.fab$b_org = FAB$C_SEQ;

#ifdef NAML$C_MAXRSS

    fil_nam = cc$rms_naml;              /* Initialize NAML. */
    fil_fab.fab$l_naml = &fil_nam;      /* Point FAB to NAML. */

    fil_fab.fab$l_dna = (char *) -1;    /* Using NAML for default name. */
    fil_fab.fab$l_fna = (char *) -1;    /* Using NAML for file name. */

#endif /* NAML$C_MAXRSS */

    FAB_OR_NAML( fil_fab, fil_nam).FAB_OR_NAML_FNA = outfile;
    FAB_OR_NAML( fil_fab, fil_nam).FAB_OR_NAML_FNS = strlen( outfile);

    fil_rab = cc$rms_rab;
    fil_rab.rab$l_fab = &fil_fab;
    fil_rab.rab$l_rbf = buffer;
    fil_rab.rab$l_bkt = 1;

    /* Read the first block of an archive member. */

    s = 0;
    binfile = binmode;
    inbytes = 0;

    if ((linkflag == LF_NORMAL) && (bytes != 0))
    {
        if ((inbytes = data_read( buffer, bytes)) < 0)
        {
            fprintf( stderr, "tar: error reading tar file (1).\n");
            fclose(tarfp);
            exit( SS$_DATALOST);
        }

        /* If automatic mode is set, then try to determine which kind of
         * file this is.
         */
        if (automode && inbytes != 0)
        {
            ctlchars = 0;
            eightbitchars = 0;

            /* Scan the buffer, counting chars with msb set and control
             * chars other than CR, LF, or FF.
             */
	    nchars = bytes < inbytes ? bytes : inbytes;
	    for (i = 0; i < nchars; ++i) {
		c = buffer[i];
	        if (c < ' ' &&
                    c != 0x0a && c != 0x0c &&
                    c != 0x0d && c != 0x09)
		    ctlchars++;
	        if (c > 127)
		    eightbitchars++;
	    }

            /* Now apply heuristics to determine if file is text or binary. */

	    ctlchars = ctlchars * 100 / nchars;
	    eightbitchars = eightbitchars * 100 / nchars;
	    if (ctlchars > 10 || eightbitchars > 30 ||
                (ctlchars > 5 && eightbitchars > 20))
                binfile = 1;
        }
    }

    /* Open/create the output file/symlink. */

#ifdef SYMLINKS

    if (symlinks && (linkflag == LF_SYMLINK))
    {
        /* To avoid problems caused by malicious symlinks in an archive,
         * symlink creation is delayed until after all the normal files
         * and directories have been created.  Here, we save the file
         * name, mode, and link text in a linked list for
         * symlink-creation post-processing.  Mode may not have much
         * value yet (if ever).
         */
        symlink_ll_p = malloc( sizeof( symlink_ll_t));
        if (symlink_ll_p == NULL)
        {
            fprintf( stderr,
             "tar: memory allocation failed (loc = 1)\n");
            fprintf( stderr, " %s\n", strerror( errno));
            exit( vaxc$errno);
        }

        /* Allocate storage for the file name and link text,
         * including NUL termination.
         */
        symlink_ll_p->name = malloc( strlen( outfile)+ 1);
        if (symlink_ll_p->name == NULL)
        {
            fprintf( stderr,
             "tar: memory allocation failed (loc = 2)\n");
            fprintf( stderr, " %s\n", strerror( errno));
            exit( vaxc$errno);
        }

        symlink_ll_p->text = malloc( strlen( linkname)+ 1);
        if (symlink_ll_p->text == NULL)
        {
            fprintf( stderr,
             "tar: memory allocation failed (loc = 3)\n");
            fprintf( stderr, " %s\n", strerror( errno));
            exit( vaxc$errno);
        }

        symlink_ll_p->next = symlink_ll_head;
        symlink_ll_head = symlink_ll_p;
        symlink_ll_p->mode = mode;
        strcpy( symlink_ll_p->name, outfile);
        strcpy( symlink_ll_p->text, linkname);
    }
    else
    {
#endif /* def SYMLINKS */

        /* 2004-11-23 SMS.
         * If RMS_DEFAULT values have been determined, and have not been
         * set by the user, then set some FAB/RAB parameters for faster
         * output.  User-specified RMS_DEFAULT values override the
         * built-in default values, so if the RMS_DEFAULT values could
         * not be determined, then these (possibly unwise) values could
         * not be overridden, and hence will not be set.  Honestly,
         * this seems to be excessively cautious, but only old VMS
         * versions will be affected.
         */

        /* If RMS_DEFAULT (and adjusted active) values are available,
         * then set the FAB/RAB parameters.  If RMS_DEFAULT values are
         * not available, then suffer with the default behavior.
         */

        if (rms_defaults_known > 0)
        {
            /* Set the FAB/RAB parameters accordingly. */
            fil_fab.fab$w_deq = rms_ext_active;
            fil_rab.rab$b_mbc = rms_mbc_active;
            fil_rab.rab$b_mbf = rms_mbf_active;

            /* Truncate at EOF on close, as we may over-extend. */
            fil_fab.fab$l_fop |= FAB$M_TEF ;

            /* If using multiple buffers, enable write-behind. */
            if (rms_mbf_active > 1)
            {
                fil_rab.rab$l_rop |= RAB$M_WBH;
            }
        }

        /* Set the initial file allocation according to the file
         * size.  Also set the "sequential access only" flag, as
         * otherwise, on a file system with highwater marking
         * enabled, allocating space for a large file may lock the
         * disk for a long time (minutes).
         */
        fil_fab.fab$l_alq = (bytes+ 511)/ 512;
        fil_fab.fab$l_fop |= FAB$M_SQO;

        if (binfile)
        {
            fil_fab.fab$b_rfm = FAB$C_FIX;
            fil_fab.fab$w_mrs = 512;
        }
        else
        {
            bzero( &fil_xab, sizeof(fil_xab));

            fil_fab.fab$b_rfm = FAB$C_STMLF;
            fil_fab.fab$b_rat = FAB$M_CR;
            fil_fab.fab$l_xab = (struct xabdef *)&fil_xab;

            fil_xab = cc$rms_xabfhc;
            fil_xab.xab$w_lrl = 32767;
        }

        sts = sys$create( &fil_fab);
        if ((sts& STS$M_SUCCESS) != STS$K_SUCCESS)
        {
            fprintf( stderr, "tar: error %08x creating ($CREATE) %s \n", sts, outfile);
            s = -1;
        }
        else
        {
            sts = sys$connect( &fil_rab);
            if ((sts& STS$M_SUCCESS) != STS$K_SUCCESS)
            {
                s = -1;
                fprintf( stderr, "tar: error creating ($CONNECT) %s \n",
                 outfile);
            }
        }

        if (s < 0)
        {
            errno = EVMSERR;
            vaxc$errno = sts;
/*            fprintf( stderr, "error: %s\n", strerror( errno));*/
            fprintf( stderr, "error: %d\n", errno);

            /* Skip the remainder of the problem archive member. */

            /* 2020-12-23 SMS. Without _LARGEFILE (typically on VAX),
             * vt_size_t is unsigned int, and a negative value of the
             * tarskip() argument was seen as positive, causing
             * tarskip() to skip the whole of the remaining archive,
             * emit a spurious error, "EOF hit while skipping.", and
             * fail to process any archive members after the one with
             * the problem.
             * Added a (valid) comparison here to avoid the problem.
             */
#ifndef _LARGEFILE
            if (bytes > inbytes)
#endif /* ndef _LARGEFILE */
                tarskip( bytes- inbytes);
        }
        else
        {
            if ((linkflag == LF_LINK) || (linkflag == LF_SYMLINK))
            {
                sprintf(buffer,"*** This file is a link to %s\n",linkname);
                fil_rab.rab$w_rsz = strlen(buffer);
                sts = sys$write(&fil_rab);
                if ((sts& STS$M_SUCCESS) != STS$K_SUCCESS)
                {
                    s = -1;
                    fprintf( stderr,
                     "tar: error writing ($WRITE) symlink file %s \n",
                     outfile);
                    errno = EVMSERR;
                    vaxc$errno = sts;
                    fprintf( stderr, " %s\n", strerror( errno));
                }

                /* Close the extracted file. */
                sys$close( &fil_fab);
            }
            else
            {
                while (bytes > 0 && s >= 0 && inbytes > 0)
                {
                    if (bytes>=BUFFERSIZE)
                        fil_rab.rab$w_rsz = BUFFERSIZE;
                    else
                        /* RMS writes an even number of bytes so zero next byte */
                        buffer[fil_rab.rab$w_rsz = bytes] = '\0';

                    sts = sys$write(&fil_rab);
                    if ((sts& STS$M_SUCCESS) != STS$K_SUCCESS)
                    {
                        s = -1;
                        fprintf( stderr,
                         "tar: error writing ($WRITE) %s\n", outfile);
                    }
                    else
                       fil_rab.rab$l_bkt += BUFFERFACTOR;
                    if (bytes > inbytes) {
                        bytes -= inbytes;
                        inbytes = data_read(buffer,bytes);
                    } else
                        bytes = 0;
                }

                /* Close the extracted file. */
                sys$close( &fil_fab);

                /* Check results. */
                if (s < 0)
                {
                    errno = EVMSERR;
                    vaxc$errno = sts;
                    fprintf( stderr, " %s\n", strerror( errno));

                    /* 2020-12-23 SMS.  See note above. */
                    /* Skip the remainder of the problem archive member. */
#ifndef _LARGEFILE
                    if (bytes > inbytes)
#endif /* ndef _LARGEFILE */
                        tarskip( bytes- inbytes);
                }

                if (inbytes == 0 && bytes != 0) {
                    fprintf( stderr, "tar: unexpected EOF on tar file.\n");
                    fclose(tarfp);
                    exit( vaxc$errno);
                }
                if (inbytes < 0) {
                    fprintf( stderr, "tar: error reading tar file (2).\n");
                    fclose(tarfp);
                    exit( vaxc$errno);
                }
            }
        }

/* 2008-11-05 SMS.
 * Disabled this message and exit.  Output errors should not be fatal,
 * and the error message has already been put out.
 */
#if 0
        if (s < 0)
        {
            fclose(tarfp);
            exit( vaxc$errno);
        }
#endif /* 0 */

#ifdef SYMLINKS
    }
#endif /* def SYMLINKS */

    if (verbose)
    {
        fprintf( stderr, "%s %s%c%s\n",
         creation+4, fofft( bytecount, "8", VT_SIZE_DU),
         binfile ? '*' : ' ',outfile);
        if ((linkflag == LF_LINK) || (linkflag == LF_SYMLINK))
            fprintf(stderr, "                         --> %s\n",linkname);
    }

    /* For a (successful) non-symlink file, set the permission/protection. */

    if (s >= 0)
    {
#ifdef SYMLINKS
        if ((linkflag != LF_LINK) && (linkflag != LF_SYMLINK))
#endif /* def SYMLINKS */
        {
            sts = chmod( outfile, (mode& 0777));
            if (sts < 0)
            {
                fprintf( stderr,
                 "tar: error setting protection (chmod) on file %s \n",
                 outfile);
                fprintf( stderr, " %s\n", strerror( errno));
            }
        }

        vtb.actime = 0;
        vtb.modtime = 0;
        if (date_policy & DP_MODIFICATION)
            vtb.actime = vmscreation;
        if (date_policy & DP_CREATION)
            vtb.modtime = vmscreation;
        VMSmunch(outfile, SET_TIMES, &vtb);
        VMSmunch(outfile, SET_EXACT_SIZE, &nbytes);
    }
    return 0;
}


/* Extract pathname (with USTAR prefix, if present) from the header. */

void store_pathname()
{
    char *cp = pathname;
    int ustar = 0;
    size_t len;

    /* 2014-11-11 SMS.
     * The "ustar" magic value differs among implementations.  POSIX
     * says "ustar"+ '\0', but "ustar"+ ' '+ '\0' (and others?) may be
     * encountered.  We'll be satisfied by "ustar" alone.
     */
    if (memcmp( header.magic, "ustar", 5) == 0)
    {
        ustar = 1;
        if (*header.prefix != '\0')             /* If the prefix exists... */
        {
	    strcpy( cp, header.prefix);         /* Copy the prefix. */
            cp += strlen( header.prefix);       /* Advance the pointer. */
            *(cp++) = '/';                      /* Append a slash. */
        }
    }
    strncpy( cp, header.title, NAMSIZE);        /* Copy the title. */
    cp[ NAMSIZE] = '\0';                        /* title may lack term. */

    /* A USTAR directory prefix and/or name/title may lack a trailing
     * slash, if the typeflag/linkflag field says that it's a directory.
     * We add the slash in that case to accommodate code elsewhere which
     * identifies a directory by that slash.  Note that the resulting
     * displayed name may be a lie, because we may append a slash to a
     * directory name when there was none in the archive.
     */
    if (ustar != 0)
    {
        if (header.linkflag == DIRTYPE)
        {
            len = strlen( cp);
            if (*(cp+ len- 1) != '/')           /* If last chr not slash ... */
            {
               *(cp+ len) = '/';                /* Append a slash. */
               *(cp+ len+ 1) = '\0';            /* NUL-terminate. */
            }
        }
    }
}


/* Decode the fields of the header.  Return non-zero when ready. */

int decode_header( int have_bytecount, int have_mtime)
{
    char ll;
    char *ptr;
    unsigned int chksum;
    unsigned int chksum_calc_sn;
    unsigned int chksum_calc_us;
    unsigned int imode;
    time_t *bintim;
    time_t idate;

    bintim = &idate;

    /* Extract saved checksum value. */
    sscanf( header.chksum, "%o", &chksum);

    /* 2009-05-06 SMS.
     * Changed to calculate checksums using both signed and unsigned
     * arithmetic.  Formerly only signed arithmetic was used, causing
     * "directory checksum error" when a high (sign) bit was set in the
     * header data.  (GNU tar did this in a UID field.)  Now, either
     * will be accepted.  Unsigned is the standard, but GNU tar accepts
     * either, and they know more than I.
     */

    /* Initialize checksum calculation with an all-space checksum field. */
    chksum_calc_sn = 8* ' ';
    chksum_calc_us = chksum_calc_sn;

    /* Add in the pre-checksum data. */
    for (ptr = (char *) &header; ptr < (char *) &header.chksum; ptr++)
    {
        chksum_calc_sn += *ptr;                 /* Add signed byte. */
        chksum_calc_us += (unsigned char) *ptr; /* Add unsigned byte. */
    }

    /* Add in the post-checksum data. */
    for (ptr = &header.linkflag; ptr < &header.dummy[ 12]; ptr++)
    {
        chksum_calc_sn += *ptr;                 /* Add signed byte. */
        chksum_calc_us += (unsigned char) *ptr; /* Add unsigned byte. */
    }

    /* Verify the checksum.  Abort if bad. */
    if ((chksum != chksum_calc_sn) && (chksum != chksum_calc_us))
    {
        /* Use the local pathname for the fatal message. */
        store_pathname();
        fprintf( stderr, "tar: directory checksum error for %s\n", pathname);
        exit( SS$_DATALOST);
    }

    /* Extract linkflag.  Replace LF_OLDNORMAL ('\0') by LF_NORMAL ('0'). */
    linkflag = header.linkflag;
    if (linkflag == LF_OLDNORMAL)
        linkflag = LF_NORMAL;

    if (have_bytecount == 0)
    {
        /* Determine size (of whatever).
         * GNU-like large size begins with %x00 or %x80.
         */
        if ((header.count[0] == 0) ||
         ((unsigned char) header.count[0] == 0x80))
        {
            /* Large (GNU tar binary) size. */
#ifdef _LARGEFILE
            int ndx;

            bytecount = 0;
            /* (Not much point in overflowing 63 bits, hence 5, not 0.) */
            for (ndx = 5; ndx <= 11; ndx++)
            {
                bytecount =
                (bytecount << 8) | (unsigned char) header.count[ ndx];
            }

#else /* def _LARGEFILE */
            /* Use the local pathname for the fatal message. */
            store_pathname();
            fprintf( stderr, "tar: file size overflow (b) for %s\n", pathname);
            exit( SS$_DATALOST);
#endif /* def _LARGEFILE [else] */
        }
        else
        {
            /* Small (conventional, octal) size. */
#ifndef _LARGEFILE
            vt_size_t bc;
            char bcs[ 12];
#endif /* ndef _LARGEFILE */
            /* Convert octal string to vt_size_t. */
            sscanf( header.count, VT_SIZE_O, &bytecount);
#ifndef _LARGEFILE
            /* Re-convert tentative value to detect overflow. */
            sprintf( bcs, "%o", bytecount);
            sscanf( bcs, VT_SIZE_O, &bc);
            if (bc != bytecount)
            {
                /* Use the local pathname for the fatal message. */
                store_pathname();
                fprintf( stderr, "tar: file size overflow (o) for %s\n",
                 pathname);
                exit( SS$_DATALOST);
            }
#endif /* ndef _LARGEFILE */
        }

    }

    /* Skip some archive member types. */
    if (linkflag == LF_LONGLINK)
        return -1;

    if (linkflag == LF_LONGNAME)
        return -1;

    if (linkflag == LF_XHD)
        return -1;

    if (linkflag == LF_XGL)
        return -1;

    /* If a link, and if there is no long link name, then use the short
     * link name.
     */
    if (((linkflag == LF_LINK) || (linkflag == LF_SYMLINK)) &&
     (*linkname == '\0'))
    {
        strncpy( linkname, header.linkname, NAMSIZE);
        linkname[ NAMSIZE] = '\0';      /* NUL-terminate, just in case. */
        linkname_len = strlen( linkname);
    }

    /* If there is no long path name, use the short path name. */
    if (*pathname == '\0')
    {
        store_pathname();
        pathname_len = strlen( pathname);
    }

    if (have_mtime == 0)
    {
        sscanf(header.time,"%o",bintim);

        strcpy(creation,ctime(bintim));     /* Work on this! */
        creation[24]=0;

        sprintf(vmscreation, "%2.2s-%3.3s-%4.4s %8.8s.00",
         &(creation[8]), &(creation[4]), &(creation[20]), &(creation[11]));
        vmscreation[4] = _toupper(vmscreation[4]);
        vmscreation[5] = _toupper(vmscreation[5]);
    }

    sscanf( header.protection, "%o", &imode);
    mode = imode;
    sscanf( header.uid, "%o", &uid);
    sscanf( header.gid, "%o", &gid);

    return 1;
}


/* Get the next file header from the input file buffer.  We will always
 * move to the next 512 byte boundary.
 */
int hdr_read( void *buffer)
{
    int stat;

    stat = fread( buffer, 1, RECSIZE, tarfp);   /* Read the header. */
    return stat;                                /* catch them next read ? */
}


/* Let's try to do our own, non-buggy mkdir ().  At least, it returns
 * better error codes, especially for non-unix statuses.  For ODS5 volumes
 * we need to do it ourselves anyway as DEC C mkdir will not preserve case
 * properly on directory specifications that are all lower case - get
 * converted to upper case unless DECC$EFS_CASE_PRESERVE is defined. It
 * could be considered rude to have to define it just for vmstar
 */

/* 2010-10-26 SMS.
 * Added RMS$_DIR to the list of correctable error codes.  Observed on
 * VMS VAX V5.5-2, DEC C V4.0-000.
 */

int mkdir_vmstar( char *dir, mode_t mode)
{
    struct dsc$descriptor dsc_dir = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0 };
    unsigned int status;

    dsc_dir.dsc$w_length = strlen( dir);
    dsc_dir.dsc$a_pointer = dir;

    status = lib$create_dir( &dsc_dir, 0, 0, 0, 0, 0);
#ifdef DEBUG
    vaxc$errno = status;
    fprintf( stderr, "mkdir_vmstar(): status = %%x%08x (%s)\n",
     status, strerror( EVMSERR));
#endif
    switch (status)
    {
        case SS$_CREATED : errno = 0; return 0;
        case SS$_NORMAL : errno = EEXIST; return -1;
        case LIB$_INVARG : errno = EINVAL; return -1;
        case LIB$_INVFILSPE : errno = EINVAL; return -1;
        case RMS$_DIR : errno = EINVAL; return -1;
        default: errno = EVMSERR; vaxc$errno = status; return -1;
    }
    return -1;
}


/* make_new -- Create a new directory. */

int make_new( char *want)
{
int status, created;
char *dotp;

#ifdef DEBUG
    fprintf( stderr, "make_new(0): want = %s (", want);
    for (dotp = want; *dotp != '\0'; dotp++)
        fprintf( stderr, "\\%o", *dotp);
    fprintf( stderr, ")\nbefore: errno = %d (%s)\n",
     errno, strerror( errno));
#endif
    created = 1;
    status = mkdir_vmstar(want, 0);   /* our mkdir creates all missing levels */
    if (status != 0)
    {
#ifdef DEBUG
        fprintf( stderr, "make_new(1): status = %d, errno = %d (%s)\n",
         status, errno, strerror( errno));
#endif
        if (errno == EEXIST)
            return 0;
        if (errno != EINVAL)
            return -1;            /* unknown error, simply return */
        else                      /* maybe too many levels of directories */
        {                         /* change "[...FOO.BAR]" to "[...FOO$BAR]" */
        for (dotp = &want[strlen(want) - 1];dotp > want && status != 0;)
            if (*--dotp == '.')
            {
                *dotp = '$';
#ifdef DEBUG
                fprintf( stderr, "make_new(1+): want = %s\n", want);
#endif
                status = mkdir_vmstar(want, 0);
                if (status != 0 && errno == EEXIST)
                {
                    status = created = 0;
                    break;
                }
#ifdef DEBUG
                fprintf( stderr, "make_new(2): status = %d, errno = %d (%s)\n",
                 status, errno, strerror( errno));
#endif
            }
        }
    }
#ifdef DEBUG
    fprintf( stderr, "make_new(3): status = %d, errno = %d (%s)\n",
     status, errno, strerror( errno));
#endif
    if (status != 0)
        return -1;
    if (verbose && created)
        fprintf( stderr, "                              %s\n", want);
    return 0;
}


/* vms_cleanup -- Remove illegal characters from directory and file
 * names.  Replace hyphens and commas with underscores.  Return the
 * number of translations that were made.
 */

vms_cleanup( char *string)
{
    int i,flag=0;
    char c, *p;
    static char *badchars, *translate;

    if (acp_type == DVI$C_ACP_F11V5)
    {
        badchars = BADCHARS_ODS5;
        translate = TRANSLATE_ODS5;
    }
    else
    {
        badchars = BADCHARS_ODS2;
        translate = TRANSLATE_ODS2;
    }

    for(i=0; c=string[i]; ++i)
    {
        if ((p = strchr(badchars, c)) != NULL)
        {                    /* Replace illegal characters by underscores */
            string[i] = translate[p-badchars];
            flag++;          /* Record if any changes were made */
        }
        else
        {
            /* Escape remaining illegal ODS2 characters for ODS5.
             * Dots must be escaped only if they are in a directory
             * portion of the path, and only if they won't be converted
             * later to "_" (dot).
             */
            if (acp_type == DVI$C_ACP_F11V5 &&
             ((p = strchr(BADCHARS_ODS2, c)) != NULL ||
             ((c == '.') && (dot) && strchr(string + i, '/') != NULL)))
            {
                strcpy(string + i + 1, string + i);
                string[i++] = '^';
            }
        }
        if (ods2) string[i] = toupper(string[i]);  /* Map to uppercase */
    }

    /* Convert dots to underscores in directory names for ODS2, or if
     * /DOTS was not specified.
     */
    if (!dot || ods2)
    {
        char *rslash;

        if ((rslash = strrchr( string, '/')) != NULL)
        {
            char *cp;

            for (cp = string; cp < rslash; cp++)
                if (*cp == '.')
                    *cp = '_';
        }
    }
    return flag;
}


/* scan_path -- decode a Un*x file name into the directory and name */

/* Return a value to indicate if this is a directory name, or another file
* We return the extracted directory string in "dire", and the
* filename (if it exists) in "fname". The full title is in "line"
* at input.
*/

int scan_path( char *line, char *dire, char *fname)
{
    char *end1;
    int len,len2,i,ind;

/* The format will be UNIX at input, so we have to scan for the
 * UNIX directory separator '/'.
 * If the name ends with '/' then it is actually a directory name.
 * If the directory consists only of '.', then don't add a subdirectory
 * The output directory will be a complete file spec, based on the
 * default directory.
 */

    strcpy(dire,curdir);                /* Start with the current dir */

    /* We need to make sure the directory delimiters are square brackets,
     * otherwise we'll get some problems... -- Richard Levitte
     */
    while ((end1 = strchr(dire,'<')) != 0)
        *end1 = '[';
    while ((end1 = strchr(dire,'>')) != 0)
        *end1 = ']';

    if (strncmp(line,"./",2)==0)
        strcpy(line,line+2);            /* ignore "./" */
    strcpy(temp,line);                  /* Start in local buffer */
    ind=vms_cleanup(temp);              /* Remove illegal vms characters */
    if ((end1=strrchr(temp,'/'))==0)    /* No directory at all  ? */
        strcpy(fname,temp);             /* Only a file name */
    else
    {                                   /* End of directory name is '/' */
        *end1 = 0;                      /* Terminate directory name */
        strcpy(fname,end1+1);           /* File name without directory */
        for (i=1;temp[i];i++)           /* Change '/' to '.' in directory */
            if (temp[i]=='/')           /* and '.' to '_' */
                temp[i]='.';
            else if (!dot && temp[i] == '.' && acp_type != DVI$C_ACP_F11V5)
                temp[i] = '_';
        if (*temp == '/')               /* absolute path ? */
        {
            *temp = '[';                /* yes, build absolute VMS path */
            strcpy(dire,temp);
        }
        else
        {
            dire[strlen(dire)-1] = (*temp=='.')?0:'.' ;
                 /* "." to indicate a subdirectory (unless already there )*/
            strcat(dire,temp);      /* Add on the new directory  */
        }
        strcat(dire,"]") ;              /* And close with ']' */
    }
    if (strlen(fname)==0)        /* Could this cause problems ? */
    {
        return ISDIRE;
    }
    else
    {
        if (acp_type != DVI$C_ACP_F11V5)
        {
            if (underdot == 0)
            {   /* Replace non-first dots with "_". */
                for(i=0,end1=fname;*end1;end1++)
                    if (*end1 == '.')
                        if (i++) *end1 = '_';
            }
            else
            {   /* Replace non-last dots with "_". */
                for (i = 0, end1 = fname+ strlen( fname)- 1 ;
                 end1 >= fname ; end1--)
                    if (*end1 == '.')
                        if (i++)
                            *end1 = '_';
            }
        }
    }
    return ISFILE;
}


/* STR$MATCH_WILD - case insensitive */

unsigned int str_case_match_wild( struct dsc$descriptor *candidate,
 struct dsc$descriptor *pattern)
{
    struct dsc$descriptor local_candidate, local_pattern;
    int i;
    unsigned int sts;

    local_candidate = *candidate;
    local_pattern = *pattern;
    local_candidate.dsc$a_pointer = malloc( candidate->dsc$w_length);
    if (local_candidate.dsc$a_pointer == NULL)
    {
        fprintf( stderr,
         "tar: memory allocation failed (loc = 11)\n");
        fprintf( stderr, " %s\n", strerror( errno));
        exit( vaxc$errno);
    }

    local_pattern.dsc$a_pointer = malloc( pattern->dsc$w_length);
    if (local_pattern.dsc$a_pointer == NULL)
    {
        fprintf( stderr,
         "tar: memory allocation failed (loc = 12)\n");
        fprintf( stderr, " %s\n", strerror( errno));
        exit( vaxc$errno);
    }

    for (i = 0 ; i < candidate->dsc$w_length; i++) {
	char *ptr = candidate->dsc$a_pointer;
	char *lptr = local_candidate.dsc$a_pointer;
        lptr[i] = toupper(ptr[i]);
    }
    for (i = 0 ; i < pattern->dsc$w_length; i++) {
	char *ptr = pattern->dsc$a_pointer;
	char *lptr = local_pattern.dsc$a_pointer;
        lptr[i] = toupper(ptr[i]);
    }
    sts = str$match_wild( &local_candidate, &local_pattern);
    free( local_candidate.dsc$a_pointer);
    free( local_pattern.dsc$a_pointer);
    return sts;
    }


/*
 * 2006-10-04 SMS.
 * vms_path_fixdown().
 *
 * Convert VMS directory spec to VMS directory file name.  That is,
 * change "dev:[a.b.c.e]" to "dev:[a.b.c]e.DIR;1".  The result (always
 * larger than the source) is returned in the user-supplied buffer.
 */

#define DIR_TYPE_VER ".DIR;1"

static char *vms_path_fixdown( const char *dir_spec, char *dir_file)
{
    char dir_close;
    char dir_open;
    unsigned i;
    unsigned dir_spec_len;

    dir_spec_len = strlen(dir_spec);
    if (dir_spec_len == 0) return NULL;
    i = dir_spec_len - 1;
    dir_close = dir_spec[i];

    /* Identify the directory delimiters (which must exist). */
    if (dir_close == ']')
    {
        dir_open = '[';
    }
    else if (dir_close == '>')
    {
        dir_open = '<';
    }
    else
    {
        return NULL;
    }

    /* Find the beginning of the last directory name segment. */
    while ((i > 0) && ((dir_spec[i - 1] == '^') ||
           ((dir_spec[i] != '.') && (dir_spec[i] != dir_open))))
    {
        i--;
    }

    /* Form the directory file name from the pieces. */
    if (dir_spec[i] == dir_open)
    {
        /* Top-level directory. */
        sprintf(dir_file, "%.*s000000%c%.*s%s",
          /*  "dev:[" "000000" "]" */
          (i + 1), dir_spec, dir_close,
          /*  "a" ".DIR;1" */
          (dir_spec_len - i - 2), (dir_spec + i + 1), DIR_TYPE_VER);
    }
    else
    {
        /* Non-top-level directory. */
        sprintf(dir_file, "%.*s%c%.*s%s",
          /*  "dev:[a.b.c" "]" */
          i, dir_spec, dir_close,
          /*  "e" ".DIR;1" */
          (dir_spec_len - i - 2), (dir_spec + i + 1), DIR_TYPE_VER);
    }
    return dir_file;
} /* end function vms_path_fixdown(). */


/*--------------------------------------------------------------------*/

/* tar2vms -- handles extract and list options */

void tar2vms( int argc, char **argv)
{
    char *argp;
    char *ptr;
    int argi;
    int file_type;
    int flag;
    int have_bytecount;
    int have_mtime;
    int inbytes;
    int j;
    int process;
    int status;
    struct dsc$descriptor pattern =
     { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, NULL };
    struct dsc$descriptor candidate =
     { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, pathname };

    /* Open the archive for reading. */
    if ((tarfp = fopen( tarfile, FOPR)) == NULL)
        {
        fprintf( stderr, "tar: error opening tarfile: %s\n", tarfile);
        exit( vaxc$errno);
        }

    /* Read headers from this archive, decode the names, and so on. */

    /* Clear the (potentially long) names. */
    *pathname = '\0';
    *linkname = '\0';

    have_bytecount = 0;
    have_mtime = 0;
    while ((status = hdr_read( &header)) == RECSIZE)    /* 0 on end of file */
    {
        process = 0;

        if (strlen( header.title) != 0)         /* Valid header */
        {
            process = decode_header( have_bytecount, have_mtime);
            have_bytecount = 0;

#ifdef DEBUG
            fprintf( stderr, "Process = %d.  Header linkflag = %d (%c).\n",
             process, linkflag, linkflag);
#endif

            if (linkflag == LF_LONGLINK)
            {
                /* Read long link name ('K'). */
                if ((inbytes = data_read( linkname, bytecount)) < 0)
                {
                    fprintf( stderr, "tar: error reading tar file (3).\n");
                    fclose(tarfp);
                    exit( SS$_DATALOST);
                }
                linkname_len = strlen( linkname);
            }
            else if (linkflag == LF_LONGNAME)
            {
                /* Read long file name ('L'). */
                if ((inbytes = data_read( pathname, bytecount)) < 0)
                {
                    fprintf( stderr, "tar: error reading tar file (4).\n");
                    fclose(tarfp);
                    exit( SS$_DATALOST);
                }
                pathname_len = strlen( pathname);
            }
            else if (linkflag == LF_VU_XHDR)
            {
                char sun_e_buf[ RECSIZE];
                char *equal;
                char *item;
                int field_len;
                int item_cnt;
                int b_off;
                vt_size_t bc;
                time_t imtimes;
                int imtimef;
                double mtime;

                /* Read in the next block with the Sun -E data. */
                if ((inbytes = data_read( sun_e_buf, bytecount)) < 0)
                {
                    fprintf( stderr, "tar: error reading tar file (5).\n");
                    fclose(tarfp);
                    exit( SS$_DATALOST);
                }

                /* Extract and interpret variable=value strings. */
                b_off = 0;
                while (b_off < bytecount)
                {
                    item_cnt = sscanf( &sun_e_buf[ b_off], "%d",
                     &field_len);

                    item = strchr( &sun_e_buf[ b_off], ' ')+ 1;
                    sun_e_buf[ b_off+ field_len- 1] = '\0';
                    equal = strchr( item, '=');
                    if (equal == NULL)
                    {
                        fprintf( stderr, "tar: Error parsing Sun -E data.\n");
                        fclose(tarfp);
                        exit( SS$_DATALOST);
                    }
                    else
                    {
                        *equal = '\0';
                        if (strcmp( item, "size") == 0)
                        {
                            sscanf( (equal+ 1), VT_SIZE_D, &bc);
                            have_bytecount = 1;
#ifndef _LARGEFILE
                            /* Expect trouble with "size=xxx" in a
                             * small-file program.  Compare re-converted
                             * size with original "size=" string.
                             */
                            if (strcmp( (equal+ 1),
                             fofft( bc, "", VT_SIZE_DU)))
                            {
                                fprintf( stderr,
                                 "tar: file size overflow (e) for %s\n",
                                 pathname);
                                exit( SS$_DATALOST);
                            }
#endif /* ndef _LARGEFILE */
                        }
                        else if (strcmp( item, "mtime") == 0)
                        {
                            /* Expect "mtime=xxx.xxx". */
                            sscanf( (equal+ 1), "%lf", &mtime);

                            /* Round floating-point mtime to hundredths
                             * of seconds, re-convert into into
                             * vmscreation[], as in decode_header(), but
                             * with actual fractional seconds (instead
                             * of a constant ".00").
                             */
                            mtime += 0.005;
                            imtimes = floor( mtime);
                            imtimef = (mtime- imtimes)* 100;

                            strcpy( creation, ctime( &imtimes));
                            creation[24]=0;

                            sprintf( vmscreation,
                             "%2.2s-%3.3s-%4.4s %8.8s.%02d",
                             &(creation[8]), &(creation[4]), &(creation[20]),
                             &(creation[11]), imtimef);
                            vmscreation[4] = _toupper(vmscreation[4]);
                            vmscreation[5] = _toupper(vmscreation[5]);

                            have_mtime = 1;
                        }
                        else
                        {
                            fprintf( stderr,
                             "tar: Unknown Sun -E datum: %s=%s.\n",
                             item, (equal+ 1));
                        }
                    }
                    b_off += field_len;
                }

                /* If we got a real "size=bytecount" value, then set our
                 * bytecount to this value, and set the flag to say we
                 * have it.
                 */
                if (have_bytecount > 0)
                {
                    bytecount = bc;
                }
                continue;
            }
            else if (process < 0)
            {
                /* Unrecognized typeflag/linkflag (including LF_XHG or
                 * LF_XGL).  Currently, do nothing.  Skip data.
                 */
                tarskip( bytecount);
            }

            if (process >= 0)
            {
                /* If file names were specified on the command line,
                 * then check if any match the current archive member.
                 */
                if (argc > 0)
                {
                    process = 0;
                    argi = 0;
                    while (argi < argc)
                    {
                        pattern.dsc$w_length = strlen( argv[ argi]);
                        pattern.dsc$a_pointer = argv[ argi++];
                        candidate.dsc$w_length = pathname_len;

                        if (str_case_match_wild( &candidate, &pattern) ==
                         STR$_MATCH)
                        {
                            process = 1;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
             status = 1;
             break;
        }

        if ((process > 0) && the_wait)
        {
            char answ0;

            answ0 = '\0';
            while (answ0 == '\0')
            {
                fprintf( stderr, "%s : Extract (Yes/No/Quit/All) [N]? ",
                 pathname);
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
                    case 'a':           /* "All".  Yes, and stop asking. */
                        the_wait = 0;
                    case 'y':           /* "Yes".  Yes. */
                        process = 1;
                        break;
                    case '\0':          /* Null answer.  Treat as "No". */
                        answ0 = 'n';    /* (Don't try again.) */
                    case 'n':           /* "No".  No. */
                        process = 0;
                        break;
                    case 'q':           /* "Quit".  Exit. */
                        exit( SS$_NORMAL);
                    default:            /* Invalid response. */
                        fprintf( stderr, "Invalid response [%.1s]\n",
                         temp);
                    answ0 = '\0';       /* Try again. */
                }
            }
        }

        if ((process > 0) && extract)
        {
            file_type = scan_path( pathname, new_dir, newfile);
            cleanup_dire( new_dir);
            if (make_new( new_dir) != 0)
                fprintf( stderr, "tar: error creating %s\n", new_dir);

            if (file_type == ISDIRE)
            {
                /* Save the mode and the directory file name in a linked
                 * list for mode-setting post-processing.
                 */
                dir_ll_p = malloc( sizeof( dir_ll_t));
                if (dir_ll_p == NULL)
                {
                    fprintf( stderr,
                     "tar: memory allocation failed (loc = 21)\n");
                    fprintf( stderr, " %s\n", strerror( errno));
                    exit( vaxc$errno);
                }

                /* Allocate storage for the directory file name.
                 * Leave space for NUL, ".DIR;1", and possible MFD
                 * ("[A]" -> [000000]A.DIR;1).  (+13 should do it.)
                 */
                dir_ll_p->name = malloc( strlen( new_dir)+ 16);
                if (dir_ll_p->name == NULL)
                {
                    fprintf( stderr,
                     "tar: memory allocation failed (loc = 22)\n");
                    fprintf( stderr, " %s\n", strerror( errno));
                    exit( vaxc$errno);
                }

                vms_path_fixdown( new_dir, dir_ll_p->name);
                dir_ll_p->next = dir_ll_head;
                dir_ll_head = dir_ll_p;
                dir_ll_p->mode = mode;
            }
            else if (file_type == ISFILE)
            {
                /* Construct the destination file specification. */
                strcpy( outfile, new_dir);
                strcat( outfile, newfile);
                /* Take care of ODS5 file names such as: foo2.0.1 */
                if (acp_type == DVI$C_ACP_F11V5)
                   strcat( outfile, ";");
                /*  Move the data into the output file (or symlink). */
                copyfile( outfile, bytecount);
            }
        }
        else
        {
            if ((process > 0) && list)               /* listing only */
            {
                /* Some tar programs set the 'linkflag' to LF_DIR ('5')
                 * and don't append a '/' at the end for directory files.
                 * However, it appears that some tar programs do (POSIX
                 * tar, for instance).  So if the file is a "regular
                 * file" (LF_NORMAL, '0') and the pathname has a
                 * trailing slash, change the 'linkflag' to LF_DIR and
                 * remove the slash.
                 */
                if ((pathname[ pathname_len- 1] == '/') &&
                 (linkflag == LF_NORMAL))
                {
                    linkflag = LF_DIR;
                    pathname_len--;
                    pathname[ pathname_len] = '\0';
                }

                if (verbose)
                {
                    char modestring[10];

                    compute_modestring(mode,modestring);

                    fprintf( stderr, "%10.10s %5d/%-5d %s %s %s\n",
                     modestring,
                     uid,
                     gid,
                     fofft( bytecount, "6", VT_SIZE_DU),
                     creation+4,
                     pathname);
                }
                else
                {
                    fprintf( stderr, "%s\n", pathname);
                }

                if ((linkflag == LF_LINK) || (linkflag == LF_SYMLINK))
                    fprintf( stderr,
                     "                                --->  %s\n",
                     linkname);
            }

            /* Skip through unused file data. */
            if (linkflag == LF_NORMAL)
                tarskip( bytecount);
        }

        /* Clear long file and link names. */
        if (process >= 0)
        {
            *pathname = '\0';
            *linkname = '\0';
        }

    } /* end while  */


#ifdef SYMLINKS

    /* Symlink post-processing. */

    for (symlink_ll_p = symlink_ll_head;
     symlink_ll_p != NULL;
     symlink_ll_p = symlink_ll_p->next)
    {
        int sts;

        sts = symlink( symlink_ll_p->text, symlink_ll_p->name);
        if (sts < 0)
        {
            fprintf( stderr, "tar: error creating symlink %s\n", outfile);
            fprintf( stderr, " %s\n", strerror( errno));
        }
    }

#endif /* def SYMLINKS */

    /* Directory permission/protection post-processing. */

    for (dir_ll_p = dir_ll_head;
     dir_ll_p != NULL;
     dir_ll_p = dir_ll_p->next)
    {
        int sts;

        sts = VMSmunch( dir_ll_p->name, SET_MODE, &dir_ll_p->mode);
        if ((sts& STS$M_SUCCESS) != STS$K_SUCCESS)
        {
            fprintf( stderr,
             "tar: error setting protection (VMSmunch) on file %s \n",
             outfile);
            errno = EVMSERR;
            vaxc$errno = sts;
            fprintf( stderr, " %s\n", strerror( errno));
        }
    }


    if (status == 1)                    /* Empty header */
    {
#if 0
        fprintf( stderr, "Do you wish to move past the EOF mark (y/n) ? ");
        fflush(stdout);
        gets(temp);
        if (tolower(*temp) == 'y')
            while ((status=hdr_read(&header)) > 0);
        else
#endif /* 0 */

        fclose(tarfp);
        exit( SS$_NORMAL);
    }
    if (status == 0)                    /* End of tar file  */
    {
        fprintf( stderr, "tar: EOF hit on tarfile.\n");
        fclose(tarfp);
        exit( SS$_DATALOST);
    }
    if (status < 0)                     /* An error  */
    {
        fprintf( stderr, "tar: error reading tarfile (6).\n");
        fclose(tarfp);
        exit( SS$_DATALOST);
    }
}

