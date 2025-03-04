/*	$OpenBSD: ipgphy.c,v 1.10 2008/06/10 21:18:41 brad Exp $	*/

/*-
 * Copyright (c) 2006, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Driver for the IC Plus IP1000A 10/100/1000 PHY.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/ipgphyreg.h>

#include <dev/pci/if_stgereg.h>

int ipgphy_probe(struct device *, void *, void *);
void ipgphy_attach(struct device *, struct device *, void *);

struct cfattach ipgphy_ca = {
	sizeof(struct mii_softc), ipgphy_probe, ipgphy_attach, mii_phy_detach,
	    mii_phy_activate
};

struct cfdriver ipgphy_cd = {
	NULL, "ipgphy", DV_DULL
};

int	ipgphy_service(struct mii_softc *, struct mii_data *, int);
void	ipgphy_status(struct mii_softc *);
int	ipgphy_mii_phy_auto(struct mii_softc *);
void	ipgphy_load_dspcode(struct mii_softc *);
void	ipgphy_reset(struct mii_softc *);

const struct mii_phy_funcs ipgphy_funcs = {
	ipgphy_service, ipgphy_status, ipgphy_reset,
};

static const struct mii_phydesc ipgphys[] = {
	{ MII_OUI_ICPLUS,		MII_MODEL_ICPLUS_IP1000A,
	  MII_STR_ICPLUS_IP1000A },

	{ 0,
	  NULL },
};

int
ipgphy_probe(struct device *parent, void *match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, ipgphys) != NULL)
		return (10);

	return (0);
}

void
ipgphy_attach(struct device *parent, struct device *self, void *aux)
{
	struct mii_softc *sc = (struct mii_softc *)self;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;

	mpd = mii_phy_match(ma, ipgphys);
	printf(": %s, rev. %d\n", mpd->mpd_name, MII_REV(ma->mii_id2));

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_funcs = &ipgphy_funcs;
	sc->mii_pdata = mii;
	sc->mii_anegticks = MII_ANEGTICKS_GIGE;

	sc->mii_flags |= MIIF_NOISOLATE;

#define ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
	    IPGPHY_BMCR_10);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
	    IPGPHY_BMCR_10 | IPGPHY_BMCR_FDX);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
	    IPGPHY_BMCR_100);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
	    IPGPHY_BMCR_100 | IPGPHY_BMCR_FDX);
	/* 1000baseT half-duplex, really supported? */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0, sc->mii_inst),
	    IPGPHY_BMCR_1000);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX, sc->mii_inst),
	    IPGPHY_BMCR_1000 | IPGPHY_BMCR_FDX);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
#undef ADD

	PHY_RESET(sc);
}

int
ipgphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	uint32_t gig, reg, speed;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, IPGPHY_MII_BMCR);
			PHY_WRITE(sc, IPGPHY_MII_BMCR,
			    reg | IPGPHY_BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		PHY_RESET(sc);

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			(void)ipgphy_mii_phy_auto(sc);
			goto done;
			break;

		case IFM_1000_T:
			/*
			 * XXX
			 * Manual 1000baseT setting doesn't seem to work.
			 */
			speed = IPGPHY_BMCR_1000;
			break;

		case IFM_100_TX:
			speed = IPGPHY_BMCR_100;
			break;

		case IFM_10_T:
			speed = IPGPHY_BMCR_10;
			break;

		default:
			return (EINVAL);
		}

		if (((ife->ifm_media & IFM_GMASK) & IFM_FDX) != 0) {
			speed |= IPGPHY_BMCR_FDX;
			gig = IPGPHY_1000CR_1000T_FDX;
		} else
			gig = IPGPHY_1000CR_1000T;

		PHY_WRITE(sc, IPGPHY_MII_1000CR, 0);
		PHY_WRITE(sc, IPGPHY_MII_BMCR, speed);

		if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T)
			break;

		PHY_WRITE(sc, IPGPHY_MII_1000CR, gig);
		PHY_WRITE(sc, IPGPHY_MII_BMCR, speed);

		if (mii->mii_media.ifm_media & IFM_ETH_MASTER)
			gig |= IPGPHY_1000CR_MASTER | IPGPHY_1000CR_MANUAL;

		PHY_WRITE(sc, IPGPHY_MII_1000CR, gig);

