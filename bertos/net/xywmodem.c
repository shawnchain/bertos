/**
 * \file
 * <!--
 * This file is part of BeRTOS.
 *
 * Bertos is free software; you can redistribute it and/or modify
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
 *
 * Copyright 2013 Robin Gilks <g8ecj@gilks.org>
 *
 * -->
 *
 * \brief External modem.
 *
 * \author Robin Gilks <g8ecj@gilks.org>
 *
 */

#include "xywmodem.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <drv/timer.h>
#include "cfg/cfg_arch.h"
#include "cfg/module.h"



static XYW *xyw;

// timer 1 compare for TX output
ISR(TIMER1_COMPA_vect)
{
	int8_t this_bit;

	// we are running at double rate so that the clock output pin toggles.
	// only output data on the falling edge of the clock
	if (PINB & BV(1))
		return;

	/* get a new TX bit. */
	/* note that hdlc module does all the NRZI as well as bit stuffing etc */
	/* if run out of data then clear sending flag, drop PTT, stop TX clock, stop interrupts */
	this_bit = hdlc_encode (&xyw->tx_hdlc, &xyw->tx_fifo);
	switch (this_bit)
	{
	case -1:
		xyw->sending = false;
		// stop TX clock on PWM B1 pin
		DDRB &= ~BV(1);
		// drop PTT
		PORTB &= ~BV(2);
		// stop TX interrupt
		TIMSK1 &= ~BV(OCIE1A);
		return;
	case 0:
	case 1:
		// output bit
		if (this_bit)
			PORTB |= BV(0);
		else
			PORTB &= ~BV(0);
		break;
	}

}


// pin change interrupt for RX clock (on PC1, PCINT9)
ISR(PCINT1_vect)
{
	uint8_t this_bit;

	// no data if no DCD!
	if ((PINC & BV(2)) == 0)
		return;

	// only look at RX data on rising edge
	if ((PINC & BV(1)) == 0)
		return;
	// read in a bit, pass up to HDLC layer and put in the fifo ready for KISS layer to pick up
	this_bit = PINC & BV(0) ? 1 : 0;
	xyw->status = hdlc_decode (&xyw->rx_hdlc, this_bit, &xyw->rx_fifo);
}


static void xyw_txStart(XYW *xyw)
{
	if (!xyw->sending)
	{
		xyw->sending = true;
		// output TX clock on PWM B1 pin
		DDRB |= BV(1);
		// raise PTT line
		PORTB |= BV(2);
		// enable 9600bps interrupt to send data
		TIMSK1 |= BV(OCIE1A);
	}
}



static size_t xyw_read(KFile *fd, void *_buf, size_t size)
{
	XYW *xyw = XYW_CAST (fd);
	uint8_t *buf = (uint8_t *)_buf;

	#if CONFIG_XYW_RXTIMEOUT == 0
	while (size-- && !fifo_isempty_locked(&xyw->rx_fifo))
	#else
	while (size--)
	#endif
	{
		#if CONFIG_XYW_RXTIMEOUT != -1
		ticks_t start = timer_clock();
		#endif

		while (fifo_isempty_locked(&xyw->rx_fifo))
		{
			cpu_relax();
			#if CONFIG_XYW_RXTIMEOUT != -1
			if (timer_clock() - start > ms_to_ticks(CONFIG_XYW_RXTIMEOUT))
				return buf - (uint8_t *)_buf;
			#endif
		}

		*buf++ = fifo_pop_locked(&xyw->rx_fifo);
	}

	return buf - (uint8_t *)_buf;
}

static size_t xyw_write(KFile *fd, const void *_buf, size_t size)
{
	XYW *xyw = XYW_CAST (fd);
	const uint8_t *buf = (const uint8_t *)_buf;

	while (size--)
	{
		while (fifo_isfull_locked(&xyw->tx_fifo))
			cpu_relax();

		fifo_push_locked(&xyw->tx_fifo, *buf++);
		xyw_txStart(xyw);
	}

	return buf - (const uint8_t *)_buf;
}

