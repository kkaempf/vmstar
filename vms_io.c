/* 2007-06-01 SMS.
 *
 *    Miscellaneous VMS-specific stuff.
 */

#include "vmstar.h"
#include "vmstar_cmdline.h"
#include "vms_io.h"

#include <stdio.h>
#include <string.h>

#include <rms.h>
#include <fab.h>
#include <nam.h>
#include <rab.h>

#include <jpidef.h>
#include <starlet.h>
#include <stsdef.h>



/* 2008-10-15 SMS.
 * Revised the char_prop[] table to add these characters (omitted in the
 * VMS documentation) to the caret-escape list:  "  :  \  |
 */

/* Character property table for (re-)escaping ODS5 extended file names.
   Note that this table ignores Unicode, and does not identify invalid
   characters.

   ODS2 valid characters: 0-9 A-Z a-z $ - _

   ODS5 Invalid characters:
      C0 control codes (0x00 to 0x1F inclusive)
      Asterisk (*)
      Question mark (?)

   ODS5 Invalid characters only in VMS V7.2 (which no one runs, right?):
      Double quotation marks (")
      Backslash (\)
      Colon (:)
      Left angle bracket (<)
      Right angle bracket (>)
      Slash (/)
      Vertical bar (|)

   Characters escaped by "^":
      SP  !  "  #  %  &  '  (  )  +  ,  .  :  ;  =
       @  [  \  ]  ^  `  {  |  }  ~

   Either "^_" or "^ " is accepted as a space.  Period (.) is a special
   case.  Note that un-escaped < and > can also confuse a directory
   spec.

   Characters put out as ^xx:
      7F (DEL)
      80-9F (C1 control characters)
      A0 (nonbreaking space)
      FF (Latin small letter y diaeresis)

   Other cases:
      Unicode: "^Uxxxx", where "xxxx" is four hex digits.

    Property table values:
      Normal escape:    1
      Space:            2
      Dot:              4
      Hex-hex escape:   8
      -------------------
      Hex digit:       64
*/

unsigned char char_prop[ 256] = {

/* NUL SOH STX ETX EOT ENQ ACK BEL   BS  HT  LF  VT  FF  CR  SO  SI */
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,

/* DLE DC1 DC2 DC3 DC4 NAK SYN ETB  CAN  EM SUB ESC  FS  GS  RS  US */
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,

/*  SP  !   "   #   $   %   &   '    (   )   *   +   ,   -   .   /  */
    2,  1,  1,  1,  0,  1,  1,  1,   1,  1,  0,  1,  1,  0,  4,  0,

/*  0   1   2   3   4   5   6   7    8   9   :   ;   <   =   >   ?  */
   64, 64, 64, 64, 64, 64, 64, 64,  64, 64,  1,  1,  1,  1,  1,  1,

/*  @   A   B   C   D   E   F   G    H   I   J   K   L   M   N   O  */
    1, 64, 64, 64, 64, 64, 64,  0,   0,  0,  0,  0,  0,  0,  0,  0,

/*  P   Q   R   S   T   U   V   W    X   Y   Z   [   \   ]   ^   _  */
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  1,  1,  1,  1,  0,

/*  `   a   b   c   d   e   f   g    h   i   j   k   l   m   n   o  */
    1, 64, 64, 64, 64, 64, 64,  0,   0,  0,  0,  0,  0,  0,  0,  0,

/*  p   q   r   s   t   u   v   w    x   y   z   {   |   }   ~  DEL */
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  1,  1,  1,  1,  8,

    8,  8,  8,  8,  8,  8,  8,  8,   8,  8,  8,  8,  8,  8,  8,  8,
    8,  8,  8,  8,  8,  8,  8,  8,   8,  8,  8,  8,  8,  8,  8,  8,
    8,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  8
};


/********************/
/* Function fofft() */
/********************/


#define FOFFT_NUM 4             /* Number of chambers. */
#define FOFFT_LEN 24            /* Number of characters/chamber. */

/* Storage cylinder. */
static char fofft_buf[FOFFT_NUM][FOFFT_LEN];
static int fofft_index = 0;

