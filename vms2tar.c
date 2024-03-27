#define module_name	VMS2TAR
#define module_version  "V2.0"
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

#include <unixio.h>
#include <unixlib.h>
#include <ssdef.h>
#include <rms.h>
#include <starlet.h>
#include <lib$routines.h>

#include "vmstar_cmdline.h"
#include "vmstarP.h"

/* Globals in this module */

static int mode, uid, gid;    /* current values used by create */
static int bufferpointer, bytecount;

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

/* vms2tar -- handles create function */

vms2tar(argc,argv)
int argc;
char **argv;
{
    int argi, process, file_type, absolute;
    char string[NAMSIZE], *cp, *dp, *wp;

    bufferpointer = 0;
    gid = getgid();
    uid = getuid();
    mode = 0644;

/* Open the output file */

    outfd = creat(tarfile,0600,"rfm=fix","mrs=512");
    if (outfd < 0)
    {
        printf("tar: error opening output tarfile %s\n", tarfile);
        exit(SS$_NORMAL);
    }  

    for (argi = 0; argi < argc; ++argi)
    {
        strcpy(string, argv[argi]);
        absolute = 0;
        cp = strchr(string, ':');   /* device name in argument ? */
        if (cp != NULL)            /* yes, force absolute mode */
            absolute = 1;
        else
        {
            cp = strchr(string, '[');  /* test argument for absolute... */
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
          while(search(newfile,NAMSIZE)!=0)
          {
            strcpy(linkname, newfile);
            cleanup_dire(newfile);
            file_type = scan_name(newfile,new_directory,outfile,absolute);
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
                if (bytecount < 0 && file_type == ISFILE)
		{
                    printf("*** skipped  %s\n", linkname);
		    printf("*** unsupported format\n");
		}
                if (verbose && (bytecount >=0 || file_type == ISDIRE))
                {
                     strcpy(creation,ctime(&vmstime)); /* work on this */
                     creation[24]=0;
                     printf("%s %8d %s\n",creation + 4,bytecount,pathname);
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
    if(bytecount < 0)          /* we don't output unsupported files  */
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
* We set the bytecount=-1 if this is funny file.
*/

int out_file(filename,nbytes,fdout)
char filename[];
int fdout, nbytes;
{
int i, n, pos;
char *bufp, *linep;
FILE *filein;
static char dbuffer[RECSIZE];

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

#if 0 /* Unfortunatelly, it won't work, because I seem to get RECSIZE
         byte blocks, and I can't find a way to get just one record.
	 Thus, I can't reliably find out if the file is text or not.
	 Pity.  -- Richard Levitte */
	    if((filein = fopen(filename,"rb")) == NULL)
	    {
                printf("tar: error opening input file %s\n",filename);
                return(-1);
            }
	    if((nbytes = fread(dbuffer,1,RECSIZE,filein)) != 0)
	    {
		if(dbuffer[nbytes-2] != '\r' || dbuffer[nbytes-1] != '\n')
		{
		    bytecount = -1;
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
		    bytecount = -1;
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
	    rewind(filein);
#else
	    bytecount = -1;
	    return(0);
#endif
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

    if(stat(fname,&sblock))
    {
        printf("tar: can't get file status on %s\n",fname);
        vmstime = 0;                    /* Prevent garbage printoout */
        bytecount = -1;                 /* of inaccessible files  */
        return(-1);
    }

/* now get the file attributes, we don't use them all */
    bytecount = sblock.st_size;
    vmsrat = sblock.st_fab_rat;
    vmsmrs = sblock.st_fab_mrs;
    vmsrfm = sblock.st_fab_rfm;
    vmstime = sblock.st_mtime;
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
char *strindex();

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
        for(cp1 = temp ,cp2 = curdir; *cp2 && (*cp1 == *cp2);
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
    cp1 = strrchr(dire, ']');       /* change trailing directory mark */
    if (cp1 != NULL)
    {
        *cp1++ = '/';
        *cp1 = '\0';                /* and terminate string (no file name) */
    }
    strcpy(temp,strrchr(line,']')+1); /* Now get the file name */
    if((cp1=strindex(temp,".DIR;1"))!=0)
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
        lowercase(fname);                     /* map to lowercase */
    }

    /* Now rewrite the directory name  */

   lowercase(dire);

    for (cp1 = dire + 1; *cp1; ++cp1)         /* Change '.' to '/'  */
        if(*cp1 == '.')
            *cp1 = '/';
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
char *strindex();
static char searchname[NAMSIZE];
static char default_name[] = DEFAULT_NAME;

    if(strindex(string,"::")!=NULL)
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

/* strindex - search for string2 in string1, return address pointer */

char *strindex(string1,string2)
char *string1,*string2;
{
char *c1, *c2, *cp;
    for(c1 = string1; *c1 !=0; c1++)
    {
        cp = c1;        /* save the start address */
        for(c2=string2; *c2 !=0 && *c1 == *c2; c1++,c2++);
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
        if((ptr=strindex(string,"][")) != NULL)
        {       /* Just collapse around the string */
          strcpy(ptr,ptr+2);
          return(1);
        }
        if((ptr=strindex(string,"[000000.")) != NULL)
        {         /* remove "000000." */
            strcpy(ptr + 1, ptr + 8);
            return(1);
        }
        return(0);
}

