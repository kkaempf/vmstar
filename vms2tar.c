#define module_name	VMS2TAR
#define module_version  "V2.1-4"
/*
 *	VMS2TAR.C - Handles the create functionality of VMSTAR.
 */

#ifdef __DECC
#pragma module module_name module_version
#else
#module module_name module_version
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stat.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <file.h>
#include <perror.h>

#include <unixio.h>
#include <unixlib.h>
#include <ssdef.h>
#include <rms.h>
#include <starlet.h>
#include <lib$routines.h>

#include "vmstar_cmdline.h"
#include "vmstarP.h"

#define EXPERIMENT 1

/* Globals in this module */

static int mode, uid, gid;    /* current values used by create */
static unsigned int bytecount;
static int bufferpointer, funnyfile;

/* File characteristics */

static int outfd;
static unsigned long vmsmrs, vmstime;  /* maximum record size */
static int vmsrat,vmsorg,vmsrfm;       /* other format (as integers) */

/* forward declarations of routines */

int initsearch();
int search();
int scan_name();
int cleanup_dire();
int get_attributes();
int fill_header();
int write_header();
int write_trailer();
int addfile();
int out_file();
int flushout();
int lowercase();

#ifdef EXPERIMENT
int outfile_rms();
int get_varfilesize();
char EmptyTrailer[RECSIZE] = {'\0'};
#endif /* EXPERIMENT */

/* vms2tar -- handles create function */

vms2tar(argc,argv)
    int argc;
    char **argv;
{
    int argi, process, file_type, absolute;
    char string[NAMSIZE], *cp, *dp, *wp;

    unsigned short res_length;
    static char res_string[255];
    $DESCRIPTOR(dev_descr,tarfile);
    $DESCRIPTOR(res_descr,res_string);

    bufferpointer = 0;
    gid = getgid();
    uid = getuid();
    mode = 0644;

/* Open the output file */

#ifdef __DECC /* Suggested by Martin Stiftinger <stifting@iue.tuwien.ac.at> */
    dev_descr.dsc$w_length = strlen(tarfile);
    if (lib$getdvi(&DVI$_ACPTYPE,0,&dev_descr,0,&res_descr,&res_length) & 1 &&
	strncmp(res_descr.dsc$a_pointer,"UNKNOWN",res_length) == 0)
	/* The following should only happen with tapes. */
	outfd = open(tarfile,O_WRONLY,0600,"rfm=fix","mrs=512");
    else
#endif /* 1 */
	outfd = creat(tarfile,0600,"rfm=fix","mrs=512");

    if (outfd < 0)
    {
        char buf[1024];
        sprintf(buf, "tar: error opening output tarfile %s", tarfile);
	perror(buf);
        exit(SS$_NORMAL);
    }  

    for (argi = 0; argi < argc; ++argi)
    {
        strcpy(string, argv[argi]);
        absolute = 0;
        cp = strchr(string, ':');   /* device name in argument ? */
        if (cp != NULL) {           /* yes, force absolute mode */
            absolute = 1;
            curdevdesc.dsc$a_pointer = string;
            curdevdesc.dsc$w_length = cp - string + 1;
            lib$getdvi(&DVI$_ACPTYPE, 0, &curdevdesc, &acp_type, 0, 0);
                                    /* update device EFS/ODS5 status */
            }
        else
        {
            cp = strchr(string, '[');  /* test argument for absolute... */
	    if (cp == NULL)
		cp = strchr(string, '<');
            if (cp != NULL)            /* ...or relative filespec */
            {
                ++cp;
                if (*cp != '.' && *cp != '-')
                    absolute = 1;
            }
        }
        if(initsearch(string) <= 0)
            printf("tar: no files matching %s\n",string);
        else
          while(search(newfile,32767)!=0)
          {
	    funnyfile = 0;
            strcpy(linkname, newfile);
            cleanup_dire(newfile);
            file_type = scan_name(newfile,new_directory,outfile,absolute);
	    if (strlen(new_directory)+strlen(outfile) > NAMSIZE)
	      {
		printf("*** skipped  %s\n", pathname);
		printf("*** file name too large\n");
		break;
	      }
            strcpy(pathname,new_directory);
            strcat(pathname,outfile);
            if (get_attributes(newfile) != 0) exit(SS$_NORMAL);
            process = 1;
            if(the_wait)
            {
                *temp = '\0';
                while (*temp != 'y' && *temp != 'n')
                {
                    printf("%s   (y/n) ? ", linkname);
                    scanf("%s", temp);
                    *temp = tolower(*temp);
                }
                process = (*temp == 'y');
            }
            if (process)
            {
                if(file_type == ISDIRE)
                {
                    bytecount =  0;
                    mode = 0755;
                    fill_header(pathname);
                    write_header(outfd);
                }
                if(file_type == ISFILE)
                {
                    mode = 0644;
                    if(addfile(newfile, pathname) < 0)
                    {
                        printf("tar: error copying %s\n",linkname);
                        exit(SS$_NORMAL);
                    }
                }
                if (funnyfile < 0 && file_type == ISFILE)
		{
                    printf("*** skipped  %s\n", linkname);
		    printf("*** unsupported format\n");
		}
                if (verbose && (funnyfile >=0 || file_type == ISDIRE))
                {
                     strcpy(creation,ctime(&vmstime)); /* work on this */
                     creation[24]=0;
                     printf("%s %8u %s\n",creation + 4,bytecount,pathname);
                }
            }
          }
    }
    write_trailer(outfd);
    close(outfd);
}

