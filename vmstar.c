/* NB: Adjust this for local conditions.  Some sites may have the
logical name TIME_ZONE defined, in which case we could handle
this transparently.  Hopefully, VMS will correct this soon, and we
don't bother for now.  */
#define SECONDS_WEST_OF_GREENWICH (6L*60L*60L)
#define VERSION "Version 2.7 [08-Mar-1990]"

 /* Read a TAR format tape or file , move files into VMS directories */
/* Copyright 1986, Sid Penstone,
*  Department of Electrical Engineering,
*  Queen's University,
*  Kingston, Ontario, Canada K7L3N6
* (613)-545-5925
* BITNET:	   PENSTONE@QUCDNEE1  (Preferred)
*	or  PENSTONE@QUCDN
*
* Version 2.7 [08-Mar-1990] <Beebe@science.utah.edu>
*	- Added n option to suppress application of heuristics for
*	  dynamic output file mode changes; if n is specified, the m=<x>
*	  option is always obeyed.  This is useful for transferring UNIX
*	  stream files (e.g. compiled EMACS LISP *.elc files) that
*	  contain binary data, but must be stored in stream format.
*	  Add VERSION macro (defined above) for output in usage message.
* Version 2.6 [23-Jan-1990] <Beebe@science.utah.edu>
*	- Rewrote vms_cleanup() to handle translations to
*	  VAX VMS filename character set [A-Za-z0-9.$_-]; the code
*	  may still fail if the filename has too many directory levels,
*	  or is too long.
* Version 2.5 [27-jun-89] <MDebar@cc.fundp.ac.be>
*	- Changed default file format to stream,
*	- Do not output padding nulls, except for variable format
*	- Provide work-around for bug in VMS 5.0-2 C runtime,
*	  which insists on every write finishing a record.
*	  This is active only if VMS_VARBUG is defined
* Version 2.4 [17-Oct-87] <Beebe@science.utah.edu>
*	- Added filename substitution option s=<subfile>; subfile
*	  contains lines of the form
*	  oldname [whitespace]newname
*	  e.g.
*	  cmr10.300pk [.300]cmr10.pk
*	  Any leading whitespace on a line is ignored.
*	  Each filename found in the tarfile (in Unix form) is matched
*	  (exactly) against the oldnames in the substitution file, and
*	  the first match found results in its replacement by the
*	  newname.  Any amount of whitespace may separate the two names.
*	- Added usage message if no (or bad) options given
*	- Changed output of error messages to stderr
*	- Reformatted source code with Unix indent utility for
*	  consistency
*	- Changed exit codes to VMS standard ones
*	- Added p=<xx> option for specification of padding character for
*	  binary files
*	- Added f option for specification of tarfile on command line
*	  like Unix
*	- Added m=<[bsv]> options for selection of output file format
*	  (Binary, Stream, Variable) (default is m=v because too many
*	   VMS utilities fail to handle stream files
*	- Implemented utime() and added code to call it to set file
*	  times (NB: This must be changed twice a year if DST is in
*	  effect)
*	- #Ifdef'ed out the translation of "-" to "_" in filenames
*	  (not needed after VMS Version 4.3)
*	- Made option switches case insensitive because VMS likes to
*	  uppercase them, and it is a nuisance to have to quote them
* Version 2.3, Jan.9,1987
* mods: - Corrected header size (thanks to Eric Gisin, U .of Waterloo)
*	- No more of the dreaded QIO's ( "  "  " )
*	- Tried to sort out link flag format
*	- Uses a tape or a file as input
*	- Note: default is NO conversion to vms standard text format (cr)
* 2.1	- Trapped commas in file names, converted to '_'
* 2.2	- Reported translations of names
*	- Continued after error in opening output file
*	- Exit correctly after error opening input file
* 2.3	- Fixed bug in make_new on top level file (thanks to Ursula Natrup,
*					natrup@vax.hmi.dfn )
*	- Reject "@" in filenames
*/


/* The input data is in record format, length 512, blocks of 10240 bytes;
 */


#include ctype
#include descrip
#include iodef
#include ssdef
#include stat
#include stdio
#include time
#include types

#define BADSUB	'$'		/* substitute for illegal filename chars */
#define DOTSUB	'_'		/* substitute for extra . in filenames */

#define ERROR1 -1
#define BUFFSIZE 512
#define ISDIRE 1
#define ISFILE 0
#define NAMSIZE 100
#define SIZE 10240		/* Block size */
#define DSIZE 512		/* Data block size */

