#define module_name VMSTAR_CMDLINE
#define module_ident "02-010"
/*
**
**  Facility:	VMSTAR
**
**  Module:	VMSTAR_CMDLINE
**
**  Author:	Hunter Goatley <goathunter@WKUVX1.WKU.EDU>
**
**  Date:	April 27, 1994
**
**  Abstract:	Routines to handle a VMS CLI interface for VMSTAR.  The CLI
**		command line is parsed and a new argc/argv are built and
**		returned to VMSTAR.
**
**  Modified by:
**
**      02-010          Steven Schweda          23-DEC-2020 21:00
**              Changed to suppress the "Press return for more..."
**              prompt in help when stderr is not a terminal.
**
**      02-009          Steven Schweda          11-NOV-2010 22:13
**              Added /SYMLINKS, /UNDERDOT.
**              Changed to distinguish between "B" and "b".
**              Added UNIX options D (/DATE_POLICY), F (/FORCE), and p
**              (/PADDING).
**              Changed to allow a lone "v" (/VERBOSE) to show the
**              program version.
**              Restructured query code for /CONFIRM ("w"), adding a
**              default response of "N".
**              Restructured and simplified get_list() code (fixing the
**              malloc() of one byte too few).  Conditionally
**              (CASE_CNVT) disabled the down-casing code.
**              Condensed the usage() help text.
**      01-008          Patrick Young           02-OCT-2001 11:37
**              Now handles /ODS2
**	02-007		Martin Stiftinger	14-FEB-1995 17:14
**		A comment was not correctly ended.
**	02-006		Richard Levitte		 5-NOV-1994 01:02 CET
**		Corrected bogus settings of found_options.
**	02-005		Richard Levitte		18-OCT-1994 10:42 CET
**		Now handles /FORCE, /PADDING, /BLOCK_FACTOR.
**	02-004		Richard Levitte		12-SEP-1994 10:09 CET
**		Initialised len in get_list() to 0 if *str is NULL.
**	02-003		Richard Levitte		 1-SEP-1994 14:42 CET
**		Initialised found_options to 0.
**	02-002		Richard Levitte		 1-SEP-1994 23:23 CET
**		/DATE_POLICY values are only handled if /DATE_POLICY
**		is present.
**	02-001		Richard Levitte		 1-SEP-1994 22:58 CET
**		Added processing of /DATE_POLICY.
**	02-000		Richard Levitte		 1-SEP-1994 11:43 CET
**		Moved the Unix command line option parsing to this file.
**	01-003		Richard Levitte		 1-JUL-1994 00:23 CET
**		Added processing of the /HELP qualifier.
**	01-002		Richard Levitte		28-APR-1994 22:50 CET
**		Made a change, so users are not required to have
**		a dash to get the Un*x interface.
**	01-001		Richard Levitte		28-APR-1994 22:25 CET
**		Simplified the handling of /DOTS.
**	01-000		Hunter Goatley		27-APR-1994 10:05
**		Original version (for VMSTAR V2.0).
**
*/


#ifdef __DECC
#pragma module module_name module_ident
#else
#module module_name module_ident
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <clidef.h>
#include <climsgdef.h>
#include <ssdef.h>
#include <ttdef.h>
#include <lib$routines.h>
#include <ots$routines.h>
#include <str$routines.h>

/* <cli$routines.h> is too new to be required, so declare the required
 * CLI functions here.
 */
#define  cli$dcl_parse  CLI$DCL_PARSE
unsigned int cli$dcl_parse();
#define  cli$get_value  CLI$GET_VALUE
unsigned int cli$get_value();
#define  cli$present  CLI$PRESENT
unsigned int cli$present();

#include "vmstar_cmdline.h"

#if 0
/* 2010-11-12 SMS.
 * No longer used.  See below.
 */
#ifndef CLI$_COMMA
globalvalue CLI$_COMMA;
#endif
#endif /* 0 */

#define VT_MAX( a, b) (((a) < (b)) ? (b) : (a))

/* Function flags, options */

#if !defined( __VAX) && defined( _LARGEFILE)
# define OPT_STR " (+large-file)"
#else
# define OPT_STR " (-large-file)"
#endif
			
