/*	$OpenBSD: ukbd.c,v 1.43 2008/06/26 05:42:18 ray Exp $	*/
/*      $NetBSD: ukbd.c,v 1.85 2003/03/11 16:44:00 augustss Exp $        */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/uhidev.h>
#include <dev/usb/hid.h>
#include <dev/usb/ukbdvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#ifdef UKBD_DEBUG
#define DPRINTF(x)	do { if (ukbddebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (ukbddebug>(n)) printf x; } while (0)
int	ukbddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define MAXKEYCODE 6
#define MAXMOD 8		/* max 32 */

struct ukbd_data {
	u_int32_t	modifiers;
	u_int8_t	keycode[MAXKEYCODE];
};

#define PRESS    0x000
#define RELEASE  0x100
#define CODEMASK 0x0ff

#if defined(WSDISPLAY_COMPAT_RAWKBD)
#define NN 0			/* no translation */
/*
 * Translate USB keycodes to US keyboard XT scancodes.
 * Scancodes >= 0x80 represent EXTENDED keycodes.
 *
 * See http://www.microsoft.com/whdc/device/input/Scancode.mspx
 */
const u_int8_t ukbd_trtab[256] = {
      NN,   NN,   NN,   NN, 0x1e, 0x30, 0x2e, 0x20, /* 00 - 07 */
    0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, /* 08 - 0f */
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, /* 10 - 17 */
    0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, /* 18 - 1f */
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, /* 20 - 27 */
    0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a, /* 28 - 2f */
    0x1b, 0x2b, 0x2b, 0x27, 0x28, 0x29, 0x33, 0x34, /* 30 - 37 */
    0x35, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, /* 38 - 3f */
    0x41, 0x42, 0x43, 0x44, 0x57, 0x58, 0xaa, 0x46, /* 40 - 47 */
    0x7f, 0xd2, 0xc7, 0xc9, 0xd3, 0xcf, 0xd1, 0xcd, /* 48 - 4f */
    0xcb, 0xd0, 0xc8, 0x45, 0xb5, 0x37, 0x4a, 0x4e, /* 50 - 57 */
    0x9c, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, /* 58 - 5f */
    0x48, 0x49, 0x52, 0x53, 0x56, 0xdd, 0x84, 0x59, /* 60 - 67 */
    0x5d, 0x5e, 0x5f,   NN,   NN,   NN,   NN,   NN, /* 68 - 6f */
      NN,   NN,   NN,   NN, 0x97,   NN, 0x93, 0x95, /* 70 - 77 */
    0x91, 0x92, 0x94, 0x9a, 0x96, 0x98, 0x99, 0xa0, /* 78 - 7f */
    0xb0, 0xae,   NN,   NN,   NN, 0x7e,   NN, 0x73, /* 80 - 87 */
    0x70, 0x7d, 0x79, 0x7b, 0x5c,   NN,   NN,   NN, /* 88 - 8f */
      NN,   NN, 0x78, 0x77, 0x76,   NN,   NN,   NN, /* 90 - 97 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* 98 - 9f */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* a0 - a7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* a8 - af */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* b0 - b7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* b8 - bf */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* c0 - c7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* c8 - cf */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* d0 - d7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* d8 - df */
    0x1d, 0x2a, 0x38, 0xdb, 0x9d, 0x36, 0xb8, 0xdc, /* e0 - e7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* e8 - ef */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* f0 - f7 */
      NN,   NN,   NN,   NN,   NN,   NN,   NN,   NN, /* f8 - ff */
};
#endif /* defined(WSDISPLAY_COMPAT_RAWKBD) */

