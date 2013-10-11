//---------------------------------------------------------------------------
// Copyright (C) 2013 Robin Gilks
//
//
//  WxTx.c   -   This program collects weather data from sensors and sends it to a LaCrosse base unit via RF
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

#include <cfg/debug.h>

#include <stdint.h>

#include <avr/eeprom.h>

#include <drv/timer.h>
#include <drv/ser.h>
#include <algo/crc8.h>

#include <drv/ow_1wire.h>
#include <drv/ow_ds2438.h>
#include <drv/ow_ds18x20.h>


#define CHKINV 0

/*--------------------------------------------------------------------------------------
TX20 and TX23 wind sensor protocol - as documented by john.geek.nz

 Note that I found that the high voltage level out of my TX20 was too low to be seen as a logic high on the Arduino so I put 
 in a npn transistor buffer which inverts the signal. This is good - it saves complicating the code.

The datagram is 41 bits long and contains six data sections. Each bit is almost exactly 1.2msec, so the datagram 
takes 49.2 msec to transmit nd uses Inverted logic (0v = 1, +Vcc = 0).

Section     Length (bits)     Inverted?     Endianness     Description     Notes
A              5               Yes          LSB first      Start Frame     Always 00100
B              4               Yes          LSB first      Wind Direction  0-15, see table below
C             12               Yes          LSB first      Wind Speed      0-511
D              4               Yes          LSB first      Checksum        See below
E              4               No           LSB first      Wind Direction  0-15, see table below
F             12               No           LSB first      Wind Speed      0-511

A – Start Frame and Triggering / Detecting start of data
If you hold the DTR line low permanently, the TxD line will be held low unless the unit is transmitting data.
The start frame is always 00100, but due to being inverted logic (making it detected as 11011), you can use a rising 
edge trigger / interrupt to detect the start of the datagram.

B – Wind Direction
The wind direction is supplied as a 4 bit value. It requires inverting and needs it’s endianness swapped. Once this 
is done it’s a value of 16th’s of a revolution from North. Thus it can be multiplied by 22.5 to get a direction in 
Degrees, or a 16 value array can be used to return the direction.

In the example image above, the Wind direction is read as 0011, Inverted to 1100 and then it’s endianness is swapped 
to 0011, which is ENE, or 67.5 degrees.

Table of the directions is below.
Binary     Hex     Decimal     Direction
0000        0        0           N
0001        1        1           NNE
0010        2        2           NE
0011        3        3           ENE
0100        4        4           E
0101        5        5           ESE
0110        6        6           SE
0111        7        7           SSE
1000        8        8           S
1001        9        9           SSW
1010        A        10          SW
1011        B        11          WSW
1100        C        12          W
1101        D        13          WNW
1110        E        14          NW
1111        F        15          NNW

C – Wind Speed
The wind speed is a 12 bit value. It requires inverting and needs it’s endianness swapped.
Once this is done it’s a value in units of 0.1 metre/sec. The 3 MSB’s are always 000, so only 9 bits are used. With 
a max value of 511, this relates to 51.1 metres per second, or 183.96 km/h (114.31 miles per hour).

From the example image above, the wind speed is read as 111010101111, Inverted to 000101010000, then endianness 
swapped to 000010101000, which is 168, or 16.8 metres per second.

D – Checksum
The checksum is a 4 bit value. it requires inverting and needs it’s endianness swapped.
This is a 4 bit value of the four least significant bits of the SUM of the wind direction and the three nibbles of 
the 12 bit windspeed.
The checksum in the datagram above is read as 0101, Inverted to 1010, then endianness swapped to 0101.

For the datagram image above, the checksum would be calculated by:
Name                 Bits     Example
Wind Direction     1 – 4      0011
Wind Speed         1 – 4      0000
Wind Speed         5 – 8      1010
Wind Speed         9 – 12     1000
SUM                4 LSBs     0101

As the Checksum received matches the calculated Checksum, the received data is likely to be correct.

E – Wind Direction (Inverted)
The wind direction (Inverted) is supplied as a 4 bit value. It doesn’t require inverting, but still needs it’s 
endianness swapped. It can then be interpreted like section “B” above.

In the example image above, the Wind direction is read as 1100 and it’s endianness is swapped to 0011, which is ENE, 
or 67.5 degrees.

F – Wind Speed (Inverted)
The wind speed (Inverted) is a 12 bit value. It doesn’t require inverting, but still needs it’s endianness swapped. 
It can then be interpreted like section “C” above.

From the example image above, the wind speed is read as 000101010000 and it’s endianness is swapped to 000010101000, 
which is 168, or 16.8 metres per second.
*/