/* Format a zoff_t value in a cylindrical buffer set. */
char *fofft( vt_size_t val, const char *pre, const char *post)
{
    /* Temporary format string storage. */
    char fmt[16];

    /* Assemble the format string. */
    fmt[0] = '%';
    fmt[1] = '\0';             /* Start after initial "%". */
    if (pre == FOFFT_HEX_WID)  /* Special hex width. */
    {
        strcat(fmt, FOFFT_HEX_WID_VALUE);
    }
    else if (pre == FOFFT_HEX_DOT_WID) /* Special hex ".width". */
    {
        strcat(fmt, ".");
        strcat(fmt, FOFFT_HEX_WID_VALUE);
    }
    else if (pre != NULL)       /* Caller's prefix (width). */
    {
        strcat(fmt, pre);
    }

    strcat(fmt, FOFFT_FMT);   /* Long or long-long or whatever. */

    if (post == NULL)
        strcat(fmt, "d");      /* Default radix = decimal. */
    else
        strcat(fmt, post);     /* Caller's radix. */

    /* Advance the cylinder. */
    fofft_index = (fofft_index + 1) % FOFFT_NUM;

    /* Write into the current chamber. */
    sprintf(fofft_buf[fofft_index], fmt, val);

    /* Return a pointer to this chamber. */
    return fofft_buf[fofft_index];
}


/* 2004-09-27 SMS.
   eat_carets().

   Delete ODS5 extended file name escape characters ("^") in the
   original buffer.
   Note that the current scheme does not handle all EFN cases, but it
   could be made more complicated.
*/

void eat_carets( char *str)
/* char *str;      Source pointer. */
{
  char *strd;   /* Destination pointer. */
  char hdgt;
  unsigned char uchr;
  unsigned char prop;

  /* Skip ahead to the first "^", if any. */
  while ((*str != '\0') && (*str != '^'))
     str++;

  /* If no caret was found, quit early. */
  if (*str != '\0')
  {
    /* Shift characters leftward as carets are found. */
    strd = str;
    while (*str != '\0')
    {
      uchr = *str;
      if (uchr == '^')
      {
        /* Found a caret.  Skip it, and check the next character. */
        uchr = *(++str);
        prop = char_prop[ uchr];
        if (prop& 64)
        {
          /* Hex digit.  Get char code from this and next hex digit. */
          if (uchr <= '9')
          {
            hdgt = uchr- '0';           /* '0' - '9' -> 0 - 9. */
          }
          else
          {
            hdgt = ((uchr- 'A')& 7)+ 10;    /* [Aa] - [Ff] -> 10 - 15. */
          }
          hdgt <<= 4;                   /* X16. */
          uchr = *(++str);              /* Next char must be hex digit. */
          if (uchr <= '9')
          {
            uchr = hdgt+ uchr- '0';
          }
          else
          {
            uchr = hdgt+ ((uchr- 'A')& 15)+ 10;
          }
        }
        else if (uchr == '_')
        {
          /* Convert escaped "_" to " ". */
          uchr = ' ';
        }
        else if (uchr == '/')
        {
          /* Convert escaped "/" (invalid Zip) to "?" (invalid VMS). */
          uchr = '?';
        }
        /* Else, not a hex digit.  Must be a simple escaped character
           (or Unicode, which is not yet handled here).
        */
      }
      /* Else, not a caret.  Use as-is. */
      *strd = uchr;

      /* Advance destination and source pointers. */
      strd++;
      str++;
    }
    /* Terminate the destination string. */
    *strd = '\0';
  }
}


/* 2004-11-23 SMS.
 *
 *       get_rms_defaults().
 *
 *    Get user-specified values from (DCL) SET RMS_DEFAULT.  FAB/RAB
 *    items of particular interest are:
 *
 *       fab$w_deq         default extension quantity (blocks) (write).
 *       rab$b_mbc         multi-block count.
 *       rab$b_mbf         multi-buffer count (used with rah and wbh).
 */

#define DIAG_FLAG (verbose > 1)

#define mesg stderr

/* Default RMS parameter values. */

#define RMS_DEQ_DEFAULT 16384   /* About 1/4 the max (65535 blocks). */
#define RMS_MBC_DEFAULT 127     /* The max, */
#define RMS_MBF_DEFAULT 2       /* Enough to enable rah and wbh. */

/* GETJPI item descriptor structure. */
typedef struct
    {
    short buf_len;
    short itm_cod;
    void *buf;
    int *ret_len;
    } jpi_item_t;

/* Durable storage */

int rms_defaults_known = 0;

/* Active RMS item values. */
unsigned short rms_ext_active;
char rms_mbc_active;
unsigned char rms_mbf_active;

/* JPI item buffers. */
static unsigned short rms_ext;
static char rms_mbc;
static unsigned char rms_mbf;

/* GETJPI item lengths. */
static int rms_ext_len;         /* Should come back 2. */
static int rms_mbc_len;         /* Should come back 1. */
static int rms_mbf_len;         /* Should come back 1. */

/* Desperation attempts to define unknown macros.  Probably doomed.
 * If these get used, expect sys$getjpiw() to return %x00000014 =
 * %SYSTEM-F-BADPARAM, bad parameter value.
 * They keep compilers with old header files quiet, though.
 */
