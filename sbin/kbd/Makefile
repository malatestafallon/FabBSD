#	$OpenBSD: Makefile,v 1.32 2008/05/13 21:29:37 miod Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "aviion" && ${MACHINE} != "landisk" && \
    ${MACHINE} != "mvme68k" && ${MACHINE} != "mvme88k" && \
    ${MACHINE} != "socppc"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha amd64 hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 vax zaurus

.include <bsd.prog.mk>