/*--------------------------------------------------------------------------------------
  The WS2355 sends 52 bits in a packet and the format is

A ’0′ bit is represented by sending a ‘long’ high signal followed by a ‘long’ low signal, while 
a ’1′ bit is represented by sending a ‘short’ high signal followed by a ‘long’ low signal.

   Long  = 1200uS
   Short = 600uS

i.e.
   0 = TTtt
   1 = Ttt
where (t) = 600uS


Nibble     0           1           2              3               4               5               6               7               8               9              10              11              12
Bit        0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  47  48  49  50  51
Function  |      I7-I0            |G |X | P1-P0 |             S7-S0             |         T4-T0     | F1-F0| D12|              D11..D4          |    D3..D0     |           Q11..Q4             |     C3..C0
Bits  Description

I7:I0  
Preamble/sync.  This never changes.
WS2550 = 00001001 (09 hex) [WS3600 = 00000110 (06 hex)]

G  For weather stations that report wind gusts this bit is set to 1 while P1:P0 is set to 11 to indicate that this is a wind gust packet.
For all other combinations of P1:P0 this bit is set to 0.
X  This bit is used as part of the error checking.  This bit when XOR’d with D0-D12 and F0-F1 will equal 1 if the data is OK.

P1:P0  
This is the indication of the type of data contained in the packet.
00 = Temperature
01 = Humidity
10 = Rain
11 = Windspeed and direction (if G is 0 this is average data. If G is 1 this is gust data)

S7:S0  
This is populated with the random sensor identification that is generated when the sensor is powered up.  If the sensor loses power a new id will be generated when power is restored.

T4:T0  
This is populated with flags indicating which packets are included in this group of packets.  For each packet in the group this will therefore be the same value.  The meaning of the bits is:
T4 = if 1 then wind gust packets are being sent (confirmed)
T3 = if 1 then wind average packets are being sent (not confirmed, always 1)
T2 = if 1 then rain packets are being sent (confirmed)
T1 = if 1 then humidity packets are being sent (not confirmed, always 1)
T0 = if 1 then temperature packets are being sent (not confirmed, always 1)

F1:F0  
This field indicates the period that the sensor is going to wait before sending the next group of packets.
00 = 4 seconds
01 = 32 seconds
10 = 128 seconds
11 = unused (not confirmed, never seen)
D12:D0  

For temperature the sensor reports the temperature in increments of 0.1C from -40 to 59.9.
Temperate = ((D11:D8*10) + (D7:D4) + (D3:D0/10)) – 40
nb. D12 is not used and is always 0

For Humidity the sensor reports the humidity as an integer.
Humidity = (D11:D8*10) + (D7:D4)
D3:D1 = NOT S3:S0
nb. D12 is not used and is always 0

For Rain the sensor reports the number of tips of the seesaw in the range of 0-4095.  Once the count reaches 4095 it wraps back to 0.  This count gets reset to 0 if the sensor loses power.  Each tip of the seesaw is 0.508mm.
Rain = D11:D0
nb. D12 is not used and is always 0

For Wind (both average and gust) the data is reported in increments of 0.1m/S from 0 to 50.
Wind speed = D12:D4 (for gust a value of 510 means no gust)
Wind Direction = D3:D0 (0000=N, 0001=NNE, 0010=NE, 0011=ENE, 0100=E, 0101=ESE, 0110=SE, 0111=SSE, 1000=S, 1001=SSW, 1010=SW, 1011=WSW, 1100=W, 1101=WNW, 1110=NW, 1111=NNW)
Q11:Q4  

Inverted data.  This provides for another check for valid packets.
Q11:Q4 = NOT D11:D4
C3:C0  

This is the checksum of the packet.
C3:C0 = The sum of nibbles 0 to 11

Example packets

nibble  0   1   2   3   4   5   6   7   8   9  10  11  12
        0123012301230123012301230123012301230123012301230123
byte    1         2         3         4         5       6
        0123456701234567012345670123456701234567012345670123
bit     0123456789012345678901234567890123456789012345678901 0011223344556
        /--||--\/--||--\/--||--\/--||--\/--||--\/--||--\/--| 
     1) 0000100101000010001001111000010100110011101011000001 0942278533AC1 23.3? (533 = 53.3deg, - 30.0deg offset)
                  ssiiiiiiii        tttttttttttt        cccc        ttt
     2) 0000100100010010001001111000010100001101101011111000 091227850DAF8 50% RH
                  ssiiiiiiii        hhhhhhhh                        hh
     3) 0000100100100010001001111000000010001100111101111000 092227808CF78 140 rainfall, 72.5 mm
                  ssiiiiiiii        rrrrrrrrrrrr                    rrr
     4) 0000100101110010001001111000000000001100111111111101 097227800CFFD W   (12) wind, speed 0.0m/s 0.0km/h
                  ssiiiiiiii        vvvvvvvvwwww
     5) 0000100101000010001001111000010100110011101011000001 0942278533AC1 23.3?
                  ssiiiiiiii
     6) 0000100100010010001001111000010100001101101011111000 091227850DAF8 50% RH
                  ssiiiiiiii
     7) 0000100100100010001001111000000010001100111101111000 092227808CF78 140 rainfall, 72.5 mm
                  ssiiiiiiii
     8) 0000100101110010001001111000000000001100111111111101 097227800CFFD W   (12) wind, speed 0.0m/s 0.0km/h
                  ssiiiiiiii


*/





