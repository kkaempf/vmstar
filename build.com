$! BUILD.COM - Compiles and link VMSTAR
$!
$!	P1	C compiler qualifiers
$!	P2	Linker qualifiers
$!
$ save_verify = 'f$verify(f$trnlnm("VERIFY_BUILD"))
$ on control_y then goto exit
$ on severe_error then goto exit
$ on error then goto exit
$ on warning then continue
$
$! Find out what version we are
$ ident=""
$ open/read/error=no_version foo VERSION.OPT
$read_version:
$ read/end=no_more_version/error=no_more_version foo line
$ 'line'
$ goto read_version
$no_more_version:
$ close foo
$no_version:
$!
$ CFLAGS = "/DEBUG"
$ if p1 .nes. "" then CFLAGS = f$edit(p1,"UPCASE")
$
$ axp = f$getsyi("HW_MODEL").ge.1024
$ decc = f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. "" -
	.and. CFLAGS - "/VAXC" .eqs. CFLAGS
$
$ ext = "VAX_"
$ if axp then ext = "ALPHA_"
$ if .not. decc then ext = ext + "VAXC_"
$
$ all_cflags = CFLAGS+"/DEFINE=VERSION=""""""''ident'"""""""
$ if decc then all_cflags = "/DECC/PREFIX=ALL " + all_cflags
$ if decc .and. axp then all_cflags = all_cflags + "/L_DOUBLE=64"
$
$ set verify
$ cc'all_cflags' vmstar.c /object=vmstar.'ext'obj
$ cc'all_cflags' data.c /object=data.'ext'obj
$ cc'all_cflags' vms2tar.c /object=vms2tar.'ext'obj
$ cc'all_cflags' tar2vms.c /object=tar2vms.'ext'obj
$ cc'all_cflags' vmsmunch.c /object=vmsmunch.'ext'obj
$ cc'all_cflags' vmstar_cmdline.c /object=vmstar_cmdline.'ext'obj
$ set command/object=vmstar_cld.'ext'obj vmstar_cld.cld
$ ! 'f$verify(0)
$
$ compiler_used = "DECC"
$ if .not. decc then compiler_used = "VAXC"
$ @link "''p2'" 'compiler_used'
$exit:
$ exit 1 + 0*f$verify(save_verify)
