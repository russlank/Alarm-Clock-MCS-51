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


#define blnkDIGIT1                0x01
#define blnkDIGIT2                0x02
#define blnkDIGIT3                0x04
#define blnkDIGIT4                0x08
#define blnkDOT1                  0x10
#define blnkDOT2                  0x20
#define blnkDOT3                  0x40
#define blnkDOT4                  0x80
#define blnkMINUTES               (blnkDIGIT1 | blnkDIGIT2)
#define blnkHOURS                 (blnkDIGIT3 | blnkDIGIT4)
#define blnkALL_DIGITS            (blnkDIGIT1 | blnkDIGIT2 | blnkDIGIT3 | blnkDIGIT4)

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

code BYTE DECODE[14] = { 0x0A, 0xFA, 0x1C, 0x98, 0xE8, 0x89, 0x09, 0xBA, 0x08, 0xA8, 0xFF};

BYTE CurrentSecond      = 0;
WORD CurrentSecondParts = 0;
BYTE DelayCounter       = 0;
BYTE SpkrCounter        = 0;
BYTE DeviceCounter      = 0;

TIME CurrentTime        = {0,0};

BYTE VideoDigits[4];
BYTE VideoDots;
BYTE VideoBlinkMask;

BYTE VideoScanState     = 0;
BYTE VideoScanMask      = 0x11;
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
	BYTE VideoOut;
	
	TL1 = 0x00;
	TH1 = DIGITS_SCAN_INTERVAL;

    if (VideoScanMask){
        BOOL DigitIsOn;
        BOOL DotIsOn = ((VideoDots & VideoScanMask) != 0x00);
        
        if (Flasher) DigitIsOn = TRUE;
        else {
            register TestMask = (VideoScanMask & VideoBlinkMask);
            DigitIsOn = ((TestMask & 0x0f) == 0x00);
            DotIsOn = (DotIsOn && ((TestMask & 0xf0) == 0x00));
        }
        
        if (DigitIsOn) VideoOut = VideoDigits[VideoScanState];
        else VideoOut = 0xff;
        
        if (DotIsOn) VideoOut = (VideoOut & 0xf7);
    }

	switch ( VideoScanState){
	case 0:
		 Keys = 1;
		 VIDEOOUT = VideoOut;
		 Digit1 = 0;
		 VideoScanState = 1;
         VideoScanMask = 0x22;
		 break;
	case 1:
		 Digit1 = 1;
		 VIDEOOUT = VideoOut;
		 Digit2 = 0;
		 VideoScanState = 2;
         VideoScanMask = 0x44;
		 break;
	case 2:
		 Digit2 = 1;
		 VIDEOOUT = VideoOut;
		 Digit3 = 0;
		 VideoScanState = 3;
         VideoScanMask = 0x88;
		 break;
	case 3:
		 Digit3 = 1;
		 VIDEOOUT = VideoOut;
		 Digit4 = 0;
		 VideoScanState = 4;
         VideoScanMask = 0x00;
		 break;
	default:
            Digit4 = 1;
			VIDEOOUT = 0xff;
            Keys = 0;
            VideoScanState = 0;
            VideoScanMask = 0x11;
			
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

VOID DisplayTime( TIME *ATime)
{
	BYTE Hour = ATime->Hour & 0x7f;    // Unmask the ALARM ON/OFF bit
	
	VideoDigits[0] = DECODE[ ATime->Minute % 10];
	VideoDigits[1] = DECODE[ ATime->Minute / 10];
	VideoDigits[2] = DECODE[ Hour % 10];
	if (Hour < 10) VideoDigits[3] = DECODE[10];
	else VideoDigits[3] = DECODE[ Hour / 10];
	VideoDots = VideoDots | 0x04;
}

BOOL EditTime( TIME *Time)
{
	TIME EditTime;
	BYTE EditState = 0;

	EditTime = *Time;
	EditTime.Hour &= 0x7f;    // Unmask the ALARM ON/OFF bit

	while (TRUE){
		DisplayTime( &EditTime);
		
		{
            register BlinkMask = (VideoBlinkMask & 0xb0);
            
    		switch (EditState){
                case 0:
                    BlinkMask |= 0x0c;
                    break;
                case 1:
                    BlinkMask |= 0x03;
                    break;
                case 2:
                    BlinkMask |= 0x0f;
                    break;
            }
            
            VideoBlinkMask = BlinkMask;
        }
            
		switch ( WaitKey(120)){
			case keyKEY1:
				 EditState = ((EditState + 1) & 0x03);
				 break;
			case keyKEY2:
                {
                    switch (EditState){
                    case 0:
                        if (EditTime.Hour < 23) EditTime.Hour++;
    					else EditTime.Hour = 0;
                        break;
                    case 1:
                        if (EditTime.Minute < 59) EditTime.Minute++;
    					else EditTime.Minute = 0;
                        break;
                    case 2:
                        return FALSE;
                        break;
                    default:
                        *Time = EditTime;
                        return TRUE;
                    }
                }
                break;
			case keyTIMEOUT:
				return FALSE;
			}
		}
}

main()
{
	IP = 0x02;                         /* set high intrrupt priorery to timer0 */
	TL0 = CLOCK_TIMER_PERIOD;          /* set timer0 period */
	TH0 = CLOCK_TIMER_PERIOD;          /* set timer0 reload period */
	TL1 = 0x00;                        /* set scan timer1 interval LO*/
	TH1 = DIGITS_SCAN_INTERVAL;        /* set scan timer1 interval HI*/
	TMOD = 0x12 ;                      /* select mode 2 for timer0 & mode 1 for timer1*/
	TR0 = 1;                           /* start timer0 */
	TR1 = 1;                           /* start timer1 */
	ET0 = 1;                           /* enable timer0 interrupt */
	ET1 = 1;                           /* enable timer1 interrupt */
	EA = 1;                            /* global interrupt enable */

	while(1){
		KeyPressed = keyNOKEY;
		
		do {
			if (TimeEvent & evMINUT_HAS_CHANGED){
                VideoDots = 0x00;
                VideoBlinkMask = 0x40;
                DisplayTime( &CurrentTime);
                TimeEvent = evTIME_HASNOT_CHANGED;
                
                if (DeviceCounter > 0) {
                    VideoDots |= 0x01;
                    if (DeviceCounter < 10) {
                        if (DeviceCounter == 5) SpkrCounter = 2;
                        VideoBlinkMask |= 0x10;
                    }
                } else VideoDots &= ~0x01;
                
                if (SpkrCounter > 0) VideoBlinkMask |= 0x0f;
                else VideoBlinkMask &= ~0x0f;
            }
		   } while (KeyPressed == keyNOKEY);
           
        switch (ReadKey()){
            case keyKEY1:
                {
                    TIME Time;
                    Time = CurrentTime;
                    if ( EditTime( &Time)) {
                        CurrentSecondParts = 0;
                        CurrentSecond = 0;
                        CurrentTime = Time;
                        }
                    TimeEvent  = evTIME_HAS_CHANGED;
                }
                break;
            case keyKEY2:
                if (DeviceCounter > 0) DeviceCounter = 0;
                else DeviceCounter = 60;
                break;
        }
	}
}