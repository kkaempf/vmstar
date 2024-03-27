# Description file for VMSTAR.

# include version.opt, since that gives us information like version number.
.include version.opt

CFLAGS = /DEBUG

LD = link
#LDFLAGS = /DEBUG
LDFLAGS = /NOTRACE

# if you want to put the executable and object files at some specific place,
# change this macro
GOAL=

# All the files we distribute
DISTFILES = aaa*.txt Changelog.* *.rnh *.hlp *.com *.*mms *.cld *.c *.h *.opt

# Some files we distribute in the .zip archive file.
ZIPDISTEXTRA = *.*_obj *.*_opt

all : setup vmstar.hlp
	$(MMS) $(MMSQUALIFIERS) /DESCRIPTION=DESCRIP.COMMON_MMS -
		/MACRO=(CFLAGS="''all_cflags'",LD="$(LD)",LDFLAGS="$(LDFLAGS)",GOAL="$(GOAL)",LIB="''lib'",EXT="''ext'",EXE_EXT="''exe_ext'"'do_option') 

setup :
	__arch = f$getsyi("ARCH_TYPE")
	__tmp = f$edit("$(CFLAGS)","UPCASE")
	__decc = f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. "" -
		.and. __tmp - "/VAXC" .eqs. __tmp
	ext = "VAX_"
	if __arch .eq. 2 then ext = "ALPHA_"
	if __arch .eq. 3 then ext = "I64_"
	exe_ext = ext
	if .not. __decc then ext = ext + "VAXC_"
	lib = "SYS$SHARE:VAXCRTL/SHARE"
	do_option = ""
	all_cflags = "$(CFLAGS)/DEFINE=VERSION="""""""""""$(ident)""""""""""""
	if .not. __decc then -
		do_option = ",OPTION=""$(GOAL)VMSTAR.''ext'OPT"",OPTIONCMD="",$(GOAL)VMSTAR.''ext'OPT/OPT"""
	if __decc then all_cflags = "/DECC/PREFIX=ALL " + all_cflags
	if __decc .and. (__arch .gt. 1) then all_cflags = all_cflags + "/L_DOUBLE=64"

vmstar.hlp : vmstar.rnh
	runoff vmstar.rnh

mostlyclean :
	- delete/log *.*_obj;*
	- delete/log *.*_opt;*
	- purge/log

clean : mostlyclean
	- delete/log *.*_exe;*
	- delete/log *.hlp;*
	- delete/log zip.*;*

dist_tar : setup versions FRC
	- mcr sys$disk:[]vmstar.'exe_ext'exe -cvf vmstar'vmsver'.tar $(DISTFILES)

dist_zip : versions zip.comment FRC
	- define/user sys$input zip.comment
	- define/user sys$output zip.log
	- zip -"Vz" vmstar'vmsver'.zip $(DISTFILES) $(ZIPDISTEXTRA)

zip_src : versions zip.comment FRC
	- define/user sys$input zip.comment
	- define/user sys$output zip.log
	- zip -"z" vmstar'vmsver'.zip $(DISTFILES)

versions : version.opt FRC
	@- ! open/read foo version.opt
	@- ! read foo line		! the name
	@- ! read foo line		! the ident
	@- ! close foo
	@ ! version = f$extract(1,10,f$element(1,"""",line))
	version = f$extract(1,10,$(ident))
	major = f$element(0,".",version)
	minor = f$element(0,"-",f$element(1,".",version))
	update = f$element(0," ",f$element(1,"-",version))
	testword = f$element(1," ",version)
	vmsver = "0''major'''minor'"
	if major .gt. 9 then vmsver = "''major'''minor'"
	if update .nes. "-" then vmsver = "U''update'"+vmsver
	if testword .nes. " " then vmsver = vmsver+"-"+testword
	
zip.comment : version.opt descrip.mms
	- open/write foo zip.comment
	- write foo " "
	- write foo "VMSTAR ''version' -- Read and write UN*X tar files under VMS"
	- write foo " "
	- write foo "Runs on both OpenVMS/VAX and OpenVMS/AXP."
	- write foo " "
	- write foo "To get the executable, just do @LINK after you extracted"
	- write foo "the files."
	- write foo " "
	- close foo

FRC :
	@ ! This clause is to force other clauses to happen.