int automode = 0;       /* z option, automatic mode */
int binmode = 0;        /* b option, binary mode */
int create = 0;         /* c option, create */
int debugg = 0;         /* g option, debug. */
int dot = 0;            /* d option, suppress dots (creation),
                           or keep dots in directory names (extraction) */
int extract = 0;        /* x option, extract */
int foption = 0;        /* f option, specify tarfile */
int help = 0;           /* h option, help */
int list = 0;           /* t option, list tape contents */
int underdot = 0;       /* u option, a.b.c -> ODS2 A_B.C (not A.B_C). */
int verbose = 0;        /* v option, report actions */
int the_wait = 0;       /* w option, prompt */

char tarfile[ T_NAM_LEN+ 1];
struct dsc$descriptor_s curdevdesc = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0 };
unsigned int acp_type; /* Destination disk supports EFS/ODS5 */

unsigned int date_policy = DP_BOTH;     /* /DATE_POLICY ("D") value. */
int force = 0;                          /* /FORCE ("F") value. */
int padding = 0;                        /* /PADDING ("P") value. */
int block_factor = -1;                  /* /BLOCK_FACTOR ("b") value. */
int block_size;                         /* Derived block size. */
int ods2 = 0;                           /* /ODS2 qualifier */

int option_arg = 0;
int block_factor_arg = -1;
int date_policy_arg = -1;
int tarfile_arg = -1;

#ifdef SYMLINKS
int symlinks = 0;		/* /SYMLINKS (s option), real symlinks */
#endif /* def SYMLINKS */

/*
**  "Macro" to initialize a dynamic string descriptor.
*/
#define init_dyndesc(dsc) {\
	dsc.dsc$w_length = 0;\
	dsc.dsc$b_dtype = DSC$K_DTYPE_T;\
	dsc.dsc$b_class = DSC$K_CLASS_D;\
	dsc.dsc$a_pointer = 0;}

/*
**  Define descriptors for all of the CLI parameters and qualifiers.
*/
$DESCRIPTOR(cli_automatic,	"AUTOMATIC");
$DESCRIPTOR(cli_binary,		"BINARY");
$DESCRIPTOR(cli_block_factor,	"BLOCK_FACTOR");
$DESCRIPTOR(cli_confirm,	"CONFIRM");
$DESCRIPTOR(cli_create,		"CREATE");
$DESCRIPTOR(cli_date_policy,	"DATE_POLICY");
$DESCRIPTOR(cli_date_policy_all,		"DATE_POLICY.ALL");
$DESCRIPTOR(cli_date_policy_none,		"DATE_POLICY.NONE");
$DESCRIPTOR(cli_date_policy_creation,		"DATE_POLICY.CREATION");
$DESCRIPTOR(cli_date_policy_modification,	"DATE_POLICY.MODIFICATION");
$DESCRIPTOR(cli_dots,		"DOTS");
$DESCRIPTOR(cli_extract,	"EXTRACT");
$DESCRIPTOR(cli_force,		"FORCE");
$DESCRIPTOR(cli_help,		"HELP");
$DESCRIPTOR(cli_infile,		"INFILE");
$DESCRIPTOR(cli_list,		"LIST");
$DESCRIPTOR(cli_ods2,		"ODS2");
$DESCRIPTOR(cli_padding,	"PADDING");

#ifdef SYMLINKS
$DESCRIPTOR(cli_symlinks,	"SYMLINKS");
#endif /* def SYMLINKS */

$DESCRIPTOR(cli_tarfile,	"TARFILE");
$DESCRIPTOR(cli_underdot,	"UNDERDOT");
$DESCRIPTOR(cli_verbose,	"VERBOSE");

$DESCRIPTOR(cli_yyz,		"YYZ");

$DESCRIPTOR(vmstar_command, "vmstar ");

#ifdef __DECC
extern void *vmstar_clitables;
#else
globalref void *vmstar_clitables;
#endif

/* extern unsigned int LIB$GET_INPUT(void), LIB$SIG_TO_RET(void); */

unsigned int unix_vmstar_cmdline (int *, char ***);
static unsigned int get_list (struct dsc$descriptor_s *, char **, char);
static unsigned int check_cli (struct dsc$descriptor_s *);