const kbd_t ukbd_countrylayout[HCC_MAX] = {
	(kbd_t)-1,
	(kbd_t)-1,	/* arabic */
	KB_BE,		/* belgian */
	(kbd_t)-1,	/* canadian bilingual */
	KB_CF,		/* canadian french */
	(kbd_t)-1,	/* czech */
	KB_DK,		/* danish */
	(kbd_t)-1,	/* finnish */
	KB_FR,		/* french */
	KB_DE,		/* german */
	(kbd_t)-1,	/* greek */
	(kbd_t)-1,	/* hebrew */
	KB_HU,		/* hungary */
	(kbd_t)-1,	/* international (iso) */
	KB_IT,		/* italian */
	KB_JP,		/* japanese (katakana) */
	(kbd_t)-1,	/* korean */
	KB_LA,		/* latin american */
	(kbd_t)-1,	/* netherlands/dutch */
	KB_NO,		/* norwegian */
	(kbd_t)-1,	/* persian (farsi) */
	KB_PL,		/* polish */
	KB_PT,		/* portuguese */
	KB_RU,		/* russian */
	(kbd_t)-1,	/* slovakia */
	KB_ES,		/* spanish */
	KB_SF,		/* swiss french */
	KB_SG,		/* swiss german */
	(kbd_t)-1,	/* switzerland */
	(kbd_t)-1,	/* taiwan */
	KB_TR,		/* turkish Q */
	KB_UK,		/* uk */
	KB_US,		/* us */
	(kbd_t)-1,	/* yugoslavia */
	(kbd_t)-1	/* turkish F */
};

#define	SUN_HCC_MIN	0x21
#define	SUN_HCC_MAX	0x3f
const kbd_t ukbd_sunlayout[1 + SUN_HCC_MAX - SUN_HCC_MIN] = {
	KB_US,	/* 021 USA */
	KB_US,	/* 022 UNIX */
	KB_FR,	/* 023 France */
	KB_DK,	/* 024 Denmark */
	KB_DE,	/* 025 Germany */
	KB_IT,	/* 026 Italy */
	KB_NL,	/* 027 The Netherlands */
	KB_NO,	/* 028 Norway */
	KB_PT,	/* 029 Portugal */
	KB_ES,	/* 02a Spain */
	KB_SV,	/* 02b Sweden */
	KB_SF,	/* 02c Switzerland/French */
	KB_SG,	/* 02d Switzerland/German */
	KB_UK,	/* 02e Great Britain */
	-1,	/* 02f Korea */
	-1,	/* 030 Taiwan */
	KB_JP,	/* 031 Japan */
	-1,	/* 032 Canada/French */
	-1,	/* 033 Hungary */
	-1,	/* 034 Poland */
	-1,	/* 035 Czech */
	-1,	/* 036 Russia */
	-1,	/* 037 Latvia */
	-1,	/* 038 Turkey-Q5 */
	-1,	/* 039 Greece */
	-1,	/* 03a Arabic */
	-1,	/* 03b Lithuania */
	-1,	/* 03c Belgium */
	-1,	/* 03d unaffected */
	-1,	/* 03e Turkey-F5 */
	-1,	/* 03f Canada/French */
};

#define KEY_ERROR 0x01

#define MAXKEYS (MAXMOD+2*MAXKEYCODE)

struct ukbd_softc {
	struct uhidev sc_hdev;

	struct ukbd_data sc_ndata;
	struct ukbd_data sc_odata;
	struct hid_location sc_modloc[MAXMOD];
	u_int sc_nmod;
	struct {
		u_int32_t mask;
		u_int8_t key;
	} sc_mods[MAXMOD];

	struct hid_location sc_keycodeloc;
	u_int sc_nkeycode;

	char sc_enabled;

	int sc_console_keyboard;	/* we are the console keyboard */

	char sc_debounce;		/* for quirk handling */
	struct timeout sc_delay;		/* for quirk handling */
	struct ukbd_data sc_data;	/* for quirk handling */

	struct hid_location sc_numloc;
	struct hid_location sc_capsloc;
	struct hid_location sc_scroloc;
	int sc_leds;

	struct timeout sc_rawrepeat_ch;

