//---------------------------------------------------------------------------
// Copyright (C) 2013 Robin Gilks
//
//
//  WxRx.c   -   This program collects weather data from RF sensors and sends it to a PC via USB/serial link
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
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <avr/eeprom.h>

#include <drv/timer.h>
#include <drv/ser.h>
#include <io/kfile.h>

#include "eeprommap.h"
#include "rtc.h"

// Comment out for a normal build
// Uncomment for a debug build
//#define DEBUG

#define INPUT_CAPTURE_IS_RISING_EDGE()    ((TCCR1B & BV(ICES1)) != 0)
#define INPUT_CAPTURE_IS_FALLING_EDGE()   ((TCCR1B & BV(ICES1)) == 0)
#define SET_INPUT_CAPTURE_RISING_EDGE()   (TCCR1B |=  BV(ICES1))
#define SET_INPUT_CAPTURE_FALLING_EDGE()  (TCCR1B &= ~BV(ICES1))
#define GREEN_TESTLED_ON()          ((PORTD &= ~BV(6)))
#define GREEN_TESTLED_OFF()         ((PORTD |=  BV(6)))
#define RED_TESTLED_ON()            ((PORTD &= ~BV(7)))
#define RED_TESTLED_OFF()           ((PORTD |=  BV(7)))


/* serial port communication (via USB) */
#define BAUD_RATE 115200

#define PACKET_SIZE 11          /* number of nibbles in packet (after inital byte) */
#define PACKET_START 0x09       /* byte to match for start of packet */

// 0.6 ms high is a one
#define MIN_ONE 135             // minimum length of '1'
#define MAX_ONE 165             // maximum length of '1'
// 1.2 ms high is a zero
#define MIN_ZERO 270            // minimum length of '0'
#define MAX_ZERO 330            // maximum length of '0'
// 1.2 ms between bits
#define MIN_WAIT 270            // minimum interval since end of last bit
#define MAX_WAIT 330            // maximum interval since end of last bit


unsigned int CapturedTime;
unsigned int PreviousCapturedTime;
unsigned int CapturedPeriod;
unsigned int PreviousCapturedPeriod;
unsigned int SinceLastBit;
unsigned int LastBitTime;
unsigned int BitCount;
float tempC;                    /* temperature in deg C */
float speedW;                   /* wind speed in km/h */
float windC;                    /* wind chill */
float dp;                       /* dewpoint (deg C) */
uint8_t dirn;                   /* wind direction in 22.5degree steps */
uint8_t rh = 50;                /* relative humidity */
uint8_t DataPacket[PACKET_SIZE];        /* actively loading packet */
uint8_t FinishedPacket[PACKET_SIZE];    /* fully read packet */
uint8_t PacketBitCounter;
bool ReadingPacket;
bool PacketDone;

Serial serial;

int16_t rain_mins[60];
int16_t rain_hours[24];
int16_t gRain;
int16_t rain1h;                 /* rain in last hour */
int16_t rain24h;                /* rain in last day */

#define RAINCONVERT(r) ((float)r * 0.51826)

const unsigned char DIRN[16][4] = {
   " N ",
   "NNE",
   " NE",
   "ENE",
   " E ",
   "ESE",
   " SE",
   "SSE",
   " S ",
   "SSW",
   " SW",
   "WSW",
   " W ",
   "WNW",
   " NW",
   "NNW"
};


uint8_t CapturedPeriodWasHigh;
uint8_t PreviousCapturedPeriodWasHigh;
uint8_t mask;                   /* temporary mask uint8_t */
uint8_t CompByte;               /* byte containing the last 8 bits read */