done:
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;
			break;
		}

		/*
		 * check for link.
		 */
		reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK) {
			sc->mii_ticks = 0;
			break;
		}

		/* Announce link loss right after it happens */
		if (sc->mii_ticks++ == 0)
			break;

		/*
		 * Only retry autonegotiation every mii_anegticks seconds.
		 */
		if (sc->mii_ticks <= sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		ipgphy_mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	mii_phy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

void
ipgphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	uint32_t bmsr, bmcr, stat;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, IPGPHY_MII_BMSR) |
	    PHY_READ(sc, IPGPHY_MII_BMSR);
	if ((bmsr & IPGPHY_BMSR_LINK) != 0)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, IPGPHY_MII_BMCR);
	if ((bmcr & IPGPHY_BMCR_LOOP) != 0)
		mii->mii_media_active |= IFM_LOOP;

	if ((bmcr & IPGPHY_BMCR_AUTOEN) != 0) {
		if ((bmsr & IPGPHY_BMSR_ANEGCOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
	}

	stat = PHY_READ(sc, STGE_PhyCtrl);
	switch (PC_LinkSpeed(stat)) {
	case PC_LinkSpeed_Down:
		mii->mii_media_active |= IFM_NONE;
		return;
	case PC_LinkSpeed_10:
		mii->mii_media_active |= IFM_10_T;
		break;
	case PC_LinkSpeed_100:
		mii->mii_media_active |= IFM_100_TX;
		break;
	case PC_LinkSpeed_1000:
		mii->mii_media_active |= IFM_1000_T;
		break;
	}

	if ((stat & PC_PhyDuplexStatus) != 0)
		mii->mii_media_active |= mii_phy_flowstatus(sc) | IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	stat = PHY_READ(sc, IPGPHY_MII_1000SR);
	if ((IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) &&
	    stat & IPGPHY_1000SR_MASTER)
		mii->mii_media_active |= IFM_ETH_MASTER;
}

int
ipgphy_mii_phy_auto(struct mii_softc *sc)
{
	uint32_t reg;

	reg = IPGPHY_ANAR_10T | IPGPHY_ANAR_10T_FDX |
	      IPGPHY_ANAR_100TX | IPGPHY_ANAR_100TX_FDX;

	if (sc->mii_flags & MIIF_DOPAUSE)
		reg |= IPGPHY_ANAR_PAUSE | IPGPHY_ANAR_APAUSE;

	PHY_WRITE(sc, IPGPHY_MII_ANAR, reg);
	reg = IPGPHY_1000CR_1000T | IPGPHY_1000CR_1000T_FDX;
	reg |= IPGPHY_1000CR_MASTER;
	PHY_WRITE(sc, IPGPHY_MII_1000CR, reg);
	PHY_WRITE(sc, IPGPHY_MII_BMCR, (IPGPHY_BMCR_FDX |
	    IPGPHY_BMCR_AUTOEN | IPGPHY_BMCR_STARTNEG));

	return (EJUSTRETURN);
}

void
ipgphy_load_dspcode(struct mii_softc *sc)
{
	PHY_WRITE(sc, 31, 0x0001);
	PHY_WRITE(sc, 27, 0x01e0);
	PHY_WRITE(sc, 31, 0x0002);
	PHY_WRITE(sc, 27, 0xeb8e);
	PHY_WRITE(sc, 31, 0x0000);
	PHY_WRITE(sc, 30, 0x005e);
	PHY_WRITE(sc, 9, 0x0700);

	DELAY(50);
}

void
ipgphy_reset(struct mii_softc *sc)
{
	struct stge_softc *stge_sc;
	struct ifnet *ifp;
	uint32_t reg;

	mii_phy_reset(sc);

	/* clear autoneg/full-duplex as we don't want it after reset */
	reg = PHY_READ(sc, IPGPHY_MII_BMCR);
	reg &= ~(IPGPHY_BMCR_AUTOEN | IPGPHY_BMCR_FDX);
	PHY_WRITE(sc, MII_BMCR, reg);

	ifp = sc->mii_pdata->mii_ifp;

	if (strcmp(ifp->if_xname, "stge") == 0) {
		stge_sc = ifp->if_softc;
		if (stge_sc->sc_rev >= 0x40 && stge_sc->sc_rev <= 0x4e)
			ipgphy_load_dspcode(sc);
	}
}
