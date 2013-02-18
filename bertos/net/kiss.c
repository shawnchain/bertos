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
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 *
 * Copyright 2012 Robin Gilks <g8ecj@gilks.org>
 *
 * -->
 *
 * \author Robin Gilks <g8ecj@gilks.org>
 *
 * \brief KISS protocol handler
 *
 * This module converts KISS encoded data from one KFile object and passes it to another as binary
 * It also passes data the other way
 * It implements KISS commands 1-6
 * It uses the standard p-persist algorithms for keying of a radio
 *
 */



#include "kiss.h"
#include "hw/hw_kiss.h"

#include <cfg/debug.h>
#include <cfg/log.h>

#include <net/hdlc.h>

#include <drv/ser.h>
#include <drv/timer.h>

#include <stdlib.h>
#include <string.h>




// KISS/SLIP framing characters
#define FEND     192       /** frame end       */
#define FESC     219       /** frame escape    */
#define TFEND    220       /** transposed fend */
#define TFESC    221       /** transposed fesc */


// KISS commands
#define TXDELAY  1
#define PERSIST  2
#define SLOT     3
#define TXTAIL   4
#define DUPLEX   5
#define HARDWARE 6


// what states I can be in
#define WAIT_FOR_FEND      1
#define WAIT_FOR_COMMAND   2
#define WAIT_FOR_PARAMETER 3
#define WAIT_FOR_TRANSPOSE 4
#define WAIT_FOR_DATA      5
#define WAIT_FOR_TRANSMIT  6



/**
 * Load kiss parameters from eeprom
 * Check a basic crc and if in error then load a set of defaults
 *
 * \param k kiss context
 *
 */
static void load_params (KissCtx * k)
{

	KISS_EEPROM_LOAD ();

	if (~(k->params.chksum) !=
		 k->params.txdelay + k->params.persist + k->params.slot + k->params.txtail + k->params.duplex + k->params.hware)
	{
		// load sensible defaults if backing store has a bad checksum
		k->params.txdelay = 50;
		k->params.persist = 64;
		k->params.slot = 10;
		k->params.txtail = 3;
		k->params.duplex = 0;
		k->params.hware = 0;
	}
}


/**
 * Save kiss parameters to eeprom
 * Calculate and store a basic CRC with the data
 *
 * \param k kiss context
 *
 */
static void save_params (KissCtx * k)
{
	k->params.chksum =
		~(k->params.txdelay + k->params.persist + k->params.slot + k->params.txtail + k->params.duplex + k->params.hware);

	KISS_EEPROM_SAVE ();

}


/**
 * Decode a KISS command
 * KISS protocol defines 6 commands so we try and make use of them
 *
 * \param b parameter value
 * \param k kiss context
 *
 * Uses previously stored command value
 */
static void kiss_decode_command (KissCtx * k, uint8_t b)
{
	switch (k->command)
	{
	case TXDELAY:
		k->params.txdelay = b;
		break;
	case PERSIST:
		k->params.persist = b;
		break;
	case SLOT:
		k->params.slot = b;
		break;
	case TXTAIL:
		k->params.txtail = b;
		break;
	case DUPLEX:
		k->params.duplex = b;
		break;
	case HARDWARE:
		LOG_INFO ("Hardware command not supported");
		break;
	}
	save_params (k);
}



///< Transmit algorithm
///<   if full_duplex
///<     > keyup
///<     PTT
///<     start TXdelay timer
///<   else
///<     start slot timer
///<   fi
///<   if slot expires
///<     if no DCD && random < persist
///<        > keyup
///<        PTT
///<        start TXdelay timer
///<     else
///<        stir random
///<        start slot timer
///<     fi
///<   fi

/**
 * Transmit to air function
 * Check KISS parameters to see if/when we can transmit
 *
 * \param k kiss context
 *
 * \return bool true if transmit completed
 */
static bool kiss_tx_to_modem (KissCtx * k)
{
	uint16_t i;

	// not really full duplex, we just crash over any other traffic on the channel
	if (k->params.duplex)
	{
		for (i = 0; i < k->tx_pos; i++)
			kfile_putc (k->tx_buf[i], k->modem);
		kfile_flush (k->modem);	  // wait for transmitter to finish
		return true;
	}

	// see if the channel is busy
	if (k->rx_pos > 0)
	{
		timer_delay (k->params.slot * 10);
		rand ();						  // stir random up a bit
		return false;				  // next time round we may be OK to TX
	}
	// the channel is clear - see if persist allows us to TX
	else
	{
		i = rand ();
		// make an 8 bit random number from a 16 bit one
		if (((i >> 8) ^ (i & 0xff)) < k->params.persist)
		{
			for (i = 0; i < k->tx_pos; i++)
				kfile_putc (k->tx_buf[i], k->modem);
			kfile_flush (k->modem);	// wait for transmitter to finish
			return true;
		}
		else
		{
			return false;
		}
	}
}





/**
 * Receive function
 * Encode the raw ax25 data as a KISS protocol stream
 *
 * \param k KISS context
 *
 */
static void kiss_tx_to_serial (KissCtx * k)
{
	/// function used by KISS poll to process completed rx'd packets
	/// here, we're just pumping out the KISS data to the serial object

	uint16_t len;
	uint8_t c;

	kfile_putc (FEND, k->serial);
	kfile_putc (0, k->serial);       /// channel 0, data command

	for (len = 0; len < k->rx_pos; len++)
	{
		c = k->rx_buf[len];
		switch (c)
		{
		case FEND:
			kfile_putc (FESC, k->serial);
			kfile_putc (TFEND, k->serial);
			break;
		case FESC:
			kfile_putc (FESC, k->serial);
			kfile_putc (TFESC, k->serial);
			break;
		default:
			kfile_putc (c, k->serial);
		}
	}

	kfile_putc (FEND, k->serial);
}