// packet types and their offsets in the array of packets
#define IDX_temp   0
#define IDX_rh     1
#define IDX_rain   2
#define IDX_wind   3
#define IDX_MAX    4


// nibble offsets into each packet
#define OFF_pre_hi    0
#define OFF_pre_lo    1
#define OFF_type      2
#define OFF_rand_hi   3
#define OFF_rand_lo   4
#define OFF_flags     5
#define OFF_rpt       6
#define OFF_D_hi      7
#define OFF_D_lo      8
#define OFF_D_ext     9
#define OFF_Q_hi     10
#define OFF_Q_lo     11
#define OFF_crc      12
#define OFF_MAX      13

int16_t EEMEM eeRain;
int16_t gRain;

// initialised with static test data
uint8_t data [IDX_MAX][OFF_MAX] = {
	{ 0x0, 0x9, 0x4, 0x2, 0x2, 0x7, 0x8, 0x5, 0x3, 0x3, 0xA, 0xC, 0x1 },
	{ 0x0, 0x9, 0x1, 0x2, 0x2, 0x7, 0x8, 0x5, 0x0, 0xD, 0xA, 0xF, 0x8 },
	{ 0x0, 0x9, 0x2, 0x2, 0x2, 0x7, 0x8, 0x0, 0x8, 0xC, 0xF, 0x7, 0x8 },
	{ 0x0, 0x9, 0x7, 0x2, 0x2, 0x7, 0x8, 0x0, 0x0, 0xC, 0xF, 0xF, 0xD }
};

uint8_t ids[4][OW_ROMCODE_SIZE];	// only expect to find 2 actually!!
int8_t battid = -1, thermid = -1;


// port PB1 as output for 433MHz transmitter on Arduino pin D9
#define RF_INIT() do { DDRB |= BV(1); } while(0)
#define RF_ON()   do { PORTB |= BV(1); } while(0)
#define RF_OFF()  do { PORTB &= ~BV(1); } while(0)

// port PB0 as rain sensor input (PCINT0) on Arduino pin D8
// set as input, pullup on, enable interrupt
#define RAIN_INIT() do { \
	DDRB &= ~BV (0);      \
	PORTB |= BV (0);      \
	PCICR |= BV(PCIE0);   \
	} while(0)
// enable pcint 0
#define RAIN_ON()  do { PCMSK0 |= BV (PCINT0); } while(0)
// disable pcint 0
#define RAIN_OFF() do { PCMSK0 &= ~BV (PCINT0); } while(0)

#define TX20_INIT()  do { \
	DDRB &= ~BV (3);      \
	PORTB |= BV (3);      \
	DDRB |= BV (4);      \
	PORTB |= BV (4);      \
	} while(0)
#define TX20_OFF()  do { PORTB |= BV(4); } while(0)
#define TX20_ON()   do { PORTB &= ~BV(4); } while(0)
#define TX20_DATA() (PINB & BV(3))
#define TX20_TIMEOUT 5000

// run timer1 at /8 of 16MHz clock
#define START_US() do { \
	TCCR1A = 0;          \
	TCCR1B = BV (CS11);  \
	TCNT1 = 0;           \
	} while(0)
