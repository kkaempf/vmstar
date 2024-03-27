# DESCRIP_SRC.MMS
#
#    VMSTAR 4.3 - MMS (or MMK) Source Description File.
#
#    Last revised:  2020-12-23
#

# This description file is included by other description files.  It is
# not intended to be used alone.  Verify proper inclusion.

.IFDEF INCL_DESCRIP_SRC
.ELSE
$$$$ THIS DESCRIPTION FILE IS NOT INTENDED TO BE USED THIS WAY.
.ENDIF

# Include version.opt, which defines the program "name" (line 1) and
# "ident" (version, line 2).
.INCLUDE version.opt

# Define MMK architecture macros when using MMS.

.IFDEF __MMK__                  # __MMK__
.ELSE                           # __MMK__
ALPHA_X_ALPHA = 1
IA64_X_IA64 = 1
VAX_X_VAX = 1
.IFDEF $(MMS$ARCH_NAME)_X_ALPHA     # $(MMS$ARCH_NAME)_X_ALPHA
__ALPHA__ = 1
.ENDIF                              # $(MMS$ARCH_NAME)_X_ALPHA
.IFDEF $(MMS$ARCH_NAME)_X_IA64      # $(MMS$ARCH_NAME)_X_IA64
__IA64__ = 1
.ENDIF                              # $(MMS$ARCH_NAME)_X_IA64
.IFDEF $(MMS$ARCH_NAME)_X_VAX       # $(MMS$ARCH_NAME)_X_VAX
__VAX__ = 1
.ENDIF                              # $(MMS$ARCH_NAME)_X_VAX
.ENDIF                          # __MMK__

# Combine command-line VAX C compiler macros.

.IFDEF VAXC                     # VAXC
VAXC_OR_FORCE_VAXC = 1
.ELSE                           # VAXC
.IFDEF FORCE_VAXC                   # FORCE_VAXC
VAXC_OR_FORCE_VAXC = 1
.ENDIF                              # FORCE_VAXC
.ENDIF                          # VAXC

# Analyze architecture-related and option macros.

.IFDEF __ALPHA__                # __ALPHA__
DECC = 1
DESTM = ALPHA
.ELSE                           # __ALPHA__
.IFDEF __IA64__                     # __IA64__
DECC = 1
DESTM = IA64
.ELSE                               # __IA64__
.IFDEF __VAX__                          # __VAX__
.IFDEF VAXC_OR_FORCE_VAXC                   # VAXC_OR_FORCE_VAXC
DESTM = VAXV
.ELSE                                       # VAXC_OR_FORCE_VAXC
DECC = 1
DESTM = VAX
.ENDIF                                      # VAXC_OR_FORCE_VAXC
.ELSE                                   # __VAX__
DESTM = UNK
UNK_DEST = 1
.ENDIF                                  # __VAX__
.ENDIF                              # __IA64__
.ENDIF                          # __ALPHA__

# Large-file support.  Always no on VAX.  Assume yes elsewhere.  On
# Alpha, the .FIRST rule will detect incompatibility (before VMS V7.2).

# Targets which can bypass the AES_WG test and, on Alpha, the (slow)
# large-file test.
# (Not "" or ALL.  Could add help- and message-related.)

TRGT_CLEAN = 1
TRGT_CLEAN_ALL = 1
TRGT_CLEAN_EXE = 1
TRGT_DASHV = 1
TRGT_SLASHV = 1

TRGT = TRGT_$(MMSTARGETS)
.IFDEF $(TRGT)                  # $(TRGT)
.IFDEF __ALPHA__                    # __ALPHA__
.IFDEF NOCHECK_LARGE                    # NOCHECK_LARGE
.ELSE                                   # NOCHECK_LARGE
NOCHECK_LARGE = 1
.ENDIF                                  # NOCHECK_LARGE
.ENDIF                              # __ALPHA__
.ENDIF                          # $(TRGT)

.IFDEF __VAX__                  # __VAX__
.IFDEF NOLARGE                      # NOLARGE
.ELSE                               # NOLARGE
NOLARGE = 1
.ENDIF                              # NOLARGE
.ENDIF                          # __VAX__