	struct device *sc_wskbddev;
#if defined(WSDISPLAY_COMPAT_RAWKBD)
#define REP_DELAY1 400
#define REP_DELAYN 100
	int sc_rawkbd;
	int sc_nrep;
	char sc_rep[MAXKEYS];
#endif /* defined(WSDISPLAY_COMPAT_RAWKBD) */

	int sc_spl;
	int sc_polling;
	int sc_npollchar;
	u_int16_t sc_pollchars[MAXKEYS];

	u_char sc_dying;
};

#ifdef UKBD_DEBUG
#define UKBDTRACESIZE 64
struct ukbdtraceinfo {
	int unit;
	struct timeval tv;
	struct ukbd_data ud;
};
struct ukbdtraceinfo ukbdtracedata[UKBDTRACESIZE];
int ukbdtraceindex = 0;
int ukbdtrace = 0;
void ukbdtracedump(void);
void
ukbdtracedump(void)
{
	int i;
	for (i = 0; i < UKBDTRACESIZE; i++) {
		struct ukbdtraceinfo *p =
		    &ukbdtracedata[(i+ukbdtraceindex)%UKBDTRACESIZE];
		printf("%lu.%06lu: mod=0x%02x key0=0x%02x key1=0x%02x "
		       "key2=0x%02x key3=0x%02x\n",
		       p->tv.tv_sec, p->tv.tv_usec,
		       p->ud.modifiers, p->ud.keycode[0], p->ud.keycode[1],
		       p->ud.keycode[2], p->ud.keycode[3]);
	}
}
#endif

#define	UKBDUNIT(dev)	(minor(dev))
#define	UKBD_CHUNK	128	/* chunk size for read */
#define	UKBD_BSIZE	1020	/* buffer size */

int	ukbd_is_console;

void	ukbd_cngetc(void *, u_int *, int *);
void	ukbd_cnpollc(void *, int);
void	ukbd_cnbell(void *, u_int, u_int, u_int);

const struct wskbd_consops ukbd_consops = {
	ukbd_cngetc,
	ukbd_cnpollc,
	ukbd_cnbell,
};

const char *ukbd_parse_desc(struct ukbd_softc *sc);

void	(*ukbd_bell_fn)(void *, u_int, u_int, u_int, int);
void	*ukbd_bell_fn_arg;

void	ukbd_bell(u_int, u_int, u_int, int);

void	ukbd_intr(struct uhidev *addr, void *ibuf, u_int len);
void	ukbd_decode(struct ukbd_softc *sc, struct ukbd_data *ud);
void	ukbd_delayed_decode(void *addr);

int	ukbd_enable(void *, int);
void	ukbd_set_leds(void *, int);

int	ukbd_ioctl(void *, u_long, caddr_t, int, struct proc *);
#ifdef WSDISPLAY_COMPAT_RAWKBD
void	ukbd_rawrepeat(void *v);
#endif

const struct wskbd_accessops ukbd_accessops = {
	ukbd_enable,
	ukbd_set_leds,
	ukbd_ioctl,
};

extern const struct wscons_keydesc ukbd_keydesctab[];

struct wskbd_mapdata ukbd_keymapdata = {
	ukbd_keydesctab
};

int ukbd_match(struct device *, void *, void *); 
void ukbd_attach(struct device *, struct device *, void *); 
int ukbd_detach(struct device *, int); 
int ukbd_activate(struct device *, enum devact); 

struct cfdriver ukbd_cd = { 
	NULL, "ukbd", DV_DULL 
}; 

const struct cfattach ukbd_ca = { 
	sizeof(struct ukbd_softc), 
	ukbd_match, 
	ukbd_attach, 
	ukbd_detach, 
	ukbd_activate, 
};

int
ukbd_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	int size;
	void *desc;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	if (!hid_is_collection(desc, size, uha->reportid,
			       HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_KEYBOARD)))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS);
}

