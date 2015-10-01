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
 * Copyright 2008 Develer S.r.l. (http://www.develer.com/)
 * All Rights Reserved.
 * -->
 *
 * \brief Configuration file for serial module.
 *
 * \author Daniele Basile <asterix@develer.com>
 */

#ifndef CFG_SER_H
#define CFG_SER_H

/**
 * Example of setting for serial port and
 * spi port.
 * Edit these define for your project.
 */

/**
 * Enable port 0
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmega"
 */
#define CONFIG_UART0_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 0 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 */
#define CONFIG_UART0_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 0 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 */
#define CONFIG_UART0_RXBUFSIZE  32

/**
 * Enable port 1
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmega"
 */
#define CONFIG_UART1_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 1 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "lm3s or lpc2 or xmega or (at91 and not atmega8 and not atmega168 and not atmega32)"
 */
#define CONFIG_UART1_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 1 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "lm3s or lpc2 or xmega or (at91 and not atmega8 and not atmega168 and not atmega32)"
 */
#define CONFIG_UART1_RXBUFSIZE  32

/**
 * Enable port 2
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmega and (not xmegad4)"
 */
#define CONFIG_UART2_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 2 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "lm3s or lpc2 or (xmega and not xmegad4)"
 */
#define CONFIG_UART2_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 2 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "lm3s or lpc2 or (xmega and not xmegad4)"
 */
#define CONFIG_UART2_RXBUFSIZE  32

/**
 * Enable port 3
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmega and not xmegad4"
 */
#define CONFIG_UART3_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 3 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "lpc2 or xmega and not xmegad4"
 */
#define CONFIG_UART3_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 3 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "lpc2 or xmega and not xmegad4"
 */
#define CONFIG_UART3_RXBUFSIZE  32

/**
 * Enable port 4
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmega and not xmegad4"
 */
#define CONFIG_UART4_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 4 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmega and not xmegad4"
 */
#define CONFIG_UART4_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 4 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmega and not xmegad4"
 */
#define CONFIG_UART4_RXBUFSIZE  32

/**
 * Enable port 5
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmegaa1 or xmegaa3"
 */
#define CONFIG_UART5_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 5 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmegaa1 or xmegaa3"
 */
#define CONFIG_UART5_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 5 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmegaa1 or xmegaa3"
 */
#define CONFIG_UART5_RXBUFSIZE  32

/**
 * Enable port 6
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmegaa1 or xmegaa3"
 */
#define CONFIG_UART6_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 6 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmegaa1 or xmegaa3"
 */
#define CONFIG_UART6_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 6 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmegaa1 or xmegaa3"
 */
#define CONFIG_UART6_RXBUFSIZE  32

/**
 * Enable port 7
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "xmegaa1"
 */
#define CONFIG_UART7_ENABLED 1

/**
 * Size of the outbound FIFO buffer for port 7 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmegaa1"
 */
#define CONFIG_UART7_TXBUFSIZE  32

/**
 * Size of the inbound FIFO buffer for port 7 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "xmegaa1"
 */
#define CONFIG_UART7_RXBUFSIZE  32

/**
 * Enable SPI.
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "False"
 */
#define CONFIG_SPI_ENABLED        0

/**
 * Size of the outbound FIFO buffer for SPI port [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "avr and not xmega"
 */
#define CONFIG_SPI_TXBUFSIZE    32

/**
 * Size of the inbound FIFO buffer for SPI port [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "avr and not xmega"
 */
#define CONFIG_SPI_RXBUFSIZE    32

/**
 * Size of the outbound FIFO buffer for SPI port 0 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "at91"
 */
#define CONFIG_SPI0_TXBUFSIZE	32

/**
 * Size of the inbound FIFO buffer for SPI port 0 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "at91"
 */
#define CONFIG_SPI0_RXBUFSIZE	32

/**
 * Size of the outbound FIFO buffer for SPI port 1 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "at91"
 */
#define CONFIG_SPI1_TXBUFSIZE	32

/**
 * Size of the inbound FIFO buffer for SPI port 1 [bytes].
 * $WIZ$ type = "int"
 * $WIZ$ min = 2
 * $WIZ$ supports = "at91"
 */
#define CONFIG_SPI1_RXBUFSIZE	32

/**
 * SPI data order.
 *
 * $WIZ$ type = "enum"
 * $WIZ$ value_list = "ser_order_bit"
 * $WIZ$ supports = "avr and not xmega"
 */
#define CONFIG_SPI_DATA_ORDER	SER_MSB_FIRST

/**
 * SPI clock division factor.
 * $WIZ$ type = "int"
 * $WIZ$ supports = "avr and not xmega"
 */
#define CONFIG_SPI_CLOCK_DIV	16

/**
 * SPI clock polarity: normal low or normal high.
 * $WIZ$ type = "enum"
 * $WIZ$ value_list = "ser_spi_pol"
 * $WIZ$ supports = "avr and not xmega"
 */
#define CONFIG_SPI_CLOCK_POL        SPI_NORMAL_LOW

/**
 * SPI clock phase you can choose sample on first edge or
 * sample on second clock edge.
 * $WIZ$ type = "enum"
 * $WIZ$ value_list = "ser_spi_phase"
 * $WIZ$ supports = "avr and not xmega"
 */
#define CONFIG_SPI_CLOCK_PHASE	    SPI_SAMPLE_ON_FIRST_EDGE

/**
 * Default transmit timeout (ms). Set to -1 to disable timeout support.
 * $WIZ$ type = "int"
 * $WIZ$ min = -1
 */
#define CONFIG_SER_TXTIMEOUT    -1

/**
 * Default receive timeout (ms). Set to -1 to disable timeout support.
 * $WIZ$ type = "int"
 * $WIZ$ min = -1
 */
#define CONFIG_SER_RXTIMEOUT    -1

/**
 * Use RTS/CTS handshake.
 * $WIZ$ type = "boolean"
 * $WIZ$ supports = "False"
 */
#define CONFIG_SER_HWHANDSHAKE   0

/**
 * Default baudrate for all serial ports (set to 0 to disable).
 * $WIZ$ type = "int"
 * $WIZ$ min = 0
 */
#define CONFIG_SER_DEFBAUDRATE   0UL

/// Enable strobe pin for debugging serial interrupt. $WIZ$ type = "boolean"
#define CONFIG_SER_STROBE        0

#endif /* CFG_SER_H */