static int xyw_flush(KFile *fd)
{
	XYW *xyw = XYW_CAST (fd);
	while (xyw->sending)
		cpu_relax();
	return 0;
}

static int xyw_error(KFile *fd)
{
	XYW *xyw = XYW_CAST (fd);
	int err;

	ATOMIC(err = xyw->status);
	return err;
}

static void xyw_clearerr (KFile * fd)
{
	XYW *xyw = XYW_CAST (fd);
	ATOMIC (xyw->status = 0);
}


/**
 * Sets head timings by defining the number of flags to output
 * Has to be done here as this is the only module that interfaces directly to hdlc!!
 *
 * \param fd caste xyw context.
 * \param c value
 *
 */
void xyw_head (KFile * fd, int c)
{
	XYW *xyw = XYW_CAST (fd);
	hdlc_head (&xyw->tx_hdlc, c);
}

/**
 * Sets tail timings by defining the number of flags to output
 * Has to be done here as this is the only module that interfaces directly to hdlc!!
 *
 * \param fd caste xyw context.
 * \param c value
 *
 */
void xyw_tail (KFile * fd, int c)
{
	XYW *xyw = XYW_CAST (fd);
	hdlc_tail (&xyw->tx_hdlc, c);
}


/**
 * Initialize an g4xyw 9600 modem.
 * \param af xyw context to operate on.
 * \param bps speed to operate at
 */

void xyw_init(XYW *_xyw, int bps)
{
	xyw = _xyw;
	#if CONFIG_XYW_RXTIMEOUT != -1
	MOD_CHECK(timer);
	#endif
	memset(xyw, 0, sizeof(*xyw));

	// set up timer 1 for 9600 and get ready to start it for transmit
	/* Set X1 prescaler to clk (16 MHz), CTC, top = OCR1A, toggle on compare */
	TCCR1A = BV(COM1A0);
	TCCR1B = BV(WGM12) | BV(CS10);
	// Set low prescaler to obtain a high res freq with toggling output
	OCR1A = (CPU_FREQ / bps / 2) - 1;
	// TX Data is on port B 0
	DDRB |= BV(0);
	// output TX clock on PWM B1 pin
	DDRB |= BV(1);
	// PTT is on port B 2
	DDRB |= BV(2);
	// Enable timer interrupt: Timer/Counter1 Output Compare
	// only do this when we have data to transmit
//	TIMSK1 |= BV(OCIE1A);


	// enable pin change interrupt for receive
	// note that this triggers on both edges
	PCICR = BV(PCIE1);
	PCMSK1 |= BV(PCINT9);
	// Use PC0 for the RX data
	DDRC &= ~BV(0);
	// PC1 (PCint9) for the RX clock
	DDRC &= ~BV(1);
	// PC2 for DCD (if used)
	DDRC &= ~BV(2);


	fifo_init(&xyw->rx_fifo, xyw->rx_buf, sizeof(xyw->rx_buf));
	fifo_init(&xyw->tx_fifo, xyw->tx_buf, sizeof(xyw->tx_buf));

	hdlc_init (&xyw->rx_hdlc);
	hdlc_init (&xyw->tx_hdlc);
	// set initial defaults for timings
	hdlc_head (&xyw->tx_hdlc, DIV_ROUND (CONFIG_XYW_PREAMBLE_LEN * CONFIG_XYW_BITRATE, 8000));
	hdlc_tail (&xyw->tx_hdlc, DIV_ROUND (CONFIG_XYW_TRAILER_LEN * CONFIG_XYW_BITRATE, 8000));
	DB (xyw->fd._type = KFT_XYW);
	xyw->fd.write = xyw_write;
	xyw->fd.read = xyw_read;
	xyw->fd.flush = xyw_flush;
	xyw->fd.error = xyw_error;
	xyw->fd.clearerr = xyw_clearerr;
}