/* addfile - copy the vms file to the output file */

int addfile(vmsname,unixname)
char vmsname[],unixname[];
{
int ind;
    if(funnyfile < 0)          /* we don't output unsupported files  */
        return(0);
    if((ind=out_file(vmsname,bytecount,outfd)) < 0)
        return(ind);
    bufferpointer = bufferpointer%BLKSIZE;
    return(1);
}


/* out_file - write out the file.
* move nbytes of data from "fdin" to "fdout";
* Always pad the output to a full RECSIZE
* If it a VMS text file, it may be various formats, so we will
* read the file twice in case of a text file
* so that we get the correct byte count.
* We set the funnyfile=-1 if this is funny file.
*/

int out_file(filename,nbytes,fdout)
char filename[];
int fdout, nbytes;
{
int i, n, pos;
char *bufp, *linep;
FILE *filein;
static char dbuffer[RECSIZE];
#ifdef EXPERIMENT
int r_value;
#endif /* EXPERIMENT */

    funnyfile = 0;

    if(vmsrfm == FAB$C_FIX || vmsrfm == FAB$C_STM ||
       vmsrfm == FAB$C_STMLF)
    {
    	if((filein = fopen(filename,"rb")) == NULL)
    	{
    		printf("tar: error opening input file %s\n",filename);
        	return(-1);
        }
        fill_header(pathname);          /* We have all of the information */
        write_header(outfd);            /* So write to the output */
        while(nbytes > 0)
        {
            n = fread(buffer, 1, nbytes>RECSIZE? RECSIZE:nbytes, filein);
            if (n == 0)
            {
                fclose(filein);
                printf("tar: error reading input file %s\n",filename);
                return(-1);
            }
            nbytes -= n;
            flushout(fdout);
        }
        fclose(filein);
        return(0);
    }
    else
    {
	if(vmsrat == 0)			/* No record attributes? */
	{				/* Might be a text file anyway */
					/* So let's check two records. */

#if 0
	    /* Unfortunatelly, it won't work, because I seem to get RECSIZE
	       byte blocks, and I can't find a way to get just one record.
	       Thus, I can't reliably find out if the file is text or not.
	       Pity.  -- Richard Levitte */
	    if((filein = fopen(filename,"r","rfm=fix","mrs=512","ctx=rec")) == NULL)
	    {
                printf("tar: error opening input file %s\n",filename);
                return(-1);
            }
	    if((nbytes = fread(dbuffer,1,RECSIZE,filein)) != 0)
	    {
		if(dbuffer[nbytes-2] != '\r' || dbuffer[nbytes-1] != '\n')
		{
		    fclose(filein);
		    funnyfile = -1;
		    return(0);
		}
	    }
	    else if (!feof(filein))
	    {
		fclose(filein);
		printf("tar: error reading input file %s\n",filename);
		printf("     record too large or incorrect RMS attributes\n");
		return(-1);
	    }
	    if((nbytes = fread(dbuffer,1,RECSIZE,filein)) != 0)
	    {
		if(dbuffer[nbytes-2] != '\r' || dbuffer[nbytes-1] != '\n')
		{
		    fclose(filein);
		    funnyfile = -1;
		    return(0);
		}
	    }
	    else if (!feof(filein))
	    {
		fclose(filein);
		printf("tar: error reading input file %s\n",filename);
		printf("     record too large or incorrect RMS attributes\n");
		return(-1);
	    }
	    fclose(filein); /* rewind(filein); */
	    /* exit(1); */
#else
#ifdef EXPERIMENT
            /*
            ** OK, the regular VMSTAR skips them, but these may be Schlumberger 
            ** file formats (PDS, DLIS etc.) so let us take another shot
            ** at them by using Stream_LF or RMS routines.
            */

	    /*
	    ** The file turned out to be a variable length file, so we
	    ** need the exact size without the bytes used for indicating size
	    ** of each record. I could not find a way of accurately knowing
	    ** the size of such files without parsing them first !!!
	    */
	    bytecount = 0;
	    funnyfile = 0;
	    if ((nbytes = get_varfilesize(filename, !force)) < 0
		|| funnyfile < 0)
		return nbytes;
	    bytecount = nbytes;

            /* r_value = outfile_stream(filename, nbytes, fdout) ; */
            r_value = outfile_rms(filename, nbytes, fdout, !force) ; 

            return (r_value) ;
#else
	    funnyfile = -1;
	    return(0);
#endif /* EXPERIMENT */
#endif /* 0 */
	}
	if((filein = fopen(filename,"r")) == NULL)
	{
	    printf("tar: error opening input file %s\n",filename);
	    return(-1);
	}
	/* compute "Unix" file size */
	nbytes = 0;
	while(fgets(dbuffer,RECSIZE,filein) !=  NULL)
	    nbytes += strlen(dbuffer);
	if (!feof(filein))
	{
	    fclose(filein);
	    printf("tar: error reading input file %s\n",filename);
	    printf("     record too large or incorrect RMS attributes\n");
	    return(-1);
	}
	rewind(filein);                 /* Back to the beginning  */
	bytecount = nbytes;             /* Use the real count  */
	fill_header(pathname);          /* Compute the header */
	write_header(outfd);            /* Write it  */
	bufp = buffer;

	if (nbytes != 0)		    /* Check for empty file */
	{
	    while(fgets(dbuffer,RECSIZE,filein) !=  NULL)
	    {                           /* Now copy to the output */
		linep = dbuffer;
		while (*linep != '\0')  /* read source file line by line */
		{
		    if (bufp >= &buffer[RECSIZE])
		    {
			bufp = buffer;
			flushout(fdout); /* if buffer full, flush it */
		    } /* copy in fixed size output buffer */
		    *bufp++ = *linep++;
		}  
	    }
	    flushout(fdout);
	}
	fclose(filein);
    }
    return (0);        
}

