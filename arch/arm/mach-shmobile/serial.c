/*
 * sci_init_pins for ARM based SH-Mobile
 *
 * Copyright (C) 2011 Takashi Yoshii <takashi.yoshii.zj@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/serial_sci.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <mach/serial.h>

#define SCPCR 0x30
#define RTSC (1<<4)
#define CTSC (1<<3)

#define SCPDR 0x34
#define RTSD (1<<4)

static void shmobile_sci_init_pins(struct uart_port *port, unsigned int cflag)
{
	unsigned short scpcr, scpdr;

	scpcr = ioread16(port->membase + SCPCR);

	if (cflag & CRTSCTS) {
		/* SCFCR.MCE should be set to 1, too */
		iowrite16(scpcr & ~(RTSC | CTSC), port->membase + SCPCR);
	} else {
		/* make RTS low (otherwize it will be Hi-Z) */
		scpdr = ioread16(port->membase + SCPDR);
		iowrite16(scpdr & ~RTSD, port->membase + SCPDR);
		iowrite16(scpcr | (RTSC | CTSC), port->membase + SCPCR);
	}
}

struct plat_sci_port_ops shmobile_sci_port_ops = {
	.init_pins	= shmobile_sci_init_pins,
};