#define READ_US() (TCNT1 / 2)


// dummy values to satisfy the API for the battery monitor functions of the 2438 driver
// current sense shunt resistor
#define RSHUNT 1
// initial amount of charge in battery
#define CHARGE 1

static
void init(void)
{
	uint8_t diff, cnt = 0;

	/* Enable all the interrupts */
	IRQ_ENABLE;

	/* Initialize debugging module (allow kprintf(), etc.) */
//	kdbg_init();
	/* Initialize system timer */
	timer_init();

	// Signal change trigger interrupt for rain tipper on pcint0 D8 (PB0)
	RAIN_INIT();
	RAIN_ON();
	// get the last rainfall value
	eeprom_read_block ((void *) &gRain, (const void *) &eeRain, sizeof (gRain));

	//output to transmitter to base unit
	RF_INIT();

	// input for the TX20 wind sensor
	TX20_INIT();

// search for battery monitor chip (used by humidity sensor) and temperature sensor
// we will only use the temperature sensor if we can't find a humidity sensor interface
	for (diff = OW_SEARCH_FIRST, cnt = 0; diff != OW_LAST_DEVICE; cnt++)
	{
		diff = ow_rom_search (diff, ids[cnt]);

		if ((diff == OW_PRESENCE_ERR) || (diff == OW_DATA_ERR))
		{
			break;	// <--- early exit!
		}
		if (crc8 (ids[cnt], 8))
			break;	// <--- suspect CRC - early exit!

		switch (ids[cnt][0])
		{
		case SBATTERY_FAM:
			battid = cnt;
			break;
		// digital thermometer chips can also be used but we will return a fixed or null humidity reading
		case DS18S20_FAMILY_CODE:
		case DS18B20_FAMILY_CODE:
		case DS1822_FAMILY_CODE:
			thermid = cnt;
			break;
		}
	}

}


// return a 16 bit int with up to 4 BCD digits in it formed from the binary parameter
static
uint16_t int2BCD(int16_t input)
{
	uint16_t high = 0;
	int16_t divs[3] = { 1000, 100, 10};
	int8_t i;

	for (i = 0; i < 3; i++)
	{
		while (input >= divs[i])
		{
			high++;
			input -= divs[i];
		}
		high <<= 4;
	}
	return high | input;
}


// fill in the sync
// fill in the type of this packet with parity set correctly
// invent some stuff for the random ID (not random!!)
// set the flags and repeat interval
// create the Q bits from the D bits
// calculate the CRC and fill it in
static
void finish_pkt(uint8_t index)
{
	uint8_t i, j = 0, x = 0;

	// calculate the error detection 'X' bit. Bit is set if parity even in D[]
	x = (data[index][OFF_D_hi] ^ data[index][OFF_D_lo] ^ data[index][OFF_D_ext]) & 0xf;
	// this magic number is a 'lookup table' that maps the number of bits (i.e. parity) of the 4 bits in the nibble
	// plus; shift the result to the correct place for the 'type' nibble
	x = ((0x9669 >> x) & 1) << 2;
	data[index][OFF_pre_hi] = 0x0;
	data[index][OFF_pre_lo] = 0x9;
	data[index][OFF_type] = x | index;
	data[index][OFF_rand_hi] = 0x2;
	data[index][OFF_rand_lo] = 0x2;
	data[index][OFF_flags] = 0x7;
	data[index][OFF_rpt] = 0x8;
	data[index][OFF_Q_hi] = ~data[index][OFF_D_hi];
	data[index][OFF_Q_lo] = ~data[index][OFF_D_lo];
	// calculate the CRC
	for (i = 0; i < OFF_crc; i++)
	{
		j += data[index][i];
	}
	data[index][OFF_crc] = j & 0xf;
}

// temperature goes into D12-D0 as 3 BCD digits with an offset of 300
static
void set_temp(int16_t temp)
{
	uint16_t t;

	t = int2BCD(temp + 300);
	data[IDX_temp][OFF_D_hi] = (t >> 8) & 0xf;
	data[IDX_temp][OFF_D_lo] = (t >> 4) & 0xf;
	data[IDX_temp][OFF_D_ext] = t & 0xf;
	finish_pkt(IDX_temp);
}