static
one_line(char *text, int *this_line, int max_lines)
{
    static int eof = 0;
    static int tty = -16;       /* Pause output only for a terminal. */

    if (tty == -16)
    {
        tty = isatty( fileno( stderr));
    }

    if (eof == 0)
    {
        if ((tty > 0) && ((*this_line)++ == max_lines-1))
        {
            fprintf( stderr, "Press return for more...");
            if (getchar() == EOF)
            {
                eof = 1;
                return 0;
            }
            *this_line = 1;
        }
#ifdef DEBUG
        fprintf( stderr, "(###DEBUG %d)", *this_line);
#endif
        fprintf( stderr, "%s\n", text);
    }
    return 0;
}

/* Usage of this program. */
void usage( int exit_sts)
{
    static char buf[255];
    $DESCRIPTOR(dev_descr,"SYS$OUTPUT");
    unsigned int status;
    int this_line = 0;
#if 1 || (!defined(__VAXC) && !defined(VAXC))
    union ttdef ttydef;
#else
    variant_union ttdef ttydef;
#endif
#define max_lines ttydef.tt$v_page

    if (((status = lib$getdvi(&DVI$_DEVDEPEND,0,&dev_descr,&ttydef,0,0))
	 & 1) == 0)
	max_lines = 24;

#ifdef DEBUG
    sprintf(buf, "###DEBUG: max_lines = %d", max_lines);
    one_line(buf, &this_line, max_lines);
#endif
    sprintf( buf, "VMSTAR %s (%s)%s", VERSION, __DATE__, OPT_STR);
    one_line( buf, &this_line, max_lines);
#ifdef SYMLINKS
    one_line( "usage (UNIX-style): vmstar -[h|c|t|x][BbDdFfopsvwz] [params ...] [file [...]]", &this_line, max_lines);
#else /* def SYMLINKS */
    one_line( "usage (UNIX-style): vmstar -[h|c|t|x][BbDdFfopvwz] [params ...] [file [...]]", &this_line, max_lines);
#endif /* def SYMLINKS [else] */
    one_line( "usage (VMS-style):  VMSTAR [options] tarfile [file [, file [...]]]", &this_line, max_lines);
    one_line( " Options (UNIX-style, VMS-style):", &this_line, max_lines);
    one_line( " h        /HELP           Print this text and exit.  (Other options ignored.)", &this_line, max_lines);
    one_line( " c        /CREATE         Create a tarfile.", &this_line, max_lines);
    one_line( " t        /LIST           List the contents of a tarfile.", &this_line, max_lines);
    one_line( " x        /EXTRACT        Extract files from tarfile.", &this_line, max_lines);
    one_line( "", &this_line, max_lines);
    one_line( " B        /BINARY         Extract files as binary (fixed-512).", &this_line, max_lines);
    one_line( " b b_f    /BLOCK_FACTOR=b_f  Number of 512-byte records in a tar block.", &this_line, max_lines);
    one_line( " D        /DATE_POLICY =  Specify which times of extracted files to set.", &this_line, max_lines);
    one_line( "  c|C|m|M  ([NO]CREATION, [NO]MODIFICATION)  (Default: CREAT, MODIF.)", &this_line, max_lines);
    one_line( "  A|n      ALL, NONE      ALL: (CREAT, MODIF), NONE: (NOCREAT, NOMODIF).", &this_line, max_lines);
    one_line( "  (UNIX-style: \"c\" = NOCREAT, \"C\" = CREAT, \"m\" = NOMODIF, \"M\" = MODIF,", &this_line, max_lines);
    one_line( "               \"A\" = ALL, \"n\" = NONE.)", &this_line, max_lines);
    one_line( " d        /DOTS           Archived names retain a trailing dot.", &this_line, max_lines);
    one_line( "                          (Default: Trim trailing dots: \"fred.\" -> \"fred\".)", &this_line, max_lines);
    one_line( "                          Extract literal dots (\"^.\") in directory name (ODS5).", &this_line, max_lines);
    one_line( "                          (Default: Convert dots in directory name to \"_\".)", &this_line, max_lines);
    one_line( " F        /FORCE          Forces archiving of unsupported file formats.", &this_line, max_lines);
    one_line( " f t_f    (first arg)     Specify a tar file (disk or tape).  Default: $TAPE", &this_line, max_lines);
    one_line( " o        /ODS2           Archive down-cased names, even on ODS-5 volumes.", &this_line, max_lines);
    one_line( "                          Extract using ODS-2 naming, even on ODS-5 volumes.", &this_line, max_lines);
    one_line( " p        /NOPADDING      Inhibits padding the last block of the tarfile.", &this_line, max_lines);
#ifdef SYMLINKS
    one_line( " s        /SYMLINKS       Extract archived symlinks as real symlinks.", &this_line, max_lines);
#endif /* def SYMLINKS */
    one_line( " u        /UNDERDOT       \"a.b.c\" -> ODS2 \"A_B.C\".  (Default: \"A.B_C\").", &this_line, max_lines);
    one_line( " v        /VERBOSE        Display processed file info.  (Alone: Show version.)", &this_line, max_lines);
    one_line( " w        /CONFIRM        Prompt for confirmation before archive/extract.", &this_line, max_lines);
    one_line( " z        /AUTOMATIC      Automatically determine file type.", &this_line, max_lines);

    exit( exit_sts);
}


