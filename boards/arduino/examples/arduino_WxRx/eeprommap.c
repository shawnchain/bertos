//---------------------------------------------------------------------------
// Copyright (C) 2013 Robin Gilks
//
//
//  eeprommap.c   -   All the variables that are in eeprom are declared here
//
//  History:   1.0 - First release. 
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// include files

#include "eeprommap.h"

#include <stdint.h>
#include <stdbool.h>
#include <avr/eeprom.h>

#include "rtc.h"

extern int16_t gRain;

// these variables are all in one place so that if another is added, we don't 
// loose the existing value provided new stuff is *ALWAYS* added to the end


// running total of idle current being summed until its large enough to affect DCS register
int16_t EEMEM eeRain;
// configured number of seconds per day to adjust clock for slow/fast 16MHz crystal
int16_t EEMEM eeAdjustTime;
// date and time stored when set and every hour so clock isn't too far out after a reset
DT_t EEMEM eeDateTime;

void load_eeprom_values(void)
{

	// initial time and date setting
	eeprom_read_block ((void *) &gDateTime, (const void *) &eeDateTime, sizeof (gDateTime));
	eeprom_read_block ((void *) &gAdjustTime, (const void *) &eeAdjustTime, sizeof (gAdjustTime));
	eeprom_read_block ((void *) &gRain, (const void *) &eeRain, sizeof (gRain));


}

void save_eeprom_values(void)
{
	eeprom_write_block ((const void *) &gDateTime, (void *) &eeDateTime, sizeof (gDateTime));
	eeprom_write_block ((const void *) &gAdjustTime, (void *) &eeAdjustTime, sizeof (gAdjustTime));
	eeprom_write_block ((const void *) &gRain, (void *) &eeRain, sizeof (gRain));

}