#define VMS_VARBUG 1
#ifdef VMS_VARBUG
char xbuf[BUFFSIZE];
xcount = 0;
#endif

struct				/* A Tar header */
{
    char title[NAMSIZE];
    char protection[8];
    char field1[8];		/* this is the user id */
    char field2[8];		/* this is the group id */
    char count[12];		/* was 11 in error */
    char time[12];		/* UNIX format date  */
    char chksum[8];		/* Header Checksum (ignored) */
    char linkcount;		/* hope this is right */
    char linkname[NAMSIZE];	/* Space for the name of the link */
    char dummy[255];		/* and the rest */
}   header;

static char buffer[DSIZE];	/* BUFFER for a record */
static time_t gbl_bintim[2];	/* global file revision time */
struct stat stat_buf;		/* for stat() call */

/* Function flags, options:*/
int apply_heuristics = 1;	/* n option turns off */
int extract = 0;		/* x option (default) */
int list = 0;			/* t option : list tape contents */
int verbose = 0;		/* v option, report actions */
int wait;			/* w option (unused) */

/* Miscellaneous globals, etc. */

char creation[NAMSIZE];		/* Date as extracted from the TAR file */
char directory[NAMSIZE];	/* Current directory */
char linkname[NAMSIZE];		/* Linked file name  */
char newfile[NAMSIZE];		/* VMS format of file name */
char new_directory[NAMSIZE];	/* Directory of current file */
char outfile[NAMSIZE];		/* Complete output file specification */
int padchar = 0;		/* padding character for binary file output */
char pathname[NAMSIZE];		/* File name as found on tape (UNIX) */
char subfile[L_tmpnam];		/* File name of name substitution file */
FILE* subfp = (FILE*)NULL;	/* Substitution file pointer */
char* tarfile = "tape";		/* Input file name  */
char temp[256];			/* Scratch */
char top[NAMSIZE];		/* Top level or root */

#define TYPE_VARIABLE  0
#define TYPE_STREAM 1
#define TYPE_BINARY 2
int output_type = TYPE_STREAM;
char* output_names[] =
{
    "variable text","stream text","fixed binary"
};

/* Data from tar file header */
int bytecount;
int mode;
int uic1;
int uic2;
int linktype;
int tarfd;			/* The input file descriptor */

/* Function declarations */

int   copyfile();
char* ctime();
int   decode_header();
int   hdr_read();
char* make_directory();
int   make_new();
int   opentar();
int   scan_title();
int   sscanf();
char* strcat();
char* strchr();
int   strcmp();
char* strcpy();
int   strcspn();
int   strlen();
int   strncmp();
int   strspn();
void  subname();
int   tarskip();
int   vms_cleanup();

