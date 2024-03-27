$!                                              4 January 2008.  SMS.
$!
$! VMSTAR VMS accessory procedure.
$!
$!    For the product named by P1,
$!    create a LINK options file named by P2,
$!    containing the (object) files in the space-separated list in P3.
$!
$! For portability, make the output file record format Stream_LF.
$!
$ create /fdl = sys$input 'p2'
RECORD
        Carriage_Control carriage_return
        Format stream_lf
$!
$ open /read /write /error = end_main opts_out 'p2'
$ on error then goto loop_main_end
$!
$! Include proper-inclusion-check preface.
$!
$ incl_macro = "INCL_"+ f$parse( p2, , , "NAME", "SYNTAX_ONLY")
$ write opts_out "!"
$ write opts_out "! ''p1' for VMS - MMS (or MMK) Objects LINK Options File."
$ write opts_out "!"
$!
$! Put out the individual object file names in P3, one per line.
$!
$ element = 0
$ objs = f$edit( P3, "COMPRESS, TRIM")
$ loop_main_top:
$    obj = f$edit( f$element( element, " ", objs), "TRIM")
$    if (obj .eqs. "") then goto loop_main_end
$    write opts_out "''obj'"
$    element = element+ 1
$ goto loop_main_top
$!
$ loop_main_end:
$ close opts_out
$!
$ end_main:
$!
