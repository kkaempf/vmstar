# DESCRIP.MMS
#
#    VMSTAR 4.1 - MMS (or MMK) Description File.
#
#    Last revised:  2014-11-24
#
# Usage:
#
#    MMS /DESCRIP = DESCRIP.MMS [/MACRO = (<see_below>)] [target]
#
# Optional macros:
#
#    "CCOPTS=xxx"   Compile with CC options xxx.  For example:
#                   "CCOPTS=/ARCH=HOST"
#
#    DBG=1          Compile with /DEBUG /NOOPTIMIZE.
#                   Link with /DEBUG /TRACEBACK.
#                   (Default is /NOTRACEBACK.)
#
#    LARGE=1        Enable/disable large-file (>2GB) support.  Always
#    NOLARGE=1      disabled on VAX.  Enabled by default on Alpha and
#                   IA64.  On Alpha, by default, large-file support is
#                   tested, and the build will fail if that test fails.
#                   Specify NOLARGE=1 explicitly to disable support (and
#                   to skip the test on Alpha).
#
#    "LINKOPTS=xxx" Link with LINK options xxx.  For example:
#                   "LINKOPTS=/NOINFO"
#
#    LIST=1         Compile with /LIST /SHOW = (ALL, NOMESSAGES).
#                   Link with /MAP /CROSS_REFERENCE /FULL.
#
#    "LOCAL_VMSTAR= c_macro_1=value1 [, c_macro_2=value2 [...]]"
#                   Compile with these additional C macros defined.
#
# VAX-specific optional macros:
#
#    VAXC=1         Use the VAX C compiler, assuming "CC" runs it.
#                   (That is, DEC C is not installed, or else DEC C is
#                   installed, but VAX C is the default.)
#
#    FORCE_VAXC=1   Use the VAX C compiler, assuming "CC /VAXC" runs it.
#                   (That is, DEC C is installed, and it is the
#                   default, but you want VAX C anyway, you fool.)
#
#
# The default target, ALL, builds the selected product executables and
# help files.
#
# Other targets:
#
#    CLEAN      deletes architecture-specific files, but leaves any
#               individual source dependency files and the help files.
#
#    CLEAN_ALL  deletes all generated files, except the main (collected)
#               source dependency file.
#
#    CLEAN_EXE  deletes only the architecture-specific executables.
#               Handy if all you wish to do is re-link the executables.
#
#    DASHV      generates a "vmstar -v" report.
#
#    SLASHV     generates a "vmstar /verbose" report.
#
# Example commands:
#
# To build the large-file product (except on VAX) using the
# DEC/Compaq/HP C compiler (Note: DESCRIP.MMS is the default description
# file name.):
#
#    MMS
#
# To get a small-file executable (on a non-VAX system):
#
#    MMS /MACRO = (NOLARGE=1)
#
# To delete the architecture-specific generated files for this system
# type:
#
#    MMS CLEAN
#
# To build a complete small-file product for debug with compiler
# listings and link maps:
#
#    MMS CLEAN
#    MMS /MACRO = (DBG=1, LIST=1)
#
########################################################################

# Include primary product description file.

INCL_DESCRIP_SRC = 1
.INCLUDE DESCRIP_SRC.MMS


# Help file name.

VMSTAR_HLP = VMSTAR.HLP


# TARGETS.

# Default target, ALL.  Build all executables and help files.

ALL : $(VMSTAR_EXE) $(VMSTAR_HLP)
	@ write sys$output "Done."

# CLEAN target.  Delete the [.$(DEST)] directory and everything in it.

CLEAN :
	if (f$search( "[.$(DEST)]*.*") .nes. "") then -
	 delete [.$(DEST)]*.*;*
	if (f$search( "$(DEST).dir") .nes. "") then -
	 set protection = w:d $(DEST).dir;*
	if (f$search( "$(DEST).dir") .nes. "") then -
	 delete $(DEST).dir;*

