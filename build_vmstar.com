$! BUILD_VMSTAR.COM
$!
$!     Build procedure for VMSTAR.
$!
$!     last revised:  2008-01-05  SMS.
$!
$!     Command arguments:
$!     - suppress help file processing: "NOHELP"
$!     - select link-only: "LINK"
$!     - select compiler environment: "VAXC", "DECC"
$!     - select large-file support: "LARGE"
$!     - select compiler listings: "LIST"  Note that the whole argument
$!       is added to the compiler command, so more elaborate options
$!       like "LIST/SHOW=ALL" (quoted or space-free) may be specified.
$!     - supply additional compiler options: "CCOPTS=xxx"  Allows the
$!       user to add compiler command options like /ARCHITECTURE or
$!       /[NO]OPTIMIZE.  For example, CCOPTS=/ARCH=HOST/OPTI=TUNE=HOST
$!       or CCOPTS=/DEBUG/NOOPTI.  These options must be quoted or
$!       space-free.
$!     - supply additional linker options: "LINKOPTS=xxx"  Allows the
$!       user to add linker command options like /DEBUG or /MAP.  For
$!       example: LINKOPTS=/DEBUG or LINKOPTS=/MAP/CROSS.  These options
$!       must be quoted or space-free.  Default is
$!       LINKOPTS=/NOTRACEBACK, but if the user specifies a LINKOPTS
$!       string, /NOTRACEBACK will not be included unless specified by
$!       the user.
$!
$!     To specify additional options, define the global symbol
$!     LOCAL_VMSTAR as a comma-separated list of the C macros to be
$!     defined, and then run BUILD_VMSTAR.COM.  For example:
$!
$!             $ LOCAL_VMSTAR == "DEBUG"
$!             $ @ [.VMS]BUILD_VMSTAR.COM
$!
$!     If editing this procedure to set LOCAL_VMSTAR, be sure to use
$!     only one "=", to avoid affecting other procedures.  For example:
$!             $ LOCAL_VMSTAR = "DEBUG"
$!
$!
$ on error then goto error
$ on control_y then goto error
$ OLD_VERIFY = f$verify( 0)
$!
$ edit := edit                  ! override customized edit commands
$ say := write sys$output
$!
$!##################### Read settings from environment ########################
$!
$ if (f$type( LOCAL_VMSTAR) .eqs. "")
$ then
$     LOCAL_VMSTAR = ""
$ else  ! Trim blanks and append comma if missing
$     LOCAL_VMSTAR = f$edit( LOCAL_VMSTAR, "TRIM")
$     if (f$extract( f$length( LOCAL_VMSTAR)- 1, 1, LOCAL_VMSTAR) .nes. ",")
$     then
$         LOCAL_VMSTAR = LOCAL_VMSTAR + ","
$     endif
$ endif
$!
$!##################### Customizing section #############################
$!
$ CCOPTS = ""
$ LINKOPTS = "/notraceback"
$ LINK_ONLY = 0
$ LISTING = " /nolist"
$ LARGE_FILE = 0
$ MAKE_HELP = 1
$ MAY_USE_DECC = 1
$!
$! Process command line parameters requesting optional features.
$!
$ arg_cnt = 1
$ argloop:
$     current_arg_name = "P''arg_cnt'"
$     curr_arg = f$edit( 'current_arg_name', "UPCASE")
$     if (curr_arg .eqs. "") then goto argloop_out
$!
$     if (f$extract( 0, 5, curr_arg) .eqs. "CCOPT")
$     then
$         opts = f$edit( curr_arg, "COLLAPSE")
$         eq = f$locate( "=", opts)
$         CCOPTS = f$extract( (eq+ 1), 1000, opts)
$         goto argloop_end
$     endif
$!
$     if (f$extract( 0, 5, curr_arg) .eqs. "LARGE")
$     then
$         LARGE_FILE = 1
$         goto argloop_end
$     endif
$!
$     if (f$extract( 0, 7, curr_arg) .eqs. "LINKOPT")
$     then
$         opts = f$edit( curr_arg, "COLLAPSE")
$         eq = f$locate( "=", opts)
$         LINKOPTS = f$extract( (eq+ 1), 1000, opts)
$         goto argloop_end
$     endif
$!
$! Note: LINK test must follow LINKOPTS test.
$!
$     if (f$extract( 0, 4, curr_arg) .eqs. "LINK")
$     then
$         LINK_ONLY = 1
$         goto argloop_end
$     endif
$!
$     if (f$extract( 0, 4, curr_arg) .eqs. "LIST")
$     then
$         LISTING = "/''curr_arg'"      ! But see below for mods.
$         goto argloop_end
$     endif
$!
$     if (curr_arg .eqs. "NOHELP")
$     then
$         MAKE_HELP = 0
$         goto argloop_end
$     endif
$!
$     if (curr_arg .eqs. "VAXC")
$     then
$         MAY_USE_DECC = 0
$         goto argloop_end
$     endif
$!
$     if (curr_arg .eqs. "DECC")
$     then
$         MAY_USE_DECC = 1
$         goto argloop_end
$     endif
$!
$     say "Unrecognized command-line option: ''curr_arg'"
$     goto error
$!
$     argloop_end:
$     arg_cnt = arg_cnt + 1
$ goto argloop
$ argloop_out:
$!
$!#######################################################################
$!
$! Find out current disk, directory, compiler and options
$!
$ workdir = f$environment( "default")
$ here = f$parse( workdir, , , "device")+ f$parse( workdir, , , "directory")
$!
$! Sense the host architecture (Alpha, Itanium, or VAX).
$!
$ if (f$getsyi( "HW_MODEL") .lt. 1024)
$ then
$     arch = "VAX"
$ else
$     if (f$getsyi( "ARCH_TYPE") .eq. 2)
$     then
$         arch = "ALPHA"
$     else
$         if (f$getsyi( "ARCH_TYPE") .eq. 3)
$         then
$             arch = "IA64"
$         else
$             arch = "unknown_arch"
$         endif
$     endif
$ endif
$!
$ dest = arch
$ cmpl = "DEC/Compaq/HP C"
$ opts = ""
$ if (arch .nes. "VAX")
$ then
$     HAVE_DECC_VAX = 0
$     USE_DECC_VAX = 0
$!
$     if (.not. MAY_USE_DECC)
$     then
$         say "VAX C is not supported for ''arch'."
$         say "You must use DEC/Compaq/HP C to build VMSTAR."
$         goto error
$     endif
$!
$     cc = "cc /prefix = all"
$     defs = "''LOCAL_VMSTAR'"
$     if (LARGE_FILE .ne. 0)
$     then
$         defs = "_LARGEFILE, ''defs'"
$     endif
$ else
$     if (LARGE_FILE .ne. 0)
$     then
$        say "Large-file support is not available on VAX."
$        LARGE_FILE = 0
$     endif
$     HAVE_DECC_VAX = (f$search( "SYS$SYSTEM:DECC$COMPILER.EXE") .nes. "")
$     HAVE_VAXC_VAX = (f$search( "SYS$SYSTEM:VAXC.EXE") .nes. "")
$     if (HAVE_DECC_VAX .and. MAY_USE_DECC)
$     then
$         ! We use DECC:
$         USE_DECC_VAX = 1
$         cc = "cc /decc /prefix = all"
$         defs = "''LOCAL_VMSTAR'"
$     else
$         ! We use VAXC:
$         USE_DECC_VAX = 0
$         defs = "''LOCAL_VMSTAR'"
$         if (HAVE_DECC_VAX)
$         then
$             cc = "cc /vaxc"
$         else
$             cc = "cc"
$         endif
$         dest = "''dest'V"
$         cmpl = "VAC C"
$         opts = "''opts', SYS$DISK:[.''dest']VAXCSHR.OPT /OPTIONS"
$     endif
$ endif
$!
$! Change the destination directory, according to the large-file option.
$!
$ seek_bz = arch
$ if (LARGE_FILE .ne. 0)
$ then
$     dest = dest+ "L"
$     seek_bz = seek_bz+ "L"
$ endif
$!
$! Reveal the plan.  If compiling, set some compiler options.
$!
$ if (LINK_ONLY)
$ then
$     say "Linking on ''arch' for ''cmpl'."
$ else
$     say "Compiling on ''arch' using ''cmpl'."
$ endif
$!
$! If [.'dest'] does not exist, either complain (link-only) or make it.
$!
$ if (f$search( "''dest'.DIR;1") .eqs. "")
$ then
$     if (LINK_ONLY)
$     then
$         say "Can't find directory ""[.''dest']"".  Can't link."
$         goto error
$     else
$         create /directory [.'dest']
$     endif
$ endif
$!
$ if (.not. LINK_ONLY)
$ then
$!
$! Arrange to get arch-specific list file placement, if LISTING, and if
$! the user didn't specify a particular "/LIST =" destination.
$!
$     L = f$edit( LISTING, "COLLAPSE")
$     if ((f$extract( 0, 5, L) .eqs. "/LIST") .and. -
       (f$extract( 4, 1, L) .nes. "="))
