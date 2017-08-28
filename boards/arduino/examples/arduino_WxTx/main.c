//---------------------------------------------------------------------------
// Copyright (C) 2013-2017 Robin Gilks
//
//
//  WxTx.c   -   This program collects weather data from sensors and sends it to a LaCrosse base unit via RF
//
//  History:   1.0 - First release. 
//             1.1 - Use state machine for rain tipper input and poll it rather than use external interrupt
//             1.2 - Ditch 1-wire to use DHT22 to replace the DS2438 hygrometer & DS18B20 temperature sensor
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
#include <stdlib.h>
#include <math.h>

#include <avr/eeprom.h>

#include <drv/timer.h>
#include <drv/ser.h>
#include <algo/crc8.h>

#include <drv/ow_1wire.h>
#include <drv/ow_ds2438.h>
#include <drv/ow_ds18x20.h>

#include <drv/i2c.h>
#include <cfg/cfg_i2c.h>

#include <drv/bme280.h>
#include <drv/bme280_defs.h>

#include "hw/hw_bme280.h"

// Connections to Arduino                    DHT-22        BME280       1-wire
// pin  4 Ground
// pin  8 port PD5 (Arduino pin D5)          supply        supply       supply
// pin  9 port PD6 (Arduino pin D6)          data          gnd          data (defined in cfg/cfg_1wire.h)
// pin 24 port PC5 (Arduino pin ADC5/SCL)    n/c           scl          n/c
// pin 23 port PC4 (Arduino pin ADC4/SDA)    gnd           sda          gnd
// pin  7 port PD4 (Arduino pin D4) as rain tipper input 
// pin 12 port PB1 (Arduino pin D9) as output for 433MHz transmitter
// pin 14 port PB3 (Arduino pin D11) as wind sensor input 
// pin 15 port PB4 (Arduino pin D12) as DTR signal to wind sensor
// pin 16 port PB5 (Arduino pin D13) as onboard LED


// DHT22 connections
// Pin 1 - vcc
// Pin 2 - data
// Pin 3 - n/c
// Pin 4 - gnd

// BME280 connections
// Pin 1 - vcc
// Pin 2 - gnd
// Pin 3 - scl
// Pin 4 - sda


// set to 1 if a TX23 rather than a TX20
#define TX23   0
// set to 1 if we want to check the invers data from the TX20/23
#define CHKINV 0
// local display (serial) on the Arduino
#define LOCAL_DISPLAY 1
// see if using 1-wire DS2438 or DHT22
#define ONEWIRE 0
#define DHT22   0
#define BME280  1