/* flushout - write a fixed size block in output tarfile */

flushout(fdout)
int fdout;
{
    if (write(fdout,buffer,RECSIZE) != RECSIZE)
    {
        printf("tar: error writing tarfile.\n");
        exit(SS$_NORMAL);
    }
    bufferpointer += RECSIZE;
}

/* write_header - copy the header to the output file  */

int write_header(fd)
int fd;
{
int n;
    if((n=write(fd,&header,RECSIZE))!=RECSIZE)
    {
        printf("tar: error writing header in tarfile.\n");
        exit(SS$_NORMAL);
    }
    bufferpointer += RECSIZE;
    return(n);
}

/*  get_attributes - get the file attributes via stat()  */

int get_attributes(fname)
    char fname[];
{
    struct stat sblock;

    funnyfile = 0;
    if(stat(fname,&sblock))
    {
        printf("tar: can't get file status on %s\n",fname);
        vmstime = 0;                    /* Prevent garbage printoout */
        funnyfile = -1;                 /* of inaccessible files  */
        return(-1);
    }

/* now get the file attributes, we don't use them all */
    vmsrat = sblock.st_fab_rat;
    vmsmrs = sblock.st_fab_mrs;
    vmsrfm = sblock.st_fab_rfm;
    vmstime = sblock.st_mtime;
    bytecount = sblock.st_size;
/* UID/GID fix */                     
    uid = sblock.st_uid & 0xffff;
    gid = sblock.st_gid;         

    return (0);
}


/* write_trailer - write the two blank blocks on the output file */
/* pad the output to a full blocksize if needed. */

