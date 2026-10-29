/*
 * screens.h
 *
 *  Created on: Apr 29, 2026
 *      Author: Ljubomir
 */

#ifndef INC_SCREENS_H_
#define INC_SCREENS_H_

#include "st7789.h"
#include <stdint.h>

/*---Energy data structure---*/
typedef struct {
	float voltage;			//Vrms 			[V]
	float current;			//Irms 			[A]
	float active_power;		//P				[W]
	float frequency;		//f				[Hz]
	float energy_wh;		//Accumulated 	[Wh]
	/* Running min/max - updated by Screen_UpdateStats() */
	float v_min, v_max;
	float i_min, i_max;
	float p_min, p_max;
	uint32_t session_sec;
}EnergyData_t;

typedef enum{
	SCREEN_VOLTAGE 	= 0,
	SCREEN_CURRENT 	= 1,
	SCREEN_POWER	= 2,
	SCREEN_ENERGY	= 3,
	SCREEN_COUNT	= 4
}ScreenState_t;

#define SPARK_LEN 55 //Sparkline buffer (shared 60 samples)

typedef struct{
	float buf[SPARK_LEN];
	uint8_t	head;
	uint8_t count;
}SparkBuf_t;

void Screens_Init(void);
void Screens_NextState(void);
void Screens_Update(const EnergyData_t *d); //Call every ~500ms for new measuring data
void Screens_UpdateStats(EnergyData_t *d); 	//Update running min/max stats
void Spark_Push(SparkBuf_t *s, float val); 	//Push new sample into sparkline for current screen
ScreenState_t Screens_GetState(void); 		//Get current screen state


#endif /* INC_SCREENS_H_ */