void
ukbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	usb_hid_descriptor_t *hid;
	u_int32_t qflags;
	const char *parseerr;
	kbd_t layout = (kbd_t)-1;
	struct wskbddev_attach_args a;

	sc->sc_hdev.sc_intr = ukbd_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	parseerr = ukbd_parse_desc(sc);
	if (parseerr != NULL) {
		printf("\n%s: attach failed, %s\n",
		       sc->sc_hdev.sc_dev.dv_xname, parseerr);
		return;
	}

	hid = usbd_get_hid_descriptor(uha->uaa->iface);

#ifdef DIAGNOSTIC
	printf(": %d modifier keys, %d key codes",
	    sc->sc_nmod, sc->sc_nkeycode);
#endif

	qflags = usbd_get_quirks(uha->parent->sc_udev)->uq_flags;
	sc->sc_debounce = (qflags & UQ_SPUR_BUT_UP) != 0;

	/*
	 * Remember if we're the console keyboard.
	 *
	 * XXX This always picks the first keyboard on the
	 * first USB bus, but what else can we really do?
	 */
	if ((sc->sc_console_keyboard = ukbd_is_console) != 0) {
		/* Don't let any other keyboard have it. */
		ukbd_is_console = 0;
	}

	if (uha->uaa->vendor == USB_VENDOR_SUN &&
	    (uha->uaa->product == USB_PRODUCT_SUN_KEYBOARD6 ||
	     uha->uaa->product == USB_PRODUCT_SUN_KEYBOARD7)) {
		/* Sun keyboard use Sun-style layout codes */
		if (hid->bCountryCode >= SUN_HCC_MIN &&
		    hid->bCountryCode <= SUN_HCC_MAX)
			layout = ukbd_sunlayout[hid->bCountryCode - SUN_HCC_MIN];
#ifdef DIAGNOSTIC
		if (hid->bCountryCode != 0)
			printf(", layout %d", hid->bCountryCode);
#endif
	} else {
		if (uha->uaa->vendor == USB_VENDOR_TOPRE &&
		    uha->uaa->product == USB_PRODUCT_TOPRE_HHKB) {
			/* ignore country code on purpose */
		} else {
			if (hid->bCountryCode <= HCC_MAX)
				layout = ukbd_countrylayout[hid->bCountryCode];
#ifdef DIAGNOSTIC
			if (hid->bCountryCode != 0)
				printf(", country code %d", hid->bCountryCode);
#endif
		}
	}
	if (layout == (kbd_t)-1) {
#ifdef UKBD_LAYOUT
		layout = UKBD_LAYOUT;
#else
		layout = KB_US;
#endif
	}
	ukbd_keymapdata.layout = layout;

	printf("\n");

	if (sc->sc_console_keyboard) {
		DPRINTF(("ukbd_attach: console keyboard sc=%p\n", sc));
		wskbd_cnattach(&ukbd_consops, sc, &ukbd_keymapdata);
		ukbd_enable(sc, 1);
	}

	a.console = sc->sc_console_keyboard;

	a.keymap = &ukbd_keymapdata;

	a.accessops = &ukbd_accessops;
	a.accesscookie = sc;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	timeout_set(&sc->sc_rawrepeat_ch, ukbd_rawrepeat, sc);
#endif
	timeout_set(&sc->sc_delay, ukbd_delayed_decode, sc);

	/* Flash the leds; no real purpose, just shows we're alive. */
	ukbd_set_leds(sc, WSKBD_LED_SCROLL | WSKBD_LED_NUM | WSKBD_LED_CAPS);
	usbd_delay_ms(uha->parent->sc_udev, 400);
	ukbd_set_leds(sc, 0);

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
ukbd_enable(void *v, int on)
{
	struct ukbd_softc *sc = v;

	if (on && sc->sc_dying)
		return (EIO);

	/* Should only be called to change state */
	if (sc->sc_enabled == on) {
		DPRINTF(("ukbd_enable: %s: bad call on=%d\n",
			 sc->sc_hdev.sc_dev.dv_xname, on));
		return (EBUSY);
	}

	DPRINTF(("ukbd_enable: sc=%p on=%d\n", sc, on));
	sc->sc_enabled = on;
	if (on) {
		return (uhidev_open(&sc->sc_hdev));
	} else {
		uhidev_close(&sc->sc_hdev);
		return (0);
	}
}