static void arg_check( void)
{
    if (extract + list + create + help == 0)
    {
        if (verbose > 0)
        {
            fprintf( stderr, "VMSTAR %s (%s)%s", VERSION, __DATE__, OPT_STR);
            exit( SS$_NORMAL);
        }
        else
        {
            fprintf( stderr, "tar: no action specified.\n");
            exit( EINVAL);
        }
    }
}


unsigned int
vmstar_cmdline (int *argc_p, char ***argv_p)
{
/*
**  Routine:	vmstar_cmdline
**
**  Function:
**
**	Parse the DCL command line and set option values accordingly.
**
**	NOTE:  Upon return, the argument list will only contain the final
**	file list.
**
*/
    register status;
    int found_options = 0;
    char *the_cmd_line = NULL, *ptr;
    int  x;

    int new_argc;
    char **new_argv;

    struct dsc$descriptor_d work_str;

    init_dyndesc (work_str);

    /*
    **  See if the program was invoked by the CLI (SET COMMAND) or by
    **  a foreign command definition.  Check for /YYZ, which is a
    **  valid default qualifier solely for this test.
    */
    status = check_cli (&cli_yyz);
    if (!(status & 1)) {
	lib$get_foreign (&work_str);
	/*
	**  If nothing was returned or the first character is a "-", then
	**  assume it's a UNIX-style command and return.
	*/
	if ((work_str.dsc$w_length == 0) || (*(work_str.dsc$a_pointer) == '-'))
	    return unix_vmstar_cmdline(argc_p, argv_p);

	str$concat (&work_str, &vmstar_command, &work_str);
	status = cli$dcl_parse(&work_str, &vmstar_clitables, lib$get_input,
			lib$get_input, 0);
	if (!(status & 1)) return status;
    }

    /*
    **  First, check to see if any of the regular options were specified.
    */

    status = cli$present (&cli_automatic);
    if (status & 1)
	found_options = 1, automode = 1;

    status = cli$present (&cli_binary);
    if (status & 1)
	found_options = 1, binmode = 1;

    status = cli$present (&cli_block_factor);
    if (status & 1)
    {
	int status;
	struct dsc$descriptor_d work_str;

	init_dyndesc(work_str);

	found_options = 1;
	if ((status = cli$get_value (&cli_block_factor, &work_str)) & 1)
	{
	    status = ots$cvt_tu_l (&work_str, &block_factor,
				   sizeof (block_factor), 0x11);
	}
    }
    else
    {
        /* Set default block_factor, if none was specified. */
        block_factor = BLOCK_FACTOR_DEFAULT;
    }
    block_size = RECSIZE* block_factor;

    status = cli$present (&cli_confirm);
    if (status & 1)
	found_options = 1, the_wait = 1;

    status = cli$present (&cli_create);
    if (status & 1)
	found_options = 1, create = 1;

    status = cli$present (&cli_date_policy);
    if (status & 1) {
#ifdef DEBUG
	fprintf( stderr,
         "/DATE_POLICY is apparently present, according to status %%x%08x\n",
         status);
#endif

	found_options = 1;
	date_policy = DP_NONE;

	status = cli$present (&cli_date_policy_all);
	if (status & 1)
	    date_policy = DP_BOTH;

	status = cli$present (&cli_date_policy_none);
	if (status & 1)
	    date_policy = DP_NONE;

	status = cli$present (&cli_date_policy_creation);
	if (status & 1)
	    date_policy |= DP_CREATION;

	status = cli$present (&cli_date_policy_modification);
	if (status & 1)
	    date_policy |= DP_MODIFICATION;
    }

    status = cli$present (&cli_dots);
    if (status & 1)
	found_options = 1, dot = 1;

    status = cli$present (&cli_extract);
    if (status & 1)
	found_options = 1, extract = 1;

    status = cli$present (&cli_force);
    if (status & 1)
	found_options = 1, force = 1;
    else
	found_options |= status != CLI$_ABSENT;

    status = cli$present (&cli_list);
    if (status & 1)
	found_options = 1, list = 1;

    status = cli$present (&cli_ods2);
    if (status & 1)
	found_options = 1, ods2 = 1;
    else
	found_options |= status != CLI$_ABSENT;

    /* This one has special handling, since it is on by default.
       To set found_options, we really want it to be present on
       the command line.  */
    status = cli$present (&cli_padding);

    if (status & 1)
	found_options |= status != CLI$_DEFAULTED, padding = 1;
    else
	found_options = 1;	/* it must be present to be negated */

#ifdef SYMLINKS
    status = cli$present (&cli_symlinks);
    if (status & 1)
	found_options = 1, symlinks = 1;
    else
	found_options |= status != CLI$_ABSENT;
#endif /* def SYMLINKS */

    status = cli$present (&cli_underdot);
    if (status & 1)
	found_options = 1, underdot = 1;

    status = cli$present (&cli_verbose);
    if (status & 1)
    {
	found_options = 1;
        status = cli$get_value (&cli_verbose, &work_str);
        if (status & 1)
        {
            work_str.dsc$a_pointer[ work_str.dsc$w_length] = '\0';
            verbose = atoi( work_str.dsc$a_pointer);
            if (verbose <= 0)
                verbose = 0;
       }
       else
       {
           /* No value.  Default to 1. */
           verbose = 1;
       }
    }

    status = cli$present (&cli_help);
    if (status & 1)
    {
        if (found_options)
	    fprintf( stderr, "tar: ignoring all other options\n");
	usage( SS$_NORMAL);
    }

    /* If the user didn't give any DCL qualifier, assume he uses the
       Un*x interface */
    if (!found_options)
	return unix_vmstar_cmdline (argc_p, argv_p);

#ifdef DEBUG
    fprintf( stderr, "VMS date_policy: %d, creation: %s, modification: %s\n",
     date_policy,
     date_policy & DP_CREATION ? "yes" : "no",
     date_policy & DP_MODIFICATION ? "yes" : "no");
#endif

    /* Ensure that at least one of create, extract, help, list (or
     * verbose=version) was specified.
     */
    arg_check();

    /*
    **
    **  OK.  We've done all the regular options, so check for the
    **	tarfile and the files to store or extract.
    **
    */
    status = cli$present (&cli_tarfile);
    if (status & 1) {
	unsigned int len;
	status = cli$get_value (&cli_tarfile, &work_str);
	len = work_str.dsc$w_length;
	strncpy(tarfile, work_str.dsc$a_pointer, len);
	tarfile[len] = '\0';
    }
    else
    {
        strcpy( tarfile, "$TAPE");
    }

    status = cli$present (&cli_infile);
    if (status & 1) {
	status = get_list (&cli_infile, &the_cmd_line, ' ');
	if (!(status & 1)) return status;
    }

    /*
    **  Now that we've gotten everything off the command line, count the
    **  number of args and build an argv array.
    */

#if defined(TEST) || defined(DEBUG)
    fprintf( stderr, "%s\n", the_cmd_line);
#endif /* TEST */

    new_argc = the_cmd_line != NULL && *the_cmd_line != '\0';
    for (ptr = the_cmd_line;
	 ptr != 0 && (ptr = strchr(ptr,' ')) != NULL;
	 ptr++, new_argc++);

    /* There must be one extra place for the final NULL.  */
    if ((new_argv = (char **) calloc (new_argc + 1, sizeof(char *))) == NULL) {
        fprintf( stderr, "Memory exhausted\n");
        fprintf( stderr, " %s\n", strerror( errno));
	return SS$_ABORT;
    }

    for (ptr = the_cmd_line, x = 0; x < new_argc; x++) {
	new_argv[x] = ptr;
	if ((ptr = strchr (ptr, ' ')) != NULL)
	    *ptr++ = '\0';
    }
    new_argv[x] = NULL;

#if defined(TEST) || defined(DEBUG)
    fprintf( stderr, "new_argc    = %d\n", new_argc);
    for (x = 0; x < new_argc; x++)
	fprintf( stderr, "new_argv[%d] = %s\n", x, new_argv[x]);
#endif /* TEST */

    *argc_p = new_argc;
    *argv_p = new_argv;

    return SS$_NORMAL;
}


