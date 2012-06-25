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
 * \brief Terminal emulator driver.
 *
 * Control codes and cursor addressing based on the old Newbury Data Recording 8000
 * series dumb terminal which is nice and simple.
 *
 * Cursor positioning is done with [0x16][0x20+row][0x20+col]. All other codes are single control characters.
 * The application generates a stream with the following control codes, the terminal emulator layer interpretss
 * the codes for a specific device -
 * eg. ANSI sequencies to send to a serial terminal,
 *     direct cursor addressing on an LCD panel
 *
 * \author Robin Gilks <g8ecj@gilks.org>
 *
 */

#include "term.h"

#include "lcd_hd44.h"
#include "timer.h"
#include <string.h>


/**
 * \brief Write a character to the display, interpreting control codes in the data stream.
 * Uses a simple set of control codes from an ancient dumb terminal.
 *
 */
static void term_putchar(uint8_t c, struct Term *fds)
{
	switch (fds->state)
	{
	case TERM_STATE_NORMAL: /**< state that indicates we're passing data straight through */
		switch (c)
		{
		case TERM_CPC:     /**< Cursor position prefix - followed by row + column */
			fds->state = TERM_STATE_ROW;      // wait for row value
			break;
		case TERM_CLR:     /**< Clear screen */
			fds->addr = 0;
			lcd_command(LCD_CMD_CLEAR);
			timer_delay(2);
			break;
		case TERM_HOME:    /**< Home */
			fds->addr = 0;
			break;
		case TERM_UP:      /**< Cursor up  - no wrap */
			if (fds->addr > CONFIG_LCD_COLS)
				fds->addr -= CONFIG_LCD_COLS;
			break;
		case TERM_DOWN:    /**< Cursor down - also Line feed. No wrap */
			if (fds->addr > (CONFIG_LCD_COLS * (CONFIG_LCD_ROWS - 1)))
				fds->addr += CONFIG_LCD_COLS;
			break;
		case TERM_LEFT:    /**< Cursor left - no wrap */
			if (!(fds->addr % CONFIG_LCD_COLS))
				fds->addr -= 1;
			break;
		case TERM_RIGHT:   /**< Cursor right */
			if (!(++fds->addr % CONFIG_LCD_COLS))
				fds->addr -= 1;               // if goes onto next line then move back
			break;
		case TERM_CR:    /**< Carriage return */
			fds->addr -= fds->addr % CONFIG_LCD_COLS;
			break;
		default:
			lcd_putc(fds->addr, c);
			fds->addr += 1;
		}
		break;
	case TERM_STATE_ROW:  /**< state that indicates we're waiting for the row address */
		fds->row = c - TERM_ROW;         /**< cursor position row offset */
		fds->state = TERM_STATE_COL;     // wait for row value
		break;
	case TERM_STATE_COL:  /**< state that indicates we're waiting for the column address */
		fds->col = c - TERM_COL;         /**< cursor position column offset */
		fds->addr = (fds->row * CONFIG_LCD_COLS) + fds->col;
		fds->state = TERM_STATE_NORMAL;  // return to normal processing - cursor address complete
		break;
	}
}



/**
 * \brief Write a buffer to LCD display.
 *
 * \return 0 if OK, EOF in case of error.
 *
 */
static size_t term_write(struct KFile *fd, const void *_buf, size_t size)
{
	Term *fds = TERM_CAST(fd);
	const char *buf = (const char *)_buf;

	while (size--)
	{
		term_putchar(*buf++, fds);
	}
	return 0;
}


void term_init(struct Term *fds)
{
	memset(fds, 0, sizeof(*fds));

	DB(fds->fd._type = KFT_TERM);
	fds->fd.write = term_write;       // leave all but the write function as default
	fds->state = TERM_STATE_NORMAL;   // start at known point
	term_putchar(TERM_CLR, fds);      // clear screen, init address pointer

}