main(argc, argv)
int argc;
char *argv[];
{
    int c;
    int err_count = 0;
    int file_type;
    int flag;
    int isterm;
    int j;
    int status;
    char *cp;
    char *p;

/* Decode the options and parameters: */

    if (argc == 1)
    {
	extract = 1;		/* Default for now */
	verbose = 1;
	wait = 0;		/* Don't wait for prompt */
    }
    while (--argc > 0)
    {
	cp = argv[1];
	while (c = *cp++)
	{
	    switch (_tolower(c))
	    {
	    case 'f':
		tarfile = argv[2];
		argc--;
		break;

	    case 'm':		/* expect m=<[bsv]> */
		if (sscanf(cp,"=<%c>",&c) != 1)
		{
		    err_count++;
		    fprintf(stderr,"?Expected m=<[bsv]>.\n");
		    c = 'v';
		}
		p = strchr(cp,'>');
		if (p == (char*)NULL)
		    cp += strspn(cp," \t=<bsvBSV>");
		else
		    cp = ++p;
		switch (_tolower(c))
		{
		case 'b':
		    output_type = TYPE_BINARY;
		    break;

		case 's':
		    output_type = TYPE_STREAM;
		    break;

		case 'v':
		    output_type = TYPE_VARIABLE;
		    break;

		default:
		    err_count++;
		    fprintf(stderr,"?Expected m=<[bsv]>.\n");
		    output_type = TYPE_STREAM;
		    break;
		}
		break;

	    case 'n':		/* suppress file mode switches */
		apply_heuristics = 0;
		break;

	    case 'p':		/* p=<xx> */
		if (sscanf(cp,"=<%x>",&padchar) != 1)
		{
		    err_count++;
		    padchar = 0;
		    fprintf(stderr,
		    "?Expected p=<xx> (hexadecimal value).\n");
		}
		p = strchr(cp,'>');
		if (p == (char*)NULL)
		    cp += strspn(cp," \t=<0123456789abcdefABCDEF>");
		else
		    cp = ++p;
		break;

	    case 't':
		list = 1;
		break;

	    case 's':		/* s=<subfile> */
		if ((*cp == '=') && (*(cp+1) == '<'))
		{
		    p = subfile;
		    for (cp += 2; (*cp) && (*cp != '>'); ++cp)
			*p++ = *cp;
		    *p = '\0';
		    cp++;
		    if ((subfp = fopen(subfile,"r")) == (char*)NULL)
		    {
			err_count++;
			fprintf(stderr,"?Cannot open substitution file [%s]\n",
				subfile);
		    }
		}
		else
		{
		    err_count++;
		    fprintf(stderr,"?Expected s=<subfile>.\n");
		}
		break;

	    case 'v':
		verbose = 1;
		break;

	    case 'w':		/* unused */
		wait = 1;
		break;

	    case 'x':
		extract = 1;
		break;

	    default:
		fprintf(stderr,"Option '%c' not recognized.\n", c);
		err_count++;
		break;

	    }
	}
    }
    if (err_count > 0)
    {
	fprintf(stderr,"UNIX tar file reader for VAX VMS -- %s\n",VERSION);
	fprintf(stderr,"Usage: tar [n][tx][m=<[bsv]>][p=<xx>][s=<subfile>]");
	fprintf(stderr,"[v][f tarfile]\n\n");
	fprintf(stderr,"n = suppress dynamic file mode switching\n");
	fprintf(stderr,
		"t = Type directory of tarfile, x = eXtract files to disk\n");
	fprintf(stderr,"m = output file Mode\n");
	fprintf(stderr,"\tb = Binary, s = Stream, v = Variable\n");
	fprintf(stderr,
		"p = Padding character for last block of binary files\n");
	fprintf(stderr,"\txx = hexadecimal value of padding character\n");
	fprintf(stderr,"s = Unix filename Substitution file");
	fprintf(stderr,"\t<subfile> = file specification in <>\n");
	fprintf(stderr,
		"\tSubfile contains lines with oldname newname pairs\n");
	fprintf(stderr,"\tseparated by whitespace\n");
	fprintf(stderr,"v = Verbose output\n");
	fprintf(stderr,"f = input File\n");
	fprintf(stderr,"\ttarfile = file specification as next argument\n\n");
	fprintf(stderr,"Defaults: tar m=<v>p=<0>xvf tape\n\n");
	exit(SS$_BADPARAM);
    }

    isterm = isatty(0);		/* Find if this is a terminal */
    strcpy(top, getenv("PATH")); /* Set up directory names */
    strcpy(directory, top);	/* Start with the default as the top */

    if ((tarfd = opentar()) < 0) /* open the file for reading */
    {
	fprintf(stderr,"?Cannot open input tar file [%s]\n",tarfile);
	exit(SS$_NOSUCHFILE);
    }

    /* Now keep reading headers from this file, and decode the names, etc. */
    while ((status = hdr_read(&header)) == DSIZE)	/* 0 on end of file */
    {
	if (strlen(header.title) != 0)	/* Valid header */
	{
	    decode_header();
	    if (extract)
	    {
		file_type = scan_title(pathname, new_directory, newfile);
		if (make_new(new_directory) != 0)
		    fprintf(stderr,"Error creating %s\n", new_directory);
		if (file_type == ISDIRE)
		{
		}
		if (file_type == ISFILE)
				/* Now move the data into the output file */
		    if (bytecount > 0)
		    {
			strcpy(outfile, new_directory);
			strcat(outfile, newfile);
			if ((j = copyfile(outfile, bytecount)) < 0)
			    fprintf(stderr,"Error writing file %s\n", outfile);
			if (utime(outfile, gbl_bintim) == -1)
			    fprintf(stderr,"Error setting file time %s\n",
				    outfile);
		    }
	    }
	    else		/* listing only */
	    {
		printf("%o %6d %s %s\n",
		       mode, bytecount, creation + 4, pathname);
		if (linktype == 0)
		    tarskip(bytecount);
		else
		    printf("	 *****( Linked to file: %s)\n", linkname);
	    }
	}
	else			/* Empty header means the end!!! */
	{
	    status = 1;
	    printf("End of Tar file found.\n");
	    break;
	}

    }				/* end while  */
    if (status == 1)		/* Empty header */
    {
	if (stat(tarfile,&stat_buf) == 0) /* on success, then tarfile is */
	    status = 0; /* on disk, not tape--don't ask about EOF mark */
    }
    if (status == 1)		/* Empty header */
    {
	printf("Do you wish to move past the EOF mark ? y/n\n");
	gets(temp);
	if (tolower(temp[0]) == 'y')
	    while ((status = hdr_read(&header)) > 0);
	else
	    exit(SS$_NORMAL);
    }
    if (status == 0)		/* End of tar file  */
    {
	printf("End of file encountered\n");
	exit(SS$_NORMAL);
    }
    if (status < 0)		/* An error  */
    {
	fprintf(stderr,"Error reading input.\n");
	exit(SS$_BADFILEHDR);
    }
}