#if 0
static unsigned int
set_boolean (struct dsc$descriptor_s *qual, struct bool_desc *desc)
{
    register status;
    struct dsc$descriptor_d work_str;

    init_dyndesc(work_str);

    status = cli$present (qual);
    if (status & 1) {

	unsigned int len, old_len;

	len = strlen(*str);
	while ((status = cli$get_value (qual, &work_str)) & 1) {
	    if (*str == NULL) {
		len = work_str.dsc$w_length + 1;
		}
		strncpy(*str,work_str.dsc$a_pointer,len);
	    } else {
		char *src, *dst; int x;
		old_len = len;
		len += work_str.dsc$w_length + 1;
		if ((*str =
			(char *) realloc (*str, len)) == NULL) {
                    fprintf( stderr, "Memory exhausted\n");
                    fprintf( stderr, " %s\n", strerror( errno));
		    return SS$_ABORT;
		}
		/*
		**  Copy the string to the buffer, converting to lowercase.
		*/
		src = work_str.dsc$a_pointer;
		dst = *str+old_len;
		for (x = 0; x < work_str.dsc$w_length; x++) {
		    if ((*src >= 'A') && (*src <= 'Z'))
			*dst++ = *src++ + 32;
		    else
			*dst++ = *src++;
		}
/*
		strncpy(*str+old_len,work_str.dsc$a_pointer,
			work_str.dsc$w_length);
*/
	    }
	    if (status == CLI$_COMMA)
		(*str)[len-1] = c;
	    else
		(*str)[len-1] = '\0';

	}
    }

    return SS$_NORMAL;

}
#endif /* 0 */


