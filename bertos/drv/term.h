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

 * $WIZ$ module_name = "term"
 * $WIZ$ module_depends = "lcd_hd44"
 *
 */

#include "cfg/compiler.h"
#include "io/kfile.h"


#define TERM_CPC     0x16     /**< Cursor position prefix - followed by row + column */
#define TERM_ROW     0x20     /**< cursor position row offset */
#define TERM_COL     0x20     /**< cursor position column offset */
#define TERM_CLR     0x1f     /**< Clear screen */
#define TERM_HOME    0x1d     /**< Home */
#define TERM_UP      0x0b     /**< Cursor up */
#define TERM_DOWN    0x0a     /**< Cursor down */
#define TERM_LEFT    0x08     /**< Cursor left */
#define TERM_RIGHT   0x18     /**< Cursor right */
#define TERM_CR      0x0d     /**< Carriage return */
#define TERM_LF      0x0a     /**< Line feed (scrolling version of cursor down!) */

#define TERM_STATE_NORMAL   0x00    /**< state that indicates we're passing data straight through */
#define TERM_STATE_ROW      0x01    /**< state that indicates we're waiting for the row address */
#define TERM_STATE_COL      0x02    /**< state that indicates we're waiting for the column address */





/** Terminal handle structure */
typedef struct Term
{
	/** Terminal has a KFile struct implementation **/
	KFile fd;
	uint8_t state;
	uint8_t row;
	uint8_t col;
	int16_t addr;
} Term;






/**
 * ID for serial.
 */
#define KFT_TERM MAKE_ID('T', 'E', 'R', 'M')


INLINE Term * TERM_CAST(KFile *fd)
{
	ASSERT(fd->_type == KFT_TERM);
	return (Term *)fd;
}




void term_init(struct Term *fds);