# Translate NO_LARGE to NOLARGE.
.IFDEF NO_LARGE                 # NO_LARGE
.IFDEF NOLARGE                      # NOLARGE
.ELSE                               # NOLARGE
NOLARGE = 1
.ENDIF                              # NOLARGE
.ENDIF                          # NO_LARGE

.IFDEF NOLARGE                  # NOLARGE
.ELSE                           # NOLARGE
.IFDEF LARGE                        # LARGE
.ELSE                               # LARGE
LARGE = 1
.IFDEF __ALPHA__                        # __ALPHA__
.IFDEF NOCHECK_LARGE                        # NOCHECK_LARGE
.ELSE                                       # NOCHECK_LARGE
CHECK_LARGE = 1
.ENDIF                                      # NOCHECK_LARGE
.ENDIF                                  # __ALPHA__
.ENDIF                              # LARGE
DESTL = L
.ENDIF                          # NOLARGE

DEST = $(DESTM)$(DESTL)

# Check for option problems.

.IFDEF __VAX__                  # __VAX__
.IFDEF LARGE                        # LARGE
LARGE_VAX = 1
.ENDIF                              # LARGE
.ELSE                           # __VAX__
.IFDEF VAXC_OR_FORCE_VAXC           # VAXC_OR_FORCE_VAXC
NON_VAX_CMPL = 1
.ENDIF                              # VAXC_OR_FORCE_VAXC
.ENDIF                          # __VAX__

# Complain about any problems (and die) if warranted.  Otherwise, show
# the destination directory.  Make the destination directory, if
# necessary.

.IFDEF UNK_DEST                 # UNK_DEST
.FIRST
	@ write sys$output -
 "   Unknown system architecture."
.IFDEF __MMK__                      # __MMK__
	@ write sys$output -
 "   MMK on IA64?  Try adding ""/MACRO = __IA64__""."
.ELSE                               # __MMK__
	@ write sys$output -
 "   MMS too old?  Try adding ""/MACRO = MMS$ARCH_NAME=ALPHA"","
	@ write sys$output -
 "   or ""/MACRO = MMS$ARCH_NAME=IA64"", or ""/MACRO = MMS$ARCH_NAME=VAX"","
	@ write sys$output -
 "   as appropriate.  (Or try a newer version of MMS.)"
.ENDIF                              # __MMK__
	@ write sys$output ""
	I_WILL_DIE_NOW.  /$$$$INVALID$$$$
.ELSE                           # UNK_DEST
.IFDEF NON_VAX_CMPL                 # NON_VAX_CMPL
.FIRST
	@ write sys$output -
 "   Macros ""VAXC"" and ""FORCE_VAXC"" are valid only on VAX."
	@ write sys$output ""
	I_WILL_DIE_NOW.  /$$$$INVALID$$$$
.ELSE                               # NON_VAX_CMPL
.IFDEF LARGE_VAX                        # LARGE_VAX
.FIRST
	@ write sys$output -
 "   Macro ""LARGE"" is invalid on VAX."
	@ write sys$output ""
	I_WILL_DIE_NOW.  /$$$$INVALID$$$$
.ELSE                                   # LARGE_VAX
.FIRST
	@ write sys$output "   Destination: [.$(DEST)]"
	@ write sys$output ""
	if (f$search( "$(DEST).DIR;1") .eqs. "") then -
	 create /directory [.$(DEST)]
.IFDEF CHECK_LARGE                          # CHECK_LARGE
        @ write sys$output ""
        @ write sys$output "   Verifying large-file support..."
        @ @CHECK_LARGE.COM $(DEST) large_ok
        @ no_large = (f$trnlnm( "large_ok") .eqs. "")
        @ if (no_large) then -
           write sys$output -
            "   Large-file support not available with this VMS/CRTL version."
        @ if (no_large) then -
           write sys$output "   Add ""/MACRO = NOLARGE=1""."
        @ if (no_large) then -
           I_WILL_DIE_NOW.  /$$$$INVALID$$$$
        @ write sys$output "   Large-file support ok."
        @ write sys$output ""