write_trailer(fdout)
int fdout;
{
int i;
    for (i=0; i < NAMSIZE; ++i)
        header.title[i] = header.linkname[i] = '\0';
    for (i=0; i < 255; ++i)
        header.dummy[i] = '\0';
    for (i=0; i < 12; ++i)
        header.count[i] = header.time[i] = '\0';
    for (i=0; i < 8; ++i)
        header.protection[i] = header.uid[i] = header.gid[i] = 
	    header.chksum[i] = '\0';
    header.linkcount = '\0';
    write_header(fdout);
    write_header(fdout);

    /*
    ** This is to satisfy Tar on NT, that works only with block sizes
    ** of 1024. 
    */
#ifdef DEBUG
    printf("padding = %d, block_factor = %d\n",padding, block_factor);
#endif
    if (!padding)
      return 1;

    bufferpointer = bufferpointer%BLKSIZE;
    while (bufferpointer < BLKSIZE)
        write_header(fdout);
    return(1);
}

/* scan_name - decode a file name */

/* Decode a file name into the directory, and the name, and convert
* to a valid UNIX pathname. Return  a value to indicate if this is
* a directory name, or another file.
* We return the extracted directory string in "dire", and the
* filename (if it exists) in "fname". The full title is in "line"
* at input.
*/

int scan_name(line,dire,fname,absolute)
char line[],dire[],fname[];
int absolute;
{
char *cp1,*cp2;
int len,len2,i;
char *strcaseindex();

/* The format will be VMS at input, so we have to scan for the
 * VMS device separator ':', and also the VMS directory separators
 * '[' and ']'.
 * If the name ends with '.dir;1' then it is actually a directory name.
 * The outputs dire and  fname will be a complete file spec, based on the
 * default directory.
 * It may be a rooted directory, in which case there will be a "][" string,
 * remove it.
 * Strip out colons from the right, in case there is a node name (should
 * not be!) If the filename ends in a trailing '.', suppress it , unless the
 * "d" option is set.
 */


    strcpy(temp,strrchr(line,':')+1);           /* Start with the whole name */

/* If relative path, get rid of default directory part of the name  */

    if (absolute == 0)
    {
        strcpy(dire,"./");
        for(cp1 = temp ,cp2 = curdir; *cp2 && (toupper(*cp1) == toupper(*cp2));
            cp1++, cp2++);
        if(*cp2 == 0)
            *cp1 = 0;              /* Perfect match, no directory spec  */
        else
        {
            switch(*cp1)
            {
            case '.':
            case '[':               /* Legal beginnings or ends  */
                break;
            default:            /* We are above the default, use full name */
                cp1 = strchr(temp,'[');    /* Fixed this from 1.5  */
                *dire = '\0';
                break;
            }
            cp1++;                 /* Get past the starting . or [ */
        }
        strcat(dire, cp1);
    }
    else
        strcpy(dire, temp + 1);

    cp1 = dire - 1;                /* change trailing directory mark */
    while ((cp1 = strchr(cp1 + 1, ']')) != NULL)
        if (*(cp1 - 1) != '^') {
            *cp1++ = '/';
            *cp1 = '\0';           /* and terminate string (no file name) */
            break;
            }

    cp1 = line - 1;
    while ((cp1 = strchr(cp1 + 1, ']')) != NULL)
        if (*(cp1 - 1) != '^') 
            break;

    strcpy(temp,cp1+1);
    if((cp1=strcaseindex(temp,".DIR;1"))!=0)
    {
        *cp1++ = '/';                          /* Terminate directory name */
        *cp1 = '\0';
        strcat(dire,temp);
        *fname = '\0';
    }
    else
    {
        strcpy(fname,temp);
        *strchr(fname,';') = '\0';;           /* no version numbers */
        if (acp_type != DVI$C_ACP_F11V5)
            lowercase(fname);                 /* map to lowercase */
        else
            for (cp1 = fname; *cp1;)
                if (*(cp1++) == '^' && *cp1 != '^')
                    strcpy(cp1 - 1, cp1);     /* remove EFS/ODS5 escape chars */
    }

    /* Now rewrite the directory name  */

   if (acp_type != DVI$C_ACP_F11V5)
       lowercase(dire);

    for (cp1 = dire + 1; *cp1; ++cp1)         /* Change '.' to '/'  */
        if(*cp1 == '.' && *(cp1 - 1) != '^')
            *cp1 = '/';
    while ((cp1 = strchr(dire, '^')) != NULL)
        strcpy(cp1, cp1 + 1);                 /* remove EFS/ODS5 escape chars */
    if((len=strlen(fname))==0)
        return(ISDIRE);
    else
        if(fname[--len] == '.')
           if (dot == 0)
                fname[len] = 0; /* No trailing dots  */
        return(ISFILE);
}
/* initsearch - parse input file name */
/* To start looking for file names to satisfy the requested input,
 * use the sys$parse routine to create a wild-card name block. When
 * it returns, we can then use the resultant FAB and NAM blocks on
 * successive calls to sys$search() until there are no more files
 * that match.
 */

