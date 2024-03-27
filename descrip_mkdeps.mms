#                                               3 January 2008.  SMS.
#
#    VMSTAR 4.0 for VMS - MMS Dependency Description File.
#
#    MMS /EXTENDED_SYNTAX description file to generate C source
#    dependency files.  Unsightly errors result when /EXTENDED_SYNTAX
#    is not specified.  Typical usage (but see below for details):
#
#    $ MMS /EXTEND /DESCRIP = DESCRIP_MKDEPS.MMS /SKIP
#
#    which discards individual source dependency files, or:
#
#    $ MMS /EXTEND /DESCRIP = DESCRIP_MKDEPS.MMS /MACRO = NOSKIP=1
#
#    which retains them.  Retaining them can save time when doing code
#    development.
#
#
# The default target is the comprehensive source dependency file,
# $(DEPS_FILE) = "DESCRIP_DEPS.MMS".
#
# Other targets:
#
#    CLEAN      deletes the individual source dependency files,
#               *.MMSD;*, but leaves the comprehensive source dependency
#               file.
#
#    CLEAN_ALL  deletes all source dependency files, including the
#               individual *.MMSD;* files and the comprehensive file,
#               DESCRIP_DEPS.MMS.*.
#
#
# This description file uses this command procedure:
#
#    COLLECT_DEPS.COM
#
# MMK users without MMS will be unable to generate the dependencies file
# using this description file, however there should be one supplied in
# the kit.  If this file has been deleted, users in this predicament
# will need to recover it from the original distribution kit.
#
# Note:  This dependency generation scheme assumes that the dependencies
# do not depend on host architecture type or other such variables.
# Therefore, no "#include" directive in the C source itself should be
# conditional on such variables.
#

# Required command procedures.

COMS = COLLECT_DEPS.COM

# Include the source file lists (among other data).

INCL_DESCRIP_SRC = 1
.INCLUDE DESCRIP_SRC.MMS

# The ultimate product, a comprehensive dependency list.

DEPS_FILE = DESCRIP_DEPS.MMS

# Detect valid qualifier and/or macro options.

.IF $(FINDSTRING Skip, $(MMSQUALIFIERS)) .eq Skip
DELETE_MMSD = 1
.ELSIF NOSKIP
PURGE_MMSD = 1
.ELSE
UNK_MMSD = 1
.ENDIF

# Dependency suffixes and rules.
#
# .FIRST is assumed to be used already, so the MMS qualifier/macro check
# is included in each rule (one way or another).

.SUFFIXES_BEFORE .C .MMSD

.C.MMSD :
.IF UNK_MMSD
	@ write sys$output -
 "   /SKIP_INTERMEDIATES is expected on the MMS command line."
	@ write sys$output -
 "   For normal behavior (delete .MMSD files), specify ""/SKIP""."
	@ write sys$output -
 "   To retain the .MMSD files, specify ""/MACRO = NOSKIP=1""."
	@ exit %x00000004
.ENDIF
	$(CC) $(CFLAGS) $(MMS$SOURCE) /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)

# List of MMS dependency files.

MODS_VMSTAR = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_VMSTAR)))

# Complete list of C object dependency file names.

DEPS = $(FOREACH NAME, \
 $(MODS_VMSTAR), \
 $(NAME).mmsd)

# Target is the comprehensive dependency list.

$(DEPS_FILE) : $(DEPS) $(COMS)
.IF UNK_MMSD
	@ write sys$output -
 "   /SKIP_INTERMEDIATES is expected on the MMS command line."
	@ write sys$output -
 "   For normal behavior (delete individual .MMSD files), specify ""/SKIP""."
	@ write sys$output -
 "   To retain the individual .MMSD files, specify ""/MACRO = NOSKIP=1""."
	@ exit %x00000004
.ENDIF
#
#       Note that the space in P4, which prevents immediate macro
#       expansion, is removed by COLLECT_DEPS.COM.
#
        @COLLECT_DEPS.COM "VMSTAR" -
         "$(MMS$TARGET)" "[...]*.mmsd" "[.$ (DEST)]" $(MMSDESCRIPTION_FILE)
        @ write sys$output -
         "Created a new dependency file: $(MMS$TARGET)"
.IF DELETE_MMSD
	@ write sys$output -
         "Deleting intermediate .MMSD files..."
	delete /log *.MMSD;*
.ELSE
	@ write sys$output -
         "Purging intermediate .MMSD files..."
	purge /log *.MMSD
.ENDIF


# CLEAN target.  Delete the individual C dependency files.

CLEAN :
	if (f$search( "[...]*.MMSD") .nes. "") then -
	 delete [...]*.MMSD;*

# CLEAN_ALL target.  Delete:
#    The individual C dependency files.
#    The collected source dependency file.

CLEAN_ALL :
	if (f$search( "[...]*.MMSD") .nes. "") then -
	 delete [...]*.MMSD;*
	if (f$search( "DESCRIP_DEPS.MMS") .nes. "") then -
	 delete DESCRIP_DEPS.MMS;*

