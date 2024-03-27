$! LINK.COM - Link VMSTAR
$!
$!	P1	Linker qualifiers
$!	P2	"DECC" if DEC C was used to compile the object files,
$!		"VAXC" if VAX C was used.  If not given, this script
$!		will simply check what library is available, and will
$!		act upon that information.
$!
$ save_verify = 'f$verify(0)
$ on control_y then goto exit
$ on severe_error then goto exit
$ on error then goto exit
$ on warning then continue
$!
$! LDFLAGS = "/debug"
$ LDFLAGS = "/NOTRACE"
$ if p1 .nes. "" then LDFLAGS = f$edit(p1,"UPCASE")
$
$ axp = f$getsyi("HW_MODEL").ge.1024
$ decc = f$search("SYS$SHARE:DECC$SHR.EXE") .nes. "" -
	.and. p2 .nes. "VAXC"
$
$ option = ""
$ ext = "VAX_"
$ if axp then ext = "ALPHA_"
$ exe_ext = ext
$ if .not. decc
$  then
$   ext = ext + "VAXC_"
$   open/write foo VMSTAR.'ext'OPT
$   write foo "SYS$SHARE:VAXCRTL.EXE/SHARE"
$   close foo
$   options = ",VMSTAR.''ext'OPT/OPT"
$  endif
$
$ set verify
$ link'LDFLAGS'/exec=vmstar.'exe_ext'exe vmstar.'ext'obj,-
	vms2tar.'ext'obj,tar2vms.'ext'obj,data.'ext'obj,-
	vmstar_cmdline.'ext'obj,vmstar_cld.'ext'obj,-
	vmsmunch.'ext'obj,version.opt/opt'options'
$ ! 'f$verify(0)
$exit:
$ exit 1 + 0*f$verify(save_verify)