int
ukbd_activate(struct device *self, enum devact act)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_wskbddev != NULL)
			rv = config_deactivate(sc->sc_wskbddev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
ukbd_detach(struct device *self, int flags)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	int rv = 0;

	DPRINTF(("ukbd_detach: sc=%p flags=%d\n", sc, flags));

	if (sc->sc_console_keyboard) {
#if 0
		/*
		 * XXX Should probably disconnect our consops,
		 * XXX and either notify some other keyboard that
		 * XXX it can now be the console, or if there aren't
		 * XXX any more USB keyboards, set ukbd_is_console
		 * XXX back to 1 so that the next USB keyboard attached
		 * XXX to the system will get it.
		 */
		panic("ukbd_detach: console keyboard");
#else
		/*
		 * Disconnect our consops and set ukbd_is_console
		 * back to 1 so that the next USB keyboard attached
		 * to the system will get it.
		 * XXX Should notify some other keyboard that it can be
		 * XXX console, if there are any other keyboards.
		 */
		printf("%s: was console keyboard\n",
		       sc->sc_hdev.sc_dev.dv_xname);
		wskbd_cndetach();
		ukbd_is_console = 1;
#endif
	}
	/* No need to do reference counting of ukbd, wskbd has all the goo. */
	if (sc->sc_wskbddev != NULL)
		rv = config_detach(sc->sc_wskbddev, flags);

	/* The console keyboard does not get a disable call, so check pipe. */
	if (sc->sc_hdev.sc_state & UHIDEV_OPEN)
		uhidev_close(&sc->sc_hdev);

	return (rv);
}

void
ukbd_intr(struct uhidev *addr, void *ibuf, u_int len)
{
	struct ukbd_softc *sc = (struct ukbd_softc *)addr;
	struct ukbd_data *ud = &sc->sc_ndata;
	int i;

#ifdef UKBD_DEBUG
	if (ukbddebug > 5) {
		printf("ukbd_intr: data");
		for (i = 0; i < len; i++)
			printf(" 0x%02x", ((u_char *)ibuf)[i]);
		printf("\n");
	}
#endif

	ud->modifiers = 0;
	for (i = 0; i < sc->sc_nmod; i++)
		if (hid_get_data(ibuf, &sc->sc_modloc[i]))
			ud->modifiers |= sc->sc_mods[i].mask;
	memcpy(ud->keycode, (char *)ibuf + sc->sc_keycodeloc.pos / 8,
	       sc->sc_nkeycode);

	if (sc->sc_debounce && !sc->sc_polling) {
		/*
		 * Some keyboards have a peculiar quirk.  They sometimes
		 * generate a key up followed by a key down for the same
		 * key after about 10 ms.
		 * We avoid this bug by holding off decoding for 20 ms.
		 */
		sc->sc_data = *ud;
		timeout_add(&sc->sc_delay, hz / 50);
#ifdef DDB
	} else if (sc->sc_console_keyboard && !sc->sc_polling) {
		/*
		 * For the console keyboard we can't deliver CTL-ALT-ESC
		 * from the interrupt routine.  Doing so would start
		 * polling from inside the interrupt routine and that
		 * loses bigtime.
		 */
		sc->sc_data = *ud;
		timeout_add(&sc->sc_delay, 1);
#endif
	} else {
		ukbd_decode(sc, ud);
	}
}

void
ukbd_delayed_decode(void *addr)
{
	struct ukbd_softc *sc = addr;

	ukbd_decode(sc, &sc->sc_data);
}