/*--------------------------------------------------------------------------------------
TX20 and TX23 wind sensor protocol - as documented by john.geek.nz

 Note that I found that the high voltage level out of my TX20 was too low to be seen as a logic high on the Arduino so I put 
 in a npn transistor buffer which inverts the signal. This is good - it saves complicating the code.

The datagram is 41 bits long and contains six data sections. Each bit is almost exactly 1.2msec, so the datagram 
takes 49.2 msec to transmit and uses Inverted logic (0v = 1, +Vcc = 0).

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
float temperature = 0, humidity = 0, pressure = 0;

// initialised with static test data
uint8_t data[IDX_MAX][OFF_MAX] = {
   {0x0, 0x9, 0x4, 0x2, 0x2, 0x7, 0x8, 0x5, 0x3, 0x3, 0xA, 0xC, 0x1}
   ,
   {0x0, 0x9, 0x1, 0x2, 0x2, 0x7, 0x8, 0x5, 0x0, 0xD, 0xA, 0xF, 0x8}
   ,
   {0x0, 0x9, 0x2, 0x2, 0x2, 0x7, 0x8, 0x0, 0x8, 0xC, 0xF, 0x7, 0x8}
   ,
   {0x0, 0x9, 0x7, 0x2, 0x2, 0x7, 0x8, 0x0, 0x0, 0xC, 0xF, 0xF, 0xD}
};

uint8_t ids[4][OW_ROMCODE_SIZE];        // only expect to find 2 actually!!
int8_t battid = -1, thermid = -1;
struct bme280_dev bme280dev;


void readsensors (void);


// port PD7 as rain tipper input on Arduino pin D7
#define DEBOUNCE 16

// set as input, pullup on, timer 2 on
// PWM phase correct, clk/1, enable output compare int, period ~= 9uS 
// need to debouce for ~= 200uS so count over 16 counts
#define RAIN_INIT() do {  \
	DDRD &= ~BV (4);       \
	PORTD |= BV (4);       \
	TCCR2A = BV(WGM21);    \
	TCCR2B = BV(CS20);     \
	OCR2A = 200;           \
	TIMSK2 = BV(OCIE2A);   \
	} while(0)
// check the state of the tipper input
#define RAIN_POLL() (PIND & BV(4))
// what states the reed switch on the rain tipper can be in
enum rain_states
{
   OPEN = 1,
   CLOSING,
   CLOSED,
   OPENING
};

// port PB1 as output for 433MHz transmitter on Arduino pin D9
#define RF_INIT() do {    \
	DDRB |= BV(1);    \
	} while(0)
#define RF_ON()   do { PORTB |= BV(1); } while(0)
#define RF_OFF()  do { PORTB &= ~BV(1); } while(0)

// port PB2 (Arduino pin D10) is already defined as the 1-wire interface in cfg/cfg_1wire.h

// port PB3 (Arduino pin D11) as wind sensor input 
// port PB4 (Arduino pin D12) as DTR signal to wind sensor
#define TX20_INIT()  do {     \
	DDRB &= ~BV (3);      \
	PORTB |= BV (3);      \
	DDRB |= BV (4);       \
	PORTB |= BV (4);      \
	} while(0)
#define TX20_OFF()  do { PORTB |= BV(4); } while(0)
#define TX20_ON()   do { PORTB &= ~BV(4); } while(0)
#define TX20_DATA() (PINB & BV(3))
#define TX20_TIMEOUT 5000


#if DHT22 == 1
// port PC6 (Arduino pin ADC6) is the DHT-22
// set as in input with pullup
#define DHT22_INPUT() do {    \
   DDRD &= ~BV (6);           \
   PORTD |= BV (6);           \
   } while(0)

// set line as an output and set it low
#define DHT22_OUTPUT() do {   \
   DDRD |= BV (6);            \
   PORTD &= ~BV (6);          \
   } while(0)


// port PD5 high, PC4 low to provide power
#define DHT22_POWER() do {    \
   DDRD |= BV (5);   \
   DDRC |= BV (4);   \
   PORTD |= BV (5);           \
   PORTC &= ~BV (4);          \
   } while(0)


// return true/false for input D6
#define DHT22_READ() ((PIND & BV(6)) ? true : false)
#endif


#if BME280 == 1
// port PC7 high, PC4 low to provide power
#define BME280_POWER() do {    \
   DDRD |= BV (5) | BV (6);   \
   PORTD |= BV (5);           \
   PORTD &= ~BV (6);          \
   } while(0)

#endif


#if ONEWIRE == 1
// port PD5 high, PC4 low to provide power
#define ONEWIRE_POWER() do {    \
   DDRD |= BV (5);   \
   DDRC |= BV (4);   \
   PORTD |= BV (5);           \
   PORTC &= ~BV (4);          \
   } while(0)

#endif

#define LED_INIT() do {       \
   DDRB |= BV (4);            \
   } while(0)

#define LED_TOGGLE() do {     \
   PORTB ^= BV (5);           \
   } while(0)

// run timer1 at /8 of 16MHz clock to get microsecond timings
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

// to emulate the WS2315, restrict the range of humidity reported
#define MAXHUMIDITY 99
#define MINHUMIDITY 5



I2c i2c;





static void
init (void)
{
#if ONEWIRE == 1
   uint8_t diff, cnt = 0;
#endif
   /* Enable all the interrupts */
   IRQ_ENABLE;

   /* Initialize debugging module (allow kprintf(), etc.) */
#if LOCAL_DISPLAY == 1
   kdbg_init ();
#endif

   /* Initialize system timer */
   timer_init ();

   // Signal change trigger interrupt for rain tipper on pcint0 D8 (PB0)
   RAIN_INIT ();
   // get the last rainfall value
   eeprom_read_block ((void *) &gRain, (const void *) &eeRain, sizeof (gRain));


   // output pin to transmit to base unit
   RF_INIT ();

   // input pin(s) for the TX20 wind sensor
   TX20_INIT ();

   // get ready to toggle D13
   LED_INIT ();

#if ONEWIRE == 1
   ONEWIRE_POWER ();         // same power pins as a DHT22