// data capture timing from Kelsey Jordahl, Marc Alexander, Jonathan Oxer
// basic capture timing stuff but it works so...
ISR (TIMER1_CAPT_vect)
{
   // Immediately grab the current capture time in case it triggers again and
   // overwrites ICR1 with an unexpected new value
   CapturedTime = ICR1;

   // GREEN test led on (flicker for debug)
   GREEN_TESTLED_ON ();
   if (INPUT_CAPTURE_IS_RISING_EDGE ())
   {
      SET_INPUT_CAPTURE_FALLING_EDGE ();        //previous period was low and just transitioned high
      CapturedPeriodWasHigh = false;    //CapturedPeriod about to be stored will be a low period
   }
   else
   {
      SET_INPUT_CAPTURE_RISING_EDGE (); //previous period was high and transitioned low
      CapturedPeriodWasHigh = true;     //CapturedPeriod about to be stored will be a high period
   }

   CapturedPeriod = (CapturedTime - PreviousCapturedTime);

   if ((CapturedPeriod > MIN_ONE) && (CapturedPeriodWasHigh == true))
   {                            // possible bit
      /* time from end of last bit to beginning of this one */
      SinceLastBit = (PreviousCapturedTime - LastBitTime);

      if ((CapturedPeriod < MAX_ONE) && (SinceLastBit > MIN_WAIT))
      {
         if (SinceLastBit > MAX_WAIT)
         {                      // too long since last bit read
            if ((SinceLastBit > (2 * MIN_WAIT + MIN_ONE)) && (SinceLastBit < (2 * MAX_WAIT + MAX_ONE)))
            {                   /* missed a one */
#ifdef DEBUG
               kfile_printf (&serial.fd, "missed one\r\n");
#endif
            }
            else
            {
               if ((SinceLastBit > (2 * MIN_WAIT + MIN_ZERO)) && (SinceLastBit < (2 * MAX_WAIT + MAX_ZERO)))
               {                /* missed a zero */
#ifdef DEBUG
                  kfile_printf (&serial.fd, "missed zero\r\n");
#endif
               }
            }
            RED_TESTLED_OFF ();
            if (ReadingPacket)
            {
#ifdef DEBUG
               kfile_printf (&serial.fd, "dropped packet. bits read: %d\r\n", PacketBitCounter);
#endif
               ReadingPacket = 0;
               PacketBitCounter = 0;
            }
            CompByte = 0xFF;    /* reset comparison byte */
         }
         else
         {                      /* call it a one */
            if (ReadingPacket)
            {                   /* record the bit as a one */
               mask = (1 << (3 - (PacketBitCounter & 0x03)));
               DataPacket[(PacketBitCounter >> 2)] |= mask;
               PacketBitCounter++;
            }
            else
            {                   /* still looking for valid packet data */
               if (CompByte != 0xFF)
               {                /* don't bother recording if no zeros recently */
                  CompByte = ((CompByte << 1) | 0x01);  /* push one on the end */
               }
            }
            LastBitTime = CapturedTime;
         }
      }
      else
      {                         /* Check whether it's a zero */
         if ((CapturedPeriod > MIN_ZERO) && (CapturedPeriod < MAX_ZERO))
         {
            if (ReadingPacket)
            {                   /* record the bit as a zero */
               mask = (1 << (3 - (PacketBitCounter & 0x03)));
               DataPacket[(PacketBitCounter >> 2)] &= ~mask;
               PacketBitCounter++;
            }
            else
            {                   /* still looking for valid packet data */
               CompByte = (CompByte << 1);      /* push zero on the end */
            }
            LastBitTime = CapturedTime;
         }
      }
   }

   if (ReadingPacket)
   {
      if (PacketBitCounter == (4 * PACKET_SIZE))
      {                         /* done reading packet */
         memcpy (&FinishedPacket, &DataPacket, PACKET_SIZE);
         RED_TESTLED_OFF ();
         PacketDone = 1;
         ReadingPacket = 0;
         PacketBitCounter = 0;
      }
   }
   else
   {
      /* Check whether we have the start of a data packet */
      if (CompByte == PACKET_START)
      {
         CompByte = 0xFF;       /* reset comparison byte */
         RED_TESTLED_ON ();
         /* set a flag and start recording data */
         ReadingPacket = 1;
      }
   }

   //save the current capture data as previous so it can be used for period calculation again next time around
   PreviousCapturedTime = CapturedTime;
   PreviousCapturedPeriod = CapturedPeriod;
   PreviousCapturedPeriodWasHigh = CapturedPeriodWasHigh;

   //GREEN test led off (flicker for debug)
   GREEN_TESTLED_OFF ();
}