void
ukbd_decode(struct ukbd_softc *sc, struct ukbd_data *ud)
{
	int mod, omod;
	u_int16_t ibuf[MAXKEYS];	/* chars events */
	int s;
	int nkeys, i, j;
	int key;
#define ADDKEY(c) ibuf[nkeys++] = (c)

#ifdef UKBD_DEBUG
	/*
	 * Keep a trace of the last events.  Using printf changes the
	 * timing, so this can be useful sometimes.
	 */
	if (ukbdtrace) {
		struct ukbdtraceinfo *p = &ukbdtracedata[ukbdtraceindex];
		p->unit = sc->sc_hdev.sc_dev.dv_unit;
		microtime(&p->tv);
		p->ud = *ud;
		if (++ukbdtraceindex >= UKBDTRACESIZE)
			ukbdtraceindex = 0;
	}
	if (ukbddebug > 5) {
		struct timeval tv;
		microtime(&tv);
		DPRINTF((" at %lu.%06lu  mod=0x%02x key0=0x%02x key1=0x%02x "
			 "key2=0x%02x key3=0x%02x\n",
			 tv.tv_sec, tv.tv_usec,
			 ud->modifiers, ud->keycode[0], ud->keycode[1],
			 ud->keycode[2], ud->keycode[3]));
	}
#endif

	if (ud->keycode[0] == KEY_ERROR) {
		DPRINTF(("ukbd_intr: KEY_ERROR\n"));
		return;		/* ignore  */
	}
	nkeys = 0;
	mod = ud->modifiers;
	omod = sc->sc_odata.modifiers;
	if (mod != omod)
		for (i = 0; i < sc->sc_nmod; i++)
			if (( mod & sc->sc_mods[i].mask) !=
			    (omod & sc->sc_mods[i].mask))
				ADDKEY(sc->sc_mods[i].key |
				       (mod & sc->sc_mods[i].mask
					  ? PRESS : RELEASE));
	if (memcmp(ud->keycode, sc->sc_odata.keycode, sc->sc_nkeycode) != 0) {
		/* Check for released keys. */
		for (i = 0; i < sc->sc_nkeycode; i++) {
			key = sc->sc_odata.keycode[i];
			if (key == 0)
				continue;
			for (j = 0; j < sc->sc_nkeycode; j++)
				if (key == ud->keycode[j])
					goto rfound;
			DPRINTFN(3,("ukbd_intr: relse key=0x%02x\n", key));
			ADDKEY(key | RELEASE);
		rfound:
			;
		}

		/* Check for pressed keys. */
		for (i = 0; i < sc->sc_nkeycode; i++) {
			key = ud->keycode[i];
			if (key == 0)
				continue;
			for (j = 0; j < sc->sc_nkeycode; j++)
				if (key == sc->sc_odata.keycode[j])
					goto pfound;
			DPRINTFN(2,("ukbd_intr: press key=0x%02x\n", key));
			ADDKEY(key | PRESS);
		pfound:
			;
		}
	}
	sc->sc_odata = *ud;

	if (nkeys == 0)
		return;

	if (sc->sc_polling) {
		DPRINTFN(1,("ukbd_intr: pollchar = 0x%03x\n", ibuf[0]));
		memcpy(sc->sc_pollchars, ibuf, nkeys * sizeof(u_int16_t));
		sc->sc_npollchar = nkeys;
		return;
	}
#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		u_char cbuf[MAXKEYS * 2];
		int c;
		int npress;

		for (npress = i = j = 0; i < nkeys; i++) {
			key = ibuf[i];
			c = ukbd_trtab[key & CODEMASK];
			if (c == NN)
				continue;
			if (c & 0x80)
				cbuf[j++] = 0xe0;
			cbuf[j] = c & 0x7f;
			if (key & RELEASE)
				cbuf[j] |= 0x80;
			else {
				/* remember pressed keys for autorepeat */
				if (c & 0x80)
					sc->sc_rep[npress++] = 0xe0;
				sc->sc_rep[npress++] = c & 0x7f;
			}
			DPRINTFN(1,("ukbd_intr: raw = %s0x%02x\n",
				    c & 0x80 ? "0xe0 " : "",
				    cbuf[j]));
			j++;
		}
		s = spltty();
		wskbd_rawinput(sc->sc_wskbddev, cbuf, j);
		splx(s);
		if (npress != 0) {
			sc->sc_nrep = npress;
			timeout_add(&sc->sc_rawrepeat_ch,
			    hz * REP_DELAY1 / 1000);
		} else
			timeout_del(&sc->sc_rawrepeat_ch);
		return;
	}