/* This function simply copies the file to the output, no conversion */

int
copyfile(outfile, nbytes)
char outfile[];			/* name of output version */
int nbytes;

{
    int block_count = 0;
    int fil;
    int inbytes;
    int output_file_type = output_type;
    int total_bytes = 0;

    switch (output_type)	/* Open the output file */
    {
    case TYPE_VARIABLE:
	fil = creat(outfile, 0, "rat=cr", "rfm=var");
	break;

    case TYPE_STREAM:
	fil = creat(outfile, 0);
	break;

    case TYPE_BINARY:
	fil = creat(outfile, 0, "rfm=fix", "bls=512", "mrs=512");
	break;

    default:
	fprintf(stderr,
		"?Internal error--wrong output file type.  Assuming [%s]\n",
		output_names[TYPE_VARIABLE]);
	fil = creat(outfile, 0, "rat=cr", "rfm=var");
	break;
    }
    if (fil == ERROR1)
    {
	fprintf(stderr," Creation error in opening %s\n", outfile);
	tarskip(bytecount);
	return (-2);
    }
    if (linktype != 0)
    {
	sprintf(buffer, "This file is linked to %s\n", linkname);
	write(fil, buffer, strlen(temp));
    }
    else
    {
	total_bytes = nbytes;
	while (nbytes > 0)
	{
	    if ((inbytes = read(tarfd, buffer, DSIZE)) > 0)
	    {
		if (   (block_count == 0)
		    && (output_file_type != TYPE_BINARY)
		    && apply_heuristics )
		{   /* check for binary data and switch file type if so; */
		    /* this heuristic is often, but not always, successful */
		    register int k;

		    for (k = inbytes; k > 0; )
		    {
			if (buffer[--k] & 0x80)
			{
			    close(fil);
			    delete(outfile);
			    output_file_type = TYPE_BINARY;
			    fil = creat(outfile, 0, "rfm=fix",
					"bls=512", "mrs=512");
			    break;
			}
		    }
		}
		block_count++;
#ifdef VMS_VARBUG
		if (output_file_type != TYPE_VARIABLE)
		  write(fil, buffer, (nbytes > DSIZE) ? DSIZE : nbytes);
		else
		  xwrite(fil, buffer, (nbytes > DSIZE) ? DSIZE : nbytes);
#else
		  write(fil, buffer, (nbytes > DSIZE) ? DSIZE : nbytes);
#endif

		nbytes -= inbytes;
	    }
	    else
	    {
		printf("End of input file detected\n");
		close(fil);
		return (-1);
	    }
	}
	if (output_file_type == TYPE_VARIABLE)
	{/* RMS requires size = multiple of 512 bytes (else lose last block) */
#ifdef VMS_VARBUG
	    xflush (fil);
#else
	    buffer[0] = padchar;
	    while (total_bytes & 511)
	    {
		write(fil,buffer,1);
		total_bytes++;
	    }
#endif
	  }
	else if (output_file_type == TYPE_BINARY)
	{/* RMS requires size = multiple of 512 bytes (else lose last block) */
	    buffer[0] = padchar;
	    while (total_bytes & 511)
	    {
		write(fil,buffer,1);
		total_bytes++;
	    }
	  }

    }
    close(fil);			/* Close the file */
    if (verbose)
    {
	printf("CREATED: %s [%s]\n", outfile, output_names[output_file_type]);
	if (linktype != 0)
	    printf(" *** REAL DATA IS IN: %s\n", linkname);
    }
    return (0);
}