// calculate dew point from humidity and temperature
static float
dewpoint (float T, float h)
{
   float td;
   // Simplified dewpoint formula from Lawrence (2005), doi:10.1175/BAMS-86-2-225
   td = T - (100 - h) * pow (((T + 273.15) / 300), 2) / 5 - 0.00135 * pow (h - 84, 2) + 0.35;
   return td;
}


// calculate wind chill from temperature and wind speed
// values are badly off over 10C and wind less than 4.8km/h so return temperature unchanged
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


// keep a history of raw total rainfall over 1 hour and 1 day 
// so that actuals can be calculated when required.
static void
update_rain (void)
{
   rain_mins[gMINUTE] = gRain;
   rain_hours[gHOUR] = gRain;
   rain1h = gRain - rain_mins[(gMINUTE + 1) % 60];
   rain24h = gRain - rain_hours[(gHOUR + 1) % 24];

}


// initialise system, get last rainfall value and set up rainfall history buffers
static void
init (void)
{
   uint8_t j;

   /* Initialize system timer */
   timer_init ();
   /* Initialize UART0 */
   ser_init (&serial, SER_UART0);
   /* Configure UART0 to work at 115.200 bps */
   ser_setbaudrate (&serial, 115200);
   // get the last rainfall value & last time setting
   load_eeprom_values ();
   // set clock up using last values from eeprom
   rtc_init ();
//  kdbg_init();

   for (j = 0; j < 60; j++)
      rain_mins[j] = gRain;
   for (j = 0; j < 24; j++)
      rain_hours[j] = gRain;
   update_rain ();

   for (j = 0; j < PACKET_SIZE; j++)
      DataPacket[j] = 0;

   DDRB = 0x2F;                // B00101111
   DDRB &= ~BV(0);             //PBO(ICP1) input
   PORTB &= ~BV(0);            //ensure pullup resistor is also disabled

   //PORTD6 and PORTD7, GREEN and RED test LED setup
   DDRD |= BV(6) | BV(7);         //(1<<PORTD6);   //DDRD  |=  (1<<PORTD7); (example of B prefix)
   GREEN_TESTLED_OFF ();        //GREEN test led off
//  RED_TESTLED_ON();         //RED test led on
   // Set up timer1 for RF signal detection
   TCCR1A = 0B00000000;        //Normal mode of operation, TOP = 0xFFFF, TOV1 Flag Set on MAX
   TCCR1B = (BV (ICNC1) | BV (CS11) | BV (CS10));
   SET_INPUT_CAPTURE_RISING_EDGE ();
   //Timer1 Input Capture Interrupt Enable
   TIMSK1 = BV (ICIE1);

   /* Enable all the interrupts */
   IRQ_ENABLE;
   kfile_printf (&serial.fd, "La Crosse weather station simulator\r\n");
}

static void
print_weather (void)
{
   kfile_printf (&serial.fd, "%02d-%02d-%02d %02d:%02d:%02d ", gDAY, gMONTH, gYEAR, gHOUR, gMINUTE, gSECOND);
   kfile_printf (&serial.fd,
                 "To:%2.1f WC:%2.1f DP:%2.1f Rtot:%4.1f R1h:%2.1f R24h:%3.1f RHo:%d WS:%3.1f DIR0:%3.1f DIR1:%s\r\n",
                 tempC, windC, dp, RAINCONVERT (gRain), RAINCONVERT (rain1h), RAINCONVERT (rain24h), rh, speedW,
                 (float) dirn * 22.5, (const char *) DIRN[(int) dirn]);
}