// search for battery monitor chip (used by humidity sensor) and temperature sensor
// we will only use the temperature sensor if we can't find a humidity sensor interface
   for (diff = OW_SEARCH_FIRST, cnt = 0; diff != OW_LAST_DEVICE; cnt++)
   {
      diff = ow_rom_search (diff, ids[cnt]);

      if ((diff == OW_PRESENCE_ERR) || (diff == OW_DATA_ERR))
      {
         break;                 // <--- early exit!
      }
      if (crc8 (ids[cnt], 8))
         break;                 // <--- suspect CRC - early exit!

      switch (ids[cnt][0])
      {
      case SBATTERY_FAM:
         battid = cnt;
         break;
         // digital thermometer chips will be used by preferene for temperature readings only
         // can also be used alone but we will return a fixed or null humidity reading
      case DS18S20_FAMILY_CODE:
      case DS18B20_FAMILY_CODE:
      case DS1822_FAMILY_CODE:
         thermid = cnt;
         break;
      }
   }
#elif DHT22 == 1
   DHT22_POWER ();
   DHT22_INPUT ();

#elif BME280 == 1

   // power up the sensor
   BME280_POWER ();
   timer_delay (10);

   //init i2c
   i2c_hw_init (&i2c, 0, CONFIG_I2C_FREQ);
   timer_udelay (10);

   bme280dev.id = BME280_I2C_ADDR_PRIM;
   bme280dev.interface = BME280_I2C_INTF;
   bme280dev.read = BME280_I2C_bus_read;
   bme280dev.write = BME280_I2C_bus_write;
   bme280dev.delay_ms = BME280_Time_Delay;

   bme280_init (&bme280dev);

#else
   #error "No sensor type defined"
#endif
}