struct FAB fblock;      /* File attribute block */
struct NAM nblock;      /* Name attribute block for rms */

int initsearch(string)
char string[];
{
int status;
char *strcaseindex();
static char searchname[NAMSIZE];
static char default_name[] = DEFAULT_NAME;

    if(strcaseindex(string,"::")!=NULL)
    {
        printf("tar: DECnet file access is not supported.\n");
        return(-1);
    }
    fblock = cc$rms_fab;
    nblock = cc$rms_nam;
    fblock.fab$l_dna = default_name;
    fblock.fab$b_dns = strlen(default_name);
    fblock.fab$l_fna = string;
    fblock.fab$b_fns = strlen(string);
    fblock.fab$l_nam = &nblock;
    nblock.nam$l_esa = searchname;
    nblock.nam$b_ess = sizeof(searchname);
#ifdef debug
    printf("searching on: %s\n",string);
#endif
    status = sys$parse(&fblock);
    if(status != RMS$_NORMAL)
    {
        if(status == RMS$_DNF)
            printf("tar: directory not found %s\n",searchname);
        else
            printf("tar: error in SYS$PARSE.\n");
        return (-1);
    }
    searchname[nblock.nam$b_esl] = 0;   /* Terminate the string  */

    /* Now reset for searching, pointing to the parsed name block */

    fblock = cc$rms_fab;
    fblock.fab$l_nam = &nblock;
    return(nblock.nam$b_esl); /* return the length of the string  */
}

/* search - get the next file name that matches the namblock */
/* that was set up by the sys$search() function. */

int search(buff,maxlen)
char buff[];
int maxlen;
{
int status;

    nblock.nam$l_rsa = buff;
    nblock.nam$b_rss = maxlen;
    nblock.nam$b_rsl = 0;       /* For next time around  */
    while ((status = sys$search(&fblock)) != RMS$_NMF)
    {
        buff[nblock.nam$b_rsl] = 0;
        if(status == RMS$_NORMAL)
            return(nblock.nam$b_rsl);
        else
        {
            if (status == RMS$_PRV)
                printf("tar: no privilege to access %s\n",buff);
            else if (status == RMS$_FNF)
                printf("tar: file not found %s\n",buff);
            else
            {
                printf("tar: error in SYS$SEARCH for %s\n", buff);
                return (0);
            }
        }
    }
    return (0);
}
/* fill_header - fill the fields of the header */
/* enter with the file name, if the file name is empty, */
/* then this is a trailer, and we should fill it with zeroes. */

int fill_header(name)
char name[];
{
int i,chksum;
int zero = 0, hdrsize = RECSIZE;
char *ptr,tem[15];

/* Fill the header with zeroes */

    lib$movc5(&zero, &zero, &zero, &hdrsize, &header);  
    if(strlen(name)!=0)         /* only fill if there is a file */
    {
        sprintf(header.title,"%s",name);        /* write file name */
        sprintf(header.protection,"%6o ",mode); /* all written with */
        sprintf(header.uid,"%6o ",uid);         /* a trailing space */
        sprintf(header.gid,"%6o ",gid);
        sprintf(tem,"%11o ",bytecount);         /* except the count */
        strncpy(header.count,tem,12);           /* and the time, which */
        sprintf(tem,"%11o ",vmstime);           /* except the count */
        strncpy(header.time,tem,12);            /* have no null */
        strncpy(header.chksum,"        ",8);    /* all blanks for sum*/

/* I know that the next two are already zero, but do them */

        header.linkcount = 0;                   /* always zero */
        sprintf(header.linkname,"%s","");       /* always blank */
        for(chksum=0, ptr = (char *)&header;ptr < (char *)&header.linkcount;ptr++)
                 chksum += *ptr;                /* make the checksum */
        sprintf(header.chksum,"%6o",chksum);
    }
    return(0);
}

