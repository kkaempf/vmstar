#include <stdio.h>
#include <types.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <rms.h>
#include <ssdef.h>
#include <descrip.h>

char* memset();

/* utime(path,times) sets the access and modification times of the
   file 'path' to the Unix binary time values, 'times'.	 Return 0
   on success, and -1 on error (setting errno as well). */

utime(path,times)		/* VAX VMS C version */
char* path;
time_t times[2];
{
    int status;
    struct dsc$descriptor_s time_desc;
    char *ftime = "23-OCT-1907 12:34:56";
    struct tm *timeval;
    static char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN",
			     "JUL","AUG","SEP","OCT","NOV","DEC"};
    struct FAB fab1;
    struct XABRDT xab1;

    /* Zero FAB and XAB structures */
    (void)memset(&fab1,'\0',sizeof(fab1));
    (void)memset(&xab1,'\0',sizeof(xab1));

    /* Convert Unix binary time to ASCII string for
       sys$bintime().  We use localtime() instead of ctime(),
       because although ctime() is simpler, it drops the seconds
       field, which we would rather preserve.  */

    timeval = (struct tm*)localtime(&times[0]);
    sprintf(ftime,"%02d-%3s-19%02d %02d:%02d:%02d",
	timeval->tm_mday,
	months[timeval->tm_mon],
	timeval->tm_year,
	timeval->tm_hour,
	timeval->tm_min,
	timeval->tm_sec);

    /* Setup fab1 and rab fields. */
    fab1.fab$b_bid = FAB$C_BID;
    fab1.fab$b_bln = FAB$C_BLN;
    fab1.fab$b_fac = FAB$M_UPD;
    fab1.fab$l_fna = path;
    fab1.fab$b_fns = strlen(path);
    fab1.fab$l_xab = &xab1;

    xab1.xab$b_bln = XAB$C_RDTLEN;
    xab1.xab$b_cod = XAB$C_RDT;
    xab1.xab$l_nxt = (char*)NULL;

    /* Open the file */
    status = sys$open(&fab1);
     if ( status != RMS$_NORMAL)
    {
      lib$signal(status);
      errno = ENOENT;
      return (-1);
    }

    /* Convert the time string to a VMS binary time value in the XAB */
    time_desc.dsc$w_length = strlen(ftime);
    time_desc.dsc$a_pointer = ftime;
    time_desc.dsc$b_class = DSC$K_CLASS_S;
    time_desc.dsc$b_dtype = DSC$K_DTYPE_T;
    status = sys$bintim(&time_desc,&xab1.xab$q_rdt);
    if (status != SS$_NORMAL)
    {
      lib$signal(status);
      status = sys$close(&fab1);
      errno = EFAULT;
      return (-1);
    }

    /* Close the file, updating the revision date/time value */
    status = sys$close(&fab1);
    if (status != RMS$_NORMAL)
    {
      lib$signal(status);
      errno = EACCES;
      return (-1);
    }
    return (0);
}

char*
memset(s,c,n)			/* set n bytes of s[] to c */
char* s;			/* and return s */
int c,n;
{
    char* p;

    p = s;
    for (; n > 0; --n)
	*p++ = (char)c;
    return (s);
}
