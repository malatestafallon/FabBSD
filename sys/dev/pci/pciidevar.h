/*	$OpenBSD: pciidevar.h,v 1.16 2004/10/17 19:00:46 grange Exp $	*/
/*	$NetBSD: pciidevar.h,v 1.6 2001/01/12 16:04:00 bouyer Exp $	*/

/*
 * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PCI_PCIIDEVAR_H_
#define _DEV_PCI_PCIIDEVAR_H_

/*
 * PCI IDE driver exported software structures.
 *
 * Author: Christopher G. Demetriou, March 2, 1998.
 */

#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

/*
 * While standard PCI IDE controllers only have 2 channels, it is
 * common for PCI SATA controllers to have more.  Here we define
 * the maximum number of channels that any one PCI IDE device can
 * have.
 */
#define PCIIDE_MAX_CHANNELS	4

struct pciide_softc {
	struct wdc_softc	sc_wdcdev;	/* common wdc definitions */
	pci_chipset_tag_t	sc_pc;		/* PCI registers info */
	pcitag_t		sc_tag;
	void			*sc_pci_ih;	/* PCI interrupt handle */
	int			sc_dma_ok;	/* bus-master DMA info */
	bus_space_tag_t		sc_dma_iot;
	bus_space_handle_t	sc_dma_ioh;
	bus_dma_tag_t		sc_dmat;

	/*
	 * Some controllers might have DMA restrictions other than
	 * the norm.
	 */
	bus_size_t		sc_dma_maxsegsz;
	bus_size_t		sc_dma_boundary;

	/* Chip description */
	const struct pciide_product_desc *sc_pp;
	/* Chip revision */
	int sc_rev;
	/* common definitions */
	struct channel_softc *wdc_chanarray[PCIIDE_MAX_CHANNELS];
	/* internal bookkeeping */
	struct pciide_channel {			/* per-channel data */
		struct channel_softc wdc_channel; /* generic part */
		const char	*name;
		int		hw_ok;		/* hardware mapped & OK? */
		int		compat;		/* is it compat? */
		int             dma_in_progress;
		void		*ih;		/* compat or pci handle */
		bus_space_handle_t ctl_baseioh;	/* ctrl regs blk, native mode */
		/* DMA tables and DMA map for xfer, for each drive */
		struct pciide_dma_maps {
			bus_dmamap_t    dmamap_table;
			struct idedma_table *dma_table;
			bus_dmamap_t    dmamap_xfer;
			int dma_flags;
		} dma_maps[2];
		/*
		 * Some controllers require certain bits to
		 * always be set for proper operation of the
		 * controller.  Set those bits here, if they're
		 * required.
		 */
		uint8_t		idedma_cmd;
	} pciide_channels[PCIIDE_MAX_CHANNELS];

	/* Chip-specific private data */
	void *sc_cookie;

	/* DMA registers access functions */
	u_int8_t (*sc_dmacmd_read)(struct pciide_softc *, int);
	void (*sc_dmacmd_write)(struct pciide_softc *, int, u_int8_t);
	u_int8_t (*sc_dmactl_read)(struct pciide_softc *, int);
	void (*sc_dmactl_write)(struct pciide_softc *, int, u_int8_t);
	void (*sc_dmatbl_write)(struct pciide_softc *, int, u_int32_t);
};

#define PCIIDE_DMACMD_READ(sc, chan) \
	(sc)->sc_dmacmd_read((sc), (chan))
#define PCIIDE_DMACMD_WRITE(sc, chan, val) \
	(sc)->sc_dmacmd_write((sc), (chan), (val))
#define PCIIDE_DMACTL_READ(sc, chan) \
	(sc)->sc_dmactl_read((sc), (chan))
#define PCIIDE_DMACTL_WRITE(sc, chan, val) \
	(sc)->sc_dmactl_write((sc), (chan), (val))
#define PCIIDE_DMATBL_WRITE(sc, chan, val) \
	(sc)->sc_dmatbl_write((sc), (chan), (val))

/*
 * Functions defined by machine-dependent code.
 */

/* Attach compat interrupt handler, returning handle or NULL if failed. */
#if !defined(pciide_machdep_compat_intr_establish)
void	*pciide_machdep_compat_intr_establish(struct device *,
	    struct pci_attach_args *, int, int (*)(void *), void *);
void	pciide_machdep_compat_intr_disestablish(pci_chipset_tag_t pc,
	    void *);
#endif

#endif	/* !_DEV_PCI_PCIIDEVAR_H_ */
