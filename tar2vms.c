#define module_name	TAR2VMS
#define module_version  "V2.1-1"
/*
 *	TAR2VMS.C - Handles the extract and list functionality of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#else
#module module_name module_version
#endif

#ifdef __DECC
#include <fabdef.h>
#include <rabdef.h>
#else
#include <fab.h>
#include <rab.h>
#endif
#include <math.h>
#include <starlet.h>
#include <stsdef.h>
#include <xabfhcdef.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <unixio.h>
#include <unixlib.h>
#include <ssdef.h>
#include <strdef.h>
#include <str$routines.h>

#include "vmstar_cmdline.h"
#include "vmsmunch.h"
#include "vmstarP.h"

int mkdir_vmstar();

/* Globals in this module */

static int mode, uid, gid;
static unsigned int bytecount;
static FILE *tarfp;		/* The input file pointer */

/* File characteristics */

static int linktype;

/* Misc. */

#define ABSASCTIM_MAX	23
char vmscreation[ABSASCTIM_MAX+1];

/* Forward declarations */

unsigned long str_case_match_wild();
int data_read();
int hdr_read();
int decode_header();
int scan_title();
int cleanup_dire();
int make_new();
int copyfile();
int tarskip();
int vms_cleanup();

void compute_modestring (short unsigned int mode, char *str);

/* tar2vms -- handles extract and list options */

tar2vms(argc,argv)
int argc;
char **argv;
{
int status,file_type,j, flag, argi, process;
char *make_directory(), *argp, *ptr;
struct dsc$descriptor pattern, candidate;
FILE *opentar();

/* open the file for reading */

    if((tarfp = opentar()) == NULL)
        {
        printf("tar: error opening tarfile.\n");
        exit(SS$_NORMAL);
        }

/* Now keep reading headers from this file, and decode the names, etc. */

    while((status=hdr_read(&header))==RECSIZE)    /* 0 on end of file */
    {
        process = 0;
        if(strlen(header.title)!=0)     /* Valid header */
        {
            decode_header();
            process = 1;

/* Now if file names were specified on the command line, check if they
   match the current one */

            if (argc > 0)
            {
                process = 0;
                argi = 0;
                while (argi < argc)
                {
                    pattern.dsc$w_length = strlen(argv[argi]);
                    pattern.dsc$a_pointer = argv[argi];
                    pattern.dsc$b_dtype = DSC$K_DTYPE_T;
                    pattern.dsc$b_class = DSC$K_CLASS_S;
                    candidate.dsc$w_length = strlen(pathname);
                    candidate.dsc$a_pointer = pathname;
                    candidate.dsc$b_dtype = DSC$K_DTYPE_T;
                    candidate.dsc$b_class = DSC$K_CLASS_S;

                    ++argi;

                    if (str_case_match_wild(&candidate, &pattern) == STR$_MATCH)
                    {
                        process = 1;
                        break;
                    }
                }
            }
        }
        else
        {
             status = 1;
             break;
        }  
        if (process && the_wait)
        {
            *temp = '\0';
            while (*temp != 'y' && *temp != 'n' && *temp != 'q' && *temp != 'a')
            {
                printf("%s   (y/n/q/a) ? ", pathname);
                if (scanf("%s", temp) == EOF) exit(SS$_NORMAL);
                *temp = tolower(*temp);
            }
	    if (*temp == 'q') exit(SS$_NORMAL);
            process = (*temp == 'y' || *temp == 'a');
	    if (*temp == 'a')
		the_wait = 0;
        }
        if (process && extract)
        {               
                file_type=scan_title(pathname,new_directory,newfile);
                cleanup_dire(new_directory);
                if( make_new(new_directory)!=0)
                    printf("tar: error creating %s\n",new_directory);
                if(file_type == ISDIRE)
                    {}
                if(file_type == ISFILE)

/*  Now move the data into the output file */

#if 0   /* bytecount is unsigned */
                    if(bytecount>0)
#endif
                    {
                        strcpy(outfile,new_directory);
                        strcat(outfile,newfile);
                        /* Take care of ODS5 file names such as: foo2.0.1 */
                        if (acp_type == DVI$C_ACP_F11V5) strcat(outfile,";");
                        copyfile(outfile,bytecount);
                    }
        }
        else
        {
            if (process && list)               /* listing only */
            {
                /* Some tar's set the 'linktype' to 5 and don't append a
                   '/' at the end for directory files.  However, it appears
                   that some tar's do (POSIX tar for instance).  So if the
                   file is a 'regular file' (0) and the pathname has a
                   trailing slash, change the 'linktype' to 5 and remove
                   the slash.
                */
                int plen;

                plen = strlen(pathname);
                if ((pathname[plen-1] == '/') && (linktype == 0))
                {
                linktype = 5;
                pathname[plen-1] = '\0';
                }

                if (verbose)
                {
                char modestring[10];

                compute_modestring(mode,modestring);

                printf("%10.10s %5d/%-5d %6d %s %s\n",
                    modestring,
                    uid,
                    gid,
                    bytecount,
                    creation+4,
                    pathname);
                }
                else
                {
                printf("%s\n",pathname);
                }

                if (linktype == 1 || linktype == 2)
                 printf("                                --->  %s\n",linkname);
            }
            if(linktype == 0)
                tarskip(bytecount);
        }
    }       /* end while  */
    if(status == 1)                     /* Empty header */
    {

/*        printf("Do you wish to move past the EOF mark (y/n) ? ");
**        fflush(stdout);
**        gets(temp);
**        if(tolower(*temp) == 'y')
**            while((status=hdr_read(&header)) >0);
**        else
*/
	    fclose(tarfp);
            exit(SS$_NORMAL);
    }
    if(status==0)                       /* End of tar file  */
    {
        printf("tar: EOF hit on tarfile.\n");
	fclose(tarfp);
        exit(SS$_NORMAL);
    }
    if(status<0)                        /* An error  */
    {
        printf("tar: error reading tarfile.\n");
	fclose(tarfp);
        exit(SS$_NORMAL);
    }
}