// RH goes into D12-D4 as 2 BCD digits
static
void set_RH(int16_t rh)
{
	uint16_t t;

	t = int2BCD(rh);
	data[IDX_rh][OFF_D_hi] = (t >> 4) & 0xf;
	data[IDX_rh][OFF_D_lo] = t & 0xf;
// don't ask about the <cr> char here!! its needed!!
	data[IDX_rh][OFF_D_ext] = 0xd;
	finish_pkt(IDX_rh);

}

// rain goes into D12-D0 as binary
static
void set_rain(int16_t rain)
{
	data[IDX_rain][OFF_D_hi] = (rain >> 8) & 0xf;
	data[IDX_rain][OFF_D_lo] = (rain >> 4) & 0xf;
	data[IDX_rain][OFF_D_ext] = rain & 0xf;
	finish_pkt(IDX_rain);

}

// wind goes into D12-D4 as binary, direction as binary in D3-D0
static
void set_wind(uint8_t dirn, int16_t speed)
{

	data[IDX_wind][OFF_D_hi] = (speed >> 4) & 0xf;
	data[IDX_wind][OFF_D_lo] = speed & 0xf;
	data[IDX_wind][OFF_D_ext] = dirn;
	finish_pkt(IDX_wind);

}


// BeRTOS fast timer on Arduino is limited to ~1.0mS so use multiple times to get delays we want
static
void tx(uint8_t bit)
{

	RF_ON();
	timer_udelay(600);
	if (bit == 0)
		timer_udelay(600);
	RF_OFF();
	timer_udelay(600);
	timer_udelay(600);
}


// send a single packet out
static
void send_pkt(uint8_t index)
{
	uint8_t i, j, nibble, bit;

	for (i = 0; i < OFF_MAX; i++)
	{
		nibble = data[index][i];
		for (j = 0; j < 4; j++)
		{
			bit = nibble & 8;
			nibble <<= 1;
			tx(bit);
		}
	}
}


// send all 4 packets out twice
static
void send_all (void)
{
	uint8_t i, j;
	// send all pkts twice
	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 4; j++)
		{
			send_pkt(j);
			timer_delay(250);
		}
	}
}

// rainfall counter. Disable interrupts when a tip seen
ISR (PCINT0_vect)
{
	gRain++;
	RAIN_OFF();
}


// data is 0v = '0'; Vcc = '1', each bit is ~1.2mS
// read the first 2 bits of the sync pattern to determine bit rate, then the following 3 bits
// and then read the following 20 bits, we can ignore the trailing inverted values
// the data is 
// 4 bits  direction
// 12 bits speed
// 4 bits  checksum
// 4 bits  inverted direction
// 12 bit  inverted speed
static int16_t read_tx20 (int16_t * Dirn, int16_t * Wind)
{
	ticks_t timeout = timer_clock();
	uint8_t i, crc, sync;
	uint16_t halfbit, dirn, wind;
#if CHKINV
	int16_t temp;
#endif

// while  overall timeout of ~3 seconds
// wait for data '0'
// start uS timer
// wait for data '1' (this means we now have 2 bits)
// use timer value as bit period
// read remaining 3 bits of sync pattern
// read next 20 bits

// look for data start
	while (TX20_DATA() != 0)
	{
		if (timer_clock() - timeout > ms_to_ticks (TX20_TIMEOUT))
		{
			return -1;
		}
	}

// start microsecond timer, wait for data to change again
	START_US();
	while (TX20_DATA() == 0)
	{
		if (timer_clock() - timeout > ms_to_ticks (TX20_TIMEOUT))
		{
			return -1;
		}
	}

// determine halfbit period - should have time of 2 bits
	halfbit = READ_US() / 4;
	timer_udelay(halfbit);
// read rest of sync
	sync = 0;
	for (i = 0; i < 3; i++)
	{
		sync >>= 1;
		sync |= TX20_DATA() ? 16 : 0;
		timer_udelay(halfbit);
		timer_udelay(halfbit);
	}
	if (sync != 0b00100)
	{
//		kprintf("Bad sync %d\n", sync);
		return -1;
	}

// read wind direction - note the reversed shift to get endianness right
	dirn = 0;
	for (i = 0; i < 4; i++)
	{
		dirn >>= 1;
		dirn |= TX20_DATA() ? 8 : 0;
		timer_udelay(halfbit);
		timer_udelay(halfbit);
	}

// read wind speed
	wind = 0;
	for (i = 0; i < 12; i++)
	{
		wind >>= 1;
		wind |= TX20_DATA() ? 2048 : 0;
		timer_udelay(halfbit);
		timer_udelay(halfbit);
	}

// read checksum
	crc = 0;
	for (i = 0; i < 4; i++)
	{
		crc >>= 1;
		crc |= TX20_DATA() ? 8 : 0;
		timer_udelay(halfbit);
		timer_udelay(halfbit);
	}

// test checksum
	i = dirn + ((wind >> 8) & 0xf) + ((wind >> 4) & 0xf) + (wind & 0xf);
	if (i != crc)
	{
//		kprintf("bad crc %d %d\n", crc, i);
		return -1;
	}

// seems pretty reliable - no need to complicate things with checking the inverse values...
#if CHKINV
	temp = 0;
	for (i = 0; i < 4; i++)
	{
		temp >>= 1;
		temp |= TX20_DATA() ? 0 : 8;
		timer_udelay(halfbit);
		timer_udelay(halfbit);
	}
	if (temp != gDirn)
	{
		kprintf("Inv dirn error %d\n", temp);
		return -1;
	}

	temp = 0;
	for (i = 0; i < 12; i++)
	{
		temp >>= 1;
		temp |= TX20_DATA() ? 0 : 2048;
		timer_udelay(halfbit);
		timer_udelay(halfbit);
	}
	if (temp != gWind)
	{
		kprintf("Inv wind error %d\n", temp);
		return -1;
	}
#endif

	// all OK, return result
	*Dirn = dirn;
	*Wind = wind;
	return 1;

}






