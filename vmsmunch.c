#define module_name	VMSMUNCH
#define module_version	"V1.4"
/*---------------------------------------------------------------------------

  VMSmunch.c                    version 1.3-4                   28 Apr 1992

  This routine is a blatant and unrepentent appropriation of all the nasty
  and difficult-to-do and complicated VMS shenanigans which Joe Meadows has
  so magnificently captured in his FILE utility.  Not only that, it's even
  allowed! (see below).  But let it be clear at the outset that Joe did all
  the work; yea, verily, he is truly a godlike unit.

  The appropriations and modifications herein were performed primarily by
  him known as "Cave Newt," although the Info-ZIP working group probably had
  their fingers in it somewhere along the line.  The idea is to put the raw
  power of Joe's original routine at the disposal of various routines used
  by UnZip (and Zip, possibly), not least among them the utime() function.
  Read on for details...

        30-Dec-2010     Steven Schweda <sms@antinode.info>
                        Added SET_MODE and SET_PROT actions.
                        Changed to prototype function declarations.

	 7-Apr-2010     Steven Schweda <sms@antinode.info>
                        Changed to use NAML instead of NAM where
			available.

	 8-May-1998	Richard Levitte <richard@levitte.org>
			Add code to change the file size (only on fixed
			size record files).

	 1-SEP-1994	Richard Levitte <levitte@e.kth.se>
			On VAX, atr$l_addr is unsigned long.  On AXP, it
			is void *.  I fixed a workaround.

			If one of the fields given to VMSmunch are NULL,
			do not update the corresponding daytime.

	23-JUL-1994	Richard Levitte <levitte@e.kth.se>
			Removed the `revision' item from the attribute list.

	18-JUL-1994	Hunter Goatley <goathunter@WKU.EDU>
			Fixed IO$_ACCESS call.

	18-Jul-1994	Richard Levitte	levitte@e.kth.se
			Changed VMSmunch() to deassign the channel before
			returning when an error has occured.

	02-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved definition of VMStimbuf struct from here
			to vmsmunch.h
  ---------------------------------------------------------------------------

  Usage (i.e., "interface," in geek-speak):

     int VMSmunch( char *filename, int action, void *ptr );

     filename   the name of the file on which to be operated, obviously
     action     an integer which specifies what action to take
     ptr        pointer to any extra item which may be needed (else NULL)

  The possible values for the action argument are as follows:

     GET_TIMES      Get the creation and revision dates of filename; ptr
                    must point to an empty VMStimbuf struct, as defined
		    in vmsmunch.h
                    (with room for at least 24 characters, including term.)

     SET_TIMES      Set the creation and revision dates of filename (utime
                    option); ptr must point to a valid VMStimbuf struct,
                    as defined in vmsmunch.h

     GET_RTYPE      Get the record type of filename; ptr must point to an
                    integer which, on return, is set to the type (as defined
                    in VMSmunch.h:  FAT$C_* defines)

     CHANGE_RTYPE   Change the record type to that specified by the integer
                    to which ptr points; save the old record type (later
                    saves overwrite earlier ones)

     RESTORE_RTYPE  Restore the record type to the previously saved value;
                    or, if none, set it to "fixed-length, 512-byte" record
                    format (ptr not used)

     SET_MODE       Set the file protection according to a UNIX-style
                    mode value (mode_t).
                    System protection = owner protection.
                    Delete protection = write protection, except for a
                    directory, for which delete protection (permission)
                    is always inhibited.

     SET_PROT       Set the file protection according to a VMS-style
                    protection value (short int).

  ---------------------------------------------------------------------------

  Comments from FILE.C, a utility to modify file characteristics:

     Written by Joe Meadows Jr, at the Fred Hutchinson Cancer Research Center
     BITNET: JOE@FHCRCVAX
     PHONE: (206) 467-4970

     There are no restrictions on this code, you may sell it, include it
     with any commercial package, or feed it to a whale.. However, I would
     appreciate it if you kept this comment in the source code so that anyone
     receiving this code knows who to contact in case of problems. Note that
     I do not demand this condition..

  ---------------------------------------------------------------------------*/