/**
 * Read incoming binary data from the modem
 * Encode into SLIP encoded data prefixed by a KISS data command and add to KISS object's buffer
 * Pass up to the serial port if HDLC CRC is OK
 *
 * \param k kiss context
 *
 */
void kiss_poll_modem (KissCtx * k)
{
	int c;

	// get octets from modem
	while ((c = kfile_getc (k->modem)) != EOF)
	{
		if (k->rx_pos < CONFIG_KISS_FRAME_BUF_LEN)
		{
			k->rx_buf[k->rx_pos++] = c;
		}
	}

	switch (kfile_error (k->modem))
	{
	case HDLC_PKT_AVAILABLE:
		if (k->rx_pos >= KISS_MIN_FRAME_LEN)
		{
			k->rx_pos -= 2;            // drop the CRC octets
			LOG_INFO ("Frame found!\n");
			kiss_tx_to_serial (k);
		}
		kfile_clearerr (k->modem);
		k->rx_pos = 0;
		break;
	case HDLC_ERROR_CRC:
		LOG_INFO ("CRC error\n");
		kfile_clearerr (k->modem);
		k->rx_pos = 0;
		break;
	case HDLC_ERROR_OVERRUN:
		if (k->rx_pos >= KISS_MIN_FRAME_LEN)
			LOG_INFO ("Buffer overrun\n");
		kfile_clearerr (k->modem);
		k->rx_pos = 0;
		break;
	case HDLC_ERROR_ABORT:
		if (k->rx_pos >= KISS_MIN_FRAME_LEN)
			LOG_INFO ("Data abort\n");
		kfile_clearerr (k->modem);
		k->rx_pos = 0;
		break;
//    default: // ignore other states
	}
}



/**
 * Read incoming KISS data from serial port
 * Decode the SLIP encoded data and add to KISS object's buffer
 * TX to radio if appropriate
 *
 * \param k kiss context
 *
 */
void kiss_poll_serial (KissCtx * k)
{

	int c;
	uint8_t b;

	while ((c = kfile_getc (k->serial)) != EOF)
	{

		k->last_tick = timer_clock ();
		// about to overflow buffer? reset
		if (k->tx_pos >= (CONFIG_KISS_FRAME_BUF_LEN - 2))
			k->tx_pos = 0;

		// trim value to 0-255, maybe add to buffer
		b = c & 0x00FF;

		switch (k->state)			  // see what we are looking for
		{
		case WAIT_FOR_FEND:
			if (b == FEND)			  // junk all data until frame start
				k->state = WAIT_FOR_COMMAND;
			break;
		case WAIT_FOR_COMMAND:
			if (b == FEND)			  // may get two FEND in a row!!
				break;
			if ((b & 0xf0) != 0)	  // we only support channel 0
			{
				LOG_INFO ("Only KISS channel 0 supported");
				k->state = WAIT_FOR_FEND;
			}
			else if ((b & 0x0f) != 0)
			{
				k->state = WAIT_FOR_PARAMETER;
				k->command = b & 0x0f;
			}
			else
			{
				k->state = WAIT_FOR_DATA;	// command == data
			}
			break;
		case WAIT_FOR_PARAMETER:
			kiss_decode_command (k, b);
			k->state = WAIT_FOR_FEND;
			break;
		case WAIT_FOR_TRANSPOSE:
			switch (b)				  // the default is to put whatever character we got in the buffer
			{
			case TFEND:
				b = FEND;
				break;
			case TFESC:
				b = FESC;
				break;
			}
			k->tx_buf[k->tx_pos++] = b;
			k->state = WAIT_FOR_DATA;
			break;
		case WAIT_FOR_DATA:
			if (b == FESC)
			{
				k->state = WAIT_FOR_TRANSPOSE;
			}
			else if (b == FEND)
			{
				if (k->tx_pos >= KISS_MIN_FRAME_LEN)
				{
					k->state = WAIT_FOR_TRANSMIT;
				}
				else
				{
					k->tx_pos = 0;	  // too short - throw it away
					k->state = WAIT_FOR_COMMAND;	// might be starting a new frame
				}
			}
			else
			{
				k->tx_buf[k->tx_pos++] = b;
			}
			break;
		}
	}

	if (k->state == WAIT_FOR_TRANSMIT)
	{
		// note that we throw data away while we wait to transmit
		if (kiss_tx_to_modem (k))
		{
			k->tx_pos = 0;			  // reset buffer pointer when we've done
			k->state = WAIT_FOR_COMMAND;	// might be starting a frame
		}
	}

	// sanity checks
	// no serial input in last 2 secs?
	if (timer_clock () - k->last_tick > ms_to_ticks (2000L))
		k->tx_pos = 0;
}


void kiss_poll_params(KissCtx * k, uint8_t *head, uint8_t *tail)
{
	*head = k->params.txdelay;
	*tail = k->params.txtail;


}


/**
 * Initialise the KISS context
 * Check KISS parameters to see if/when we can transmit
 *
 * \param k kiss context
 * \param channel KFile object to the modem
 * \param serial KFile object to the serial (UART) port
 *
 */
void kiss_init (KissCtx * k, KFile * channel, KFile * serial)
{
	/// Initialize KISS TX object
	ASSERT (k);
	ASSERT (channel);
	ASSERT (serial);

	memset (k, 0, sizeof (*k));
	k->modem = channel;
	k->serial = serial;
	k->state = WAIT_FOR_FEND;

	// get KISS parameters from EEPROM
	load_params (k);

}