$     then
$         LISTING = " /LIST = [.''dest']"+ f$extract( 5, 1000, LISTING)
$     endif
$!
$! Define compiler command.
$!
$     cc = cc+ " "+ LISTING+ CCOPTS
$!
$ endif
$!
$! Define linker command.
$!
$ link = "link ''LINKOPTS'"
$!
$! Make a VAXCRTL options file for VAC C, if needed.
$!
$ if ((opts .nes. "") .and. -
   (f$locate( "VAXCSHR", f$edit( opts, "UPCASE")) .lt. f$length( opts)) .and. -
   (f$search( "[.''dest']VAXCSHR.OPT") .eqs. ""))
$ then
$     create /fdl = STREAM_LF.FDL [.'dest']VAXCSHR.OPT
$     open /read /write opt_file_ln [.'dest']VAXCSHR.OPT
$     write opt_file_ln "SYS$SHARE:VAXCRTL.EXE /SHARE"
$     close opt_file_ln
$ endif
$!
$! Show interesting facts.
$!
$ say "   architecture = ''arch' (destination = [.''dest'])"
$ if (.not. LINK_ONLY)
$ then
$     say "   cc = ''cc'"
$ endif
$ say "   link = ''link'"
$ if (.not. MAKE_HELP)
$ then
$     say "   Not making new help files."
$ endif
$ say ""
$!
$ MODS_OBJS_VMSTAR = -
 "[.''dest']DATA.OBJ "+ -
 "[.''dest']TAR2VMS.OBJ "+ -
 "[.''dest']VMS2TAR.OBJ "+ -
 "[.''dest']VMSMUNCH.OBJ "+ -
 "[.''dest']VMSTAR.OBJ "+ -
 "[.''dest']VMSTAR_CMDLINE.OBJ "+ -
 "[.''dest']VMS_IO.OBJ "
