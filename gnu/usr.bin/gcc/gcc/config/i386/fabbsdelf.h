/* Configuration for an FabBSD i386 target.
   
   Copyright (C) 1999 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* This gets defined in tm.h->linux.h->svr4.h, and keeps us from using
   libraries compiled with the native cc, so undef it. */
#undef NO_DOLLAR_IN_LABEL

/* Override the default comment-starter of "/".  */
#undef ASM_COMMENT_START
#define ASM_COMMENT_START "#"

#undef DBX_REGISTER_NUMBER
#define DBX_REGISTER_NUMBER(n)  svr4_dbx_register_map[n]

/* This goes away when the math-emulator is fixed */
#undef TARGET_DEFAULT
#define TARGET_DEFAULT \
  (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS | MASK_NO_FANCY_MATH_387)

/* Run-time target specifications */

#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
    	FABBSD_OS_CPP_BUILTINS_ELF();		\
    }						\
  while (0)

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
	%{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} \
	crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"

/* Layout of source language data types.  */

/* This must agree with <machine/_types.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#undef INTMAX_TYPE
#define INTMAX_TYPE "long long int"

#undef UINTMAX_TYPE
#define UINTMAX_TYPE "long long unsigned int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD

/* Assembler format: overall framework.  */

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

#undef SET_ASM_OP
#define SET_ASM_OP	"\t.set\t"

/* The following macros were originally stolen from i386v4.h.
   These have to be defined to get PIC code correct.  */

/* Assembler format: dispatch tables.  */

/* Assembler format: sections.  */

/* Stack & calling: aggregate returns.  */

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Assembler format: alignment output.  */

#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE,LOG,MAX_SKIP) \
  if ((LOG) != 0) {\
    if ((MAX_SKIP) == 0) fprintf ((FILE), "\t.p2align %d\n", (LOG)); \
    else fprintf ((FILE), "\t.p2align %d,,%d\n", (LOG), (MAX_SKIP)); \
  }
#endif

/* Stack & calling: profiling.  */

/* FabBSD's profiler recovers all information from the stack pointer.
   The icky part is not here, but in machine/profile.h.  */
#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)  \
  fputs (flag_pic ? "\tcall __mcount@PLT\n": "\tcall __mcount\n", FILE);

/* Assembler format: exception region output.  */

/* Assembler format: alignment output.  */

/* Note that we pick up ASM_OUTPUT_MAX_SKIP_ALIGN from i386/gas.h */

/* Note that we pick up ASM_OUTPUT_MI_THUNK from unix.h.  */

#undef LINK_SPEC
#define LINK_SPEC \
  "%{!shared:%{!nostdlib:%{!r*:%{!e*:-e __start}}}} \
   %{shared:-shared} %{R*} \
   %{static:-Bstatic} \
   %{!static:-Bdynamic} \
   %{assert*} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so}"

#define FABBSD_HAS_CORRECT_SPECS

/* pick up defines for mprotect (used in TRANSFER_FROM_TRANPOLINE) */
#include <sys/types.h>
#include <sys/mman.h>