#ifdef __DECC
#pragma module module_name module_version
#else
#module module_name module_version
#endif

/*****************************/
/*  Includes, Defines, etc.  */
/*****************************/

#include <stdio.h>
#include <string.h>
#include <types.h>                      /* mode_t */

#include <atrdef.h>
#include <descrip.h>
#include <fibdef.h>
#include <iodef.h>
#include <rms.h>
#include <ssdef.h>
#include <starlet.h>
#include <xabprodef.h>

#include "VMSmunch.h"  /* GET/SET_TIMES, RTYPE, etc. */

#ifndef __MODE_T                        /* VAX C. */
# define mode_t unsigned short
#endif /* ndef __MODE_T */

#ifdef __DECC
#pragma member_alignment __save
#pragma nomember_alignment
#endif
#include "VMSmunch_private.h"	/* fatdef.h, etc. */
#ifdef __DECC
#pragma member_alignment __restore
#endif

#define RTYPE     fat$r_rtype_overlay.fat$r_rtype_bits
#define RATTRIB   fat$r_rattrib_overlay.fat$r_rattrib_bits


/* from <ssdef.h> */
#ifndef SS$_NORMAL
#  define SS$_NORMAL    1
#  define SS$_BADPARAM  20
#endif


/* VMS protection from UNIX permission table. */

static unsigned short prot_of_perm[ 2][ 8] =
{
    /* [0][perm]: Not a directory, delete = write. */
    {
        XAB$M_NOREAD | XAB$M_NOWRITE | XAB$M_NODEL | XAB$M_NOEXE,    /* --- */
        XAB$M_NOREAD | XAB$M_NOWRITE | XAB$M_NODEL,                  /* --x */
        XAB$M_NOREAD |                               XAB$M_NOEXE,    /* -w- */
        XAB$M_NOREAD,                                                /* -wx */
                       XAB$M_NOWRITE | XAB$M_NODEL | XAB$M_NOEXE,    /* r-- */
                       XAB$M_NOWRITE | XAB$M_NODEL,                  /* r-x */
                                                     XAB$M_NOEXE,    /* rw- */
        0                                                            /* rwx */
    },
    /* [1][perm]: Directory, always nodelete. */
    {
        XAB$M_NOREAD | XAB$M_NOWRITE | XAB$M_NODEL | XAB$M_NOEXE,    /* --- */
        XAB$M_NOREAD | XAB$M_NOWRITE | XAB$M_NODEL,                  /* --x */
        XAB$M_NOREAD |                 XAB$M_NODEL | XAB$M_NOEXE,    /* -w- */
        XAB$M_NOREAD |                 XAB$M_NODEL,                  /* -wx */
                       XAB$M_NOWRITE | XAB$M_NODEL | XAB$M_NOEXE,    /* r-- */
                       XAB$M_NOWRITE | XAB$M_NODEL,                  /* r-x */
                                       XAB$M_NODEL | XAB$M_NOEXE,    /* rw- */
                                       XAB$M_NODEL                   /* rwx */
    }
};




/***********************/
/*  Function asctim()  */
/***********************/

/* Convert 64-bit binval to string, put in time. */

void asctim( char *time, int binval[ 2])
{
    static struct dsc$descriptor date_str =
     { 23, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0 };

    date_str.dsc$a_pointer = time;
    sys$asctim( 0, &date_str, binval, 0);
    time[ 23] = '\0';
}




/***********************/
/*  Function bintim()  */
/***********************/

/* Convert time string to 64 bits, put in binval. */

void bintim( char *time, int binval[ 2])
{
    static struct dsc$descriptor date_str =
     { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0 };

    date_str.dsc$w_length = strlen( time);
    date_str.dsc$a_pointer = time;
    sys$bintim( &date_str, binval);
}




/*************************/
/*  Function VMSmunch()  */
/*************************/

