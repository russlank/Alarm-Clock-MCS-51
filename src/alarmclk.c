#include <reg51.h>
#include <intrins.h>

#define TIMER0          Timer0() interrupt 1 using 1
#define TIMER1          Timer1() interrupt 3 using 2
#define BOOL            bit
#define BYTE            unsigned char
#define WORD            unsigned int
#define DWORD           unsigned long int
#define SINT            char
#define INT             int
#define LINT            long int
#define TRUE            1
#define FALSE           0
#define VOID            void

// CLOCK_TIMER_PERIOD: 
#define CLOCK_TIMER_PERIOD        (-250)

// DIGITS_SCAN_INTERVAL: The high 8 bits of the 16 bit timer reload value
#define DIGITS_SCAN_INTERVAL      0xfd

// KEYMASK: Determines to which bits keys are connected
#define KEYS_MASK                 0xf3


#define keyNOKEY                  0x00
#define keyKEY1                   0x01
#define keyKEY2                   0x02
#define keyTIMEOUT                0x40

#define evSECOND_HAS_CHANGED      (0x01)
#define evMINUT_HAS_CHANGED       (0x02 | evSECOND_HAS_CHANGED)
#define evHOUR_HAS_CHANGED        (0x04 | evMINUT_HAS_CHANGED)
#define evDAY_HAS_CHANGED         (0x08 | evHOUR_HAS_CHANGED)
#define evTIME_HAS_CHANGED        (0x00 | evDAY_HAS_CHANGED)
#define evTIME_HASNOT_CHANGED     0x00
    
#define KEYS_REPEAT_DELAY         30

typedef struct tagTIME {
	BYTE Hour;
	BYTE Minute;
	} TIME;

#define VIDEOOUT                  P1

sbit Digit1        = P3^5;
sbit Digit2        = P3^4;
sbit Digit3        = P3^3;
sbit Digit4        = P3^2;

sbit Keys          = P3^7;

sbit Spkr          = P3^0;
sbit Device        = P3^1;

code BYTE DECODE[14] = { 0x0A, 0xFA, 0x1C, 0x98, 0xE8, 0x89, 0x09, 0xBA, 0x08, 0xA8, 0xF9, 0xFF};

BYTE CurrentSecond      = 0;
WORD CurrentSecondParts = 0;
BYTE DelayCounter       = 0;
BYTE SpkrCounter        = 0;
BYTE DeviceCounter      = 0;

TIME CurrentTime        = {0,0};
WORD SecCounter         = 0;

BYTE Video[4];
BYTE VideoScanState     = 0;
BOOL Flasher            = FALSE;
BYTE TimeEvent          = evTIME_HAS_CHANGED;

BYTE KeyPressed         = keyNOKEY;
BYTE CurrentKeyDown     = keyNOKEY;
BYTE KEYS_REPEAT_DELAYConter    = KEYS_REPEAT_DELAY;

TIMER0
{
	if (SpkrCounter > 0) Spkr = 0;
	else Spkr = 1;
    
    if (DeviceCounter > 0) Device = 0;
    else Device = 1;
	
	CurrentSecondParts += 12;
	if ( CurrentSecondParts >= 16000){
	   Flasher = FALSE;
	   CurrentSecondParts -= 16000;
	   CurrentSecond++;
       SecCounter++;
	   if (SpkrCounter > 0) SpkrCounter--;
       if (DeviceCounter > 0) DeviceCounter--;
	   if (DelayCounter > 0) DelayCounter--;
	   if (CurrentSecond >= 60){
		  CurrentSecond = 0;
		  CurrentTime.Minute++;
		  if ( CurrentTime.Minute >= 60){
			 CurrentTime.Minute = 0;
			 CurrentTime.Hour++;
			 if (CurrentTime.Hour >= 24) {
				CurrentTime.Hour = 0;
				TimeEvent = evDAY_HAS_CHANGED;
				}
			 else TimeEvent = evHOUR_HAS_CHANGED;
			 }
		  else TimeEvent = evMINUT_HAS_CHANGED;
		  }
	   else TimeEvent = evSECOND_HAS_CHANGED;
	   }
	else {
		if (CurrentSecondParts > 4000) Flasher = TRUE;
		};
}

