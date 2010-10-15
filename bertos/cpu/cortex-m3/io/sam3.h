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
 * Copyright 2010 Develer S.r.l. (http://www.develer.com/)
 *
 * -->
 *
 * \author Stefano Fedrigo <aleph@develer.com>
 */

#ifndef SAM3_H
#define SAM3_H

#include <cpu/detect.h>
#include <cfg/compiler.h>

/*
 * Peripherals IDs, same as interrupt numbers.
 */
#define SUPC_ID    INT_SUPC     ///< Supply Controller (SUPC)
#define RSTC_ID    INT_RSTC     ///< Reset Controller (RSTC)
#define RTC_ID     INT_RTC      ///< Real Time Clock (RTC)
#define RTT_ID     INT_RTT      ///< Real Time Timer (RTT)
#define WDT_ID     INT_WDT      ///< Watchdog Timer (WDT)
#define PMC_ID     INT_PMC      ///< Power Management Controller (PMC)
#define EFC_ID     INT_EFC      ///< Enhanced Flash Controller (EFC)
#define UART0_ID   INT_UART0    ///< UART 0 (UART0)
#define UART1_ID   INT_UART1    ///< UART 1 (UART1)
#define UART2_ID   INT_UART2    ///< UART 0 (UART0)
#define UART3_ID   INT_UART3    ///< UART 1 (UART1)
#define PIOA_ID    INT_PIOA     ///< Parallel I/O Controller A (PIOA)
#define PIOB_ID    INT_PIOB     ///< Parallel I/O Controller B (PIOB)
#define PIOC_ID    INT_PIOC     ///< Parallel I/O Controller C (PIOC)
#define US0_ID     INT_USART0   ///< USART 0 (USART0)
#define US1_ID     INT_USART1   ///< USART 1 (USART1)
#define TWI0_ID    INT_TWI0     ///< Two Wire Interface 0 (TWI0)
#define TWI1_ID    INT_TWI1     ///< Two Wire Interface 1 (TWI1)
#define SPI0_ID    INT_SPI      ///< Serial Peripheral Interface (SPI)
#define TC0_ID     INT_TC0      ///< Timer/Counter 0 (TC0)
#define TC1_ID     INT_TC1      ///< Timer/Counter 1 (TC1)
#define TC2_ID     INT_TC2      ///< Timer/Counter 2 (TC2)
#define TC3_ID     INT_TC3      ///< Timer/Counter 3 (TC3)
#define TC4_ID     INT_TC4      ///< Timer/Counter 4 (TC4)
#define TC5_ID     INT_TC5      ///< Timer/Counter 5 (TC5)
#define ADC_ID     INT_ADC      ///< Analog To Digital Converter (ADC)
#define DACC_ID    INT_DACC     ///< Digital To Analog Converter (DACC)
#define PWM_ID     INT_PWM      ///< Pulse Width Modulation (PWM)

/*
 * Hardware features for drivers.
 */
#define USART_HAS_PDC  1


#include "sam3_sysctl.h"
#include "sam3_pmc.h"
#include "sam3_ints.h"
#include "sam3_pio.h"
#include "sam3_nvic.h"
#include "sam3_uart.h"
#include "sam3_usart.h"
#include "sam3_spi.h"
#include "sam3_flash.h"
#include "sam3_wdt.h"

/**
 * UART I/O pins
 */
/*\{*/
#if CPU_CM3_AT91SAM3U
	#define RXD0   11
	#define TXD0   12
#else
	#define RXD0    9
	#define TXD0   10
	#define RXD1    2
	#define TXD1    3
#endif
/*\}*/

/**
 * PIO I/O pins
 */
/*\{*/
#if CPU_CM3_AT91SAM3U
	#define SPI0_SPCK   15
	#define SPI0_MOSI   14
	#define SPI0_MISO   13
#else
	#define SPI0_SPCK   14
	#define SPI0_MOSI   13
	#define SPI0_MISO   12
#endif
/*\}*/
#endif /* SAM3_H */
