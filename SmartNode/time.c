#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "boards.h"
#include "nrf_gpio.h"
#include "time.h"
#include "ble.h"
#include "serial.h"
#include "schedule.h"

static unsigned long overflowCnt = 0;
static unsigned long count = 0;
long beaconCount = 0;
int serverReconnectPeriod = 1;
int time = 0;

//Configuration RTC with 
void rtc_init()
{
	NRF_RTC0->CC[0] = LF_CLOCK_PERIOD*LOOP_PERIOD_IN_MS/1000;
	NRF_RTC0->INTENSET = (RTC_INTENSET_COMPARE0_Enabled << RTC_INTENSET_COMPARE0_Pos) | (RTC_INTENSET_OVRFLW_Enabled << RTC_INTENSET_OVRFLW_Pos);
    NRF_RTC0->EVENTS_COMPARE[0] = 0;
    NRF_RTC0->EVENTS_OVRFLW = 0;
	NVIC_EnableIRQ(RTC0_IRQn);
	NRF_RTC0->TASKS_START = 1;
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
    //100 ms periodic clock
	if((NRF_RTC0->EVENTS_COMPARE[0]) && (NRF_RTC0->INTENSET & RTC_INTENSET_COMPARE0_Msk)) 
    {
		NRF_RTC0->EVENTS_COMPARE[0] = 0;
        if(true == isBeaconEnabled() &&((count != 0) && (count % getBLEBeaconTransmitPeriod())) == 0) {
            beaconCount++;
            sendBLEBeacon(0);
        }
        //Handle minute boundaries
        if((count != 0) && (0 == count % (10 * 60))) {
            time++;
            if(time > 7 * 24 * 60) {
                time = 0;
            }
            checkSchedule();
            if((time != 0) && (0 == (time % serverReconnectPeriod))) {
                setReportStatus();
            }
        }
        NRF_RTC0->CC[0] += LF_CLOCK_PERIOD*LOOP_PERIOD_IN_MS/1000;
        count++;
	}
    if((NRF_RTC0->EVENTS_OVRFLW) && (NRF_RTC0->INTENSET & RTC_INTENSET_OVRFLW_Msk))
    {
        overflowCnt++;
        NRF_RTC0->EVENTS_OVRFLW = 0;
    }
}

void getTime(struct tm * time) {
    const unsigned long long currentTicks = NRF_RTC0->COUNTER;
    const unsigned long long tickTimeInUs = (currentTicks * 30517)/1000;
    time->usec = tickTimeInUs % 1000000;
    time->sec = (tickTimeInUs/1000000) + overflowCnt * 512;
}

long getBeaconCount() {
    return beaconCount;
}

void resetBeaconCount() {
    beaconCount = 0;
}

long getUptime() {
    return count * LOOP_PERIOD_IN_MS/1000;
}

void setSystemTime(const long newTime) {
    time = newTime;
}

const long getSystemTime() {
    return time;
}

struct tm startTime;
struct tm endTime;
void startTimer() {
    getTime(&startTime);
}

float endTimerAndGetResult() {
    getTime(&endTime);
    float startTimeNum = (float)startTime.sec + ((float)startTime.usec/1000000);
    float endTimeNum = (float)endTime.sec + ((float)endTime.usec/1000000);
    return endTimeNum - startTimeNum;
}

void setServerReconnectPeriod(int minutesBetweenConnects)
{
    serverReconnectPeriod = minutesBetweenConnects;
}

void TIMER0_IRQHandler(void) 
{
	if((NRF_TIMER0->EVENTS_COMPARE[0]) &&
		  (NRF_TIMER0->INTENSET & TIMER_INTENSET_COMPARE0_Msk)) 
    {
        NRF_TIMER0->EVENTS_COMPARE[0] = 0;
        NRF_RADIO->EVENTS_READY = 0U;
        NRF_RADIO->TASKS_TXEN   = 1;
	}
}