#endif

	s = spltty();
	for (i = 0; i < nkeys; i++) {
		key = ibuf[i];
		wskbd_input(sc->sc_wskbddev,
		    key&RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN,
		    key&CODEMASK);
	}
	splx(s);
}

void
ukbd_set_leds(void *v, int leds)
{
	struct ukbd_softc *sc = v;
	u_int8_t res;

	DPRINTF(("ukbd_set_leds: sc=%p leds=%d, sc_leds=%d\n",
		 sc, leds, sc->sc_leds));

	if (sc->sc_dying)
		return;

	if (sc->sc_leds == leds)
		return;
	sc->sc_leds = leds;
	res = 0;
	/* XXX not really right */
	if ((leds & WSKBD_LED_SCROLL) && sc->sc_scroloc.size == 1)
		res |= 1 << sc->sc_scroloc.pos;
	if ((leds & WSKBD_LED_NUM) && sc->sc_numloc.size == 1)
		res |= 1 << sc->sc_numloc.pos;
	if ((leds & WSKBD_LED_CAPS) && sc->sc_capsloc.size == 1)
		res |= 1 << sc->sc_capsloc.pos;
	uhidev_set_report_async(&sc->sc_hdev, UHID_OUTPUT_REPORT, &res, 1);
}

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
ukbd_rawrepeat(void *v)
{
	struct ukbd_softc *sc = v;
	int s;

	s = spltty();
	wskbd_rawinput(sc->sc_wskbddev, sc->sc_rep, sc->sc_nrep);
	splx(s);
	timeout_add(&sc->sc_rawrepeat_ch, hz * REP_DELAYN / 1000);
}
#endif

int
ukbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ukbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_USB;
		return (0);
	case WSKBDIO_SETLEDS:
		ukbd_set_leds(v, *(int *)data);
		return (0);
	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_leds;
		return (0);
	case WSKBDIO_COMPLEXBELL:
#define d ((struct wskbd_bell_data *)data)
		ukbd_bell(d->pitch, d->period, d->volume, 0);
#undef d
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		DPRINTF(("ukbd_ioctl: set raw = %d\n", *(int *)data));
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		timeout_del(&sc->sc_rawrepeat_ch);
		return (0);
#endif
	}
	return (-1);
}

void
ukbd_bell(u_int pitch, u_int period, u_int volume, int poll)
{
	if (ukbd_bell_fn != NULL)
		(*ukbd_bell_fn)(ukbd_bell_fn_arg, pitch, period,
		    volume, poll);
}

void
ukbd_hookup_bell(void (*fn)(void *, u_int, u_int, u_int, int), void *arg)
{
	if (ukbd_bell_fn == NULL) {
		ukbd_bell_fn = fn;
		ukbd_bell_fn_arg = arg;
	}
}

/* Console interface. */
void
ukbd_cngetc(void *v, u_int *type, int *data)
{
	struct ukbd_softc *sc = v;
	int c;

	DPRINTFN(0,("ukbd_cngetc: enter\n"));
	sc->sc_polling = 1;
	while(sc->sc_npollchar <= 0)
		usbd_dopoll(sc->sc_hdev.sc_parent->sc_iface);
	sc->sc_polling = 0;
	c = sc->sc_pollchars[0];
	sc->sc_npollchar--;
	memcpy(sc->sc_pollchars, sc->sc_pollchars+1,
	       sc->sc_npollchar * sizeof(u_int16_t));
	*type = c & RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*data = c & CODEMASK;
	DPRINTFN(0,("ukbd_cngetc: return 0x%02x\n", c));
}

