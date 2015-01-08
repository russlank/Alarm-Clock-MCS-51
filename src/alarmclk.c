#pragma SMALL DEBUG CODE LISTINCLUDE VERBOSE

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

#define KEYS_REPEAT_DELAY         30

#define MAX_DISPLAY_WIDTH         12
#define MAX_STEPS                 10

#define VIDEO_SHIFT_DELAY         70

#define ALARM_TIME                1800
#define SNOOZE_TIME               255

#define keyNOKEY                  0x00
#define keyKEY1                   0x01
#define keyKEY2                   0x02
#define keyTIMEOUT                0x40

#define evSECOND_HAS_CHANGED      (0x01)
#define evMINUTE_HAS_CHANGED      (0x02 | evSECOND_HAS_CHANGED)
#define evHOUR_HAS_CHANGED        (0x04 | evMINUTE_HAS_CHANGED)
#define evTIME_HAS_CHANGED        (0x00 | evHOUR_HAS_CHANGED)
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

typedef struct tagTIME {
	BYTE Hour;
	BYTE Minute;
	} TIME;

#define VIDEOOUT                  P1

sbit Digit1        = P3^2;
sbit Digit2        = P3^3;
sbit Digit3        = P3^4;
sbit Digit4        = P3^5;

sbit Keys          = P3^7;

sbit Spkr          = P3^0;
sbit Device        = P3^1;

code BYTE DECODE[10] = { 0x0A, 0xFA, 0x1C, 0x98, 0xE8, 0x89, 0x09, 0xBA, 0x08, 0xA8};
code BYTE MSG_ADD[] = {0x28, 0x58, 0x58};
code BYTE MSG_SET[] = {0x89, 0x0d, 0x4d};
code BYTE MSG_SXX_XXX_XXXX[] = {0x89};

BYTE CurrentSecond      = 0;
WORD CurrentSecondParts = 0;
BYTE DelayCounter       = 0;
WORD SpkrCounter        = 0;
BYTE SnoozeCounter      = 0;

TIME CurrentTime        = {0,0};

TIME StepsTimes[MAX_STEPS];
BYTE StepsCount = 0;

BYTE VideoDigits[MAX_DISPLAY_WIDTH];
BYTE VideoBlinkMask;
BYTE VideoMaxOffset = 0;
BYTE VideoOffset = 0;
BYTE VideoShiftTimer = VIDEO_SHIFT_DELAY;
BYTE VideoShiftDir = 0;

BOOL VideoMode                  = 0;
BYTE VideoScanState             = 0;
BYTE VideoScanMask              = 0x11;
BOOL Flasher                    = FALSE;
BOOL TimeChanged                = 1;

BYTE KeyPressed                 = keyNOKEY;
BYTE CurrentKeyDown             = keyNOKEY;
BYTE KeysRepeatDelayCounter    = KEYS_REPEAT_DELAY;

TIMER0
{
	CurrentSecondParts += 12;
	if ( CurrentSecondParts >= 16000){
	   Flasher = FALSE;
	   CurrentSecondParts -= 16000;
	   CurrentSecond++;
	   if (SnoozeCounter != 0){
           SnoozeCounter--;
           SpkrCounter = ALARM_TIME;
       } else {
           if (SpkrCounter != 0){
               SpkrCounter--;
           }
       }
	   if (DelayCounter > 0) DelayCounter--;
	   if (CurrentSecond >= 60){
           TimeChanged = 1;
           CurrentSecond = 0;
           CurrentTime.Minute++;
           if ( CurrentTime.Minute >= 60){
               CurrentTime.Minute = 0;
               CurrentTime.Hour++;
               if (CurrentTime.Hour >= 24) CurrentTime.Hour = 0;
           }
           {
                register BYTE i;
                for(i = 0; i < StepsCount; i++) if (( StepsTimes[i].Hour == CurrentTime.Hour) && ( StepsTimes[i].Minute == CurrentTime.Minute)) SpkrCounter = ALARM_TIME;
           }
       }
	} else if (CurrentSecondParts > 4000) Flasher = TRUE;

	if ((SnoozeCounter == 0) && (SpkrCounter > 0)) Spkr = Flasher;
	else Spkr = 1;
}

