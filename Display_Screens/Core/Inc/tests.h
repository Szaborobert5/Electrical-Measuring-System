/*
 * tests.h
 *
 *  Created on: May 9, 2026
 *      Author: Ljubomir
 */
#ifndef INC_TESTS_H_
#define INC_TESTS_H_

#include "main.h"
#include "st7789.h"
#include "screens.h"
#include "atm90e26.h"


/* ── Test result counters ─────────────────────────────────────────── */


void Run_Tests(void);
void Test_SPI_Comms(void);
void Test_CS_Pin(void);
void Test_Voltage(void);
void Test_Current(void);
void Test_Power(void);
void Test_Energy_Accumulation(void);
void Test_MinMax_Tracking(void);
void Test_Report(const char *name, uint8_t pass, const char *detail);
void ATM_CheckStatus(void);


#endif /* INC_TESTS_H_ */
