/*	$FabBSD$	*/
/*	$OpenBSD: if_pgt_cardbus.c,v 1.7 2006/11/11 10:28:20 maja Exp $ */

/*
 * Copyright (c) 2006 Marcus Glocker <mglocker@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * CardBus front-end for the PrismGT
 */

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/pgtreg.h>
#include <dev/ic/pgtvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

struct pgt_cardbus_softc {
	struct pgt_softc	 sc_pgt;
	cardbus_devfunc_t	 sc_ct;
	cardbustag_t		 sc_tag;
	int			 sc_intrline;

	void			*sc_ih;
	bus_size_t		 sc_mapsize;
	pcireg_t		 sc_bar0_val;
};

int	pgt_cardbus_match(struct device *, void *, void *);
void	pgt_cardbus_attach(struct device *, struct device *, void *);
int	pgt_cardbus_detach(struct device *, int);
int	pgt_cardbus_enable(struct pgt_softc *);
void	pgt_cardbus_disable(struct pgt_softc *);
void	pgt_cardbus_power(struct pgt_softc *, int);
void	pgt_cardbus_setup(struct pgt_cardbus_softc *);

struct cfattach pgt_cardbus_ca = {
	sizeof(struct pgt_cardbus_softc), pgt_cardbus_match, pgt_cardbus_attach,
	pgt_cardbus_detach
};

const struct cardbus_matchid pgt_cardbus_devices[] = {
	{ PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3877 },
	{ PCI_VENDOR_INTERSIL, PCI_PRODUCT_INTERSIL_ISL3890 },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CRWE154G72 }
};

int
pgt_cardbus_match(struct device *parent, void *match, void *aux)
{
	return (cardbus_matchbyid((struct cardbus_attach_args *)aux,
	    pgt_cardbus_devices,
	    sizeof(pgt_cardbus_devices) / sizeof(pgt_cardbus_devices[0])));
}

void
pgt_cardbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct pgt_cardbus_softc *csc = (struct pgt_cardbus_softc *)self;
	struct pgt_softc *sc = &csc->sc_pgt;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_addr_t base;
	int error;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;
	csc->sc_intrline = ca->ca_intrline;

	/* power management hooks */
	sc->sc_enable = pgt_cardbus_enable;
	sc->sc_disable = pgt_cardbus_disable;
	sc->sc_power = pgt_cardbus_power;

	/* remember chipset */
	if (CARDBUS_PRODUCT(ca->ca_id) == PCI_PRODUCT_INTERSIL_ISL3877)
		sc->sc_flags |= SC_ISL3877;

	/* map control / status registers */
	error = Cardbus_mapreg_map(ct, CARDBUS_BASE0_REG,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_iotag, &sc->sc_iohandle, &base, &csc->sc_mapsize);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}
	csc->sc_bar0_val = base | CARDBUS_MAPREG_TYPE_MEM;

	/* disable all interrupts */
	bus_space_write_4(sc->sc_iotag, sc->sc_iohandle, PGT_REG_INT_EN, 0);
	(void)bus_space_read_4(sc->sc_iotag, sc->sc_iohandle, PGT_REG_INT_EN);
	DELAY(PGT_WRITEIO_DELAY);

	/* set up the PCI configuration registers */
	pgt_cardbus_setup(csc);

	printf(": irq %d\n", csc->sc_intrline);

	if (rootvp == NULL)
		mountroothook_establish(pgt_attach, sc);
	else
		pgt_attach(sc);
}

int
pgt_cardbus_detach(struct device *self, int flags)
{
	struct pgt_cardbus_softc *csc = (struct pgt_cardbus_softc *)self;
	struct pgt_softc *sc = &csc->sc_pgt;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	int error;

	error = pgt_detach(sc);
	if (error != 0)
		return (error);

	/* unhook the interrupt handler */
	if (csc->sc_ih != NULL) {
		cardbus_intr_disestablish(cc, cf, csc->sc_ih);
		csc->sc_ih = NULL;
	}

	/* release bus space and close window */
	Cardbus_mapreg_unmap(ct, CARDBUS_BASE0_REG,
	    sc->sc_iotag, sc->sc_iohandle, csc->sc_mapsize);

	return (0);
}

int
pgt_cardbus_enable(struct pgt_softc *sc)
{
        struct pgt_cardbus_softc *csc = (struct pgt_cardbus_softc *)sc;
        cardbus_devfunc_t ct = csc->sc_ct;
        cardbus_chipset_tag_t cc = ct->ct_cc;
        cardbus_function_tag_t cf = ct->ct_cf;

        /* power on the socket */
        Cardbus_function_enable(ct);

        /* setup the PCI configuration registers */
        pgt_cardbus_setup(csc);

        /* map and establish the interrupt handler */
        csc->sc_ih = cardbus_intr_establish(cc, cf, csc->sc_intrline, IPL_NET,
            pgt_intr, sc, sc->sc_dev.dv_xname);
        if (csc->sc_ih == NULL) {
                printf("%s: could not establish interrupt at %d\n",
                    sc->sc_dev.dv_xname, csc->sc_intrline);
                Cardbus_function_disable(ct);
                return (1);
        }

        return (0);
}

void
pgt_cardbus_disable(struct pgt_softc *sc)
{
	struct pgt_cardbus_softc *csc = (struct pgt_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* unhook the interrupt handler */
	cardbus_intr_disestablish(cc, cf, csc->sc_ih);
	csc->sc_ih = NULL;

	/* power down the socket */
	Cardbus_function_disable(ct);
}

void
pgt_cardbus_power(struct pgt_softc *sc, int why)
{
	if (why == PWR_RESUME)
		if (sc->sc_enable != NULL)
			(*sc->sc_enable)(sc);
	if (why == PWR_SUSPEND)
		if (sc->sc_disable != NULL)
			(*sc->sc_disable)(sc);
}

void
pgt_cardbus_setup(struct pgt_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg;

	/* program the BAR */
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_BASE0_REG,
	    csc->sc_bar0_val);

	/* make sure the right access type is on the cardbus bridge */
 	(*cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(*cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* enable the appropriate bits in the PCI CSR */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	reg |= CARDBUS_COMMAND_MASTER_ENABLE | CARDBUS_COMMAND_MEM_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG,
	    reg);
}