/* This function simply copies the file to the output, no conversion */

int copyfile(outfile,nbytes)
    char outfile[]; /* name of output version */
    unsigned int nbytes;
{
    int inbytes, fil, s, i, ctlchars, eightbitchars, nchars;
    register unsigned char c;
    struct VMStimbuf vtb;
    int binfile;		/* "extract as binary" flag */
    unsigned int bytes = nbytes;
    struct FAB fil_fab;
    struct RAB fil_rab;
    struct XABFHC fil_xab;

    /* Setup FAB/RAB for new file */

    bzero(&fil_fab, sizeof(fil_fab));
    bzero(&fil_rab, sizeof(fil_rab));

    fil_fab = cc$rms_fab;
    fil_fab.fab$b_fac = FAB$M_BIO;
    fil_fab.fab$l_fna = outfile;
    fil_fab.fab$b_fns = strlen(outfile);
    fil_fab.fab$b_org = FAB$C_SEQ;

    fil_rab = cc$rms_rab;
    fil_rab.rab$l_fab = &fil_fab;
    fil_rab.rab$l_rbf = buffer;
    fil_rab.rab$l_bkt = 1;

/* Read the first block of the tarred file */

    binfile = binmode;
    inbytes = 0;
    if (linktype == 0 && bytes != 0) {
        if((inbytes=data_read(buffer,bytes)) < 0)
        {
	    printf("tar: error reading tar file.\n");
	    fclose(tarfp);
	    exit(SS$_NORMAL);
        }

/* If automatic mode is set, then try to figure out what kind of file this
   is */

        if (automode && inbytes != 0) {
            ctlchars = 0;
	    eightbitchars = 0;

/* Scan the buffer, counting chars with msb set and control chars
   not CR, LF or FF */

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

/* Now apply some heuristics to determine file is text or binary */

	    ctlchars = ctlchars * 100 / nchars;
	    eightbitchars = eightbitchars * 100 / nchars;
	    if (ctlchars > 10 || eightbitchars > 30 ||
                (ctlchars > 5 && eightbitchars > 20))
                binfile = 1;
        }
    }

/*  Open the output file */

    fil_fab.fab$l_alq = (unsigned long) ((bytes+511)/512);

    if (binfile)
    {
        fil_fab.fab$b_rfm = FAB$C_FIX;
        fil_fab.fab$w_mrs = 512;
    }
    else
    {
        bzero(&fil_xab, sizeof(fil_xab));

        fil_fab.fab$b_rfm = FAB$C_STMLF;
        fil_fab.fab$b_rat = FAB$M_CR;
        fil_fab.fab$l_xab = &fil_xab;

        fil_xab = cc$rms_xabfhc;
        fil_xab.xab$w_lrl = 32767;
    }

    if(!(sys$create(&fil_fab) & STS$M_SUCCESS) || !(sys$connect(&fil_rab) &
        STS$M_SUCCESS))
    {
        printf("tar: error creating %s \n",outfile);
	tarskip(bytes - inbytes);
	s = -1;
    }
    else
	s = 0;
    if(linktype == 1 || linktype == 2)
    {
        sprintf(buffer,"*** This file is a link to %s\n",linkname);
        fil_rab.rab$w_rsz = strlen(buffer);
        if (!(sys$write(&fil_rab) & STS$M_SUCCESS)) s = -1;
	sys$close(&fil_fab);
    }
    else
    {
        while(bytes > 0 && s >= 0 && inbytes > 0)
        {
            if (bytes>=BUFFERSIZE)
                fil_rab.rab$w_rsz = BUFFERSIZE;
            else
                /* RMS writes an even number of bytes so zero next byte */
                buffer[fil_rab.rab$w_rsz = bytes] = '\0';

            if (!(sys$write(&fil_rab) & STS$M_SUCCESS)) 
               s = -1;
            else            
               fil_rab.rab$l_bkt += BUFFERFACTOR;
            if (bytes > inbytes) {
		bytes -= inbytes;
		inbytes = data_read(buffer,bytes);
	    } else
		bytes = 0;
        }

/* Close the extracted file, check results */

	sys$close(&fil_fab);
        if (inbytes == 0 && bytes != 0) {
	    printf("tar: unexpected EOF on tar file.\n");
	    fclose(tarfp);
	    exit(SS$_NORMAL);
	}
	if (inbytes < 0) {
	    printf("tar: error reading tar file.\n");
	    fclose(tarfp);
	    exit(SS$_NORMAL);
        }
    }
    if (s < 0)
    {
        printf("tar: error writing file %s\n",outfile);
	fclose(tarfp);
        exit(SS$_NORMAL);
    }

    if(verbose)
    {
        printf("%s %8u%c%s\n",
               creation+4,bytecount,
	       binfile ? '*' : ' ',outfile);
        if (linktype == 1 || linktype == 2)
            printf("                         --> %s\n",linkname);
    }
    vtb.actime = 0;
    vtb.modtime = 0;
    if (date_policy & dp_modification)
	vtb.actime = vmscreation;
    if (date_policy & dp_creation)
	vtb.modtime = vmscreation;
    VMSmunch(outfile, SET_TIMES, &vtb);
    VMSmunch(outfile, SET_EXACT_SIZE, &nbytes);
    return(0);
}

