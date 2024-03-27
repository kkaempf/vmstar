#define module_name VMSTAR_CMDLINE
#define module_ident "02-004"
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

#include <stdio.h>
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <clidef.h>
#include <stdlib.h>
#include <string.h>
#include <lib$routines.h>
#include <str$routines.h>

#include "vmstar_cmdline.h"

#ifndef CLI$_COMMA
globalvalue CLI$_COMMA;
#endif

/* Function flags, options */

int help = 0,		/* h option, help */
    extract = 0,	/* x option, extract */
    list = 0,		/* t option, list tape contents */
    verbose = 0,	/* v option, report actions */
    the_wait = 0,	/* w option, prompt */
    dot = 0,		/* d option, suppress dots (creation),
			   or keep dots in directory names (extraction) */
    create = 0,		/* c option, create */
    binmode = 0,	/* b option, binary mode */
    automode = 0,	/* z option, automatic mode */
    foption = 0;	/* f option, specify tarfile */
char tarfile[NAMSIZE];
unsigned long date_policy = dp_both; /* /DATE_POLICY qualifier */

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
$DESCRIPTOR(cli_tarfile,	"TARFILE");
$DESCRIPTOR(cli_infile,		"INFILE");
$DESCRIPTOR(cli_extract,	"EXTRACT");
$DESCRIPTOR(cli_help,		"HELP");
$DESCRIPTOR(cli_list,		"LIST");
$DESCRIPTOR(cli_create,		"CREATE");
$DESCRIPTOR(cli_verbose,	"VERBOSE");
$DESCRIPTOR(cli_confirm,	"CONFIRM");
$DESCRIPTOR(cli_binary,		"BINARY");
$DESCRIPTOR(cli_automatic,	"AUTOMATIC");
$DESCRIPTOR(cli_dots,		"DOTS");
$DESCRIPTOR(cli_date_policy,	"DATE_POLICY");
$DESCRIPTOR(cli_date_policy_all,		"DATE_POLICY.ALL");
$DESCRIPTOR(cli_date_policy_none,		"DATE_POLICY.NONE");
$DESCRIPTOR(cli_date_policy_creation,		"DATE_POLICY.CREATION");
$DESCRIPTOR(cli_date_policy_modification,	"DATE_POLICY.MODIFICATION");
$DESCRIPTOR(cli_yyz,		"YYZ");
$DESCRIPTOR(vmstar_command, "vmstar ");

#ifdef __DECC
extern void *vmstar_clitables;
#else
globalref void *vmstar_clitables;
#endif

/* extern unsigned long LIB$GET_INPUT(void), LIB$SIG_TO_RET(void); */

extern unsigned long cli$dcl_parse ();
extern unsigned long cli$present ();
extern unsigned long cli$get_value ();

unsigned long unix_vmstar_cmdline (int *, char ***);
static unsigned long get_list (struct dsc$descriptor_s *, char **, char);
static unsigned long check_cli (struct dsc$descriptor_s *);


/* Usage of this program. */
usage()
{
    printf("usage (VMS style):  TAR tarfile file[,...]\n");
    printf("   Options: /HELP         -- Print this text and exits\n");
    printf("            /LIST         -- List contents of tarfile\n");
    printf("            /CREATE       -- Create a tarfile\n");
    printf("            /EXTRACT      -- Extract files from tarfile\n");
    printf("            /VERBOSE      -- Display processed file info\n");
    printf("            /CONFIRM      -- Prompt before store/extract\n");
    printf("            /BINARY       -- Create binary files\n");
    printf("            /AUTOMATIC    -- Automatically determine file type\n");
    printf("            /DOTS         -- Maintain \".\" usage\n");
    printf("            /DATE_POLICY  -- Decide what times of extracted files\n");
    printf("                             will be set:\n");
    printf("                                 [NO]CREATION\n");
    printf("                                 [NO]MODIFICATION\n");
    printf("\n");
    printf("Press return for more..."); getchar();
    printf("\n");
    printf("usage (UNIX style): tar -h|x|t|c[vwbzd][f tarfile] [file [file...]]\n");
    printf("   Options: h  =  /HELP\n");
    printf("            t  =  /LIST\n");
    printf("            c  =  /CREATE\n");
    printf("            x  =  /EXTRACT\n");
    printf("            v  =  /VERBOSE\n");
    printf("            w  =  /CONFIRM\n");
    printf("            b  =  /BINARY\n");
    printf("            z  =  /AUTOMATIC\n");
    printf("            d  =  /DOTS\n");
    printf("            f file     -- Specify a tar file or magtape.  Default is $TAPE\n");
    exit(SS$_NORMAL);
}

