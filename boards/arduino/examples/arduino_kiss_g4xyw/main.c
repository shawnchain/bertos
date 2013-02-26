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
 * \brief Arduino KISS g4xyw modem demo.
 *
 * This example shows how to read and write KISS data to the KISS module(s)
 * It uses the following modules:
 * xywmodem
 * hdlc
 * kiss
 * ser
 *
 * You will see how to implement the KISS protocol, init the g4xyw modem and
 * how to process messages using kiss module and how the p-persist algorithm works.
 */



#include <cpu/irq.h>
#include <cfg/debug.h>
#include <cfg/log.h>

#include <net/xywmodem.h>
#include <net/kiss.h>

#include <drv/ser.h>
#include <drv/timer.h>

#include <stdlib.h>
#include <stdio.h>

#include <avr/eeprom.h>


static KissCtx kiss;
static XYW xyw;


/*
   For testing, define the 'TEST' macro below and 'TX' macro (or not) to build either the TX or the RX flavour of the code.
   Load an Arduino with one of each and connect the TX to RX to test the data flow
*/

//#define TEST


#ifdef TEST

#define TX


#ifdef TX

#include <struct/kfile_mem.h>

KFileMem TXdata;						  // used to send data

// set into full duplex
uint8_t kiss_params[] = {192,5,1,192};

uint8_t kiss_packet_kiss1[] = { 192, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	180, 181, 182, 183, 184, 185, 185, 187, 188, 189,
	190, 191, 219, 220, 193, 194, 195, 196, 197, 198, 199,
	210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 221,
	220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 192
};

uint8_t kiss_packet_kiss2[] = {192,0,9,8,7,6,5,4,3,2,1,0,192};

#else

// make a fake kfile object that just outputs what data arrives at it in decimal.
typedef struct FAKE
{
	/** Base "class" */
	KFile fd;
} FAKE;


FAKE fake;


#define KFT_FAKE MAKE_ID('F', 'A', 'K', 'E')

INLINE FAKE *FAKE_CAST(KFile *fd)
{
  ASSERT(fd->_type == KFT_FAKE);
  return (FAKE *)fd;
}


static size_t fake_write(UNUSED_ARG(KFile, *fd), const void *_buf, size_t size)
{
	const uint8_t *buf = (const uint8_t *)_buf;

	while (size--)
	{
		kprintf("%d ", *buf++);
	}
	return buf - (const uint8_t *)_buf;
}

#endif


int
main (void)
{

	IRQ_ENABLE;
	kdbg_init ();
	timer_init ();
	srand (9898);

	// single loop but this test relies on there being 2 CPUs back to back
	// one is set to TX (using the TX define above), and the other to RX (by commenting out the TX define)
	// Init modem driver
	xyw_init (&xyw, 9600);

#ifdef TX
	// initialise data flow from fake serial KISS, TX to external g4xyw modem
	kiss_init (&kiss, &xyw.fd, &TXdata.fd);
	// send KISS parameters
	kfilemem_init (&TXdata, kiss_params, sizeof (kiss_params));
	// poll the fake serial port to parse the KISS data
	if (kiss_poll_serial (&kiss))
	{
		uint8_t head, tail;
		kputs("Setting params\n");
		// pass head and tail timing values to modem
		kiss_poll_params(&kiss, &head, &tail);
		xyw_head (&xyw.fd, head);
		xyw_tail (&xyw.fd, tail);
	}
	timer_delay(100);
	for(;;)
	{
		// put some real data into the fake serial buffer
		kfilemem_init (&TXdata, kiss_packet_kiss1, sizeof (kiss_packet_kiss1));
		// poll the fake serial port for KISS data and push out of the modem port
		kputs("Txing\n");
		kiss_poll_serial (&kiss);
		// delay between packets
		timer_delay(40);
		kfilemem_init (&TXdata, kiss_packet_kiss2, sizeof (kiss_packet_kiss2));
		// and again
		kiss_poll_serial (&kiss);
		// delay between packets
		timer_delay(3000);
	}

#else
	// initialise the fake kfile object (would normally be the serial port TX line)
	memset(&fake, 0, sizeof(fake));
	DB (fake.fd._type = KFT_FAKE);
	fake.fd.write = fake_write;
	// data flow this time is from modem to the fake serial port
	kiss_init (&kiss, &xyw.fd, &fake.fd);
	for(;;)
	{
		kiss_poll_modem (&kiss);
	}
#endif
}

#else /* not TEST */

static Serial ser;


/**
 * Module initialse
 *
 */
static void
init (void)
{

	IRQ_ENABLE;
	timer_init ();

	/* Initialize serial port */
	ser_init (&ser, SER_UART0);
	ser_setbaudrate (&ser, 115200L);

	// Init modem driver
	xyw_init (&xyw, 9600);

	// Init KISS context
	kiss_init (&kiss, &xyw.fd, &ser.fd);

}


/**
 * Main loop
 * Initialises all objects then polls the serial and modem data sources for data
 *
 */
int
main (void)
{
	uint8_t head, tail;

	init ();

	while (1)
	{
		// Use KISS module call to check modem and serial objects for incoming data
		kiss_poll_modem (&kiss);
		// see if a param command was processed and update modem if so
		if (kiss_poll_serial (&kiss))
		{
			// pass head and tail timing values to modem
			kiss_poll_params(&kiss, &head, &tail);
			xyw_head (&xyw.fd, head);
			xyw_tail (&xyw.fd, tail);
		}
	}
	return 0;
}

#endif