void
ukbd_cnpollc(void *v, int on)
{
	struct ukbd_softc *sc = v;
	usbd_device_handle dev;

	DPRINTFN(2,("ukbd_cnpollc: sc=%p on=%d\n", v, on));

	usbd_interface2device_handle(sc->sc_hdev.sc_parent->sc_iface, &dev);
	if (on)
		sc->sc_spl = splusb();
	else
		splx(sc->sc_spl);
	usbd_set_polling(dev, on);
}

void
ukbd_cnbell(void *v, u_int pitch, u_int period, u_int volume) 
{
	ukbd_bell(pitch, period, volume, 1);
}	

int
ukbd_cnattach(void)
{

	/*
	 * XXX USB requires too many parts of the kernel to be running
	 * XXX in order to work, so we can't do much for the console
	 * XXX keyboard until autconfiguration has run its course.
	 */
	ukbd_is_console = 1;
	return (0);
}

const char *
ukbd_parse_desc(struct ukbd_softc *sc)
{
	struct hid_data *d;
	struct hid_item h;
	int size;
	void *desc;
	int imod;

	uhidev_get_report_desc(sc->sc_hdev.sc_parent, &desc, &size);
	imod = 0;
	sc->sc_nkeycode = 0;
	d = hid_start_parse(desc, size, hid_input);
	while (hid_get_item(d, &h)) {
		/*printf("ukbd: id=%d kind=%d usage=0x%x flags=0x%x pos=%d size=%d cnt=%d\n",
		  h.report_ID, h.kind, h.usage, h.flags, h.loc.pos, h.loc.size, h.loc.count);*/
		if (h.kind != hid_input || (h.flags & HIO_CONST) ||
		    HID_GET_USAGE_PAGE(h.usage) != HUP_KEYBOARD ||
		    h.report_ID != sc->sc_hdev.sc_report_id)
			continue;
		DPRINTF(("ukbd: imod=%d usage=0x%x flags=0x%x pos=%d size=%d "
			 "cnt=%d\n", imod,
			 h.usage, h.flags, h.loc.pos, h.loc.size, h.loc.count));
		if (h.flags & HIO_VARIABLE) {
			if (h.loc.size != 1)
				return ("bad modifier size");
			/* Single item */
			if (imod < MAXMOD) {
				sc->sc_modloc[imod] = h.loc;
				sc->sc_mods[imod].mask = 1 << imod;
				sc->sc_mods[imod].key = HID_GET_USAGE(h.usage);
				imod++;
			} else
				return ("too many modifier keys");
		} else {
			/* Array */
			if (h.loc.size != 8)
				return ("key code size != 8");
			if (h.loc.count > MAXKEYCODE)
				return ("too many key codes");
			if (h.loc.pos % 8 != 0)
				return ("key codes not on byte boundary");
			if (sc->sc_nkeycode != 0)
				return ("multiple key code arrays\n");
			sc->sc_keycodeloc = h.loc;
			sc->sc_nkeycode = h.loc.count;
		}
	}
	sc->sc_nmod = imod;
	hid_end_parse(d);

	hid_locate(desc, size, HID_USAGE2(HUP_LEDS, HUD_LED_NUM_LOCK),
		   sc->sc_hdev.sc_report_id, hid_output, &sc->sc_numloc, NULL);
	hid_locate(desc, size, HID_USAGE2(HUP_LEDS, HUD_LED_CAPS_LOCK),
		   sc->sc_hdev.sc_report_id, hid_output, &sc->sc_capsloc, NULL);
	hid_locate(desc, size, HID_USAGE2(HUP_LEDS, HUD_LED_SCROLL_LOCK),
		   sc->sc_hdev.sc_report_id, hid_output, &sc->sc_scroloc, NULL);

	return (NULL);
}
