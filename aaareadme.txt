            VMSTAR V4.4, 2022-06-28
            =======================

------------------------------------------------------------------------

      Description
      -----------

   VMSTAR is a "tar" archive reader/writer for VMS.  VMSTAR can work with
"tar" archives on disk or tape.  VMSTAR is intended primarily for data
interchange with UNIX(-like) systems, where "tar" programs are more
commonly used.  It can be expected to have trouble with VMS files which
do not have UNIX-like record formats (Stream_LF, fixed-512), so it's not
a good replacement for more RMS-aware programs like BACKUP or Info-ZIP
Zip and UnZip.

   When built with large-file support enabled (the default on reasonably
modern, non-VAX systems), VMSTAR should be able to extract large files
(>2GB or >8GB) from archives which were created using Solaris "tar -E"
or GNU "tar".  Similarly, long archive path names (>100 characters)
should be handled properly if the VMS file system can deal with them.
VMSTAR uses GNU-"tar"-style archive format extensions when it creates an
archive containing large files or long names.

------------------------------------------------------------------------

      History
      -------

   VMSTAR is based on the TAR2VMS and VMS2TAR programs written (long
ago) by Sid Penstone.  The following people are believed to have made
changes to the product in the many years since:

      Alain Fauconnet
      Patrick Young <P.Young@unsw.EDU.AU>
      Richard Levitte <levitte@lp.se>
      Jamie Hanrahan <jeh@cmkrnl.com>
      Hunter Goatley <goathunter@WKUVX1.WKU.EDU>
      Steven Schweda <sms@antinode.info>

   For details, see CHANGELOG.TXT.  Problem reports from users have been
helpful, and are appreciated.

------------------------------------------------------------------------

      Usage Basics
      ------------

   VMSTAR offers both UNIX-style and VMS-style command-line interfaces.
Running VMSTAR with no options or arguments provides brief "Usage"
notes:

$ vmstar
VMSTAR V4.4 (Jun 22 2022) (+large-file)
usage (UNIX-style): vmstar -[h|c|t|x][BbDdFfopsvwz] [params ...] [file [...]]
usage (VMS-style):  VMSTAR [options] tarfile [file [, file [...]]]
 Options (UNIX-style, VMS-style):
 h        /HELP           Print this text and exit.  (Other options ignored.)
 c        /CREATE         Create a tarfile.
 t        /LIST           List the contents of a tarfile.
 x        /EXTRACT        Extract files from tarfile.

 B        /BINARY         Extract files as binary (fixed-512).
 b b_f    /BLOCK_FACTOR=b_f  Number of 512-byte records in a tar block.
 D        /DATE_POLICY =  Specify which times of extracted files to set.
  c|C|m|M  ([NO]CREATION, [NO]MODIFICATION)  (Default: CREAT, MODIF.)
  A|n      ALL, NONE      ALL: (CREAT, MODIF), NONE: (NOCREAT, NOMODIF).
  (UNIX-style: "c" = NOCREAT, "C" = CREAT, "m" = NOMODIF, "M" = MODIF,
               "A" = ALL, "n" = NONE.)
 d        /DOTS           Archived names retain a trailing dot.
                          (Default: Trim trailing dots: "fred." -> "fred".)
                          Extract literal dots ("^.") in directory name (ODS5).
                          (Default: Convert dots in directory name to "_".)
 F        /FORCE          Forces archiving of unsupported file formats.
 f t_f    (first arg)     Specify a tar file (disk or tape).  Default: $TAPE
 o        /ODS2           Archive down-cased names, even on ODS-5 volumes.
                          Extract using ODS-2 naming, even on ODS-5 volumes.
 p        /NOPADDING      Inhibits padding the last block of the tarfile.
 s        /SYMLINKS       Extract archived symlinks as real symlinks.
 u        /UNDERDOT       "a.b.c" -> ODS2 "A_B.C".  (Default: "A.B_C").
 v        /VERBOSE        Display processed file info.  (Alone: Show version.)
 w        /CONFIRM        Prompt for confirmation before archive/extract.
 z        /AUTOMATIC      Automatically determine file type.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Usage Details
      -------------

   Command-line Syntax
   -------------------

   Either a UNIX-style or a VMS-style command line may be used.  To
specify a list of files, when using a UNIX-style command line, use a
space-separated file list.  When using a VMS-style command line, use a
comma-separated file list.  For example:

      vmstar -cf [-]hc.tar *.h *.c
      vmstar /create [-]hc.tar *.h, *.c

   With a UNIX-style command line, if no tarfile is specified ("f"),