/* Decode a file name into the directory, and the name, return
* a value to indicate if this is a directory name, or another file
* We return the extracted directory string in "dire", and the
* filename (if it exists) in "fname". The full title is in "line"
* at input.
*/

int
scan_title(line, dire, fname)
char line[];
char dire[];
char fname[];
{
    char temp[NAMSIZE];
    char* end1;
    int len;
    int len2;
    int i;
    int ind;
/* The format will be UNIX at input, so we have to scan for the
* UNIX directory separator '/'
* If the name ends with '/' then it is actually a directory name.
* If the directory consists only of '.', then don't add a subdirectory
* The output directory will be a complete file spec, based on the default
* directory.
*/
    strcpy(dire, top);		/* Start with the top level */
    if (strncmp(line, "./", 2) == 0)
	strcpy(line, line + 2); /* ignore "./" */
    strcpy(temp, line);		/* Start in local buffer */
    ind = vms_cleanup(temp);	/* Remove illegal vms characters */
    if ((end1 = strrchr(temp, '/')) == 0)	/* No directory at all	? */
	strcpy(fname, temp);	/* Only a file name */
    else
    {				/* End of directory name is '/' */
	*end1 = 0;		/* Terminate directory name */
	strcpy(fname, end1 + 1);/* File name without directory */
	for (i = 0; temp[i]; i++)	/* Change '/' to '.' in directory */
	    if (temp[i] == '/')
		temp[i] = '.';
	dire[strlen(dire) - 1] = (temp[0] == '.') ? 0 : '.';
	/* "." to indicate a subdirectory (unless already there ) */
	strcat(dire, temp);	/* Add on the new directory  */
	strcat(dire, "]");	/* And close with ']' */
    }
    if (strlen(fname) == 0)	/* Could this cause problems ? */
    {
	return (ISDIRE);
    }
    else
	for (i = 0, end1 = fname; *end1; end1++) /* Replace multiple . */
	    if (*end1 == '.')
		if (i++)
		    *end1 = DOTSUB; /* After the first */
    if ((i > 1 || ind) && verbose)	/* Any translations ? */
    {
	printf("****RENAMED: %s\n",line);
	printf("         TO: %s\n",fname);
    }
    return (ISFILE);
}

/* Create a new directory, finding out any higher levels that are missing */

/* We will parse the directory name into the next higher directory, and the
* desired directory as "desired.dir".
* Thus: "DEV:[top.sub1.sub2]" is made into "DEV:[top.sub1]sub2.dir" . If
* the directory does not exist , then create the original directory. There
* may be higher levels missing, so we can recurse until we reach the top
* level directory, then work our way back, creating directories at each
* successive level.
* Bug fix: if the input file was at top level, we will not find a '.'
*	and 'name' will be garbage.
*/

int
make_new(want)
char want[];
{
    int i;
    int len;
    char a[NAMSIZE];
    char* end;
    char name[NAMSIZE];
    char parent[NAMSIZE];

    strcpy(parent, want);
    len = strlen(parent);
    parent[len - 1] = 0;	/* Get rid of the "]" */
    end = strrchr(parent, '.'); /* Find the last '.' */
    if (end != NULL)
    {
	strcpy(a, end + 1);	/* Get the last parent */
	strcat(a, ".dir");	/* Add the extension */
	*end++ = ']';		/* Reduce the directory parent */
	*end = 0;		/* Terminate the directory */
	strcpy(name, parent);
	strcat(name, a);

	if (access(name, 0) < 0)/* Does the directory exist ? */
	{
	    if (strcmp(parent, top) != 0)	/* No, are we at the top? */
		if (make_new(parent))	/* No, look again */
		    return (-1);/* recurse */
	    if (mkdir(want, 0755, 0, 0, 0))	/* make it */
		return (-1);	/* Leave on error */
	    else
	    if (verbose)
		printf("CREATED: %s\n", want);
	    return (0);
	}
    }
    return (0);
}

 /* Function to open and get data from the blocked input file */
int
opentar()
{
    int fd;

    fd = open(tarfile, 0, "rfm = fix", "mrs = 512");
    return (fd);
}

/* Get the next file header from the input file buffer. We will always
* move to the next 512 byte boundary.
*/
int
hdr_read(buffer)
char *buffer;
{
    int stat;

    stat = read(tarfd, buffer, DSIZE);	/* read the header */
    return (stat);		/* Catch them next read ? */
}


