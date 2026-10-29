#ifndef ATM90E26_H
#define ATM90E26_H

#include "main.h"
#include "screens.h"
#include <stdint.h>

/* ── SPI handle (shared with ST7789, different CS) ───────────────────
   ATM90E26: CS = PA4 (CS2), SPI Mode 3 (CPOL=1, CPHA=1)
   ST7789:   CS = PA15 (CS1), SPI Mode 0
   Reconfigure SPI before each device access.
──────────────────────────────────────────────────────────────────── */
extern EnergyData_t g_energy;
#define ATM_CS_PORT   CS_ATM90E26_GPIO_Port
#define ATM_CS_PIN    CS_ATM90E26_Pin

#define ATM_CS_LOW()    HAL_GPIO_WritePin(ATM_CS_PORT, ATM_CS_PIN, GPIO_PIN_RESET)
#define ATM_CS_HIGH()   HAL_GPIO_WritePin(ATM_CS_PORT, ATM_CS_PIN, GPIO_PIN_SET)

/* ── Metering calibration & configuration ────────────────────────────
 * Unlock: write 0x5678 to ATM_CALSTART
 * Lock  : write 0x8765 to ATM_CALSTART (enables checksum checking)
 * ──────────────────────────────────────────────────────────────────── */
#define ATM_SOFTRESET 	0x00
#define ATM_SYSSTATUS   0x01
#define ATM_FUNCEN      0x02
#define ATM_SAGTH       0x03
#define ATM_SMALLPMOD   0x04
#define ATM_LASTDATA    0x06

#define ATM_LSB         0x08   /* R   RMS/power 16-bit LSB                  */
#define ATM_CALSTART    0x20   /* R/W 0x5678=unlock, 0x8765=lock+check      */
#define ATM_PLCONSTH    0x21   /* R/W PL_Constant high word                 */
#define ATM_PLCONSTL    0x22   /* R/W PL_Constant low word                  */
#define ATM_LGAIN       0x23   /* R/W L-line calibration gain               */
#define ATM_LPHI        0x24   /* R/W L-line calibration phase angle        */
#define ATM_NGAIN       0x25   /* R/W N-line calibration gain               */
#define ATM_NPHI        0x26   /* R/W N-line calibration phase angle        */
#define ATM_PSTARTTH    0x27   /* R/W active startup power threshold        */
#define ATM_PNOLTH      0x28   /* R/W active no-load power threshold        */
#define ATM_QSTARTTH    0x29   /* R/W reactive startup power threshold      */
#define ATM_QNOLTH      0x2A   /* R/W reactive no-load power threshold      */
#define ATM_MMODE       0x2B   /* R/W metering mode configuration           */
#define ATM_CS1         0x2C   /* R/W checksum 1 (covers regs 0x21-0x2B)   */

/* ── Measurement calibration ──────────────────────────────────────────
 * Unlock: write 0x5678 to ATM_ADJSTART
 * Lock  : write 0x8765 to ATM_ADJSTART
 * ──────────────────────────────────────────────────────────────────── */
#define ATM_ADJSTART    0x30   /* R/W 0x5678=unlock, 0x8765=lock+check      */
#define ATM_UGAIN_CAL   0x31   /* R/W voltage rms GAIN (calibration reg)    */
#define ATM_IGAINL      0x32   /* R/W L-line current rms gain               */
#define ATM_IGAINN      0x33   /* R/W N-line current rms gain               */
#define ATM_UOFFSET     0x34   /* R/W voltage offset                        */
#define ATM_IOFFSETL    0x35   /* R/W L-line current offset                 */
#define ATM_IOFFSETN    0x36   /* R/W N-line current offset                 */
#define ATM_POFFSETL    0x37   /* R/W L-line active power offset            */
#define ATM_QOFFSETL    0x38   /* R/W L-line reactive power offset          */
#define ATM_POFFSETN    0x39   /* R/W N-line active power offset            */
#define ATM_QOFFSETN    0x3A   /* R/W N-line reactive power offset          */
#define ATM_CS2         0x3B   /* R/W checksum 2 (covers regs 0x31-0x3A)   */

/* ── Energy registers (cleared after read) ───────────────────────── */
#define ATM_APENERGY    0x40   /* R/C forward active energy                 */
#define ATM_ANENERGY    0x41   /* R/C reverse active energy                 */
#define ATM_ATENERGY    0x42   /* R/C absolute active energy                */
#define ATM_RPENERGY    0x43   /* R/C forward (inductive) reactive energy   */
#define ATM_RNENERGY    0x44   /* R/C reverse (capacitive) reactive energy  */
#define ATM_RTENERGY    0x45   /* R/C absolute reactive energy              */
#define ATM_ENSTATUS    0x46   /* R   metering status                       */

/* ── Live measurement registers (0x48-0x4F) ──────────────────────────
 * CHANGE: entirely missing from original. These are the actual real-time
 * measurement outputs. The original code was reading cal regs 0x31-0x38
 * instead, which are gain/offset calibration values — not live data.
 * ──────────────────────────────────────────────────────────────────── */
#define ATM_IRMS        0x48   /* R  L-line current rms  format XX.XXX [A]  */
#define ATM_URMS        0x49   /* R  voltage rms         format XXX.XX [V]  */
#define ATM_PMEAN       0x4A   /* R  L-line mean active power  XX.XXX kW, signed */
#define ATM_QMEAN       0x4B   /* R  L-line mean reactive power XX.XXX kvar, signed */
#define ATM_FREQ        0x4C   /* R  voltage frequency   format XX.XX  [Hz] */
#define ATM_POWERF      0x4D   /* R  L-line power factor format X.XXX, signed */
#define ATM_PANGLE      0x4E   /* R  phase angle V vs I  format XXX.X  [°], signed */
#define ATM_SMEAN       0x4F   /* R  L-line apparent power XX.XXX kVA       */
#define ATM_IRMS2       0x68   /* N-line current rms */

//test
#define SAG_WARNING_BIT   0x0002   // bit 1

#define REVP_CHANGE_BIT   0x0020   // bit 5
#define REVQ_CHANGE_BIT   0x0040   // bit 6

#define LN_CHANGE_BIT     0x0080   // bit 7

#define ADJ_ERROR_MASK    0x3000   // bits 12-13
#define CAL_ERROR_MASK    0xC000   // bits 14-15



/* ── Public API ──────────────────────────────────────────────────── */
void     ATM_Init(void);
uint16_t ATM_Read(uint8_t reg);
void     ATM_Write(uint8_t reg, uint16_t val);
void     ATM_ReadAll(void);  /* updates global g_energy */
#endif /* ATM90E26_H */