then the logical name "$TAPE" is used.  With a VMS-style command line,
the first argument specifies the tarfile, but if there are no arguments,
then the logical name "$TAPE" is used.  So, for example, the following
commands are equivalent:

      vmstar -tfv $TAPE
      vmstar -tv
      vmstar /list /verbose $TAPE
      vmstar /list /verbose

   Unlike a normal UNIX "tar" program, when creating an archive, VMSTAR
does not automatically descend through a directory tree.  Use the usual
VMS "[...]" wildcard syntax to get this kind of behavior.  For example,
instead of:

      tar cf ../all.tar .

Use:

      vmstar -cf [-]all.tar [...]
      vmstar /create [-]all.tar [...]

(When creating an archive, VMSTAR uses a default file specification of
"*.*;" for input files, so "[...]" here is equivalent to "[...]*.*;".)

   When creating an archive, VMSTAR will use relative path names
(without a leading slash), or absolute path names (with a leading
slash), depending on the form of the input file specifications.  Using a
relative VMS directory specification will result in relative paths in
the archive. Using an absolute VMS directory specification in the file
specification will result in absolute paths in the archive.  For
example:

      $ vmstar -cfv alphal_rel.tar [.alphal].exe        ! Relative.
      Oct 28 23:04:13 2010    77312 ALPHAL/VMSTAR.EXE

      $ vmstar -cfv html_abs.tar [html]robots.txt       ! Absolute.
      Nov 19 13:52:11 2001      914 /html/robots.txt

   A directory specification is considered absolute if it includes a
device specification ("xxx:"), or if the directory part of the
specification does not begin with "[-", "[.", or "[]".  (So, explicitly
specifying any device, even "SYS$DISK:", makes any directory spec an
absolute one.)

   A rooted logical name can be used to get absolute path names into an
archive without including the whole chain of real directory names (like
using "chroot" on a UNIX system).  For example:

      $ here_root = f$environment( "default") - "]"+ ".]"
      $ define /trans = conc here_root 'here_root'
      $ set default here_root:[000000]
      $ vmstar -cfv alphal_abs.tar [alphal].exe        	! Absolute.
      Oct 28 23:04:13 2010    77312 /ALPHAL/VMSTAR.EXE

   Using absolute paths in a "tar" archive is generally unwise, because
some "tar" implementations (including VMSTAR) can not easily extract a
file with an absolute path to anywhere other than where that absolute
path specifies.

   When extracting files from an archive, VMSTAR will base a relative
path name (without a leading slash), at the current default device and
directory.  Absolute paths (those with a leading slash) are extracted
into the MFD ("[000000]") of the current default device.  For example:

      v4_0/aaareadme.txt  -> [.v4_0]aaareadme.txt
      /v4_0/aaareadme.txt -> [v4_0]aaareadme.txt

   As when creating an archive, a rooted logical name can be used to
deceive VMSTAR into extracting into some other directory when it thinks
that it's extracting into the MFD ("[000000]").

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   Attributes of Extracted Files
   -----------------------------

   By default, VMSTAR will extract files from archives with attributes
rfm:stream_lf, rat:cr.  If /BINARY ("B") is specified, then extracted
files are created as rfm:fixed, mrs:512, rat:none.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   Wildcards in Extracted File List
   --------------------------------

   VMSTAR allows VMS-style wildcards in UNIX-style file names to be