// parse a raw data string from the receiver
// looks at the packet type and only outputs data when all 4 types have been updated since last time
static void
ParsePacket (uint8_t * Packet)
{

   uint8_t chksum, j;
   int16_t rain;
   static uint8_t collectedData = 0;

#ifdef DEBUG
   kfile_printf (&serial.fd, "RAW: ");
   for (j = 0; j < PACKET_SIZE; j++)
   {
      kfile_printf (&serial.fd, "%01x", Packet[j]);
   }
   kfile_printf (&serial.fd, "\r\n");
#endif

   chksum = PACKET_START;
   for (j = 0; j < PACKET_SIZE - 1; j++)
   {
      chksum += Packet[j];
   }

   if ((chksum & 0xf) == (Packet[PACKET_SIZE - 1] & 0xf))
   {                            /* checksum pass */
      /* make sure that most significant digits repeat inverted */
      if (((Packet[5] & 0xf) == (~Packet[8] & 0xf)) && ((Packet[6] & 0xf) == (~Packet[9] & 0xf)))
      {
         switch (Packet[0] & 0x3)
         {
         case 0:               /* temperature packet */
            tempC = ((float) (Packet[5] * 100 + Packet[6] * 10 + Packet[7]) - 300) / 10;
            dp = dewpoint (tempC, rh);
            collectedData |= 1;
            break;
         case 1:               /* humidity packet */
            rh = (Packet[5] * 10 + Packet[6]);
            collectedData |= 2;
            break;
         case 2:               /* rain */
            rain = Packet[5] * 256 + Packet[6] * 16 + Packet[7];
            // if rain has changed then record it
            if (rain != gRain)
            {
               gRain = rain;
               save_eeprom_values ();
               update_rain ();
            }
            collectedData |= 4;
            break;
         case 3:               /* wind speed and direction */
            speedW = (Packet[5] * 16 + Packet[6]) * 0.36;
            dirn = Packet[7] & 0xf;
            windC = windchill (tempC, speedW);
            collectedData |= 8;
            break;
         }

         if (collectedData == 15)
         {
            print_weather ();
            collectedData = 0;
         }
      }
      else
      {
#ifdef DEBUG
         kfile_printf (&serial.fd, "Fail secondary data check\r\n");
#endif
      }
   }
   else
   {                            /* checksum fail */
#ifdef DEBUG
      kfile_printf (&serial.fd, "chksum = %2x, data chksum %2x\r\n", chksum & 0x0f, Packet[PACKET_SIZE - 1]);
#endif
   }
}

// parse a string for decimal, used to get user numeric input
static char *
get_decimal (char *ptr, int16_t * v)
{
   int16_t t = 0;

   while (*ptr >= '0' && *ptr <= '9')
      t = (t * 10) + (*ptr++ - '0');

   // skip over terminator
   ptr++;

   *v = t;
   return ptr;
}

// check a value is within the limits supplied
static bool
check_value (int16_t * var, int16_t value, int16_t lower, int16_t upper)
{

   if ((value < lower) || (value > upper))
      return true;

   *var = value;
   return false;

}

