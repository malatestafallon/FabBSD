/*	$OpenBSD: device.h,v 1.37 2007/11/24 18:31:05 dlg Exp $	*/
/*	$NetBSD: device.h,v 1.15 1996/04/09 20:55:24 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)device.h	8.2 (Berkeley) 2/17/94
 */

#ifndef _SYS_DEVICE_H_
#define	_SYS_DEVICE_H_

#include <sys/queue.h>

/*
 * Minimal device structures.
 * Note that all ``system'' device types are listed here.
 */
enum devclass {
	DV_DULL,		/* generic, no special info */
	DV_CPU,			/* CPU (carries resource utilization) */
	DV_DISK,		/* disk drive (label, etc) */
	DV_IFNET,		/* network interface */
	DV_TAPE,		/* tape device */
	DV_TTY			/* serial line interface (???) */
};

/*
 * Actions for ca_activate.
 */
enum devact {
	DVACT_ACTIVATE,		/* activate the device */
	DVACT_DEACTIVATE	/* deactivate the device */
};

struct device {
	enum	devclass dv_class;	/* this device's classification */
	TAILQ_ENTRY(device) dv_list;	/* entry on list of all devices */
	struct	cfdata *dv_cfdata;	/* config data that found us */
	int	dv_unit;		/* device unit number */
	char	dv_xname[16];		/* external name (name + unit) */
	struct	device *dv_parent;	/* pointer to parent device */
	int	dv_flags;		/* misc. flags; see below */
	int	dv_ref;			/* ref count */
};

/* dv_flags */
#define	DVF_ACTIVE	0x0001		/* device is activated */

TAILQ_HEAD(devicelist, device);

/*
 * Configuration data (i.e., data placed in ioconf.c).
 */
struct cfdata {
	struct	cfattach *cf_attach;	/* config attachment */
	struct	cfdriver *cf_driver;	/* config driver */
	short	cf_unit;		/* unit number */
	short	cf_fstate;		/* finding state (below) */
	int	*cf_loc;		/* locators (machine dependent) */
	int	cf_flags;		/* flags from config */
	short	*cf_parents;		/* potential parents */
	int	cf_locnames;		/* start of names */
	void	(**cf_ivstubs)(void);	/* config-generated vectors, if any */
	short	cf_starunit1;		/* 1st usable unit number by STAR */
};
extern struct cfdata cfdata[];
#define FSTATE_NOTFOUND	0	/* has not been found */
#define	FSTATE_FOUND	1	/* has been found */
#define	FSTATE_STAR	2	/* duplicable */
#define FSTATE_DNOTFOUND 3	/* has not been found, and is disabled */
#define FSTATE_DSTAR	4	/* duplicable, and is disabled */

typedef int (*cfmatch_t)(struct device *, void *, void *);
typedef void (*cfscan_t)(struct device *, void *);

/*
 * `configuration' attachment and driver (what the machine-independent
 * autoconf uses).  As devices are found, they are applied against all
 * the potential matches.  The one with the best match is taken, and a
 * device structure (plus any other data desired) is allocated.  Pointers
 * to these are placed into an array of pointers.  The array itself must
 * be dynamic since devices can be found long after the machine is up
 * and running.
 *
 * Devices can have multiple configuration attachments if they attach
 * to different attributes (busses, or whatever), to allow specification
 * of multiple match and attach functions.  There is only one configuration
 * driver per driver, so that things like unit numbers and the device
 * structure array will be shared.
 */
struct cfattach {
	size_t	  ca_devsize;		/* size of dev data (for malloc) */
	cfmatch_t ca_match;		/* returns a match level */
	void	(*ca_attach)(struct device *, struct device *, void *);
	int	(*ca_detach)(struct device *, int);
	int	(*ca_activate)(struct device *, enum devact);
};

/* Flags given to config_detach(), and the ca_detach function. */
#define	DETACH_FORCE	0x01		/* force detachment; hardware gone */
#define	DETACH_QUIET	0x02		/* don't print a notice */

struct cfdriver {
	void	**cd_devs;		/* devices found */
	char	*cd_name;		/* device name */
	enum	devclass cd_class;	/* device classification */
	int	cd_indirect;		/* indirectly configure subdevices */
	int	cd_ndevs;		/* size of cd_devs array */
};

/*
 * Configuration printing functions, and their return codes.  The second
 * argument is NULL if the device was configured; otherwise it is the name
 * of the parent device.  The return value is ignored if the device was
 * configured, so most functions can return UNCONF unconditionally.
 */
typedef int (*cfprint_t)(void *, const char *);
#define	QUIET	0		/* print nothing */
#define	UNCONF	1		/* print " not configured\n" */
#define	UNSUPP	2		/* print " not supported\n" */

/*
 * Pseudo-device attach information (function + number of pseudo-devs).
 */
struct pdevinit {
	void	(*pdev_attach)(int);
	int	pdev_count;
};

#ifdef _KERNEL
struct cftable {
	struct cfdata *tab;
	TAILQ_ENTRY(cftable) list;
};
TAILQ_HEAD(cftable_head, cftable);

extern struct devicelist alldevs;	/* list of all devices */

extern int autoconf_verbose;
extern __volatile int config_pending;	/* semaphore for mountroot */

void config_init(void);
void *config_search(cfmatch_t, struct device *, void *);
void *config_rootsearch(cfmatch_t, char *, void *);
struct device *config_found_sm(struct device *, void *, cfprint_t,
    cfmatch_t);
struct device *config_rootfound(char *, void *);
void config_scan(cfscan_t, struct device *);
struct device *config_attach(struct device *, void *, void *, cfprint_t);
int config_detach(struct device *, int);
int config_detach_children(struct device *, int);
int config_activate(struct device *);
int config_deactivate(struct device *);
int config_activate_children(struct device *, enum devact);
struct device *config_make_softc(struct device *parent,
    struct cfdata *cf);
void config_defer(struct device *, void (*)(struct device *));
void config_pending_incr(void);
void config_pending_decr(void);

struct device *device_lookup(struct cfdriver *, int unit);
void device_ref(struct device *);
void device_unref(struct device *);

struct nam2blk {
	char	*name;
	int	maj;
};

int	findblkmajor(struct device *dv);
char	*findblkname(int);
void	setroot(struct device *, int, int);
struct	device *getdisk(char *str, int len, int defpart, dev_t *devp);
struct	device *parsedisk(char *str, int len, int defpart, dev_t *devp);
void	device_register(struct device *, void *);

int loadfirmware(const char *name, u_char **bufp, size_t *buflen);
#define FIRMWARE_MAX	5*1024*1024

/* compatibility definitions */
#define config_found(d, a, p)	config_found_sm((d), (a), (p), NULL)

#endif /* _KERNEL */

#endif /* !_SYS_DEVICE_H_ */