.ENDIF                                      # CHECK_LARGE
.ENDIF                                  # LARGE_VAX
.ENDIF                              # NON_VAX_CMPL
.ENDIF                          # UNK_DEST

# DBG options.

.IFDEF DBG                      # DBG
CFLAGS_DBG = /debug /nooptimize
LINKFLAGS_DBG = /debug /traceback
.ELSE                           # DBG
CFLAGS_DBG =
.IFDEF TRC                          # TRC
LINKFLAGS_DBG = /traceback
.ELSE                               # TRC
LINKFLAGS_DBG = /notraceback
.ENDIF                              # TRC
.ENDIF                          # DBG

# Large-file options.

.IFDEF LARGE                    # LARGE
CDEFS_LARGE = , _LARGEFILE
.ELSE                           # LARGE
CDEFS_LARGE =
.ENDIF                          # LARGE

# C compiler defines.

.IFDEF LOCAL_VMSTAR
C_LOCAL_VMSTAR = , $(LOCAL_VMSTAR)
.ELSE
C_LOCAL_VMSTAR =
.ENDIF

CDEFS_VERSION = , VERSION=""$(ident)""

CDEFS = VMS $(CDEFS_LARGE) $(C_LOCAL_VMSTAR) $(CDEFS_VERSION)

# Other C compiler options.

.IFDEF DECC                             # DECC
CFLAGS_ARCH = /decc /prefix = (all)
.ELSE                                   # DECC
.IFDEF FORCE_VAXC                           # FORCE_VAXC
CFLAGS_ARCH = /vaxc
.IFDEF VAXC                                     # VAXC
.ELSE                                           # VAXC
VAXC = 1
.ENDIF                                          # VAXC
.ELSE                                       # FORCE_VAXC
CFLAGS_ARCH =
.ENDIF                                      # FORCE_VAXC
.ENDIF                                  # DECC

# LINK library options.

.IFDEF VAXC_OR_FORCE_VAXC               # VAXC_OR_FORCE_VAXC
OPT_FILE_CRTL = [.$(DEST)]VAXCSHR.OPT
LINKFLAGS_ARCH = , SYS$DISK:$(OPT_FILE_CRTL) /options
.ELSE                                   # VAXC_OR_FORCE_VAXC
OPT_FILE_CRTL =
LINKFLAGS_ARCH =
.ENDIF                                  # VAXC_OR_FORCE_VAXC

# LIST options.

.IFDEF LIST                     # LIST
.IFDEF DECC                         # DECC
CFLAGS_LIST = /list = $*.LIS /show = (all, nomessages)
.ELSE                               # DECC
CFLAGS_LIST = /list = $*.LIS /show = (all)
.ENDIF                              # DECC
LINKFLAGS_LIST = /map = $*.MAP /cross_reference /full
.ELSE                           # LIST
CFLAGS_LIST =
LINKFLAGS_LIST =
.ENDIF                          # LIST

# Common CFLAGS and LINKFLAGS.

CFLAGS = \
 $(CFLAGS_ARCH) $(CFLAGS_DBG) $(CFLAGS_INCL) $(CFLAGS_LIST) $(CCOPTS) \
 /define = ($(CDEFS)) /object = $(MMS$TARGET)

LINKFLAGS = \
 $(LINKFLAGS_DBG) $(LINKFLAGS_LIST) $(LINKOPTS) \
 /executable = $(MMS$TARGET)

# Object lists.

MODS_OBJS_VMSTAR = \
 [.$(DEST)]DATA.OBJ \
 [.$(DEST)]TAR2VMS.OBJ \
 [.$(DEST)]VMS2TAR.OBJ \
 [.$(DEST)]VMSMUNCH.OBJ \
 [.$(DEST)]VMSTAR.OBJ \
 [.$(DEST)]VMSTAR_CMDLINE.OBJ \
 [.$(DEST)]VMS_IO.OBJ

# CLI table.

VMSTAR_CLD_OBJ = [.$(DEST)]VMSTAR_CLD.OBJ

# Executables.

VMSTAR_EXE = [.$(DEST)]VMSTAR.EXE

# Object list options file.

OPT_FILE_MODS_OBJS = [.$(DEST)]VMSTAR_OBJS.OPT