// interpret a command from the serial interface
// implements get/set time in form hh:mm:ss
//            get/set date in form dd-mm-yy
//            get/set time correction factor in seconds per day
// can use any non-numeric for delimiter in time/date strings!
// empty string outputs the currently gathered data
static void
process_command (char *command, uint8_t count)
{
   if (count == 0)              // display now (empty input!!)
   {
      print_weather ();
   }
   if (strncmp (command, "date", 4) == 0)
   {
      int16_t t;
      bool res = false;
      uint8_t month, day;

      command += 4;
      while (*command == ' ')
         command++;

      if (command[0] == '\0')
      {
         kfile_printf (&serial.fd, "%02d-%02d-%02d\r\n", day, month, gYEAR);
      }
      else
      {
         // parse date
         command = get_decimal (command, &t);
         res |= check_value (&gDAY, t, 1, 31);
         command = get_decimal (command, &t);
         res |= check_value (&gMONTH, t, 1, 12);
         command = get_decimal (command, &t);
         res |= check_value (&gYEAR, t, 13, 20);
         if (res)
            kfile_printf (&serial.fd, "Invalid date\r\n");
         else
            set_epoch_time ();
      }
   }

   else if (strncmp (command, "time", 4) == 0)
   {
      int16_t t;
      bool res = false;

      // skip command string
      command += 4;
      // skip whitespace
      while (*command == ' ')
         command++;
      if (command[0] == '\0')
      {
         kfile_printf (&serial.fd, "%02d:%02d:%02d\r\n", gHOUR, gMINUTE, gSECOND);
      }
      else
      {
         // parse time
         command = get_decimal (command, &t);
         res |= check_value (&gHOUR, t, 0, 23);
         command = get_decimal (command, &t);
         res |= check_value (&gMINUTE, t, 0, 59);
         command = get_decimal (command, &t);
         res |= check_value (&gSECOND, t, 0, 59);
         if (res)
            kfile_printf (&serial.fd, "Invalid time\r\n");
         else
            set_epoch_time ();
      }
   }
   else if (strncmp (command, "adjust", 6) == 0)
   {
      int16_t t;
      bool res = false, neg = false;

      // skip command string
      command += 6;
      // skip whitespace
      while (*command == ' ')
         command++;
      if (command[0] == '\0')
      {
         kfile_printf (&serial.fd, "%3d\r\n", gAdjustTime);
      }
      else
      {
         // parse time adjustment value
         if (*command == '-')
         {
            neg = true;
            command++;
         }
         command = get_decimal (command, &t);
         if (neg)
            t = -t;
         res |= check_value (&gAdjustTime, t, -719, 719);
         if (res)
            kfile_printf (&serial.fd, "Invalid time adjustment\r\n");
         else
            set_epoch_time ();
      }
   }

}

// get a string from the user with backspace 
static void
get_input (void)
{
   int16_t c;
   static uint8_t bcnt = 0;
#define CBSIZE 20
   static char cbuff[CBSIZE];   /* console I/O buffer       */

   while ((c = kfile_getc (&serial.fd)) != EOF)
   {
      c &= 0x7f;
      switch ((char) c)
      {
      case 0x03:               // ctl-c
         bcnt = 0;
      case '\r':
// process what is in the buffer
         cbuff[bcnt] = '\0';    // don't include terminator in count
         kfile_printf (&serial.fd, "\r\n");
         process_command (cbuff, bcnt);
         bcnt = 0;
         break;
      case 0x08:               // backspace
      case 0x7f:               // rubout
         if (bcnt > 0)
         {
            kfile_putc (0x08, &serial.fd);
            kfile_putc (' ', &serial.fd);
            kfile_putc (0x08, &serial.fd);
            bcnt--;
         }
         break;
      default:
         if ((c >= ' ') && (bcnt < CBSIZE))
         {
            cbuff[bcnt++] = c;
            kfile_putc (c, &serial.fd); // echo...
         }
         break;
      }

   }
}

// in the main loop, 
// see if any user input to process (does so on <return> being pressed
// pokes the clock
// see whether the interrupt routine has gathered a full packet yet
int
main (void)
{
   static ticks_t rain_refresh;
   init ();

   while (1)
   {
      get_input ();
      run_rtc ();
      if (timer_clock () - rain_refresh > ms_to_ticks (1000))
      {
         rain_refresh = timer_clock ();
         update_rain ();
      }
      timer_delay (2);          // wait for a short time
      if (PacketDone)
      {                         // have a bit string that's ended
         ParsePacket (FinishedPacket);
         PacketDone = 0;
      }
   }
}