/* Search the substitution file for an oldname matching filename, and return
its newname in filename.  If no match is found, filename is left intact. */
void
subname(filename)
char* filename;
{
    register char* p;
    char* oldp;
    char* newp;

    rewind(subfp);
    while (fgets(temp,sizeof(temp),subfp) != (char*)NULL)
    {
	p = temp;
	oldp = p + strspn(p," \t"); /* ignore leading whitespace */
	p = oldp + strcspn(oldp," \t"); /* find first following whitespace */
	*p++ = '\0';		/* change to EOS marker */
	if (strcmp(oldp,filename) == 0) /* filename == newname? */
	{
	    newp = p + strspn(p," \t"); /* find first non-whitespace */
	    p = newp + strcspn(newp," \t\n"); /* find trailing whitespace */
	    *p = '\0';			/* and terminate newname */
	    strcpy(filename,newp);	/* and replace filename by newname */
	    return;
	}
    }
}

/* This is supposed to skip over data to get to the desired position */
/* Position is the number of bytes to skip. We should never have to use
* this during data transfers; just during listings. */
int
tarskip(bytes)
int bytes;
{
    int i = 0;

    while (bytes > 0)
    {
	if ((i = read(tarfd, buffer, DSIZE)) == 0)
	{
	    fprintf(stderr,"End of file encountered while skipping.\n");
	    return (-1);
	}
	bytes -= i;
    }
    return (0);
}

/* Decode the fields of the header */

int
decode_header()
{
    int* bintim;
    int idate;
    char ll;

    bintim = &idate;
    linktype = 0;
    strcpy(linkname, "");
    strcpy(pathname, header.title);
    if (subfp != (FILE*)NULL)
	subname(pathname);
    sscanf(header.time, "%o", bintim);
    gbl_bintim[0] = *bintim;	/* Greenwich time */

    /* VMS has no notion of GMT, so all the C time functions treat binary
       times as if they were local times, instead of GMT times. A Unix tar
       file has the binary time in GMT, so we need to hack it here, and
       worse, the value SECONDS_WEST_OF_GREENWICH depends on the time of
       year, sigh.... */

    gbl_bintim[0] -= SECONDS_WEST_OF_GREENWICH; /* adjust to local time */

    strcpy(creation, ctime(bintim));	/* Work on this! */
    creation[24] = 0;
    sscanf(header.count, "%o", &bytecount);
    sscanf(header.protection, "%o", &mode);
    sscanf(header.field1, "%o", &uic1);
    sscanf(header.field2, "%o", &uic2);
    /* We may have the link written as binary or as character:	*/
    linktype = isdigit(header.linkcount) ?
	(header.linkcount - '0') : header.linkcount;
    if (linktype != 0)
	sscanf(header.linkname, "%s", linkname);
    return (0);
}


/* Remove illegal characters from directory and file names; replace
* illegal characters with BADSUB.  Return number of translations
* that were made.  Legal chars are [A-Za-z0-9$_-], plus . and /, which
* will be handled separately above as directory and extension separators.
*/
int
vms_cleanup(string)
char string[];
{
    char c;
    int change_count = 0;
    int i;

    for (i = 0; c = string[i]; i++)
    {
	if ( !(
	       isalnum(c) ||
	       (c == '-') ||
	       (c == '$') ||
	       (c == '_') ||
	       (c == '/') ||
	       (c == '.')
	      )
	   )
	{
	    string[i] = BADSUB; /* change non-VMS chars to substitute */
	    change_count++;	/* count the change */
	}
    }
    /* First and last characters may not be hyphens */
    if (string[0] == '-')
	change_count++, string[0] = BADSUB;
    if (string[i-1] == '-')
	change_count++, string[i-1] = BADSUB;

    return (change_count);
}


#ifdef VMS_VARBUG
/*
  There is a bug in the Version 5.02 of VMS C runtime, which
  insists that every write in variable record files ends
  a record. This work-around buffers writes by actual line.
  It is definitely expected to fail on non text files...
  */

xwrite (fil, buf, count)
int fil, count;
char *buf;
{
char *p = buf;
while (count--) {
   if ( (xbuf[xcount++] = *p++) == '\n' || (xcount == BUFFSIZE)) {
   write (fil, xbuf, xcount);
   xcount = 0;
   }
 }


}

xflush (fil)
int fil;
{
  write (fil, xbuf, xcount);
  xcount = 0;
}

#endif
