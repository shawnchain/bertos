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

#ifndef HW_XYWMODEM_H
#define HW_XYWMODEM_H

#include <stdint.h>
#include <avr/io.h>

#include "cfg/cfg_arch.h"


void hw_xyw_init(int bps);





#define XYW_HW_INIT(bps)     hw_xyw_init(bps)
#define XYW_TX_START         do { DDRB |= BV(1); PORTB |= BV(2); TIMSK1 |= BV(OCIE1A); } while (0)
#define XYW_TX_DATA(data)    do { if (data) PORTB |= BV(0); else PORTB &= ~BV(0); } while (0)
#define XYW_TX_STOP          do { DDRB &= ~BV(1); PORTB &= ~BV(2); TIMSK1 &= ~BV(OCIE1A); } while (0)


/*
Default connections are as follows:
TxData  on Port B bit 0 (arduino pin D8)
TxClock on Port B bit 1 (arduino pin D9)
PTT     on Port B bit 2 (arduino pin D10)

RxData  on Port C bit 0 (arduino pin AIN0)
RxClock on Port C bit 1 (arduino pin AIN1)
DCD     on Port C bit 2 (arduino pin AIN2)
*/


#endif