// return a 16 bit int with up to 4 BCD digits in it formed from the binary parameter
static uint16_t
int2BCD (int16_t input)
{
   uint16_t high = 0;
   int16_t divs[3] = { 1000, 100, 10 };
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
static void
finish_pkt (uint8_t index)
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
static void
set_temp (int16_t temp)
{
   uint16_t t;

   t = int2BCD (temp + 300);
   data[IDX_temp][OFF_D_hi] = (t >> 8) & 0xf;
   data[IDX_temp][OFF_D_lo] = (t >> 4) & 0xf;
   data[IDX_temp][OFF_D_ext] = t & 0xf;
   finish_pkt (IDX_temp);
}

// RH goes into D12-D4 as 2 BCD digits
static void
set_RH (int16_t rh)
{
   uint16_t t;

   t = int2BCD (rh);
   data[IDX_rh][OFF_D_hi] = (t >> 4) & 0xf;
   data[IDX_rh][OFF_D_lo] = t & 0xf;
// don't ask about the <cr> char here!! its needed!!
   data[IDX_rh][OFF_D_ext] = 0xd;
   finish_pkt (IDX_rh);

}

// rain goes into D12-D0 as binary
static void
set_rain (int16_t rain)
{
   data[IDX_rain][OFF_D_hi] = (rain >> 8) & 0xf;
   data[IDX_rain][OFF_D_lo] = (rain >> 4) & 0xf;
   data[IDX_rain][OFF_D_ext] = rain & 0xf;
   finish_pkt (IDX_rain);

}

// wind goes into D12-D4 as binary, direction as binary in D3-D0
static void
set_wind (uint8_t dirn, int16_t speed)
{

   data[IDX_wind][OFF_D_hi] = (speed >> 4) & 0xf;
   data[IDX_wind][OFF_D_lo] = speed & 0xf;
   data[IDX_wind][OFF_D_ext] = dirn;
   finish_pkt (IDX_wind);

}


// BeRTOS fast timer on Arduino is limited to ~1.0mS so use multiple times to get delays we want
static void
tx (uint8_t bit)
{

   RF_ON ();
   timer_udelay (600);
   if (bit == 0)
      timer_udelay (600);
   RF_OFF ();
   timer_udelay (600);
   timer_udelay (600);
}


// send a single packet out
static void
send_pkt (uint8_t index)
{
   uint8_t i, j, nibble, bit;

   for (i = 0; i < OFF_MAX; i++)
   {
      nibble = data[index][i];
      for (j = 0; j < 4; j++)
      {
         bit = nibble & 8;
         nibble <<= 1;
         tx (bit);
      }
   }
}


// send all 4 packets out twice
static void
send_all (void)
{
   uint8_t i, j;
   // send all pkts twice
   for (i = 0; i < 2; i++)
   {
      for (j = 0; j < 4; j++)
      {
         send_pkt (j);
         timer_delay (250);
      }
   }
}



ISR (TIMER2_COMPA_vect)
{
   static uint8_t state = OPEN;
   static int16_t count = 0;
   uint8_t newstate = state;

   switch (state)
   {
   case OPEN:
      if (RAIN_POLL () == 0)    // closing?
      {
         newstate = CLOSING;
      }
      break;
   case CLOSING:
      if (RAIN_POLL () == 0)    // still closing?
      {
         if (++count == DEBOUNCE)
         {
            newstate = CLOSED;
         }
      }
      else                      // bouncing or opening
      {
         newstate = OPEN;
      }
      break;
   case CLOSED:
      if (RAIN_POLL () != 0)    // opening?
      {
         newstate = OPENING;
      }
      break;
   case OPENING:
      if (RAIN_POLL () != 0)    // still opening
      {
         if (++count == DEBOUNCE)
         {
            newstate = OPEN;
            gRain++;            // we have seen one more tip
         }
      }
      else
      {
         newstate = CLOSED;
      }
      break;
   }

// if the state changes then always reset the counter to zero
   if (newstate != state)
   {
      count = 0;
      state = newstate;
   }

}


#if LOCAL_DISPLAY == 1
static float
dewpoint (float T, float h)
{
   float td;
   // Simplified dewpoint formula from Lawrence (2005), doi:10.1175/BAMS-86-2-225
   td = T - (100 - h) * pow (((T + 273.15) / 300), 2) / 5 - 0.00135 * pow (h - 84, 2) + 0.35;
   return td;
}

static float
windchill (float temp, float wind)
{
   float wind_chill;
   float wind2 = pow (wind, 0.16);

   wind_chill = 13.12 + (0.6215 * temp) - (11.37 * wind2) + (0.3965 * temp * wind2);
   wind_chill = (wind <= 4.8) ? temp : wind_chill;
   wind_chill = (temp > 10) ? temp : wind_chill;
   return wind_chill;
}
#endif


// data is 0v = '0'; Vcc = '1', each bit is ~1.2mS
// read the first 2 bits of the sync pattern to determine bit rate, then the following 3 bits
// and then read the following 20 bits, we can ignore the trailing inverted values
// the data is 
// 4 bits  direction
// 12 bits speed
// 4 bits  checksum
// 4 bits  inverted direction
// 12 bit  inverted speed
static int16_t
read_tx20 (int16_t * Dirn, int16_t * Wind)
{
   ticks_t timeout = timer_clock ();
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
   while (TX20_DATA () != 0)
   {
      if (timer_clock () - timeout > ms_to_ticks (TX20_TIMEOUT))
      {
         return -1;
      }
   }

// start microsecond timer, wait for data to change again
   START_US ();
   while (TX20_DATA () == 0)
   {
      if (timer_clock () - timeout > ms_to_ticks (TX20_TIMEOUT))
      {
         return -1;
      }
   }

// determine halfbit period - should have time of 2 bits
   halfbit = (READ_US () / 4);
   timer_udelay (halfbit);
   // read rest of sync
   sync = 0;
   for (i = 0; i < 3; i++)
   {
      sync >>= 1;
      sync |= TX20_DATA ()? 16 : 0;
      timer_udelay (halfbit);
      timer_udelay (halfbit);
   }
   if (sync != 0b00100)
   {
//              kprintf("Bad sync %d\n", sync);
      return -1;
   }

// read wind direction - note the reversed shift to get endianness right
   dirn = 0;
   for (i = 0; i < 4; i++)
   {
      dirn >>= 1;
      dirn |= TX20_DATA ()? 8 : 0;
      timer_udelay (halfbit);
      timer_udelay (halfbit);
   }

// read wind speed
   wind = 0;
   for (i = 0; i < 12; i++)
   {
      wind >>= 1;
      wind |= TX20_DATA ()? 2048 : 0;
      timer_udelay (halfbit);
      timer_udelay (halfbit);
   }

// read checksum
   crc = 0;
   for (i = 0; i < 4; i++)
   {
      crc >>= 1;
      crc |= TX20_DATA ()? 8 : 0;
      timer_udelay (halfbit);
      timer_udelay (halfbit);
   }

// test checksum
   i = (dirn + ((wind >> 8) & 0xf) + ((wind >> 4) & 0xf) + (wind & 0xf)) & 0xf;
   if (i != crc)
   {
//              kprintf("read crc %d my crc %d dirn %d wind %d \n", crc, i, dirn, wind);
      return -1;
   }

// seems pretty reliable - no need to complicate things with checking the inverse values...
#if CHKINV == 1
   temp = 0;
   for (i = 0; i < 4; i++)
   {
      temp >>= 1;
      temp |= TX20_DATA ()? 0 : 8;
      timer_udelay (halfbit);
      timer_udelay (halfbit);
   }
   if (temp != gDirn)
   {
      kprintf ("Inv dirn error %d\n", temp);
      return -1;
   }

   temp = 0;
   for (i = 0; i < 12; i++)
   {
      temp >>= 1;
      temp |= TX20_DATA ()? 0 : 2048;
      timer_udelay (halfbit);
      timer_udelay (halfbit);
   }
   if (temp != gWind)
   {
      kprintf ("Inv wind error %d\n", temp);
      return -1;
   }
#endif

   // all OK, return result
   *Dirn = dirn;
   *Wind = wind;
   return 1;

}

void
readsensors (void)
{
#if ONEWIRE == 1
   float supply_volts, sensor_volts;
   CTX2438_t BatteryCtx;
   int16_t rawtemperature = 0;

   if (thermid >= 0)
   {
      // 10 bit resolution is enough (more is much slower!!)
      // in the range -10 to +85 the accuracy is only +/-0.5C
      ow_ds18x20_resolution (ids[thermid], 10);
      ow_ds18X20_start (ids[thermid], false);
      while (ow_busy ());
      ow_ds18X20_read_temperature (ids[thermid], &rawtemperature);
      temperature = rawtemperature;
   }

   if (battid >= 0)
   {
      // default initialise gets volts from the Vad pin
      ow_ds2438_init (ids[battid], &BatteryCtx, RSHUNT, CHARGE);
      ow_ds2438_doconvert (ids[battid]);
      if (!ow_ds2438_readall (ids[battid], &BatteryCtx))
         return;           // bad read - exit fast!!
      // if no DS18x20 temperature sensor then use this one
      // its only +/-2C but its better than nothing!!
      if (thermid < 0)
         temperature = BatteryCtx.Temp;
      sensor_volts = (float) BatteryCtx.Volts;

      // switch voltage sense to supply pin
      ow_ds2438_setup (ids[battid], CONF2438_IAD | CONF2438_AD | CONF2438_CA | CONF2438_EE);
      ow_ds2438_doconvert (ids[battid]);
      if (!ow_ds2438_readall (ids[battid], &BatteryCtx))
         return;           // bad read - exit fast!!
      supply_volts = (float) BatteryCtx.Volts;
      humidity = (((sensor_volts / supply_volts) - 0.16) / 0.0062) / (1.0546 - 0.00216 * (temperature / 100.0));
   }
/* HIH-5030
VOUT=(VSUPPLY)(0.00636(sensor RH) + 0.1515), typical at 25 C
Temperature compensation True RH = (Sensor RH)/(1.0546 0.00216T), T
*/


#elif DHT22 == 1


   static ticks_t lastReadTime = 0;
   static int16_t average = 220;
   int16_t i;
   uint16_t rawHumidity = 0;
   uint16_t rawTemperature = 0;
   uint16_t data = 0;
   uint8_t age;

   // Make sure we don't poll the sensor too often
   // - Max sample rate DHT11 is 1 Hz   (duty cicle 1000 ms)
   // - Max sample rate DHT22 is 0.5 Hz (duty cicle 2000 ms)
   ticks_t startTime = timer_clock ();
   if ((startTime - lastReadTime) < 2100L)
      return;

   LED_TOGGLE ();

   lastReadTime = startTime;

   temperature = NAN;
   humidity = NAN;

   // Request sample
   DHT22_OUTPUT ();             // Send start signal
   timer_delay (20);
   DHT22_INPUT ();              // Switch bus to receive data

   // We're going to read 83 edges:
   // - First a FALLING, RISING, and FALLING edge for the start bit
   // - Then 40 bits: RISING and then a FALLING edge per bit
   // To keep our code simple, we accept any HIGH or LOW reading if it's max 85 usecs long
   // _US runs on a 16 bit 2MHz clock so can handle times up to ~32ms, we require up to 42 * (50 + 70) == ~0.5ms

   START_US ();

   for (i = -3; i < 2 * 40; i++)
   {
      startTime = READ_US ();

      do
      {
         age = READ_US () - startTime;
         if (age > 90)
            return;
      }  while (DHT22_READ() == (i & 1) ? true : false);

      if (i >= 0 && (i & 1))
      {
         // Now we are being fed our 40 bits
         data <<= 1;

         // A zero max 30 usecs, a one at least 68 usecs.
         if (age > 30)
            data |= 1;          // we got a one
      }

      switch (i)
      {
      case 31:
         rawHumidity = data;
         break;
      case 63:
         rawTemperature = data;
         data = 0;
         break;
      }
   }


   // Verify checksum

   if ((uint8_t) (((uint8_t) rawHumidity) + (rawHumidity >> 8) + ((uint8_t) rawTemperature) + (rawTemperature >> 8)) != data)
      return;

   // Store readings
   humidity = rawHumidity * 0.1;

   if (rawTemperature & 0x8000)
      rawTemperature = -(int16_t) (rawTemperature & 0x7FFF);

   // if temperature difference to running average less than 0.5c then update temperature
   // prevents bad values of 0 getting through
   if (abs(average - rawTemperature) < 5)
      temperature = (int16_t) rawTemperature;
#define alpha 3
   average = (alpha * (uint16_t) rawTemperature + (100 - alpha) * (uint16_t) average) / 100;


#elif BME280 == 1

   struct bme280_data comp_data;
   uint8_t settings_sel;

   bme280dev.settings.osr_h = BME280_OVERSAMPLING_4X;
   bme280dev.settings.osr_p = BME280_OVERSAMPLING_4X;
   bme280dev.settings.osr_t = BME280_OVERSAMPLING_4X;

   settings_sel = BME280_OSR_PRESS_SEL|BME280_OSR_TEMP_SEL|BME280_OSR_HUM_SEL;
   bme280_set_sensor_settings(settings_sel, &bme280dev);
   bme280_set_sensor_mode(BME280_FORCED_MODE, &bme280dev);
   /* Give some delay for the sensor to go into normal mode */
   bme280dev.delay_ms(5);

   if (bme280_get_sensor_data(BME280_PRESS | BME280_HUM | BME280_TEMP, &comp_data, &bme280dev) == 0)
   {
      temperature = comp_data.temperature / 10;
      humidity = comp_data.humidity / 1000;
      pressure = comp_data.pressure / 10000;
//      kprintf("Raw temp %ld, hum %ld, press %ld\n", comp_data.temperature,  comp_data.humidity, comp_data.pressure);
   }
#endif
}



// initialise hardware I/O and timers
// poll serial port for commands
// send data packets to transmitter
int
main (void)
{
   int16_t last_rain;
   int16_t Dirn = 0, Wind = 0;

   init ();

   // if a TX20 (rather than a TX23) then set DTR low
   // TX23 requires a waggle (see later)
#if TX23 == 0
   TX20_ON ();
#endif

   // get the last rainfall value
   last_rain = gRain;

   while (1)
   {
      readsensors ();

      if (humidity > MAXHUMIDITY)
         humidity = MAXHUMIDITY;
      else if (humidity < MINHUMIDITY)
         humidity = MINHUMIDITY;

      // if the rain changed then save the new value in eeprom
      if (gRain != last_rain)
      {
         gRain &= 4095;
         last_rain = gRain;
         eeprom_write_block ((const void *) &gRain, (void *) &eeRain, sizeof (gRain));
      }


      set_temp (temperature);
      set_RH (humidity);
      set_rain (gRain);

// if a TX23 then we have to waggle 'DTR' first. For a TX20 we only have to do it once at startup
#if TX23 == 1
      TX20_ON ();
      timer_delay (500);
      TX20_OFF ();
      timer_delay (2000);
#endif
      if (read_tx20(&Dirn, &Wind) > 0)
         set_wind(Dirn, Wind);

#if LOCAL_DISPLAY == 1
      {
         float wc, dp;
         float height = 90, p;
         wc = windchill ((float) temperature / 100, (float) Wind * 0.36);
         dp = dewpoint ((float) temperature / 100, (float) humidity);
         // P = ((Po * 1000) * Math.exp((g*Zg)/(Rd *  (Tv_avg + 273.15))))/1000;
         p = ((pressure * 1000) * exp((9.8 * height)/(287 *  ((temperature / 10) + 273.15))))/1000;

         kprintf ("To: %2.2f WC: %2.1f DP: %2.1f Rtot: %4.1f RHo: %2.2f WS: %3.1f DIR0: %3.1f RP: %4.1f  P0: %4.1f\n",
                  (float) temperature / 10, wc, dp, (float) gRain * 0.51826, humidity, (float) Wind * 0.36,
                  (float) Dirn * 22.5, pressure, p);
      }
#endif

// test data while debugging!!
//              set_temp(233);
//              set_RH(50);
//              set_rain(140);
//              set_wind(12, 0);

      send_all ();
      timer_delay (100);
   }
}