/* strcaseindex - search for string2 in string1, return address pointer */

char *strcaseindex(string1,string2)
char *string1,*string2;
{
char *c1, *c2, *cp;
    for(c1 = string1; *c1 !=0; c1++)
    {
        cp = c1;        /* save the start address */
        for(c2=string2; *c2 !=0 && toupper(*c1) == toupper(*c2); c1++,c2++);
           if(*c2 == 0)
                return(cp);
    }
    return(NULL);
}

/* lowercase - function to change a string to lower case */

int lowercase(string)
char string[];
{
int i;
        for(i=0;string[i]=tolower(string[i]);i++);
        return (--i);           /* return string length */
}

/* cleanup_dire - routine to get rid of rooted directory problems */
/* and any others that turn up */

int cleanup_dire(string)
    char string[];
{
    char *ptr;

    /* We need to make sure the directory delimiters are square brackets,
       otherwise we'll get some problems... -- Richard Levitte */
    while ((ptr = strchr(string,'<')) != 0)
	*ptr = '[';
    while ((ptr = strchr(string,'>')) != 0)
	*ptr = ']';
    
    if((ptr=strcaseindex(string,"][")) != NULL)
    {       /* Just collapse around the string */
	strcpy(ptr,ptr+2);
	return(1);
    }
    if((ptr=strcaseindex(string,"[000000.")) != NULL)
    {         /* remove "000000." */
	strcpy(ptr + 1, ptr + 8);
	return(1);
    }
    return(0);
}

#ifdef EXPERIMENT
int outfile_rms(filename,nbytes,fdout,check_text)
char filename[];
int fdout, nbytes;
int check_text;
{
    int n, rms_status ;
    struct FAB myfab ;
    struct XABFHC myxab;
    struct RAB myrab ;
    unsigned short int b_size ;
    char *UserBuffer, *TransBuffer ;
    int BytesRead, TransBufSize, i, TotalBytes;

#if 0
    BytesRead = TotalBytes = 0 ;
    b_size = vmsmrs ;
    UserBuffer = (char*) malloc(b_size * sizeof(char)) ;
    TransBuffer = (char*) malloc((b_size+512) * sizeof(char)) ;
#endif

    myfab = cc$rms_fab ;
    myfab.fab$l_fna = filename ;
    myfab.fab$b_fns = strlen(filename) ;
    myfab.fab$b_rat = vmsrat ;
    myfab.fab$w_mrs = vmsmrs ;
    myfab.fab$b_org = FAB$C_SEQ ;
    myfab.fab$b_rfm = vmsrfm ;

    myxab = cc$rms_xabfhc;
    myfab.fab$l_xab = (char*)&myxab;

    rms_status = sys$open(&myfab) ;
    if (rms_status & 1)
    {
#if 1
	BytesRead = TotalBytes = 0 ;
	b_size = myxab.xab$w_lrl ;
	UserBuffer = (char*) malloc(b_size * sizeof(char)) ;
	TransBuffer = (char*) malloc((b_size+512) * sizeof(char)) ;
#endif
        myrab = cc$rms_rab ;
        myrab.rab$w_usz = b_size ;
#if 0
        if (b_size > 512)
#endif
            myrab.rab$l_ubf = UserBuffer ;
#if 0
        else
            myrab.rab$l_ubf = buffer ;
#endif
        myrab.rab$l_fab = &myfab ;
        rms_status = sys$connect(&myrab) ; 
    }
    if ( !(rms_status & 1) )
    {
        printf(" Unable to define RMS structures ...\n") ;
        printf(" Skipping file %s \n", filename) ;
        return (-1) ;
    }

    fill_header(pathname);          /* We have all of the information */
    write_header(fdout);            /* So write to the output */

#ifdef debug
    printf(" Starting to transter %d bytes\n", nbytes) ;
#endif
#if 0
    if (b_size > 512)
    {
#endif
        /* 
        ** OK, this is the tricky case, and output to tar file
        ** will need to be buffered/divided to fit 512 blocks.
        */
        TransBufSize = 0 ;
        rms_status = sys$get(&myrab) ;
        while( rms_status != RMS$_EOF && rms_status == RMS$_NORMAL)
        {
            BytesRead = myrab.rab$w_rsz ;
	    if (check_text)
		if (UserBuffer[BytesRead-2] == '\r'
		    && UserBuffer[BytesRead-1] == '\n')
		{
		    UserBuffer[(--BytesRead)-1] = '\n';
		}
            nbytes -= BytesRead ;
            memcpy( (TransBuffer + TransBufSize), UserBuffer, BytesRead) ;
            TransBufSize += BytesRead ;
            if (TransBufSize >= 512)
            {
                /*
                ** OK, time to flush out the Transfer Buffer
                */
                i = 0 ;
                while (TransBufSize >= 512)
                {
                    memcpy(buffer, (TransBuffer + i), RECSIZE) ;
                    flushout(fdout) ;
                    i += RECSIZE ;
                    TransBufSize -= RECSIZE ;
                }
                memcpy(TransBuffer, (TransBuffer + i), TransBufSize) ;
            }
            rms_status = sys$get(&myrab) ;
        }
        if (TransBufSize != 0 )
        {
            /* 
            ** Flush the buffer one last time 
            */
            memcpy(buffer, EmptyTrailer, RECSIZE) ;
            memcpy(buffer, TransBuffer, TransBufSize) ;
            flushout(fdout) ;
        }
#if 0				/* This part should NOT be needed */
    }
    else
    {
        /* 
        ** This is the simple case of PDS files where max. record length
        ** is 512, so no splitting is required. 
        ** Infact even with this case some records could be less than 
        ** 512 bytes, and so the output to tar file will need to be 
        ** "buffered" - i.e. it will be safe to use the previous option
        ** for all cases.
        */
        rms_status = sys$get(&myrab) ;
        while( rms_status != RMS$_EOF && rms_status == RMS$_NORMAL)
        {
            BytesRead = myrab.rab$w_rsz ;
            nbytes -= BytesRead ;
            flushout(fdout); 
            rms_status = sys$get(&myrab) ;
        }
    }
#endif
    sys$close(&myfab) ;
    return(0);
}