#ifndef JPI$_RMS_EXTEND_SIZE
#  define JPI$_RMS_EXTEND_SIZE 542
#endif /* ndef JPI$_RMS_EXTEND_SIZE */

#ifndef JPI$_RMS_DFMBC
#  define JPI$_RMS_DFMBC 535
#endif /* ndef JPI$_RMS_DFMBC */

#ifndef JPI$_RMS_DFMBFSDK
#  define JPI$_RMS_DFMBFSDK 536
#endif /* ndef JPI$_RMS_DFMBFSDK */

/* GETJPI item descriptor set. */

struct
    {
    jpi_item_t rms_ext_itm;
    jpi_item_t rms_mbc_itm;
    jpi_item_t rms_mbf_itm;
    int term;
    } jpi_itm_lst =
     { { 2, JPI$_RMS_EXTEND_SIZE, &rms_ext, &rms_ext_len },
       { 1, JPI$_RMS_DFMBC, &rms_mbc, &rms_mbc_len },
       { 1, JPI$_RMS_DFMBFSDK, &rms_mbf, &rms_mbf_len },
       0
     };

int get_rms_defaults()
{
int sts;

/* Get process RMS_DEFAULT values. */

sts = sys$getjpiw( 0, 0, 0, &jpi_itm_lst, 0, 0, 0);
if ((sts& STS$M_SEVERITY) != STS$M_SUCCESS)
    {
    /* Failed.  Don't try again. */
    rms_defaults_known = -1;
    }
else
    {
    /* Fine, but don't come back. */
    rms_defaults_known = 1;
    }

/* Limit the active values according to the RMS_DEFAULT values. */

if (rms_defaults_known > 0)
    {
    /* Set the default values. */

    rms_ext_active = RMS_DEQ_DEFAULT;
    rms_mbc_active = RMS_MBC_DEFAULT;
    rms_mbf_active = RMS_MBF_DEFAULT;

    /* Default extend quantity.  Use the user value, if set. */
    if (rms_ext > 0)
        {
        rms_ext_active = rms_ext;
        }

    /* Default multi-block count.  Use the user value, if set. */
    if (rms_mbc > 0)
        {
        rms_mbc_active = rms_mbc;
        }

    /* Default multi-buffer count.  Use the user value, if set. */
    if (rms_mbf > 0)
        {
        rms_mbf_active = rms_mbf;
        }
    }

if (DIAG_FLAG)
    {
    fprintf( stderr,
     "Get RMS defaults.  getjpi sts = %%x%08x.\n",
     sts);

    if (rms_defaults_known > 0)
        {
        fprintf( stderr,
         "               Default: deq = %6d, mbc = %3d, mbf = %3d.\n",
         rms_ext, rms_mbc, rms_mbf);
        }
    }
return sts;
}

#ifdef __DECC

/* 2004-11-23 SMS.
 *
 *       acc_cb(), access callback function for DEC C zfopen().
 *
 *    Set some RMS FAB/RAB items, with consideration of user-specified
 * values from (DCL) SET RMS_DEFAULT.  Items of particular interest are:
 *
 *       fab$w_deq         default extension quantity (blocks).
 *       rab$b_mbc         multi-block count.
 *       rab$b_mbf         multi-buffer count (used with rah and wbh).
 *
 *    See also the FOP* macros in OSDEP.H.  Currently, no notice is
 * taken of the caller-ID value, but options could be set differently
 * for read versus write access.  (I assume that specifying fab$w_deq,
 * for example, for a read-only file has no ill effects.)
 */

/* Global storage. */

int fopm_id = FOPM_ID;          /* Callback id storage, modify. */
int fopr_id = FOPR_ID;          /* Callback id storage, read. */
int fopw_id = FOPW_ID;          /* Callback id storage, write. */
int crea_id = CREA_ID;          /* Callback id storage, create. */

/* acc_cb() */

int acc_cb( int *id_arg, struct FAB *fab, struct RAB *rab)
{
int sts;

/* Get process RMS_DEFAULT values, if not already done. */
if (rms_defaults_known == 0)
    {
    get_rms_defaults();
    }

/* If RMS_DEFAULT (and adjusted active) values are available, then set
 * the FAB/RAB parameters.  If RMS_DEFAULT values are not available,
 * suffer with the default parameters.
 */
if (rms_defaults_known > 0)
    {
    /* Set the FAB/RAB parameters accordingly. */
    fab-> fab$w_deq = rms_ext_active;
    rab-> rab$b_mbc = rms_mbc_active;
    rab-> rab$b_mbf = rms_mbf_active;

    /* Truncate at EOF on close, as we'll probably over-extend. */
    fab-> fab$v_tef = 1;

    /* If using multiple buffers, enable read-ahead and write-behind. */
    if (rms_mbf_active > 1)
        {
        rab-> rab$v_rah = 1;
        rab-> rab$v_wbh = 1;
        }

    if (DIAG_FLAG)
        {
        fprintf( mesg,
         "Open callback.  ID = %d, deq = %6d, mbc = %3d, mbf = %3d.\n",
         *id_arg, fab-> fab$w_deq, rab-> rab$b_mbc, rab-> rab$b_mbf);
        }
    }

/* Declare success. */
return 0;
}

