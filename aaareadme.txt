*******
VMSTAR V3.2-3   Includes modifications by:
		Hunter Goatley <goathunter@WKUVX1.WKU.EDU>
		Jamie Hanrahan <jeh@cmkrnl.com>
		Richard Levitte <levitte@e.kth.se>
*******

General information
-------------------

VMSTAR  is  a  TAR  reader/writer  for  VMS.  It can read archives ("tarfiles")
created  by  the Un*x command "tar" and also create such archives. Tarfiles can
be disk files or directly on tape.

This version supports both VMS-style and UNIX-style command lines.

VMSTAR is based on the TAR2VMS and VMS2TAR programs written by:

phone (613) 545-2925

BITNET:         PENSTONE@QUCDNEE1       (Preferred)
                PENSTONE@QUCDN  (If the other doesn't work)

The extra work has been done by:

Alain Fauconnet
SIM/INSERM U194         (complete address at the end of this file)
PARIS - FRANCE

Bitnet: FAUCONNE@FRSIM51

TAR2VMS and VMS2TAR have been merged into a  single  program.  I  made  several
improvements, bug fixes and message cleanup. For those  who  know  TAR2VMS  and
VMS2TAR, the main differences are:

- everything is now in a single program that can be used for extracting files
from tar archives, listing the contents of tar archives or creating them.

- VMSTAR now accepts a `f tarfile' option to explicitely specify the tarfile
name (either a VMS file name or a VMS device name).

- if this option is not used, the logical name "$TAPE" is translated.

- checksums are verified at file extraction.

- VMSTAR will extract files from archives as VMS rfm=stream_lf, rat=cr files,
except if new option `b' is specified. In this case, extracted files ared
created as rfm=fixed, mrs=512, rat=none i.e. suitable for compressed files to
be decompressed using LZDCMP or for VMS images.

- VMSTAR has a more Un*x-like syntax, if several file names are specified as
command line parameters they must be separated by spaces (not commas) and there
is not context propagation "a la BACKUP".

- VMSTAR allows VMS-style wildcarded strings for Un*x-style file  names  to  be
specified when extracting from a tar archive, e.g. :

$ tar xvf foo.tar */source/*/sa%%%.c

- VMSTAR will attempt to create relative tar archives i.e. archives where
filenames are recorded as "./foo/bar/baz" whenever possible. This can be
specifically avoided by having a device name in file name argument, e.g. :

$ tar cvf foo.tar DISK$USERS[...]*.c

or specifying an absolute VMS file specification, e.g. :

$ tar cvf foo.tar [SMITH.C...]

- VMSTAR will handle tar archives which when restored would create more than 8
levels of directories (the X11 distribution from MIT for instance !). Excessive
levels of directories will be resolved as follows:

d1/d2/d3/d4/d5/d6/d7/d8/d9/foo -> [D1.D2.D3.D4.D5.D6.D7.D8$D9]FOO

- VMSTAR no longer requires the creation of an intermediate scratch file when
archiving text files as VMS2TAR did.

- VMSTAR does *not* allow to read tarfiles past the EOF mark as TAR2VMS did.

- the `w' option (same as "/CONFIRM" for VMS commands) has been implemented for
create archive and extract functions.

- VMSTAR has a VMS help file (VMSTAR.HLP) that can be added to your HELPLIB.HLB
to provide online help.

- normally, VMSTAR will convert dots in the Un*x directory specifications
to underscores, like this:

emacs-19.22/src/buffer.c -> [EMACS-19_22.SRC]BUFFER.C

If you want to suppress this feature, use the option `d'.

- VMSTAR now comes with a DCL interface as well.  It recognises quite well
if you're using the Un*x interface, or the DCL interface.

- many other differences, the code has been extensively reworked with
simplification as a goal. This probably caused the introduction of some bugs...


Build instructions
------------------

Compile and link VMSTAR.C as follows:

$ @BUILD

or if you have MMS (the free clone MMK should work as well):

$ MMS/IGNORE=WARNING

Installation instructions
-------------------------

When you're done building, define a foreign command symbol in SYLOGIN.COM:

$ VMSTAR :== $ <wherever>VMSTAR

I used VAX/VMS C V3.1 to build VMSTAR. I have no idea whether it can be built
using other versions or other compilers...

You can optionally add VMSTAR in your VMS help library as follows:

$ LIBRARY/HELP/INSERT SYS$COMMON:[SYSHLP]HELPLIB.HLB VMSTAR.HLP

        Usage
        -----

        tar h|x|t|c[v][w][b][d][f tarfile] [file [file...]]
	h - print a description and then exit.
        x - extract from tarfile, create VMS files
        t - type directory of tarfile
        c - create tarfile, archive VMS files
        v - verbose (list names of files being archived/extracted)
        w - wait for confirmation before extracting/archiving
        b - binary mode extract, create (rfm=fixed, rat=none, mrs=512) files
        d - keep trailing dots in file names
        f - specify tarfile name, default is $TAPE
        file - space-separated list of file names, can include VMS-style
               string wildcards on extract, can be any VMS file name
               specification (except DECnet) on create archive.


Tapes for reading/writing of tarfiles should be mounted
/FOREIGN/RECORD=512/BLOCK=10240

Example:

$ MOUNT/FOREIGN/RECORD=512/BLOCK=10240 MUA0: "" $TAPE
$ VMSTAR/EXTRACT/VERBOSE MUA0:

NOTE:  VMSTAR is only guaranteed to work with TU81 and similar tape devices.
VMSTAR has been shown to work on DEC TLZ04 (DAT) and some odd DAT player
from Hewlett Packard.

Restrictions
------------

Because of diffrences in the Un*x and VMS filesystems, some files may  fail  to
be correctly  transferred  to/from  the  tarfile.  This  can  be  caused  by  :

- restrictions in VMS file naming scheme: extra dots in file names will be
mapped to underscores, dummy directory names will be generated if archive
contains more than 8 levels of subdirectories, links are extracted as empty
files containing only a short message "this file is only a link to...", all
file names are mapped to uppercase etc.

- restrictions of the Un*x filesystem: tar will only get the latest version of
a VMS file to enter it into the archive, no trace of the orginal file device
name is kept in the archive.

- VMS strong file typing: VMSTAR can only safely tranfer back and forth 
VMS "text" files (rfm=vfc or stream_lf with rat=cr) or VMS fixed size record,
512 bytes/record, rat=none files (e.g. .EXE image files).
VMSTAR will skip other file types (this includes .OBJ, they *can't* be
archived.  Library files may work, but be cautious with them).

Other restrictions:

RMS file attributes are lost in the archive process, text files are archived as
<record><LF><record><LF>, fixed files are archived as is.

VMSTAR  will always restore files relative to your current RMS default if names
in  tarfile do not begin by `/'. If file names in tarfile begin with a `/' (bad
practice), an attempt will be made to restore files to the absolute path. There
is currently no way to explicitely specify the target VMS directory where files
should be extracted.

No  attempt  has  been  made to handle search list as RMS defaults (e.g. SYSTEM
account). Be very careful about that.

The  current  version  of  VMSTAR  has  *not*  been  fully  tested.  I probably
introduced  many  bugs  not  existing  in  Sid  Penstone's  programs. VMSTAR is
provided "as-is", I cannot guarantee it will do what you want or even what it's
supposed  to  do  but  I'd  like  to hear about it if you have problems. If you
report  a  problem,  don't  bother  with  providing me a fix but *do* try to be
precise on what happened and how it happened.

	BUGS
	----

No known bugs.

+-------------------------------------------------------------------------+
|  Alain ("HAL 1") Fauconnet   Research laboratory in medical informatics |
|  System Manager              (expert systems, NL proc., statistics...)  |
|  SIM/INSERM U194             EARN/Bitnet:       FAUCONNE@FRSIM51        |
|  Faculte de Medecine         VMS PSI Mail: PSI%+208075090517::FAUCONNET |
|  91 Boulevard de l'Hopital   FAX:          (+33) 1-45-86-56-85          |
|  75634 PARIS CEDEX 13 FRANCE PTT net:      (+33) 1-45-85-15-29          |
|  "HAL... open the door, please!" (2001 Space Odyssey)                   |
|  Disclaimer: This is machine-generated random text, no meaning at all.  |
+-------------------------------------------------------------------------+
!++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++!
! Richard Levitte, GNU on VMS hacker   ! tel: int+46-8-83 12 15            !
! Södra Långgatan 39, II               ! fax: none for the moment          !
! S-171 49  SOlna                      ! Internet: levitte@e.kth.se        !
! SWEDEN                               !                                   !
!--------------------------------------------------------------------------!
