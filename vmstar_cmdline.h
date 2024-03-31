/*
 *   VMSTAR_CMDLINE.H
 *
 *   Declarations of variables used for command-line options.
 */
#ifndef LOADED_VMSTAR_CMDLINE_H
#define LOADED_VMSTAR_CMDLINE_H

#include "vmstar.h"

/* Default option values. */

#define BLOCK_FACTOR_DEFAULT 20

/* /DATE_POLICY ("D") constants. */

#define DP_NONE         (0)
#define DP_CREATION     (1)
#define DP_MODIFICATION (2)
#define DP_BOTH         (3)

/* Function flags, options */

extern int automode;    /* z option, automatic mode */
extern int binmode;     /* b option, binary mode */
extern int create;      /* c option, create */
extern int debugg;      /* g option, debug. */
extern int dot;         /* d option, suppress dots (creation),
                         * or keep dots in directory names (extraction) */
extern int extract;     /* x option, extract */
extern int foption;     /* f option, specify tarfile */
extern int help;        /* h option, help */
extern int list;        /* t option, list tape contents */
extern int underdot;    /* u option, a.b.c -> ODS2 A_B.C (not A.B_C). */
extern int verbose;     /* v option, report actions */
extern int the_wait;    /* w option, prompt */

extern char tarfile[ T_NAM_LEN+ 1];     /* Tarfile name  */

extern struct dsc$descriptor_s curdevdesc;
extern unsigned long acp_type;   /* Destination disk supports EFS/ODS5 */

extern unsigned int date_policy;        /* /DATE_POLICY ("D") value. */
extern int force;                       /* /FORCE ("F") value. */
extern int padding;                     /* /PADDING ("P") value. */
extern int block_size;                  /* /BLOCK_FACTOR ("b")-derived val. */
extern int ods2;                        /* /ODS2 ("o") value. */

#ifdef SYMLINKS
extern int symlinks;                    /* /SYMLINKS ("s") value. */
#endif /* def SYMLINKS */

extern unsigned int vmstar_cmdline( int *argc_ptr, char ***argv_ptr);
extern void usage( int exit_sts);

#endif /* ndef LOADED_VMSTAR_CMDLINE_H */