specified when extracting from an archive.  These user-specified
patterns are matched against the (UNIX-style) path names in the archive.
For example:

      vmstar -xvf foo.tar */source/*/sa%%%.c

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   Deep Directories
   ----------------

   VMSTAR will attempt to deal with tar archives which, when restored,
would create more levels of directories than the local file system
allows (8, on old VMS versions).  It does this by concatenating the
lowest directory names (with "$" separation), as needed.  For example:

      d1/d2/d3/d4/d5/d6/d7/d8/d9/foo -> [D1.D2.D3.D4.D5.D6.D7.D8$D9]FOO.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   Name Restrictions (ODS2)
   ------------------------

   When extracting to an ODS2 file system, VMSTAR will convert excess
dots in an archive path specification to underscores, as in the
following example.  (The default mode is "dot-under", preserving the
first dot in a file name, and replacing the others with underscores.)

      wget-1.12/src/config.h.in -> [.WGET-1_12.SRC]CONFIG.H_IN

Or, with /UNDERDOT ("u") (preserving the last dot in a file name):

      wget-1.12/src/config.h.in -> [.WGET-1_12.SRC]CONFIG_H.IN

   When extracting to an ODS5 file system, VMSTAR will, by default,
convert dots in directory names to underscores, but will preserve
multiple dots in file names:

      wget-1.12/src/config.h.in -> [.wget-1_12.src]config^.h.in

To preserve dots in directory names, too, specify /DOTS ("d"):

      wget-1.12/src/config.h.in -> [.wget-1^.12.src]config^.h.in

Specifying /ODS2 ("o") will force the ODS2 behavior even on an ODS5
volume:

      wget-1.12/src/config.h.in -> [.WGET-1_12.SRC]CONFIG.H_IN

or, with /UNDERDOT ("u"):

      wget-1.12/src/config.h.in -> [.WGET-1_12.SRC]CONFIG_H.IN

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

   Tape
   ----

   Nowadays, VMSTAR is typically used with "tar" archives on disk, but
it should work with real tape devices, too.  For the default
/BLOCK_FACTOR ("b") of 20, use these MOUNT options (20* 512 = 10240):

      /FOREIGN /RECORDSIZE = 512 /BLOCKSIZE = 10240

For different /BLOCK_FACTOR ("b") values, adjust the MOUNT /BLOCKSIZE
value accordingly.

Example:

      $ MOUNT /FOREIGN /RECORD = 512 /BLOCK = 10240 MUA0: "" $TAPE
      $ VMSTAR /EXTRACT /VERBOSE MUA0:

Specifying the logical name "$TAPE" here is optional, but it may be
convenient, especially when using the UNIX-style command line of VMSTAR
(where "f $TAPE" is a potentially useful default).

------------------------------------------------------------------------

      Building VMSTAR
      ---------------

   The kit includes MMS/MMK builders and a DCL script.  Comments in the
main builder files (BUILD_VMSTAR.COM, DESCRIP.MMS) explain usage
details.  Typical build commands are shown below.

      @ BUILD_VMSTAR.COM
      @ BUILD_VMSTAR.COM NOLARGE ! Non-VAX, without large-file support.

      MMS                        ! Or MMK.
      MMS /MACRO = NOLARGE=1     ! Non-VAX, without large-file support.

   Product (object, link option, and executable) files should be created
in an architecture-specific subdirectory:

      [.ALPHA]          Alpha, no large-file support.
      [.ALPHAL]         Alpha, large-file support.
      [.IA64]           IA64, no large-file support.
      [.IA64L]          IA64, large-file support.
      [.VAX]            VAX, DEC C.
      [.VAXV]           VAX, VAX C.
      [.X86_64]         x86_64, no large-file support.
      [.X86_64L]        x86_64, large-file support.

------------------------------------------------------------------------

      Installation
      ------------

   To complete the installation, the executable may be left in place, or
moved (or copied) to a convenient place.  While other methods (like
DCL$PATH) exist, most users define a DCL symbol to make VMSTAR available
as a foreign command.  This symbol definition may be placed in a user's
SYS$LOGIN:LOGIN.COM, or in a more central location, like
SYS$MANAGER:SYLOGIN.COM.  A typical symbol definition might look like
this:

           VMSTAR :== $ dev:[dir]VMSTAR.EXE


   A VMS HELP file, VMSTAR.HLP, is provided.  It can be added to an
existing HELP library, or a separate HELP library can be created:

      LIBRARY /HELP dev:[dir]existing_library.HLB VMSTAR.HLP
or:
      LIBRARY /CREATE /HELP VMSTAR.HLB VMSTAR.HLP

      HELP HELP /LIBRARY      ! For more information on alternate or
      HELP HELP /USERLIBRARY  ! user-supplied HELP libraries.

For example, to add the VMSTAR help to the system HELP library:

      LIBRARY /HELP SYS$COMMON:[SYSHLP]HELPLIB.HLB VMSTAR.HLP

------------------------------------------------------------------------

      Bugs
      ----

   Traditionally, VMSTAR is not well tested, and this version is no
exception.  Potential weak spots include error handling in builds
without large-file support (typically on VAX or older Alpha systems)
when trying to deal with archives which contain large files.  Similarly,
archives with long names which can't be handled on an old ODS2 file
system might cause unexpected problems.

   VMS files named "." or ".." ("^..") will be archived as "." or "..",
and those names may be expected to cause trouble on a UNIX(-like) file
system.

   After extensive changes to the command-line interface and the
functional code since V3.4-1, many things are possible.  Complaints are
always welcome.

------------------------------------------------------------------------