unsigned long
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
	if (!(status & 1)) return(status);
    }

    /*
    **  First, check to see if any of the regular options were specified.
    */

    status = cli$present (&cli_dots);
    if (status & 1)
	found_options = 1, dot = 1;

    status = cli$present (&cli_binary);
    if (status & 1)
	found_options = 1, binmode = 1;

    status = cli$present (&cli_help);
    if (status & 1)
	found_options = 1, help = 1;

    status = cli$present (&cli_list);
    if (status & 1)
	found_options = 1, list = 1;

    status = cli$present (&cli_confirm);
    if (status & 1)
	found_options = 1, the_wait = 1;

    status = cli$present (&cli_extract);
    if (status & 1)
	found_options = 1, extract = 1;

    status = cli$present (&cli_verbose);
    if (status & 1)
	found_options = 1, verbose = 1;

    status = cli$present (&cli_create);
    if (status & 1)
	found_options = 1, create = 1;

    status = cli$present (&cli_automatic);
    if (status & 1)
	found_options = 1, automode = 1;

    status = cli$present (&cli_date_policy);
    if (status & 1) {
#ifdef DEBUG
	printf ("/DATE_POLICY is apparently present, according to status %d\n",
		status);
#endif

	found_options = 1;
	date_policy = dp_none;

	status = cli$present (&cli_date_policy_all);
	if (status & 1)
	    date_policy = dp_both;
    
	status = cli$present (&cli_date_policy_none);
	if (status & 1)
	    date_policy = dp_none;
    
	status = cli$present (&cli_date_policy_creation);
	if (status & 1)
	    date_policy |= dp_creation;

	status = cli$present (&cli_date_policy_modification);
	if (status & 1)
	    date_policy |= dp_modification;
    }
#ifdef DEBUG
    printf ("date_policy: %d\ncreation: %s\nmodification: %s\n",
	    date_policy,
	    date_policy & dp_creation ? "yes" : "no",
	    date_policy & dp_modification ? "yes" : "no");
#endif

    /* If the user didn't give any DCL qualifier, assume he uses the
       Un*x interface */
    if (!found_options)
	return unix_vmstar_cmdline (argc_p, argv_p);

    /*
    **
    **  OK.  We've done all the regular options, so check for the
    **	tarfile and the files to store or extract.
    **
    */
    status = cli$present (&cli_tarfile);
    if (status & 1) {
	unsigned long len;
	status = cli$get_value (&cli_tarfile, &work_str);
	len = work_str.dsc$w_length;
	strncpy(tarfile, work_str.dsc$a_pointer, len);
	tarfile[len] = '\0';
    }

    status = cli$present (&cli_infile);
    if (status & 1) {
	status = get_list (&cli_infile, &the_cmd_line, ' ');
	if (!(status & 1)) return (status);
    }

    /*
    **  Now that we've gotten everything off the command line, count the
    **  number of args and build an argv array.
    */

#if defined(TEST) || defined(DEBUG)
    printf("%s\n",the_cmd_line);
#endif /* TEST */

    new_argc = the_cmd_line != NULL && *the_cmd_line != 0;
    for (ptr = the_cmd_line;
	 ptr != 0 && (ptr = strchr(ptr,' ')) != NULL;
	 ptr++, new_argc++);

    /* There must be one extra place for the final NULL.  */
    if ((new_argv = (char **) calloc (new_argc + 1, sizeof(char *))) == NULL) {
	perror("Memory exhausted");
	return(SS$_ABORT);
    }

    for (ptr = the_cmd_line, x = 0; x < new_argc; x++) {
	new_argv[x] = ptr;
	if ((ptr = strchr (ptr, ' ')) != NULL)
	    *ptr++ = '\0';
    }
    new_argv[x] = NULL;

#if defined(TEST) || defined(DEBUG)
    printf("new_argc    = %d\n", new_argc);
    for (x = 0; x < new_argc; x++)
	printf("new_argv[%d] = %s\n", x, new_argv[x]);
#endif /* TEST */

    *argc_p = new_argc;
    *argv_p = new_argv;

    return(SS$_NORMAL);
}


