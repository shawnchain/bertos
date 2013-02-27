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
 * \author Robin Gilks <g8ecj@gilks.org>
 *
 */

#include "hw_xywmodem.h"

#include <cpu/irq.h>
#include <avr/interrupt.h>

#include <net/xywmodem.h>

// timer 1 compare for TX output
ISR(TIMER1_COMPA_vect)
{
	// we are running at double rate so that the clock output pin toggles.
	// only output data on the falling edge of the clock
	if (PINB & BV(1))
		return;

	xyw_tx_int();

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
	xyw_rx_int(this_bit);
}



void hw_xyw_init(int bps)
{
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
}

/*
Default connections are as follows:
TxData  on Port B bit 0 (arduino pin D8)
TxClock on Port B bit 1 (arduino pin D9)
PTT     on Port B bit 2 (arduino pin D10)

RxData  on Port C bit 0 (arduino pin AIN0)
RxClock on Port C bit 1 (arduino pin AIN1)
DCD     on Port C bit 2 (arduino pin AIN2)
*/