$!
$ tmp = f$verify( 1)    ! Turn echo on to see what's happening.
$!
$!
$ if (.not. LINK_ONLY)
$ then
$!
$! Process the help file, if desired.
$!
$     if (MAKE_HELP)
$     then
$         runoff /output = VMSTAR.HLP VMSTAR.RNH
$     endif
$     tmp = f$verify( 0)    ! Turn echo off.
$!
$! Extract the program version from VERSION.OPT (ident="V3.5").
$!
$     ident = ""
$     open /read /error = no_version vers VERSION.OPT
$     loopvers:
$         read /end = no_more_version /error = no_more_version vers line
$         if (f$locate( "ident", line) .lt. f$length( line))
$         then
$             'line'
$             goto no_more_version
$         endif
$     goto loopvers
$     no_more_version:
$     close vers
$     no_version:
$!
$     defs = "''defs' VERSION=""""""''ident'"""""""
$!
$! Compile the sources.
$!
$     element = 0
$     objs = f$edit( MODS_OBJS_VMSTAR, "COMPRESS, TRIM")
$     cmplloop:
$         obj = f$edit( f$element( element, " ", objs), "TRIM")
$         if (obj .eqs. "") then goto cmplloop_end
$         src = f$element( 0, ".", f$element( 1, "]", obj))+ ".C"
$         tmp = f$verify( 1)    ! Turn echo on.
$         cc /define = ('defs') /object = 'obj' 'src'
$         tmp = f$verify( 0)    ! Turn echo off.
$         element = element+ 1
$     goto cmplloop
$     cmplloop_end:
$!
$     tmp = f$verify( 1)    ! Turn echo on.
$     set command /object = [.'dest']VMSTAR_CLD.OBJ VMSTAR_CLD.CLD
$     tmp = f$verify( 0)    ! Turn echo off.
$!
$ endif
$!
$ tmp = f$verify( 0)    ! Turn echo off.
$!
$! Create the object list options file.
$!
$ @ LIST_TO_OPT.COM "VMSTAR" [.'dest']VMSTAR_OBJS.OPT -
   "''MODS_OBJS_VMSTAR'"
$!
$! Link the executable.
$!
$ tmp = f$verify( 1)    ! Turn echo on.
$ link /executable = [.'dest']VMSTAR.EXE -
   [.'dest']VMSTAR_OBJS.OPT /options, -
   [.'dest']VMSTAR_CLD.OBJ -
   'opts'
$ tmp = f$verify( 0)    ! Turn echo off.
$!
$! Restore the original default directory and DCL verify status.
$!
$ error:
$!
$ if (f$type( here) .nes. "")
$ then
$     if (here .nes. "")
$     then
$         set default 'here'
$     endif
$ endif
$!
$ if (f$type( OLD_VERIFY) .nes. "")
$ then
$     tmp = f$verify( OLD_VERIFY)
$ endif
$!
$ say "Done."
$!
$ exit
$!