#if 0
static unsigned long
set_boolean (struct dsc$descriptor_s *qual, struct bool_desc *desc)
{
    register status;
    struct dsc$descriptor_d work_str;

    init_dyndesc(work_str);

    status = cli$present (qual);
    if (status & 1) {

	unsigned long len, old_len;

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
		    perror("Memory exhausted");
		    return(SS$_ABORT);
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

    return (SS$_NORMAL);

}
#endif


static unsigned long
get_list (struct dsc$descriptor_s *qual, char **str, char c)
{
    register status;
    struct dsc$descriptor_d work_str;

    init_dyndesc(work_str);

    status = cli$present (qual);
    if (status & 1) {

	unsigned long len, old_len;

	len = *str == NULL ? 0 : strlen(*str);
	while ((status = cli$get_value (qual, &work_str)) & 1) {
	    if (*str == NULL) {
		len = work_str.dsc$w_length + 1;
		if ((*str = (char *) malloc (work_str.dsc$w_length))
			== NULL) {
		    perror("Memory exhausted");
		    return(SS$_ABORT);
		}
		strncpy(*str,work_str.dsc$a_pointer,len);
	    } else {
		char *src, *dst; int x;
		old_len = len;
		len += work_str.dsc$w_length + 1;
		if ((*str =
			(char *) realloc (*str, len)) == NULL) {
		    perror("Memory exhausted");
		    return(SS$_ABORT);
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

    return (SS$_NORMAL);

}


static unsigned long
check_cli (struct dsc$descriptor_s *qual)
{
/*
**  Routine:	check_cli
**
**  Function:	Check to see if a CLD was used to invoke the program.
**
*/
    lib$establish(lib$sig_to_ret);	/* Establish condition handler */
    return (cli$present(qual));		/* Just see if something was given */
}


unsigned long
unix_vmstar_cmdline(int *argc_p, char ***argv_p)
{
char *cp, c;
register int argindex = 2;
int argc = *argc_p;
char **argv = *argv_p;

    if (argc == 1)
        usage();

    strcpy(tarfile,"$TAPE");

    cp = argv[1];
    while(c = *cp++)
    {
        switch(c)
        {
	case 'h':
	    help=1;
	    break;
        case 't':
            list=1;
            break;
        case 'x':
            extract=1;
            break;
        case 'c':
            create=1;
            break;
        case 'v':
            verbose=1;
            break;
        case 'w':
            the_wait=1;
            break;
        case 'd':
            dot=1;
            break;
        case 'b':
            binmode=1;
            break;
	case 'z':
	    automode=1;
	    break;
        case 'f':
	    if (*cp != '\0') {
		printf("tar: no other options may follow \"f\".\n");
		usage();
		exit(SS$_NORMAL);
	    }
	    foption = 1;
	    break;
        case '-':
            break;
        default:
            printf("tar: option '%c' not recognized.  Continuing...\n",c);
            usage();
	    exit(SS$_NORMAL);
        }
    }

    if (help)
    {
        if (extract + list + create + verbose + the_wait
	    + binmode + automode + dot)
	    printf("tar: ignoring all other options\n");
	usage();
	exit(SS$_NORMAL);
    }

    if (foption)
        if (argc < 3)
	{
	    printf("tar: tar file missing, using $TAPE.\n");
#if 0
	    usage();
#endif
	}
	else
	{
	    strcpy(tarfile,argv[2]);
	    argindex = 3;
	}

/* Check options are coherent */

    if (extract + list + create + help == 0)
    {
        printf("tar: no action specified.\n");
        exit(SS$_NORMAL);
    }
    if (extract + create == 2)
    {
        printf("tar: incompatible options 'x' and 'c' specified.\n");
        exit(SS$_NORMAL);
    }
    if (list + create == 2)
    {
        printf("tar: incompatible options 't' and 'c' specified.\n");
        exit(SS$_NORMAL);
    }
    if (binmode + automode == 2)
    {
	printf("tar: incompatible options 'b' and 'z' specified.\n");
        exit(SS$_NORMAL);
    }
    *argc_p -= argindex;
    *argv_p += argindex;
    return(SS$_NORMAL);
}


#ifdef TEST
unsigned long
main(int argc, char **argv)
{
    register status;
    return (vmstar_cmdline(&argc, &argv));
}
#endif /* TEST */

