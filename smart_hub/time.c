#include <stdint.h>
#include "boards.h"
#include "nrf_gpio.h"
#include "time.h"
#include "hub.h"
#include "radio.h"

#define LF_CLOCK_PERIOD         		 32768
#define HF_CLOCK_PERIOD                  16000000
#define LOOP_PERIOD_IN_MS                100
#define TICKS_TO_RX LF_CLOCK_PERIOD/128

static unsigned long count = 0;

extern uint8_t packetQueueHead;
extern uint8_t packetQueueTail;
extern uint8_t * packetQueue;
extern uint8_t * pilot;

//Configuration RTC with 
void rtc_init()
{
	NRF_RTC0->CC[0] = LF_CLOCK_PERIOD*LOOP_PERIOD_IN_MS/1000;
	NRF_RTC0->INTENSET = (RTC_INTENSET_COMPARE0_Enabled << RTC_INTENSET_COMPARE0_Pos);
	NRF_RTC0->TASKS_START = 1;
	NVIC_EnableIRQ(RTC0_IRQn);
}

//Configuration TIMER0 with a 31250 us period
void timer0_init()
{
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER0->PRESCALER = 0; //16000000/2^0 = 16000000 Ticks/Sec
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_16Bit; //Roll over at 2^16 = 65536. Max bitmode is 16 bit.
    NRF_TIMER0->SHORTS = (TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos) | 
                         (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
    NRF_TIMER0->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
    NVIC_EnableIRQ(TIMER0_IRQn);
}

void RTC0_IRQHandler(void)
{
	if((NRF_RTC0->EVENTS_COMPARE[0]) && 
		 (NRF_RTC0->INTENSET & RTC_INTENSET_COMPARE0_Msk)) {
	    NRF_RTC0->EVENTS_COMPARE[0] = 0;
            if(((count != 0) && (count % HUB_BEACON_PERIOD)) == 0) {
                if(packetQueueHead != packetQueueTail) {
                    NRF_RADIO->PACKETPTR = (uint32_t)packetQueue[packetQueueHead];
                    packetQueueHead++;
                    packetQueueHead |= 16 - 1;
                    sendPacket(RADIO_TXPOWER_TXPOWER_Pos4dBm, 24);
                }
                else {
                    setPilot();
                    NRF_RADIO->PACKETPTR = (uint32_t)pilot;
                    sendPacket(RADIO_TXPOWER_TXPOWER_Pos4dBm, 24);
                }
                NRF_TIMER0->CC[0] = TICKS_TO_RX;
                NRF_TIMER0->TASKS_START = 1;
            }
            NRF_RTC0->CC[0] += LF_CLOCK_PERIOD*LOOP_PERIOD_IN_MS/1000;
            count++;
	}
}

void TIMER0_IRQHandler(void) 
{
	if((NRF_TIMER0->EVENTS_COMPARE[0]) &&
		  (NRF_TIMER0->INTENSET & TIMER_INTENSET_COMPARE0_Msk)) 
    {
        NRF_TIMER0->EVENTS_COMPARE[0] = 0;
        NRF_RADIO->TASKS_DISABLE = 1;//Turn off the radio
	}
}
