/*---------------------------------------------------------------------------

  VMSmunch.h

  A few handy #defines, plus the contents of three header files from Joe
  Meadows' FILE program.  Used by VMSmunch and by various routines which
  call VMSmunch (e.g., in Zip and UnZip).

	 7-Apr-2010     Steven Schweda <sms@antinode.info>
			Added generic NAMX (NAM or NAML) and related
			macros.

	02-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved definition of VMStimbuf struct from vmsmunch.c
			to here.

	06-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved "contents of three header files" (not needed by
			callers of vmsmunch) to vmsmunch_private.h .

	07-Apr-1994	Richard Levitte levitte@e.kth.se
			Inserted a forward declaration of VMSmunch.

	08-May-1998	Richard Levitte richard@levitte.org
			Inserted definition of SET_EXACT_SIZE.

  ---------------------------------------------------------------------------*/

#define GET_TIMES       4
#define SET_TIMES       0
#define GET_RTYPE       1
#define CHANGE_RTYPE    2
#define RESTORE_RTYPE   3
#define SET_EXACT_SIZE  5
#define SET_MODE        6
#define SET_PROT        7


/*
 *  Under Alpha (DEC C in VAXC mode) and under `good old' VAXC, the FIB unions
 *  are declared as variant_unions.  DEC C (Alpha) in ANSI modes and third
 *  party compilers which do not support `variant_union' define preprocessor
 *  symbols to `hide' the "intermediate union/struct" names from the
 *  programmer's API.
 *  We check the presence of these defines and for DEC's FIBDEF.H defining
 *  __union as variant_union to make sure we access the structure correctly.
 */
#define variant_union 1
#if defined(fib$w_did) || (defined(__union) && (__union == variant_union))
#  define FIB$W_DID     fib$w_did
#  define FIB$W_FID     fib$w_fid
#  define FIB$L_ACCTL   fib$l_acctl
#  define FIB$W_EXCTL   fib$w_exctl
#else
#  define FIB$W_DID     fib$r_did_overlay.fib$w_did
#  define FIB$W_FID     fib$r_fid_overlay.fib$w_fid
#  define FIB$L_ACCTL   fib$r_acctl_overlay.fib$l_acctl
#  define FIB$W_EXCTL   fib$r_exctl_overlay.fib$w_exctl
#endif
#undef variant_union


/* Define macros for use with either NAM or NAML. */

#ifdef NAML$C_MAXRSS            /* NAML is available.  Use it. */

#  define NAMX NAML

#  define FAB_OR_NAML( fab, nam) nam
#  define FAB_OR_NAML_DNA naml$l_long_defname
#  define FAB_OR_NAML_DNS naml$l_long_defname_size
#  define FAB_OR_NAML_FNA naml$l_long_filename
#  define FAB_OR_NAML_FNS naml$l_long_filename_size

#  define CC_RMS_NAMX cc$rms_naml
#  define FAB_L_NAMX fab$l_naml
#  define NAMX_W_DID naml$w_did
#  define NAMX_T_DVI naml$t_dvi
#  define NAMX_L_ESA naml$l_long_expand
#  define NAMX_B_ESL naml$l_long_expand_size
#  define NAMX_B_ESS naml$l_long_expand_alloc
#  define NAMX_W_FID naml$w_fid
#  define NAMX_L_FNB naml$l_fnb
#  define NAMX_L_RSA naml$l_long_result
#  define NAMX_B_RSL naml$l_long_result_size
#  define NAMX_B_RSS naml$l_long_result_alloc
#  define NAMX_MAXRSS NAML$C_MAXRSS
#  define NAMX_B_NOP naml$b_nop
#  define NAMX_M_EXP_DEV NAML$M_EXP_DEV
#  define NAMX_M_SYNCHK NAML$M_SYNCHK
#  define NAMX_B_DEV naml$l_long_dev_size
#  define NAMX_L_DEV naml$l_long_dev
#  define NAMX_B_DIR naml$l_long_dir_size
#  define NAMX_L_DIR naml$l_long_dir
#  define NAMX_B_NAME naml$l_long_name_size
#  define NAMX_L_NAME naml$l_long_name
#  define NAMX_B_TYPE naml$l_long_type_size
#  define NAMX_L_TYPE naml$l_long_type
#  define NAMX_B_VER naml$l_long_ver_size
#  define NAMX_L_VER naml$l_long_ver

#else /* def NAML$C_MAXRSS */   /* NAML is not available.  Use NAM. */

#  define NAMX NAM

#  define FAB_OR_NAML( fab, nam) fab
#  define FAB_OR_NAML_DNA fab$l_dna
#  define FAB_OR_NAML_DNS fab$b_dns
#  define FAB_OR_NAML_FNA fab$l_fna
#  define FAB_OR_NAML_FNS fab$b_fns

#  define CC_RMS_NAMX cc$rms_nam
#  define FAB_L_NAMX fab$l_nam
#  define NAMX_W_DID nam$w_did
#  define NAMX_T_DVI nam$t_dvi
#  define NAMX_L_ESA nam$l_esa
#  define NAMX_B_ESL nam$b_esl
#  define NAMX_B_ESS nam$b_ess
#  define NAMX_W_FID nam$w_fid
#  define NAMX_L_FNB nam$l_fnb
#  define NAMX_L_RSA nam$l_rsa
#  define NAMX_B_RSL nam$b_rsl
#  define NAMX_B_RSS nam$b_rss
#  define NAMX_MAXRSS NAM$C_MAXRSS
#  define NAMX_B_NOP nam$b_nop
#  define NAMX_M_EXP_DEV NAM$M_EXP_DEV
#  define NAMX_M_SYNCHK NAM$M_SYNCHK
#  define NAMX_B_DEV nam$b_dev
#  define NAMX_L_DEV nam$l_dev
#  define NAMX_B_DIR nam$b_dir
#  define NAMX_L_DIR nam$l_dir
#  define NAMX_B_NAME nam$b_name
#  define NAMX_L_NAME nam$l_name
#  define NAMX_B_TYPE nam$b_type
#  define NAMX_L_TYPE nam$l_type
#  define NAMX_B_VER nam$b_ver
#  define NAMX_L_VER nam$l_ver

#endif /* def NAML$C_MAXRSS */


struct VMStimbuf {      /* VMSmunch */
    char *actime;       /* VMS revision date, ASCII format */
    char *modtime;      /* VMS creation date, ASCII format */
};

extern int VMSmunch();