#endif /* def __DECC */

/*
 * 2004-09-19 SMS.
 *
 *----------------------------------------------------------------------
 *
 *       decc_init()
 *
 *    On non-VAX systems, uses LIB$INITIALIZE to set a collection of C
 *    RTL features without using the DECC$* logical name method.
 *
 *----------------------------------------------------------------------
 */

#ifdef __DECC

#ifdef __CRTL_VER

#if !defined( __VAX) && (__CRTL_VER >= 70301000)

#include <unixlib.h>

/*--------------------------------------------------------------------*/

/* Global storage. */

/*    Flag to sense if decc_init() was called. */

int decc_init_done = -1;

/*--------------------------------------------------------------------*/

/* decc_init()

      Uses LIB$INITIALIZE to set a collection of C RTL features without
      requiring the user to define the corresponding logical names.
*/

/* Structure to hold a DECC$* feature name and its desired value. */

typedef struct
   {
   char *name;
   int value;
   } decc_feat_t;

/* Array of DECC$* feature names and their desired values. */

decc_feat_t decc_feat_array[] = {

   /* Preserve command-line case with SET PROCESS/PARSE_STYLE=EXTENDED */
 { "DECC$ARGV_PARSE_STYLE", 1 },

   /* Preserve case for file names on ODS5 disks. */
 { "DECC$EFS_CASE_PRESERVE", 1 },

   /* Enable multiple dots (and most characters) in ODS5 file names,
      while preserving VMS-ness of ";version". */
 { "DECC$EFS_CHARSET", 1 },

   /* List terminator. */
 { (char *)NULL, 0 } };

/* LIB$INITIALIZE initialization function. */

static void decc_init( void)
{
int feat_index;
int feat_value;
int feat_value_max;
int feat_value_min;
int i;
int sts;

/* Set the global flag to indicate that LIB$INITIALIZE worked. */

decc_init_done = 1;

/* Loop through all items in the decc_feat_array[]. */

for (i = 0; decc_feat_array[ i].name != NULL; i++)
   {
   /* Get the feature index. */
   feat_index = decc$feature_get_index( decc_feat_array[ i].name);
   if (feat_index >= 0)
      {
      /* Valid item.  Collect its properties. */
      feat_value = decc$feature_get_value( feat_index, 1);
      feat_value_min = decc$feature_get_value( feat_index, 2);
      feat_value_max = decc$feature_get_value( feat_index, 3);

      if ((decc_feat_array[ i].value >= feat_value_min) &&
       (decc_feat_array[ i].value <= feat_value_max))
         {
         /* Valid value.  Set it if necessary. */
         if (feat_value != decc_feat_array[ i].value)
            {
            sts = decc$feature_set_value( feat_index,
             1,
             decc_feat_array[ i].value);
            }
         }
      else
         {
         /* Invalid DECC feature value. */
         fprintf( stderr, " INVALID DECC FEATURE VALUE, %d: %d <= %s <= %d.\n",
          feat_value,
          feat_value_min, decc_feat_array[ i].name, feat_value_max);
         }
      }
   else
      {
      /* Invalid DECC feature name. */
      fprintf( stderr, " UNKNOWN DECC FEATURE: %s.\n",
       decc_feat_array[ i].name);
      }
   }
}

/* Get "decc_init()" into a valid, loaded LIB$INITIALIZE PSECT. */

#pragma nostandard

/* Establish the LIB$INITIALIZE PSECTs, with proper alignment and
   other attributes.  Note that "nopic" is significant only on VAX.
*/
#pragma extern_model save

#pragma extern_model strict_refdef "LIB$INITIALIZ" 2, nopic, nowrt
const int spare[ 8] = { 0 };

#pragma extern_model strict_refdef "LIB$INITIALIZE" 2, nopic, nowrt
void (*const x_decc_init)() = decc_init;

#pragma extern_model restore

/* Fake reference to ensure loading the LIB$INITIALIZE PSECT. */

#pragma extern_model save

int LIB$INITIALIZE( void);

#pragma extern_model strict_refdef
int dmy_lib$initialize = (int) LIB$INITIALIZE;

#pragma extern_model restore

#pragma standard

#endif /* !defined( __VAX) && (__CRTL_VER >= 70301000) */

#endif /* def __CRTL_VER */

#endif /* def __DECC */