TIMER1
{
	BYTE VideoOut;
	
	TL1 = 0x00;
	TH1 = DIGITS_SCAN_INTERVAL;

    if (VideoScanMask){
        if (!VideoMode){
            VideoOut = VideoDigits[VideoScanState];
            if (!Flasher) {
                register TestMask = (VideoScanMask & VideoBlinkMask);
                if (TestMask & 0x0f) VideoOut |= 0xf7;
                if (TestMask & 0xf0) VideoOut |= ~0xf7;
            }
        } else {
            VideoOut = VideoDigits[ VideoOffset + VideoScanState];
            if (VideoShiftTimer > 0) VideoShiftTimer --;
            else {
                VideoShiftTimer = VIDEO_SHIFT_DELAY;
                if ( VideoShiftDir){
                    if (VideoOffset > 0) VideoOffset--;
                    else {
                        VideoShiftDir = 0;
                        VideoShiftTimer += VideoShiftTimer;
                    }
                } else {
                    if (VideoOffset < VideoMaxOffset) VideoOffset++;
                    else {
                        VideoShiftDir = 1;
                        VideoShiftTimer += VideoShiftTimer;
                    }
                }
            }
        }
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
                /*
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
                */
                
                KeysStatus = VIDEOOUT | KEYS_MASK;

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
                
			if (CurrentKeyDown == keyNOKEY) KeysRepeatDelayCounter = 0;
			else if ( KeysRepeatDelayCounter > 0) KeysRepeatDelayCounter--;
				 else if ( KeyPressed == keyNOKEY) {
                         if (SnoozeCounter != 0) {
                             SnoozeCounter = 0;
                             SpkrCounter = 0;
                         } else if (SpkrCounter != 0){
                             SnoozeCounter = SNOOZE_TIME;
                         } else KeyPressed = CurrentKeyDown;
    					 KeysRepeatDelayCounter = KEYS_REPEAT_DELAY;
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

VOID SetShiftVideoMode( BYTE AWidth, BYTE AMessageLength, BYTE *AMessage)
{
    /*
    {
        register i;
        for ( i = 0; i < AMessageLength; i++) VideoDigits[i] = AMessage[i];
        for ( ; i < AWidth; i++) VideoDigits[i] = 0xff;
    }
    */
    {
        register i;
        for ( i = 0; i < AWidth; i++) VideoDigits[i] = (i < AMessageLength)?(AMessage[i]):(0xff);
    }
    
    if ( AWidth > 4) VideoMaxOffset = (AWidth - 4);
    else VideoMaxOffset = 0;
    
    VideoOffset = 0;
    VideoMode = 1;
    VideoShiftDir = 1;
}

VOID ByteToString( BYTE AValue, BYTE *ABuffer)
{
    ABuffer[0] = DECODE[ AValue / 10];
    ABuffer[1] = DECODE[ AValue % 10];
}

VOID TimeToString( TIME *ATime, BYTE *ABuffer)
{
    register BYTE Hour = ATime->Hour & 0x1f;    // Unmask the ALARM ON/OFF bit
	if ( Hour < 10) ABuffer[0] = 0xff;
	else ABuffer[0] = DECODE[ Hour / 10];
    ABuffer[1] = DECODE[ Hour % 10] & 0xF7;
    ABuffer[2] = DECODE[ ATime->Minute / 10];
    ABuffer[3] = DECODE[ ATime->Minute % 10];
}

VOID DisplayTime( TIME *ATime)
{
	VideoMode = 0;
    TimeToString( ATime, &VideoDigits[0]);
}

BOOL EditTime( TIME *Time)
{
	TIME EditTime;
	BYTE EditState = 0;

	EditTime = *Time;
	EditTime.Hour &= 0x1f;    // Unmask the ALARM ON/OFF bit

	while (TRUE){
		DisplayTime( &EditTime);
		
		{
            code BYTE SUB_MASK[] = { 0x0f, 0x03, 0x0c, 0x00};
            VideoBlinkMask = ((VideoBlinkMask & 0xd0) | SUB_MASK[EditState]);
        }
            
		switch ( WaitKey(120)){
			case keyKEY1:
				 EditState = ((EditState + 1) & 0x03);
				 break;
			case keyKEY2:
                {
                    switch (EditState){
                    case 0:
                        return FALSE;
                    case 1:
                        if (EditTime.Hour < 23) EditTime.Hour++;
    					else EditTime.Hour = 0;
                        break;
                    case 2:
                        if (EditTime.Minute < 59) EditTime.Minute++;
    					else EditTime.Minute = 0;
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

VOID Menu1( VOID)
{
    BYTE Key;

AddStep:
    if ( StepsCount < MAX_STEPS){
        SetShiftVideoMode( 4, sizeof(MSG_ADD), MSG_ADD);
        Key = WaitKey( 30);
        if ( Key == keyKEY1) {
            StepsTimes[StepsCount].Hour = 0;
            StepsTimes[StepsCount].Minute = 0;
            if (EditTime(StepsTimes[StepsCount])) {
                StepsCount++;
                // SortSteps()
                goto AddStep;
            }
        } else if (Key == keyTIMEOUT) goto End;
    }

SetTime:
    SetShiftVideoMode( 4, sizeof(MSG_SET), MSG_SET);
    Key = WaitKey( 30);
    if ( Key == keyKEY1) {
        TIME Time;
        Time = CurrentTime;
        if ( EditTime( &Time)) {
            CurrentSecondParts = 0;
            CurrentSecond = 0;
            CurrentTime = Time;
            }
        goto End;
    } else if (Key == keyTIMEOUT) goto End;
    
    {
        BYTE i;
        for ( i = 0; i < StepsCount; i++){
RepStep:
            SetShiftVideoMode( 12, sizeof(MSG_SXX_XXX_XXXX), MSG_SXX_XXX_XXXX);
            ByteToString( i, &VideoDigits[1]);
            TimeToString( StepsTimes[i], &VideoDigits[8]);
            
            if ((StepsTimes[i].Hour & 0x80) != 0){
                VideoDigits[4] = 0x59;
                if ((StepsTimes[i].Hour & 0x40) != 0){
                    VideoDigits[5] = 0x79;
                } else {
                    VideoDigits[5] = 0x2d;
                    VideoDigits[6] = 0x2d;
                }
            } else {
                VideoDigits[4] = 0x28;
                VideoDigits[5] = 0x4f;
                VideoDigits[6] = 0x28;
            }
            
            Key = WaitKey( 30);
            if ( Key == keyKEY1) {
                StepsCount--;
                if ( StepsCount > 0){
                    register BYTE j;
                    for( j = i; j < StepsCount; j++) StepsTimes[j] = StepsTimes[j + 1];
                    if ( i >= StepsCount) i = ( StepsCount - 1);
                    goto RepStep;
                }
                goto AddStep;
            } else if (Key == keyTIMEOUT) goto End;
        }
    }

End:;
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
			if (TimeChanged){
                VideoBlinkMask = 0x20;
                DisplayTime( &CurrentTime);
                TimeChanged = 0;
            }
		   } while (KeyPressed == keyNOKEY);
           
        switch (ReadKey()){
            case keyKEY2:
                Menu1();
                TimeChanged  = 1;
                break;
        }
	}
}