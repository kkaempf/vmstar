/*
 *	VMSTAR_CMDLINE.H	holds all the declarations of variables
 *				used to hold command line options.
 */
#ifndef LOADED_VMSTAR_CMDLINE_H
#define LOADED_VMSTAR_CMDLINE_H
#include "vmstar.h"

/* Function flags, options */

extern
int help,		/* h option, help */
    extract,            /* x option, extract */
    list,               /* t option, list tape contents */
    verbose,            /* v option, report actions */
    the_wait,           /* w option, prompt */
    dot,                /* d option, suppress dots (creation),
			   or keep dots in directory names (extraction) */
    create,             /* c option, create */
    binmode,            /* b option, binary mode */
    automode,		/* z option, automatic mode */
    foption;		/* f option, specify tarfile */

extern
char tarfile[32767];		/* Tarfile name  */

extern struct dsc$descriptor_s curdevdesc;
extern unsigned long acp_type;	/* Destination disk supports EFS/ODS5 */

#define dp_none		(0)
#define dp_creation	(1)
#define dp_modification	(2)
#define dp_both		(3)

extern
unsigned long date_policy;	/* /DATE_POLICY qualifier */
extern
int force,			/* /FORCE qualifier */
    padding,			/* /PADDING qualifier */
    block_factor,		/* /BLOCK_FACTOR qualifier */
    ods2;			/* /ODS2 qualifier */

extern unsigned long vmstar_cmdline (int *argc_ptr, char ***argv_ptr);
extern int usage();

#endif