/* scan_title -- decode a Un*x file name into the directory and name */

/* Return a value to indicate if this is a directory name, or another file
* We return the extracted directory string in "dire", and the
* filename (if it exists) in "fname". The full title is in "line"
* at input.
*/

int scan_title(line,dire,fname)
char line[],dire[],fname[];
{
char *end1;
int len,len2,i,ind;
/* The format will be UNIX at input, so we have to scan for the
* UNIX directory separator '/'
* If the name ends with '/' then it is actually a directory name.
* If the directory consists only of '.', then don't add a subdirectory
* The output directory will be a complete file spec, based on the default
* directory.
*/



    strcpy(dire,curdir);                /* Start with the current dir */

    /* We need to make sure the directory delimiters are square brackets,
       otherwise we'll get some problems... -- Richard Levitte */
    while ((end1 = strchr(dire,'<')) != 0)
	*end1 = '[';
    while ((end1 = strchr(dire,'>')) != 0)
	*end1 = ']';
    
    if(strncmp(line,"./",2)==0)
        strcpy(line,line+2);            /* ignore "./" */
    strcpy(temp,line);                  /* Start in local buffer */
    ind=vms_cleanup(temp);              /* Remove illegal vms characters */
    if((end1=strrchr(temp,'/'))==0)     /* No directory at all  ? */
        strcpy(fname,temp);             /* Only a file name */
    else
    {                                   /* End of directory name is '/' */
        *end1 = 0;                      /* Terminate directory name */
        strcpy(fname,end1+1);           /* File name without directory */
        for (i=1;temp[i];i++)           /* Change '/' to '.' in directory */
            if(temp[i]=='/')		/* and '.' to '_' */
                temp[i]='.';
	    else if (!dot && temp[i] == '.' && acp_type != DVI$C_ACP_F11V5)
                temp[i] = '_';
        if (*temp == '/')               /* absolute path ? */
        {
            *temp = '[';		/* yes, build absolute VMS path */
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
    if(strlen(fname)==0)        /* Could this cause problems ? */
    {
        return(ISDIRE);
    }
    else
        if (acp_type != DVI$C_ACP_F11V5) {
            for(i=0,end1=fname;*end1;end1++) /* Replace multiple . */
                if(*end1 == '.')
                    if(i++)*end1 = '_'; /* After the first */
            }
    return(ISFILE);
}

/* make_new -- create a new directory */

int make_new(want)
char want[];
{
int status, created;
char *dotp;

#ifdef debug
    fprintf(stderr, "want = %s (", want);
    for (dotp = want; *dotp != '\0'; dotp++)
	fprintf(stderr, "\\%o", *dotp);
    fprintf(stderr, ")\nbefore: errno = %d -- ", errno); perror("");
#endif
    created = 1;
    status = mkdir_vmstar(want, 0);   /* our mkdir creates all missing levels */
    if (status != 0)
    {
#ifdef debug
        fprintf(stderr, "1: status = %d, errno = %d -- ",
		status, errno); perror("");
#endif
        if (errno == EEXIST)
            return (0);
        if (errno != EINVAL)
            return (-1);          /* unknown error, simply return */
        else                      /* maybe too many levels of directories */
        {                         /* change "[...FOO.BAR]" to "[...FOO$BAR]" */
        for (dotp = &want[strlen(want) - 1];dotp > want && status != 0;)
            if (*--dotp == '.') 
            {
                *dotp = '$';
                status = mkdir_vmstar(want, 0);
                if (status != 0 && errno == EEXIST)
                {
                    status = created = 0;
                    break;
                }
#ifdef debug
		fprintf(stderr, "2: status = %d, errno = %d -- ",
			status, errno); perror("");
#endif
            }
        }
    }
#ifdef debug
    fprintf(stderr, "3: status = %d, errno = %d -- ",
	    status, errno); perror("");
#endif
    if (status != 0)
        return (-1);
    if(verbose && created)
        printf("                              %s\n",want);
    return(0);
}

 /* Function to open and get data from the blocked input file */
FILE *opentar()
{
FILE *fp;
    fp = fopen(tarfile, "rb");
    return(fp);
}

/* STR$MATCH_WILD - case insensitive */

unsigned long str_case_match_wild(candidate, pattern)
struct dsc$descriptor_s *candidate, *pattern;
{
struct dsc$descriptor_s local_candidate, local_pattern;
int i;
unsigned long sts;
    local_candidate = *candidate;
    local_pattern = *pattern;
    local_candidate.dsc$a_pointer = malloc(candidate->dsc$w_length);
    local_pattern.dsc$a_pointer = malloc(pattern->dsc$w_length);
    for (i = 0 ; i < candidate->dsc$w_length; i++) 
        local_candidate.dsc$a_pointer[i] = toupper(candidate->dsc$a_pointer[i]);
    for (i = 0 ; i < pattern->dsc$w_length; i++) 
        local_pattern.dsc$a_pointer[i] = toupper(pattern->dsc$a_pointer[i]);
    sts = str$match_wild(&local_candidate, &local_pattern);
    free(local_candidate.dsc$a_pointer);
    free(local_pattern.dsc$a_pointer);
    return(sts);
    }

/* Get the next chunk of data belonging to the current file being extracted */

int data_read(buffer, bytes_to_read)
char *buffer;
int bytes_to_read;
{
double block_count;
    /* Be sure to not read past the number of bytes remaining to be read
       and handle the case the number of bytes to read is a RECSIZE multiple */
    if (bytes_to_read>=BUFFERSIZE)
        block_count = BUFFERFACTOR;
    else
        if (modf((double)bytes_to_read/RECSIZE, &block_count)) block_count++;
    return(fread(buffer,1,(size_t)block_count*RECSIZE,tarfp));
}

/* Get the next file header from the input file buffer. We will always
* move to the next 512 byte boundary.
*/
int hdr_read(buffer)
char *buffer;
{
int stat;
    stat = fread(buffer,1,RECSIZE,tarfp);    /* read the header */
    return(stat);                       /* catch them next read ? */
}


/* This is supposed to skip over data to get to the desired position */
/* Position is the number of bytes to skip. We should never have to use
* this during data transfers; just during listings. */
int tarskip(bytes)
unsigned int bytes;
{
int i=0;
    while(bytes > 0)
    {
        if((i=fread(buffer,1,RECSIZE,tarfp)) == 0)
	{
            printf("tar: EOF hit while skipping.\n");
            return(-1);
	}
        if (bytes > i)
	    bytes -= i;
	else
	    bytes = 0;
    }
    return(0);
}

/* Decode the fields of the header */

int decode_header()
{
unsigned long idate, *bintim, chksum, value;
char ll, *ptr;
bintim = &idate;
    linktype=0; strcpy(linkname,"");
    strcpy(pathname,header.title);
    sscanf(header.time,"%o",bintim);
    strcpy(creation,ctime(bintim));     /* Work on this! */
    creation[24]=0;

    sprintf(vmscreation, "%2.2s-%3.3s-%4.4s %8.8s.00", 
	&(creation[8]), &(creation[4]), &(creation[20]), &(creation[11]));
    vmscreation[4] = _toupper(vmscreation[4]);
    vmscreation[5] = _toupper(vmscreation[5]);

    sscanf(header.count,"%o",&bytecount);
    sscanf(header.protection,"%o",&mode);
    sscanf(header.uid,"%o",&uid);
    sscanf(header.gid,"%o",&gid);
    sscanf(header.chksum,"%o",&chksum);

/* Verify checksum */

    for(value = 0, ptr = (char *)&header; ptr < (char *)&header.chksum; ptr++)
             value += *ptr;                /* make the checksum */
    for(ptr = &header.linkcount; ptr <= &header.dummy[255]; ptr++)
             value += *ptr;
    value += (8 * ' ');	               /* checksum considered as all spaces */

    if (chksum != value)
    {                                       /* abort if incorrect */
        printf("tar: directory checksum error for %s\n",pathname);
        exit(SS$_NORMAL);
    }


/* We may have the link written as binary or as character:  */

    linktype = isdigit(header.linkcount)?
            (header.linkcount - '0'):header.linkcount;
    if(linktype != 0)
        sscanf(header.linkname,"%s",linkname);
    return(0);
}


/* vms_cleanup -- removes illegal characters from directory and file names
 * Replaces hyphens and commas with underscores. Returns number of translations
 * that were made.
 */

vms_cleanup(string)
char string[];
{
    int i,flag=0;
    char c, *p;
    static char *badchars, *translate;

    if (acp_type == DVI$C_ACP_F11V5) {

        badchars = BADCHARS_ODS5;
        translate = TRANSLATE_ODS5;

        }

    else {

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
        else {
            /* Escape remaining illegal ODS2 characters for ODS5. Dots must be
               escaped only if they are in a directory portion of the path */
            if (acp_type == DVI$C_ACP_F11V5 && ((p = strchr(BADCHARS_ODS2, c))
                != NULL || (c == '.' && strchr(string + i, '/') != NULL))) {
                strcpy(string + i + 1, string + i);
                string[i++] = '^';
                }
            }
        if (ods2) string[i] = toupper(string[i]);  /* Map to uppercase */
    }
    return(flag);
}

/* Let's try to do our own, non-buggy mkdir ().  At least, it returns
   better error codes, especially for non-unix statuses.  For ODS5 volumes
   we need to do it ourselves anyway as DEC C mkdir will not preserve case
   properly on directory specifications that are all lower case - get
   converted to upper case unless DECC$EFS_CASE_PRESERVE is defined. It
   could be considered rude to have to define it just for vmstar */

#include <libdef.h>
#include <lib$routines.h>

int mkdir_vmstar(dir, mode)
char *dir;
int mode;
{
  struct dsc$descriptor_s dsc_dir = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0 };
  unsigned long status;

  dsc_dir.dsc$w_length = strlen(dir);
  dsc_dir.dsc$a_pointer = dir;

  status = lib$create_dir(&dsc_dir, 0, 0, 0, 0, 0);
  switch (status)
    {
    case SS$_CREATED : errno = 0; return 0;
    case SS$_NORMAL : errno = EEXIST; return -1;
    case LIB$_INVARG : errno = EINVAL; return -1;
    case LIB$_INVFILSPE : errno = EINVAL; return -1;
    default: errno = EVMSERR; vaxc$errno = status; return -1;
    }
  return -1;
}

#ifndef S_IRUSR  /* VAX C doesn't have these */
/*
**  ISO POSIX-1 definitions
*/
#define S_IRUSR  0000400    /* read permission: owner */
#define S_IWUSR  0000200    /* write permission: owner */
#define S_IXUSR  0000100    /* execute/search permission: owner */
#endif /* S_IRUSR */

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
compute_modestring (short unsigned int mode, char *str)
{
  switch (linktype)
    {
    case 1:	
    case 2:	str[0] = 'l'; break;	/* symbolic or hard link */
    case 5:	str[0] = 'd'; break;	/* directory */
    default:	str[0] = '-'; break;	/* regular */
    }

  rwx ((mode & 0700) << 0, &str[1]);
  rwx ((mode & 0070) << 3, &str[4]);
  rwx ((mode & 0007) << 6, &str[7]);
  setst (mode, str);
}
