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
 * Copyright 2012 Robin Gilks (g8ecj at gilks.org)
 *
 * -->
 *
 * \brief Terminal emulator example.
 *
 * \author Robin Gilks <g8ecj@gilks.org>
 *
 * \brief Example of driving the terminal emulator to an LCD Panel.
 *
 */

#include "hw/hw_led.h"

#include <cfg/debug.h>

#include <cpu/irq.h>
#include <cpu/power.h>

#include <drv/timer.h>
#include <drv/ser.h>
#include <drv/lcd_hd44.h>
#include <drv/term.h>

static Serial out;
static Term term;

static void init(void)
{
	/* Enable all the interrupts */
	IRQ_ENABLE;

	/* Initialize debugging module (allow kprintf(), etc.) */
	kdbg_init();
	/* Initialize system timer */
	timer_init();

	/*
	 * XXX: Arduino has a single UART port that was previously
	 * initialized for debugging purpose.
	 * In order to activate the serial driver you should disable
	 * the debugging module.
	 */
#if 0
	/* Initialize UART0 */
	ser_init(&out, SER_UART0);
	/* Configure UART0 to work at 115.200 bps */
	ser_setbaudrate(&out, 115200);
#else
	(void)out;
#endif
	lcd_hw_init();
	term_init(&term);

}

int main(void)
{
	init();

	while (1)
	{
		// Assumes 20x4 display
		// Basic: CR/LF functionality, scrolling on LF on bottom line and wrapping
		kfile_printf(&term.fd, "On line 1 I hope!!\r\n");
		timer_delay(3000);
		kfile_printf(&term.fd, "On line 2\r\nand now line 3\r\n");
		timer_delay(3000);
		kfile_printf(&term.fd, "On line 4\r");
		timer_delay(3000);
		kfile_printf(&term.fd, "\nScrolled - this on 4");
		timer_delay(3000);
		kfile_printf(&term.fd, "Wrap onto line 1\r\n");
		timer_delay(5000);

		// Intermediate: Cursor up/down/left/right and direct addressing
		kfile_printf(&term.fd, "Onto next line and  clear to EOL");
		timer_delay(3000);
		kfile_printf(&term.fd, "\r \r");
		timer_delay(3000);
		kfile_printf(&term.fd, "\x16\x22\x24Row 3 col 5");
		timer_delay(3000);
		kfile_printf(&term.fd, "\xb**Up");
		timer_delay(3000);
		kfile_printf(&term.fd, "\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8**Back 29");
		timer_delay(3000);
		kfile_printf(&term.fd, "\xb\xb\xb\xb\xb\xb\xbUp 7=down 1\r\n");
		timer_delay(3000);

		// Advanced: Cursor control, pseudo flash
		// a way to avoid strange character sequences and use the #defines instead
		kfile_printf(&term.fd, "%c", TERM_CLR);
		kfile_printf(&term.fd, "Cursor on ");
		kfile_printf(&term.fd, "%c", TERM_CURS_ON);
		timer_delay(3000);
		kfile_printf(&term.fd, "\r\nCursor blink  ");
		kfile_printf(&term.fd, "%c", TERM_BLINK_ON);
		timer_delay(3000);
		kfile_printf(&term.fd, "\r\nCursor off \r\n");
		kfile_printf(&term.fd, "%c%c", TERM_CURS_OFF, TERM_BLINK_OFF);
		timer_delay(3000);

		kfile_printf(&term.fd, "%c%c%c%s", TERM_CPC, TERM_ROW + 3, TERM_COL + 0, "Simulated");
		for (int i=0; i < 10; i++)
		{
			char spaces[10] = "         ";
			kfile_printf(&term.fd, "%c%c%c%s", TERM_CPC, TERM_ROW + 3, TERM_COL + 10, " flash");
			timer_delay(800);
			kfile_printf(&term.fd, "%c%c%c%*s", TERM_CPC, TERM_ROW + 3, TERM_COL + 10, 6, spaces);
			timer_delay(500);
		}
		timer_delay(3000);
		kfile_printf(&term.fd, "%c%c%c", TERM_CPC, TERM_ROW + 0, TERM_COL + 5);
		kfile_printf(&term.fd, "\rDone - repeating");
		timer_delay(3000);
		kfile_printf(&term.fd, "%c", TERM_CLR);
	}
}