static unsigned int
get_list (struct dsc$descriptor_s *qual, char **str, char c)
{
    unsigned int status;
    struct dsc$descriptor_d work_str;

    init_dyndesc( work_str);

    status = cli$present( qual);
    if (status & 1) {

	unsigned int len;
	unsigned int len_old;

	len = *str == NULL ? 0 : strlen( *str);
	while ((status = cli$get_value( qual, &work_str)) & 1) {
            len_old = len;
            len += work_str.dsc$w_length+ 1;

            if ((*str = realloc( *str, len)) == NULL) {
                fprintf( stderr, "Memory exhausted\n");
                fprintf( stderr, " %s\n", strerror( errno));
                return SS$_ABORT;
            }

#ifdef CASE_CNVT
            /* Copy the string to the buffer, converting to lowercase. */
            {
                char *dst;
                char *src;
                int x;

                src = work_str.dsc$a_pointer;
                dst = *str+ len_old;
                for (x = 0; x < work_str.dsc$w_length; x++) {
                    *dst++ = tolower( *src++);
                }
            }
#else /* def CASE_CNVT */
            /* Copy the string to the buffer. */
            strncpy( (*str+ len_old), work_str.dsc$a_pointer,
             work_str.dsc$w_length);
#endif /* def CASE_CNVT [else] */

            /* If not the last item, then set the extra byte to c (" ").
             * Actually, the test probably wastes more time than
             * double-setting the last one, so don't bother testing.
             * Was: if (status == CLI$_COMMA)
             */
            (*str)[ len- 1] = c;
	}

        /* NUL-terminate the arg list string. */
        (*str)[ len- 1] = '\0';
    }
    return SS$_NORMAL;
}