# CLEAN_ALL target.  Delete:
#    The [.$(DEST)] directories and everything in them.
#    All help-related derived files,
#    All individual C dependency files.
# Also mention:
#    Comprehensive dependency file.
#
CLEAN_ALL :
	if (f$search( "[.ALPHA*]*.*") .nes. "") then -
	 delete [.ALPHA*]*.*;*
	if (f$search( "ALPHA*.dir", 1) .nes. "") then -
	 set protection = w:d ALPHA*.dir;*
	if (f$search( "ALPHA*.dir", 2) .nes. "") then -
	 delete ALPHA*.dir;*
	if (f$search( "[.IA64*]*.*") .nes. "") then -
	 delete [.IA64*]*.*;*
	if (f$search( "IA64*.dir", 1) .nes. "") then -
	 set protection = w:d IA64*.dir;*
	if (f$search( "IA64*.dir", 2) .nes. "") then -
	 delete IA64*.dir;*
	if (f$search( "[.VAX*]*.*") .nes. "") then -
	 delete [.VAX*]*.*;*
	if (f$search( "VAX*.dir", 1) .nes. "") then -
	 set protection = w:d VAX*.dir;*
	if (f$search( "VAX*.dir", 2) .nes. "") then -
	 delete VAX*.dir;*
	if (f$search( "*.MMSD") .nes. "") then -
	 delete *.MMSD;*
	if (f$search( "$(VMSTAR_HLP)") .nes. "") then -
	 delete $(VMSTAR_HLP);*
	@ write sys$output ""
	@ write sys$output "Note:  This procedure will not"
	@ write sys$output "   DELETE DESCRIP_DEPS.MMS;*"
	@ write sys$output -
 "You may choose to, but a recent version of MMS (V3.5 or newer?) is"
	@ write sys$output -
 "needed to regenerate it.  (It may also be recovered from the original"
	@ write sys$output -
 "distribution kit.)  See DESCRIP_MKDEPS.MMS for instructions on"
	@ write sys$output -
 "generating DESCRIP_DEPS.MMS."
        @ write sys$output ""

# CLEAN_EXE target.  Delete the executables in [.$(DEST)].

CLEAN_EXE :
        if (f$search( "[.$(DEST)]*.EXE") .nes. "") then -
         delete [.$(DEST)]*.EXE;*

# DASHV target.  Generate a "zip -v" report.

DASHV :
        mcr [.$(DEST)]vmstar -v

# SLASHV target.  Generate a "zip_cli /verbose" report.

SLASHV :
        mcr [.$(DEST)]vmstar /verbose


# Default C compile rule.

.C.OBJ :
        $(CC) $(CFLAGS) $(MMS$SOURCE)

# VAX C LINK options file.

.IFDEF OPT_FILE_CRTL
$(OPT_FILE_CRTL) :
	create /fdl = STREAM_LF.FDL $(MMS$TARGET)
        open /read /write opt_file_ln $(MMS$TARGET)
        write opt_file_ln "SYS$SHARE:VAXCRTL.EXE /SHARE"
        close opt_file_ln
.ENDIF

# Object list options file.

$(OPT_FILE_MODS_OBJS) :
	@LIST_TO_OPT.COM "VMSTAR" "$(MMS$TARGET)" "$(MODS_OBJS_VMSTAR)"

# VMSTAR executable.

$(VMSTAR_EXE) : $(MODS_OBJS_VMSTAR) $(VMSTAR_CLD_OBJ) \
                $(OPT_FILE_CRTL) $(OPT_FILE_MODS_OBJS)
	$(LINK) $(LINKFLAGS) $(OPT_FILE_MODS_OBJS) /options, -
	 $(VMSTAR_CLD_OBJ) $(LINKFLAGS_ARCH)

# Help file.

$(VMSTAR_HLP) : VMSTAR.RNH
        RUNOFF /OUTPUT = $(MMS$TARGET) $(MMS$SOURCE)

# CLI table object.

$(VMSTAR_CLD_OBJ) : VMSTAR_CLD.CLD
	SET COMMAND /OBJECT = $(MMS$TARGET) $(MMS$SOURCE)


# Include generated source dependencies.

INCL_DESCRIP_DEPS = 1
.INCLUDE DESCRIP_DEPS.MMS