// initialise hardware I/O and timers
// poll serial port for commands
// send data packets to transmitter
int main (void)
{
	int16_t last_rain;
	int16_t temperature = 0, humidity = 0, Dirn = 0, Wind = 0;
	float supply_volts, sensor_volts;
	CTX2438_t BatteryCtx;

	init();

	// if a TX20 (rather than a TX23) then set DTR low
	// TX23 requires a waggle (see later)
	TX20_ON();

	// get the last rainfall value
	last_rain = gRain;

	while (1)
	{

		if (battid >= 0)
		{
			// default initialise gets volts from the Vad pin
			ow_ds2438_init (ids[battid], &BatteryCtx, RSHUNT, CHARGE);
			ow_ds2438_doconvert (ids[battid]);
			if (!ow_ds2438_readall (ids[battid], &BatteryCtx))
				continue;                   // bad read - exit fast!!
			temperature = BatteryCtx.Temp;
			sensor_volts = (float)BatteryCtx.Volts;

			// switch voltage sense to supply pin
			ow_ds2438_setup(ids[battid], CONF2438_IAD | CONF2438_AD | CONF2438_CA | CONF2438_EE);
			ow_ds2438_doconvert (ids[battid]);
			if (!ow_ds2438_readall (ids[battid], &BatteryCtx))
				continue;                   // bad read - exit fast!!
			supply_volts = (float)BatteryCtx.Volts;
			humidity = (((sensor_volts / supply_volts) - 0.16) / 0.0062) / (1.0546 - 0.00216 * (temperature / 100.0));
			if (humidity > 100)
				humidity = 100;
			else if (humidity < 0)
				humidity = 0;
		} 

		else if (thermid >= 0)
		{
			// 10 bit resolution is enough (more is much slower!!)
			ow_ds18x20_resolution(ids[thermid], 10);
			ow_ds18X20_start (ids[thermid], false);
			while (ow_busy ());
			ow_ds18X20_read_temperature (ids[thermid], &temperature);
			// the minimum value the console will display is 10%RH
			// set to 0 if you want a blank section in the display!!
			humidity = 10;
		}

		// if the rain changed then save the new value in eeprom
		if (gRain != last_rain)
		{
			last_rain = gRain;
			eeprom_write_block ((const void *) &gRain, (void *) &eeRain, sizeof (gRain));
			RAIN_ON();          // enable rain sensor input again
		}


		set_temp(temperature / 10);
		set_RH(humidity);
		set_rain(gRain);

// if a TX23 then we have to waggle 'DTR' first. For a TX20 we only have to do it once at startup
//		TX20_ON();
//		timer_delay(500);
//		TX20_OFF();
//		timer_delay(2000);
		if (read_tx20(&Dirn, &Wind) > 0)
			set_wind(Dirn, Wind);

// test data while debugging!!
//		set_temp(233);
//		set_RH(50);
//		set_rain(140);
//		set_wind(12, 0);

		send_all();
		timer_delay(2000);
	}
}

