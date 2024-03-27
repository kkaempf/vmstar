#define module_name	TAR2VMS
#define module_version  "V2.0-1"
/*
 *	TAR2VMS.C - Handles the extract and list functionality of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#else
#module module_name module_version
#endif

#ifdef USE_OWN_MKDIR
#define mkdir oldmkdir
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <unixio.h>
#include <unixlib.h>
#include <descrip.h>
#include <ssdef.h>
#include <strdef.h>
#include <str$routines.h>

#include "vmstar_cmdline.h"
#include "vmsmunch.h"
#include "vmstarP.h"

#ifdef USE_OWN_MKDIR
#undef mkdir
int mkdir();
#endif

/* Globals in this module */

static int mode, uid, gid;
static int bytecount;
static FILE *tarfp;		/* The input file pointer */

/* File characteristics */

static int linktype;

/* Misc. */

#define ABSASCTIM_MAX	23
char vmscreation[ABSASCTIM_MAX+1];

/* Forward declarations */

int hdr_read();
int decode_header();
int scan_title();
int cleanup_dire();
int make_new();
int copyfile();
int tarskip();
int vms_cleanup();

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

                    if (str$match_wild(&candidate, &pattern) == STR$_MATCH)
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

                    if(bytecount>=0)
                    {
                        strcpy(outfile,new_directory);
                        strcat(outfile,newfile);
                        copyfile(outfile,bytecount);
                    }
        }
        else
        {
            if (process && list)               /* listing only */
            {
                printf("%6o %8d %s %s\n",
                    mode,bytecount,creation+4,pathname);
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
int nbytes;
{
int inbytes, fil, s, i, ctlchars, eightbitchars, nchars;
register unsigned char c;
struct VMStimbuf vtb;
int binfile;		       /* "extract as binary" flag */

/* Read the first block of the tarred file */

    binfile = binmode;
    inbytes = 0;
    if (linktype == 0 && nbytes != 0) {
        if((inbytes=fread(buffer,1,RECSIZE,tarfp)) < 0)
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

	    nchars = nbytes < inbytes ? nbytes : inbytes;
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

    if (binfile)
    {
#if 0
        fil = creat(outfile,0,"rfm=fix","mrs=512","alq=5");
#else
        char alq[10];
	sprintf(alq,"alq=%d",(nbytes+511)/512);
        fil = creat(outfile,0,"rfm=fix","mrs=512",alq);
#endif
    }
    else
    {
#if 0
        fil = creat(outfile,0,"rfm=stmlf","rat=cr","mbc=16");
#else
        char alq[10];
	sprintf(alq,"alq=%d",(nbytes+511)/512);
        fil = creat(outfile,0,"rfm=stmlf","rat=cr","mbc=16",alq);
#endif
    }

    if(fil == ERROR1)
    {
        printf("tar: error creating %s \n",outfile);
	tarskip(nbytes - inbytes);
    }
    s = 0;
    if(linktype !=0)
    {
        sprintf(buffer,"*** This file is a link to %s\n",linkname);
        s = write(fil,buffer,strlen(buffer));
	close(fil);
    }
    else
    {
        while(nbytes > 0 && s >= 0 && inbytes > 0)
        {
	    /* Some tar files have lots of junk at the end of the last
	     * record (unless the file to extractt has a size which is
	     * a multiple of 512 bytes, of course).  This is not very
	     * clean, so we simply fill the end of the last record
	     * with NULs */
	    if (binfile && nbytes<RECSIZE)
	      memset(buffer+nbytes,'\0',RECSIZE-nbytes);

            s = write(fil,buffer,(binfile || (nbytes>RECSIZE)) ? RECSIZE : nbytes);
            nbytes -= inbytes;
            if (nbytes > 0)
		inbytes = fread(buffer,1,RECSIZE,tarfp);
        }

/* Close the extracted file, check results */

	close (fil);
        if (inbytes == 0 && nbytes != 0) {
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
        printf("%s %8d%c%s\n",
               creation+4,bytecount,
	       binfile ? '*' : ' ',outfile);
        if(linktype!=0)
            printf("                         --> %s\n",linkname);
    }
    vtb.actime = 0;
    vtb.modtime = 0;
    if (date_policy & dp_modification)
	vtb.actime = vmscreation;
    if (date_policy & dp_creation)
	vtb.modtime = vmscreation;
    VMSmunch(outfile, SET_TIMES, &vtb);
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
	    else if (!dot && temp[i] == '.')
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
        for(i=0,end1=fname;*end1;end1++) /* Replace multiple . */
            if(*end1 == '.')
                if(i++)*end1 = '_'; /* After the first */
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
    status = mkdir(want, 0);   /* mkdir in VAX C and DEC C creates all
				  missing levels */
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
                status = mkdir(want, 0);
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
int bytes;
{
int i=0;
    while(bytes > 0)
        {
        if((i=fread(buffer,1,RECSIZE,tarfp)) == 0)
            {
            printf("tar: EOF hit while skipping.\n");
            return(-1);
            }
        bytes -= i;
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
    char c;
    char *p;
    static char badchars[] = BADCHARS;
    static char translate[] = TRANSLATE;

    for(i=0; c=string[i]; ++i)
    {
        if ((p = strchr(badchars, c)) != NULL)
        {                    /* Replace illegal characters by underscores */
            string[i] = translate[p-badchars];
            flag++;          /* Record if any changes were made */
        }
        else
            string[i] = toupper(c); /* Map to uppercase */
    }
    return(flag);
}

#ifdef USE_OWN_MKDIR
/* Let's try to do our own, non-buggy mkdir ().  At least, it returns
   better error codes, especially for non-unix statuses.  */

#include <libdef.h>

int mkdir(dir, mode)
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
#endif /* USE_OWN_MKDIR */
