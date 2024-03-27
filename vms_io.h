/*
 * 2009-10-01 SMS.
 *
 * Various I/O-related things.
 */

/* fofft() infrastructure. */

#include <types.h>                      /* Get off_t. */

#ifdef _LARGEFILE
typedef off_t vt_size_t;
# define FOFFT_FMT "ll"
# define FOFFT_HEX_WID_VALUE     "16"   /* width of 64-bit hex values */
# define VT_SIZE_DU "d"
# define VT_SIZE_D "%lld"
# define VT_SIZE_O "%llo"
# define VT_SIZE_11O "%11llo "
# define VT_SIZE_011O "%011llo "
#else /* def _LARGEFILE */
typedef unsigned int vt_size_t;
# define FOFFT_FMT "l"
# define FOFFT_HEX_WID_VALUE     "8"    /* width of 32-bit hex values */
# define VT_SIZE_DU "u"
# define VT_SIZE_D "%u"
# define VT_SIZE_O "%o"
# define VT_SIZE_11O "%11o "
# define VT_SIZE_011O "%011o "
#endif /* def _LARGEFILE [else] */

#define FOFFT_HEX_WID ((char *) -1)
#define FOFFT_HEX_DOT_WID ((char *) -2)

/* Prototypes. */

extern char *fofft( vt_size_t val, const char *pre, const char *post);
extern void eat_carets( char *str);


/* RMS parameter sense-set, file access callback function. */

/* Durable storage */

extern unsigned char char_prop[ 256];   /* Character property table. */

extern int rms_defaults_known;

/* Active RMS item values. */
extern unsigned short rms_ext_active;
extern char rms_mbc_active;
extern unsigned char rms_mbf_active;


#ifdef __DECC

/* File open callback ID values. */

#  define FOPM_ID 1
#  define FOPR_ID 2
#  define FOPW_ID 3
#  define CREA_ID 4

/* File open callback ID storage. */

extern int fopm_id;
extern int fopr_id;
extern int fopw_id;
extern int crea_id;

/* File open callback ID function. */

extern int acc_cb();

/* Option macros.
 * General: Stream access
 * Output: fixed-length, 512-byte records.
 *
 * Callback function (DEC C only) sets deq, mbc, mbf, rah, wbh, ...
 */

#  define FOPM "r+b", "ctx=stm", "rfm=fix", "mrs=512", "acc", acc_cb, &fopm_id
#  define FOPR "rb",  "ctx=stm", "acc", acc_cb, &fopr_id
#  define FOPW "wb",  "ctx=stm", "rfm=fix", "mrs=512", "acc", acc_cb, &fopw_id
#  define CREA        "ctx=stm", "rfm=fix", "mrs=512", "acc", acc_cb, &crea_id

#else /* def __DECC */ /* (So, GNU C, VAX C, ...)*/

#  define FOPM "r+b", "ctx=stm", "rfm=fix", "mrs=512"
#  define FOPR "rb",  "ctx=stm"
#  define FOPW "wb",  "ctx=stm", "rfm=fix", "mrs=512"
#  define CREA        "ctx=stm", "rfm=fix", "mrs=512"

#endif /* def __DECC [else] */