int
get_varfilesize(filename,check_text)
char *filename;
int check_text;			/* Boolean: Check if text file on true. */
{
int n, rms_status ;
struct FAB myfab ;
struct XABFHC myxab;
struct RAB myrab ;
unsigned short int b_size ;
char *UserBuffer ;
int BytesRead, TotalBytes;

#if 0
    BytesRead = TotalBytes = 0 ;
    b_size = vmsmrs ;
    UserBuffer = (char*) malloc(b_size * sizeof(char)) ;
#endif

    myfab = cc$rms_fab ;
    myfab.fab$l_fna = filename ;
    myfab.fab$b_fns = strlen(filename) ;
    myfab.fab$b_rat = vmsrat ;
    myfab.fab$w_mrs = vmsmrs ;
    myfab.fab$b_org = FAB$C_SEQ ;
    myfab.fab$b_rfm = vmsrfm ;

    myxab = cc$rms_xabfhc;
    myfab.fab$l_xab = (char*)&myxab;

    rms_status = sys$open(&myfab) ;
    if (rms_status & 1)
    {
#if 1
	BytesRead = TotalBytes = 0 ;
	b_size = myxab.xab$w_lrl ;
	UserBuffer = (char*) malloc(b_size * sizeof(char)) ;
#endif
        myrab = cc$rms_rab ;
        myrab.rab$w_usz = b_size ;
        myrab.rab$l_ubf = UserBuffer ;
        myrab.rab$l_fab = &myfab ;
        rms_status = sys$connect(&myrab) ; 
    }
    if ( !(rms_status & 1) )
    {
        printf(" Unable to define RMS structures ...\n") ;
        return (-1) ;
    }

    rms_status = sys$get(&myrab) ;
    if (rms_status != RMS$_NORMAL)
    {
	sys$close(&myfab);
	funnyfile = -1;
	return (0);
    }
    while( rms_status != RMS$_EOF && rms_status == RMS$_NORMAL)
    {
        BytesRead = myrab.rab$w_rsz ;
	if (check_text)
	    if (UserBuffer[BytesRead-2] != '\r'
		|| UserBuffer[BytesRead-1] != '\n')
	    {
		sys$close(&myfab);
		funnyfile = -1;
		return (0);
	    }
	    else
		BytesRead--; /* The \r will be thrown out the window */

        TotalBytes += BytesRead ;
        rms_status = sys$get(&myrab) ;
    }
    sys$close(&myfab) ;
    return (TotalBytes) ;
}
#endif /* EXPERIMENT */
