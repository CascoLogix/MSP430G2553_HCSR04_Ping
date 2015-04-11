/******************************************************************************/
//	ping.c
//  
//	 Created on: Apr 7, 2015
//	     Author: Clint Stevenson
//
/******************************************************************************/


/******************************************************************************/
//	Includes
/******************************************************************************/
#include <ping.h>
/******************************************************************************/
//	End Includes


/******************************************************************************/
//	Defines
/******************************************************************************/
//Put defines here
// Configuration for Trigger Pins - these drive the Ultrasonic device
#define PING_PORT_OUT 				(P2OUT)
#define PING_PORT_DIR 				(P2DIR)
#define PING_ECHO_SEL				(P2SEL)
#define PING_TRIG_PIN 				(BIT1)
#define PING_ECHO_PIN 				(BIT0)
#define PING_PORT_REN				(P2REN)

#define PING_PULSE_WIDTH			(10)		// In microseconds
#define PING_DELAY_COUNT			(_FCPU * PING_PULSE_WIDTH / 1000000)		// _FCPU in MHz

#define TIMERA1_PRESCALER			(4)

#define WAIT_FOR_ECHO()				{while(!echoReceived){}}

#define SET_CAPTURE_ON_NONE			(CM_0 + CCIS_0 + CAP)
#define SET_CAPTURE_ON_RISE			(CM_1 + CCIS_0 + CAP)
#define SET_CAPTURE_ON_FALL			(CM_2 + CCIS_0 + CAP)
#define SET_CAPTURE_ON_BOTH			(CM_3 + CCIS_0 + CAP)
/******************************************************************************/
//	End Defines


/******************************************************************************/
//	Constant Declarations
/******************************************************************************/
//Put Constant Declarations here
/******************************************************************************/
//	End Constant Declarations


/******************************************************************************/
//	Variable Declarations
/******************************************************************************/
//Put Variable Declarations here
volatile uint8_t echoReceived;

static struct {
	uint32_t tEchoStart;
	uint32_t tEchoStop;
	uint32_t numOVF;
	uint32_t tEchoResult;
} ping;
/******************************************************************************/
//	End Variable Declarations


/******************************************************************************/
//	Function Prototypes
/******************************************************************************/
//Put function prototypes here
static void setupTimerA1 (void);
static void ping_waitForEcho (uint32_t timeout);
/******************************************************************************/
//	End Function Prototypes


/******************************************************************************/
//	Function Definitions
/******************************************************************************/
//Put function definitions here
/**
 * \brief Sets up the IO for the Ultrasonic module.
 *
 * This function sets up the IO for driving the ultrasonic distance measurement
 * module.  This function should be customized per application needs.
 *
 * @param none
 */
void ping_init (void) // parameter should be a pointer to the timer interface
{
	// Initialize pins
	PING_PORT_REN |= PING_ECHO_PIN;		// Enable pullup resistor on echo input pin
	PING_PORT_OUT &= ~PING_TRIG_PIN;	// Last Set trigger pin low to initialze ping module
	PING_PORT_DIR |= PING_TRIG_PIN;		// Set trigger pin as output
	PING_ECHO_SEL = PING_ECHO_PIN;		// Set echo pin module select to timer capture

	// Initialize ping struct
	ping.tEchoStart = 0;
	ping.tEchoStop = 0;
	ping.numOVF = 0;
	ping.tEchoResult = 0;

	// Initialize timer interface
	setupTimerA1();						// Setup Timer module
}


/**
 * \brief Sends a "ping" from the ultrasonic measurement device
 *
 * This function sends a ping from the ultrasonic measurement device by a pulse
 * to the device's trigger pin.  The trigger pulse width is defined via
 * preprocessor macros utilizing the CPU clock frequency constant and prescalers
 *
 * @param none
 */
uint32_t ping_triggerPing (void)
{
    PING_PORT_OUT |= PING_TRIG_PIN;		// Set trigger pin high
    _delay_cycles(PING_DELAY_COUNT);	// 8MHz * 10uS = 80 CPU Cycles
	PING_PORT_OUT &= ~PING_TRIG_PIN;	// Set trigger pin low
	TA1CCTL0 = SET_CAPTURE_ON_RISE;		// Clears any set interrupt flags also
    TA1CCTL0 |= CCIE;					// Enable interrupt

    ping_waitForEcho(0);
    //WAIT_FOR_ECHO();

	return ping.tEchoResult;
}


uint32_t ping_markEchoStart (void)
{
	ping.tEchoStart = TA1CCR0;  				// Take first time measure
	ping.numOVF = 0;
	TA1CCTL0 = SET_CAPTURE_ON_FALL;				// Set edge select to falling edge
    TA1CCTL0 |= CCIE;							// Enable interrupt
    TA1CTL &= ~TAIFG;							// Clear any pending overflow interrupts
    TA1CTL |= TAIE;								// Enable overflow interrupts

    return ping.tEchoStart;
}


uint32_t ping_markEchoStop (void)
{
	ping.tEchoStop = TA1CCR0; 					// Take second time measure
	ping.tEchoResult = (65536 * ping.numOVF) + ping.tEchoStop - ping.tEchoStart;
	echoReceived = 1;							// Mark echo received
	TA1CCTL0 &= ~CCIE;							// Disable interrupt

	return ping.tEchoStop;
}


uint32_t ping_getResult (void)
{
	return ping.tEchoResult;
}


void ping_Overflow (void)
{
	ping.numOVF++;
}


/**
 * \brief Sets up the Timer A1 module.
 *
 * This function sets up the Timer A1 module for operation.  This function
 * should be customized per application needs.
 *
 * @param none
 */
static void setupTimerA1 (void)
{
	//select smclock for timer a source + make ta1ccr0 count continously up + no division
#if (TIMERA1_PRESCALER == 1)
	TA1CTL |= TASSEL_2 + MC_2 + ID_0;
#elif (TIMERA1_PRESCALER == 2)
	TA1CTL |= TASSEL_2 + MC_2 + ID_1;
#elif (TIMERA1_PRESCALER == 4)
	TA1CTL |= TASSEL_2 + MC_2 + ID_2;
#elif (TIMERA1_PRESCALER == 8)
	TA1CTL |= TASSEL_2 + MC_2 + ID_3;
#endif
}


/**
 * \brief Waits for an "echo" from the ultrasonic measurement device
 *
 * This function waits for an echo from the ultrasonic measurement device.  The
 * timeout parameter, when set to zero, will wait indefinitely for an echo.
 * When set to anything greater than zero, will decrement the timeout value in
 * a loop until the value reaches zero, or an echo is received; whichever
 * happens first.
 *
 * @param timeout
 */
void ping_waitForEcho (uint32_t timeout)
{
	if(timeout == 0)					// If no timeout was set
	{
		while(!echoReceived);			// Wait indefinitely for echo
	}

	else
	{
		while((!echoReceived) && (timeout > 0))
		{
			timeout--;					// Decrement timeout
		}

		if(!echoReceived)				// If timeout elapsed and no echo received
		{
			ping.tEchoResult = 0;		// Return invalid result
		}
	}
}
/******************************************************************************/
//	End Function Definitions