int VMSmunch( char *filename, int action, void *ptr)
{
    /* original file.c variables */

    static struct FAB Fab;
    static struct NAMX Nam;
    static struct fibdef Fib; /* short fib */

    static struct dsc$descriptor FibDesc =
      {sizeof(Fib),DSC$K_DTYPE_Z,DSC$K_CLASS_S,(char *)&Fib};
    static struct dsc$descriptor_s DevDesc =
      {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,&Nam.NAMX_T_DVI[1]};
    static struct fatdef Fat;
    static union {
      struct fchdef fch;
      int lword;
    } uchar;
    static int is_dir;                  /* Is-directory flag. */
    static struct fjndef jnl;
    static int Cdate[ 2];
    static int Rdate[ 2];
    static int Edate[ 2];
    static int Bdate[ 2];
    static short int revisions;
    static unsigned long uic;
    static union {
      unsigned short int value;
      struct {
        unsigned system : 4;            /* LS bits. */
        unsigned owner :  4;
        unsigned group :  4;
        unsigned world :  4;            /* MS bits. */
      } bits;
    } prot;
    static short mode;                  /* UNIX-format mode value. */


/* On VAX, define Goofy VAX Type-Cast to obviate /standard = vaxc.
 * Otherwise, lame (ATR) system headers on VAX cause compiler warnings.
 */
#ifdef __VAX
# define GVTC (unsigned int)
#else
# define GVTC
#endif

    static struct atrdef Atr[] = {
      {ATR$S_RECATTR, ATR$C_RECATTR, GVTC &Fat},    /* Record attributes */
      {ATR$S_UCHAR, ATR$C_UCHAR, GVTC &uchar},      /* File characteristics */
      {ATR$S_CREDATE, ATR$C_CREDATE, GVTC &Cdate[0]}, /* Creation date */
      {ATR$S_REVDATE, ATR$C_REVDATE, GVTC &Rdate[0]}, /* Revision date */
      {ATR$S_EXPDATE, ATR$C_EXPDATE, GVTC &Edate[0]}, /* Expiration date */
      {ATR$S_BAKDATE, ATR$C_BAKDATE, GVTC &Bdate[0]}, /* Backup date */
      {ATR$S_FPRO, ATR$C_FPRO, GVTC &prot},         /* File protection  */
      {ATR$S_UIC, ATR$C_UIC, GVTC &uic},            /* File owner */
      {ATR$S_JOURNAL, ATR$C_JOURNAL, GVTC &jnl},    /* Journal flags */
      {0, 0, 0}
    };

    static char EName[NAMX_MAXRSS];
    static char RName[NAMX_MAXRSS];

/* Special ODS5-QIO-compatible name storage. */
#ifdef NAML$C_MAXRSS
    static char QName[ NAML$C_MAXRSS];          /* Probably need less here. */
#endif /* NAML$C_MAXRSS */

    static struct dsc$descriptor_s FileName =
      {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    static struct dsc$descriptor_s string =
      {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    static short int DevChan;
    static short int iosb[4];

    static long int i,status;


    /* new VMSmunch variables */

    static int  old_rtype=FAT$C_FIXED;   /* storage for record type */



/*---------------------------------------------------------------------------
    Initialize attribute blocks, parse filename, resolve any wildcards, and
    get the file info.
  ---------------------------------------------------------------------------*/

    /* initialize RMS structures, we need a NAM to retrieve the FID */
    Fab = cc$rms_fab;
    Fab.FAB_L_NAMX = &Nam;              /* FAB has an associated NAM[L]. */
    Nam = CC_RMS_NAMX;

#ifdef NAML$C_MAXRSS

    Fab.fab$l_dna = (char *) -1;        /* Using NAML for default name. */
    Fab.fab$l_fna = (char *) -1;        /* Using NAML for file name. */

    /* Special ODS5-QIO-compatible name storage. */
    Nam.naml$l_filesys_name = QName;
    Nam.naml$l_filesys_name_alloc = sizeof( QName);

#endif /* NAML$C_MAXRSS */

    FAB_OR_NAML( Fab, Nam).FAB_OR_NAML_FNA = filename;
    FAB_OR_NAML( Fab, Nam).FAB_OR_NAML_FNS = strlen( filename);

    Nam.NAMX_L_ESA = EName;     /* expanded filename */
    Nam.NAMX_B_ESS = sizeof(EName);
    Nam.NAMX_L_RSA = RName;     /* resultant filename */
    Nam.NAMX_B_RSS = sizeof(RName);

    /* do $PARSE and $SEARCH here */
    status = sys$parse(&Fab);
    if (!(status & 1)) return status;

    /* search for the first file.  If none signal error */
    status = sys$search(&Fab);
    if (!(status & 1)) return status;

    while (status & 1) {
        /* initialize Device name length, note that this points into the NAM
           to get the device name filled in by the $PARSE, $SEARCH services */
        DevDesc.dsc$w_length = Nam.NAMX_T_DVI[0];

        status = sys$assign(&DevDesc,&DevChan,0,0);
        if (!(status & 1)) return status;

        /* Prepare the FileName for $QIO(). */

#ifdef NAML$C_MAXRSS

        /* Enable fancy name characters.  Note that "fancy" here does
         * not include Unicode, for which there's no support elsewhere.
         */
        Fib.fib$v_names_8bit = 1;
        Fib.fib$b_name_format_in = FIB$C_ISL1;

        /* ODS5 Extended names used as input to QIO have peculiar
         * encoding (perhaps to minimize storage?), so the special
         * filesys_name result (typically containing fewer carets) must
         * be used here.
         */
        FileName.dsc$a_pointer = Nam.naml$l_filesys_name;
        FileName.dsc$w_length = Nam.naml$l_filesys_name_size;

#else /* def NAML$C_MAXRSS */

        /* Old-fashioned "name.type;ver". */
        FileName.dsc$a_pointer = Nam.NAMX_L_NAME;
        FileName.dsc$w_length =
         Nam.NAMX_B_NAME+ Nam.NAMX_B_TYPE+ Nam.NAMX_B_VER;

#endif /* def NAML$C_MAXRSS [else] */

        /* Initialize the FIB */
        for (i=0;i<3;i++) {
            Fib.FIB$W_FID[i]=Nam.NAMX_W_FID[i];
            Fib.FIB$W_DID[i]=Nam.NAMX_W_DID[i];
        }

        /* Use the IO$_ACCESS function to return info about the file */
        /* Note, used this way, the file is not opened, and the expiration */
        /* and revision dates are not modified */
        status = sys$qiow(0,DevChan,IO$_ACCESS,&iosb,0,0,
                          &FibDesc,&FileName,0,0,&Atr,0);
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
#ifdef DEBUG
	    fprintf( stderr,
             "vmsmunch() returns after IO$_ACCESS with status %%x%08x\n",
             status);
#endif
	    return status;
	  }
        status = iosb[0];
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
#ifdef DEBUG
	    fprintf( stderr,
             "vmsmunch() returns after IO$_ACCESS with status %%x%08x\n",
             status);
#endif
	    return status;
	  }

    /*-----------------------------------------------------------------------
        We have the current information from the file:  now see what user
        wants done with it.
      -----------------------------------------------------------------------*/

        switch (action) {

          case GET_TIMES:
              asctim(((struct VMStimbuf *)ptr)->modtime, Cdate);
              asctim(((struct VMStimbuf *)ptr)->actime, Rdate);
              break;

          case SET_TIMES:
              if (((struct VMStimbuf *)ptr)->modtime != 0)
		  bintim(((struct VMStimbuf *)ptr)->modtime, Cdate);
              if (((struct VMStimbuf *)ptr)->actime != 0)
		  bintim(((struct VMStimbuf *)ptr)->actime, Rdate);
#ifdef DEBUG
	      fprintf( stderr, "setting Cdate to %s\n",
               ((struct VMStimbuf *)ptr)->modtime);
	      fprintf( stderr, "setting Rdate to %s\n",
               ((struct VMStimbuf *)ptr)->actime);
#endif
              break;

          case GET_RTYPE:   /* non-modifying */
              *(int *)ptr = Fat.RTYPE.fat$v_rtype;
              return RMS$_NORMAL;     /* return to user */
              break;

          case CHANGE_RTYPE:
              old_rtype = Fat.RTYPE.fat$v_rtype;         /* save current one */
              if ((*(int *)ptr < FAT$C_UNDEFINED) ||
                  (*(int *)ptr > FAT$C_STREAMCR))
                  Fat.RTYPE.fat$v_rtype = FAT$C_STREAMLF;  /* Unix I/O happy */
              else
                  Fat.RTYPE.fat$v_rtype = *(int *)ptr;
              break;

          case RESTORE_RTYPE:
              Fat.RTYPE.fat$v_rtype = old_rtype;
              break;

	  case SET_EXACT_SIZE:
	      if (Fat.RTYPE.fat$v_rtype != FAT$C_FIXED)
		  break;
	      Fat.fat$r_efblk_overlay.fat$r_efblk_fields.fat$w_efblkh =
		((*(int *)ptr+511) / 512 >> 16) & 0xFFFF;
	      Fat.fat$r_efblk_overlay.fat$r_efblk_fields.fat$w_efblkl =
		((*(int *)ptr+511) / 512) & 0xFFFF;
	      Fat.fat$w_ffbyte = *(int *)ptr % 512;
	      break;

          case SET_MODE:
              /* Set file protection from a UNIX-style mode value.
               * System protection = owner protection.
               * Delete protection = write protection, except for a
               * directory, for which delete protection (permission) is
               * always inhibited.
               */
              is_dir = ((uchar.lword& FCH$M_DIRECTORY) != 0);
              mode = *(mode_t *)ptr;
              prot.bits.world = prot_of_perm[ is_dir][(mode>> 0)& 7];
              prot.bits.group = prot_of_perm[ is_dir][(mode>> 3)& 7];
              prot.bits.owner = prot_of_perm[ is_dir][(mode>> 6)& 7];
              prot.bits.system = prot.bits.owner;
              break;

          case SET_PROT:
              /* Set file protection from a VMS protection value. */
              prot.value = *(unsigned short int *)ptr;
              break;

          default:
              return SS$_BADPARAM;   /* anything better? */
        }

    /*-----------------------------------------------------------------------
        Go back and write modified data to the file header.
      -----------------------------------------------------------------------*/

        /* note, part of the FIB was cleared by earlier QIOW, so reset it */
        Fib.FIB$L_ACCTL = FIB$M_NORECORD;
        for (i=0;i<3;i++) {
            Fib.FIB$W_FID[i]=Nam.NAMX_W_FID[i];
            Fib.FIB$W_DID[i]=Nam.NAMX_W_DID[i];
        }

        /* Use the IO$_MODIFY function to change info about the file */
        /* Note, used this way, the file is not opened, however this would */
        /* normally cause the expiration and revision dates to be modified. */
        /* Using FIB$M_NORECORD prohibits this from happening. */
        status = sys$qiow(0,DevChan,IO$_MODIFY,&iosb,0,0,
                          &FibDesc,&FileName,0,0,&Atr,0);
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
#ifdef DEBUG
	    fprintf( stderr,
             "vmsmunch() returns after IO$_MODIFY with status %%x%08x\n",
             status);
#endif
	    return status;
	  }

        status = iosb[0];
        if (!(status & 1))
	  {
	    sys$dassgn(DevChan);
#ifdef DEBUG
	    fprintf( stderr,
             "vmsmunch() returns after IO$_MODIFY with status %%x%08x\n",
             status);
#endif
	    return status;
	  }

        status = sys$dassgn(DevChan);
        if (!(status & 1))
	  {
#ifdef DEBUG
	    fprintf( stderr,
             "vmsmunch() returns after sys$dassgn() with status %%x%08x\n",
             status);
#endif
	    return status;
	  }

#if 0
        /* 2010-11-31 SMS.
         * We don't want a $SEARCH() loop, or a "%RMS-E-NMF, no more
         * files found" return status value.
         */

        /* look for next file, if none, no big deal.. */
        status = sys$search(&Fab);
#endif /* 0 */

        return status;
    }
} /* end function VMSmunch() */