static unsigned int
check_cli (struct dsc$descriptor_s *qual)
{
/*
**  Routine:	check_cli
**
**  Function:	Check to see if a CLD was used to invoke the program.
**
*/
    lib$establish(lib$sig_to_ret);	/* Establish condition handler */
    return cli$present( qual);		/* Just see if something was given */
}


unsigned int
unix_vmstar_cmdline(int *argc_p, char ***argv_p)
{
    char *cp, c;
    int argindex = 2;
    int argc = *argc_p;
    char **argv = *argv_p;
    int arg_opt;
    int hyphen_hyphen = 0;
    int opt_value_expected = 0;

    if (argc == 1)
        usage( SS$_NORMAL);

    strcpy( tarfile, "$TAPE");

    for (arg_opt = 1; arg_opt < argc; arg_opt++)
    {
        cp = argv[ arg_opt];

        /* If not a "-" option and not expecting an option value, then
         * break out of the "-" option loop.
         */
        if ((*cp != '-') && (opt_value_expected <= 0))
        {
            break;
        }

        if ((hyphen_hyphen == 0) && (*cp++ == '-'))
        {
            argindex++;

            while ((c = *cp++) != '\0')
            {
                switch (c)
                {
                case 'B':
                    binmode = 1;
                    break;
                case 'b':
                    opt_value_expected++;
                    block_factor_arg = option_arg++;
                    break;
                case 'c':
                    create = 1;
                    break;
                case 'D':
                    opt_value_expected++;
                    date_policy_arg = option_arg++;
                    break;
                case 'd':
                    dot = 1;
                    break;
                case 'F':
                    force = 1;
                    break;
                case 'f':
                    opt_value_expected++;
                    tarfile_arg = option_arg++;
                    foption = 1;
                    break;
                case 'g':
                    debugg++;
                    break;
                case 'h':
                    help = 1;
                    break;
                case 'o':
                    ods2 = 1;
                    break;
                case 'p':
                    padding = 0;
                    break;

#ifdef SYMLINKS
                case 's':
                    symlinks = 1;
                    break;
#endif /* def SYMLINKS */

                case 't':
                    list = 1;
                    break;
                case 'u':
                    underdot = 1;
                    break;
                case 'v':
                    verbose++;
                    break;
                case 'w':
                    the_wait = 1;
                    break;
                case 'x':
                    extract = 1;
                    break;
                case 'z':
                    automode = 1;
                    break;
                case '-':
                    /* Break out of the "-" option loop. */
                    hyphen_hyphen = 1;
                    /* arg_opt = argc; */
                    break;
                default:
                    fprintf( stderr,
                     "tar: unrecognized option: '%c'.\n", c);
                    exit( EINVAL);
                }
            }
        }
        else
        {
            if (block_factor_arg >= 0)
            {
                block_factor_arg += hyphen_hyphen;
                argindex = VT_MAX( argindex, (3+ block_factor_arg));
                if (argc <= 2+ block_factor_arg)
                {
                    fprintf( stderr, "tar: block_factor (b) missing.\n");
                    exit( EINVAL);
                }
                else
                {
                    int n;
                    int b;

                    n = sscanf( argv[ 2+ block_factor_arg], "%d", &b);
                    if (b <= 0)
                    {
                        fprintf( stderr, "tar: Invalid block_factor (b): %s.\n",
                         argv[ 2+ block_factor_arg]);
                        exit( EINVAL);
                    }
                    else
                    {
                        block_factor = b;
                    }
                }
                opt_value_expected--;
                block_factor_arg = -1;
            }

            if (date_policy_arg >= 0)
            {
                date_policy_arg += hyphen_hyphen;
                argindex = VT_MAX( argindex, (3+ date_policy_arg));
                if (argc <= 2+ date_policy_arg)
                {
                    fprintf( stderr, "tar: date_policy (D) missing.\n");
                    exit( EINVAL);
                }
                else
                {
                    char *chr_p = argv[ 2+ date_policy_arg];
                    char chr;

                    /* Note: Simple parsing.  Last c|C or m|M option wins.
                     * For "c", "C", "m", and "M", upper-case is positive,
                     * lower-case is negative.  Docs show "A" (positive) and
                     * "n" (negative), but code here accepts "a" or "N", too.
                     * It's not clear what makes less sense.
                     */
                    while ((chr = *chr_p++) != '\0')
                    {
                        if ((chr == 'A') || (chr == 'a'))
                            date_policy = DP_BOTH;
                        else if (chr == 'c')
                            date_policy &= ~DP_CREATION;
                        else if (chr == 'C')
                            date_policy |= DP_CREATION;
                        else if (chr == 'm')
                            date_policy &= ~DP_MODIFICATION;
                        else if (chr == 'M')
                            date_policy |= DP_MODIFICATION;
                        else if ((chr == 'n') || (chr == 'N'))
                            date_policy = DP_NONE;
                        else
                        {
                            fprintf( stderr,
                             "tar: Invalid date_policy (D): %s (%c).\n",
                             argv[ 2+ date_policy_arg], chr);
                            exit( EINVAL);
                        }
                    }
                }
#ifdef DEBUG
                fprintf( stderr,
                 "UNIX date_policy: %d, creation: %s, modification: %s\n",
                 date_policy,
                 date_policy & DP_CREATION ? "yes" : "no",
                 date_policy & DP_MODIFICATION ? "yes" : "no");
#endif
                opt_value_expected--;
                date_policy_arg = -1;
            }

            if (tarfile_arg >= 0)
            {
                tarfile_arg += hyphen_hyphen;
                argindex = VT_MAX( argindex, (3+ tarfile_arg));
                if (argc <= 2+ tarfile_arg)
                {
                    fprintf( stderr, "tar: tarfile (f) missing.\n");
                }
                else
                {
                    strcpy( tarfile, argv[ 2+ tarfile_arg]);
                }
                opt_value_expected--;
                tarfile_arg = -1;
            }

#if 0
            if (foption)
            {
                if (argc < 3)
	        {
	            fprintf( stderr, "tar: tar file missing, using $TAPE.\n");
#if 0
                    usage( EINVAL);
#endif
                }
                else
                {
                    strcpy(tarfile,argv[2]);
                    argindex = 3;
                }
            }
#endif /* 0 */

        } /* end if */

        option_arg++;

    } /* end for */

    if (help)
    {
        if (extract + list + create + verbose + the_wait
         + binmode + automode + dot)
	    fprintf( stderr, "tar: ignoring all other options\n");
	usage( SS$_NORMAL);
    }

    /* Adjust argument count, pointer.  Display, if requested ("-g"). */

    *argc_p -= argindex;
    *argv_p += argindex;

    if (debugg > 0)
    {
        fprintf( stderr, "   argc = %d.\n", *argc_p);
        for (argc = 0; argc < *argc_p; argc++)
        {
            fprintf( stderr, "   >%s<.\n", (*argv_p)[ argc]);
        }
    }

/* Check options are coherent */

    /* Ensure that at least one of create, extract, help, list (or
     * verbose=version) was specified.
     */
    arg_check();

#ifdef SYMLINKS
    if (symlinks && !extract)
    {
        fprintf( stderr,
         "tar: incompatible options 's' may be used only with 'x'.\n");
        exit( EINVAL);
    }
#endif /* def SYMLINKS */

    if ((block_factor >= 0) && !create)
    {
        fprintf( stderr,
         "tar: incompatible options 'b' may be used only with 'c'.\n");
        exit( EINVAL);
    }

    /* Set default block_factor, if none was specified. */
    if (block_factor < 0)
    {
        block_factor = BLOCK_FACTOR_DEFAULT;
    }
    block_size = RECSIZE* block_factor;

    if (extract + create == 2)
    {
        fprintf( stderr, "tar: incompatible options 'x' and 'c' specified.\n");
        exit( EINVAL);
    }
    if (list + create == 2)
    {
        fprintf( stderr, "tar: incompatible options 't' and 'c' specified.\n");
        exit( EINVAL);
    }
    if (binmode + automode == 2)
    {
	fprintf( stderr, "tar: incompatible options 'B' and 'z' specified.\n");
        exit( EINVAL);
    }

    return SS$_NORMAL;
}


#ifdef TEST
unsigned int
main( int argc, char **argv)
{
    register status;
    return vmstar_cmdline(&argc, &argv);
}
#endif /* TEST */