TIMER1
{
	TL1 = 0x00;
	TH1 = DIGITS_SCAN_INTERVAL;


	switch ( VideoScanState){
	case 0:
		 Keys = 1;
		 VIDEOOUT = Video[0];
		 Digit1 = 0;
		 VideoScanState = 1;
		 break;
	case 1:
		 Digit1 = 1;
		 VIDEOOUT = Video[1];
		 Digit2 = 0;
		 VideoScanState = 2;
		 break;
	case 2:
		 Digit2 = 1;
		 //VIDEOOUT = Video[2] ^ DotMask;
		 VIDEOOUT = Video[2];
		 Digit3 = 0;
		 VideoScanState = 3;
		 break;
	case 3:
		 Digit3 = 1;
		 VIDEOOUT = Video[3];
		 Digit4 = 0;
		 VideoScanState = 4;
		 break;
	default:
            Digit4 = 1;
			VIDEOOUT = 0xff;
            Keys = 0;
            VideoScanState = 0;
			
			{
				register BYTE KeysStatus = 0x00;
				do {
				   BYTE Sample1, Sample2, Sample3;
				   _nop_();
				   Sample1 = VIDEOOUT | KEYS_MASK;
				   _nop_();
				   Sample2 = VIDEOOUT | KEYS_MASK;
				   _nop_();
				   Sample3 = VIDEOOUT | KEYS_MASK;
				   if ((Sample1 == Sample2) || (Sample1 == Sample3)) KeysStatus = Sample1;
				   else if (Sample2 == Sample3) KeysStatus = Sample2;
				   } while (KeysStatus == 0x00);

				switch ( KeysStatus){
					case ~0x04:
						 CurrentKeyDown = keyKEY1;
						 break;
					case ~0x08:
						 CurrentKeyDown = keyKEY2;
						 break;
					default:
                         CurrentKeyDown = keyNOKEY;
					}
				}
                
			if (CurrentKeyDown == keyNOKEY) KEYS_REPEAT_DELAYConter = 0;
			else if ( KEYS_REPEAT_DELAYConter > 0) KEYS_REPEAT_DELAYConter--;
				 else if ( KeyPressed == keyNOKEY) {
						 KEYS_REPEAT_DELAYConter = KEYS_REPEAT_DELAY;
						 KeyPressed = CurrentKeyDown;
						 }
	}
}

BYTE ReadKey( VOID)
{
	register Key;
	while (KeyPressed == keyNOKEY);
	Key = KeyPressed;
	KeyPressed = keyNOKEY;
	return Key;
}

BYTE WaitKey( BYTE ATime)
{
	if (ATime == 0) return ReadKey();
	else {
		DelayCounter = ATime;
		while ((KeyPressed == keyNOKEY) && (DelayCounter > 0));
		if (KeyPressed == keyNOKEY) return keyTIMEOUT;
		else return ReadKey();
		}
}

/*
VOID DisplayTime( TIME *ATime)
{
	BYTE Hour = ATime->Hour & 0x7f;
	Video[0] = DECODE[ ATime->Minute % 10];
	Video[1] = DECODE[ ATime->Minute / 10];
	Video[2] = DECODE[ Hour % 10];
	if (Hour < 10) Video[3] = DECODE[10];
	else Video[3] = DECODE[ Hour / 10];
	Video[4] = 0xff; // Day dots off
}
*/

VOID DisplayValue( WORD *AValue)
{
    BYTE i;
    WORD Remained = *AValue;
    for (i=0; i < 4; i++){
        Video[i] = DECODE[ Remained % 10];
        Remained = Remained / 10;
    }
}

main()
{
	IP = 0x02;               /* set high intrrupt priorery to timer0 */
	TL0 = CLOCK_TIMER_PERIOD;  /* set timer0 period */
	TH0 = CLOCK_TIMER_PERIOD;  /* set timer0 reload period */
	TL1 = 0x00;              /* set scan timer1 interval LO*/
	TH1 = DIGITS_SCAN_INTERVAL;      /* set scan timer1 interval HI*/
	TMOD = 0x12 ;            /* select mode 2 for timer0 & mode 1 for timer1*/
	TR0 = 1;                 /* start timer0 */
	TR1 = 1;                 /* start timer1 */
	ET0 = 1;                 /* enable timer0 interrupt */
	ET1 = 1;                 /* enable timer1 interrupt */
	EA = 1;                  /* global interrupt enable */

	while(1){
		KeyPressed = keyNOKEY;
		
		do {
			if (TimeEvent != 0){
                DisplayValue( &SecCounter);
                TimeEvent = evTIME_HASNOT_CHANGED;
            }
		   } while (KeyPressed == keyNOKEY);
           
        switch (ReadKey()){
            case keyKEY1:
                if (SpkrCounter > 0) SpkrCounter = 0;
                else SpkrCounter = 5;
                break;
            case keyKEY2:
                if (DeviceCounter > 0) DeviceCounter = 0;
                else DeviceCounter = 240;
                break;
        }
	}
}