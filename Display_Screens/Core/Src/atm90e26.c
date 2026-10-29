#include "atm90e26.h"
#include "screens.h"   /* for EnergyData_t */

//static SPI_HandleTypeDef *_hspi;
extern SPI_HandleTypeDef hspi1;
/* Global energy data — updated by ATM_ReadAll() */

EnergyData_t g_energy = {0};

/* ── SPI mode switch helpers ─────────────────────────────────────── */


static void SPI_SetModeATM(SPI_HandleTypeDef *hspi)
{
	while(__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY));
	__HAL_SPI_DISABLE(hspi);
    hspi->Init.CLKPolarity = SPI_POLARITY_HIGH;  /* CPOL=1 idle high   */
    hspi->Init.CLKPhase    = SPI_PHASE_2EDGE;   /* CPHA=1 capture rising */
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; /* ~166 kHz @ 170MHz/APB2/2 */
    HAL_SPI_Init(hspi);
    __HAL_SPI_ENABLE(hspi);
}

/* Restore Mode 0 at display speed after every ATM transaction */
static void SPI_SetModeST7789(SPI_HandleTypeDef *hspi)
{
	while(__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY));
	__HAL_SPI_DISABLE(hspi);
    hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi->Init.CLKPhase    = SPI_PHASE_1EDGE;
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* ~10.6 MHz for ST7789 */
    HAL_SPI_Init(hspi);
    __HAL_SPI_ENABLE(hspi);
}

/* ── Register access ─────────────────────────────────────────────── */
static uint16_t ATM_ReadRaw(uint8_t reg)
{
	HAL_StatusTypeDef status;
    uint8_t tx[3] = { 0x80 | (reg & 0x7F), 0xFF, 0xFF };
    uint8_t rx[3] = { 0, 0, 0 };

    ATM_CS_LOW();
    status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 3, 20);
    ATM_CS_HIGH();

    if(status != HAL_OK)
	{
		printf("ATM SPI Read Error!\n");
		return 0xFFFF;
	}
    return (uint16_t)((rx[1] << 8) | rx[2]);
}

uint16_t ATM_Read(uint8_t reg)
{
	uint16_t val;
	SPI_SetModeATM(&hspi1);
	val = ATM_ReadRaw(reg);
	SPI_SetModeST7789(&hspi1);
	return val;
}

void ATM_Write(uint8_t reg, uint16_t val)
{
	HAL_StatusTypeDef status;
    uint8_t tx[3];
    tx[0] = reg & 0x7F;
    tx[1] = (val >> 8) & 0xFF;
    tx[2] =  val       & 0xFF;

    SPI_SetModeATM(&hspi1);
    ATM_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, tx, 3, 20);
    ATM_CS_HIGH();
    SPI_SetModeST7789(&hspi1);
    if(status != HAL_OK)
	{
		printf("ATM SPI Write Error!\n");
	}
}

/* ── Initialisation ──────────────────────────────────────────────── */
void ATM_Init(void)
{
    ATM_CS_HIGH();

    /* Hardware reset pulse — ATM RESET pin = PB0 (Reset_ATM_Pin).
       MX_GPIO_Init already leaves it HIGH; pulse LOW for clean start. */
    HAL_GPIO_WritePin(Reset_ATM_GPIO_Port, Reset_ATM_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(Reset_ATM_GPIO_Port, Reset_ATM_Pin, GPIO_PIN_SET);
    HAL_Delay(200); /* 200 ms: generous post-reset settling */

    /* Software reset as well for belt-and-braces */
    ATM_Write(ATM_SOFTRESET, 0x789A);
    HAL_Delay(100);

    /* Function enable: RevPEn=1 (bit2), RevQEn=1 (bit3) — default 0x000C */
    ATM_Write(ATM_FUNCEN,  0x000C);

    /* Sag threshold: default 0x1D6A ≈ 78% of 220 Vrms */
    ATM_Write(ATM_SAGTH,   0x1D6A);

    /* ── Metering calibration block ──────────────────────────────── */
    ATM_Write(ATM_CALSTART, 0x5678);  /* unlock */

    ATM_Write(ATM_PLCONSTH, 0x00D9);  /* PL_Constant = 0x00D91C71          */
    ATM_Write(ATM_PLCONSTL, 0x1C71);

    /* CHANGE: was writing to wrong registers with wrong names.
     * 0x23 = Lgain — L-line calibration gain. Tune for your shunt.  */
    ATM_Write(ATM_LGAIN,    0x0000);  /* default, tune after calibration   */

    /* 0x24 = Lphi — L-line phase angle compensation */
    ATM_Write(ATM_LPHI,     0x0000);

    /* 0x25 = Ngain — N-line calibration gain (N-line not used in L-line mode) */
    ATM_Write(ATM_NGAIN,    0x0000);

    /* 0x26 = Nphi — N-line phase angle */
    ATM_Write(ATM_NPHI,     0x0000);

    /* 0x27 = PStartTh — active startup power threshold (datasheet default) */
    ATM_Write(ATM_PSTARTTH, 0x08BD);

    /* 0x28 = PNolTh — active no-load power threshold */
    ATM_Write(ATM_PNOLTH,   0x0000);

    /* 0x29 = QStartTh — reactive startup power threshold (datasheet default) */
    ATM_Write(ATM_QSTARTTH, 0x0AEC);

    /* 0x2A = QNolTh */
    ATM_Write(ATM_QNOLTH,   0x0000);

    /* 0x2B = MMode: L-line only (MMD pins=01), Lgain=24, HPF on
     * Default 0x9422: Lgain[2:0]=100(=1x)→ field, Ngain=10(=1x),
     * LNSel=1(L), DisHPF=00, Amod=0, Rmod=0, ZXcon=10 , Pthresh=0010 */
    ATM_Write(ATM_MMODE,    0x9422);

    /* CS1=0x0000 + CalStart=0x5678 → diagnosis bypassed, metering runs */
    ATM_Write(ATM_CS1,      0x0000);
    /* ── Measurement calibration block ──────────────────────────── */
    ATM_Write(ATM_ADJSTART, 0x5678);  /* unlock */

    /* 0x31 = Ugain_cal — voltage rms gain (datasheet default 0x6720) */
    ATM_Write(ATM_UGAIN_CAL, 0x6720);

    /* 0x32 = IgainL — L-line current rms gain (datasheet default 0x7A13) */
    ATM_Write(ATM_IGAINL,    0x7A13);

    /* 0x33 = IgainN — N-line current rms gain (datasheet default 0x7530) */
    ATM_Write(ATM_IGAINN,    0x7530);

    /* All offsets = 0 initially */
    ATM_Write(ATM_UOFFSET,   0x0000);
    ATM_Write(ATM_IOFFSETL,  0x0000);
    ATM_Write(ATM_IOFFSETN,  0x0000);
    ATM_Write(ATM_POFFSETL,  0x0000);
    ATM_Write(ATM_QOFFSETL,  0x0000);
    ATM_Write(ATM_POFFSETN,  0x0000);
    ATM_Write(ATM_QOFFSETN,  0x0000);

    ATM_Write(ATM_CS2,      0x0000);

    /* Init running stats */
    g_energy.v_min = 999.0f;
    g_energy.v_max = 0.0f;
    g_energy.i_min = 999.0f;
    g_energy.i_max = 0.0f;
    g_energy.p_min = 999999.0f;
    g_energy.p_max = 0.0f;
}

/* ── Read all live measurements ──────────────────────────────────────
 *
 * Scaling (from datasheet Table-7):
 *   Urms  : format XXX.XX → raw / 100  = volts
 *   Irms  : format XX.XXX → raw / 1000 = amps
 *   Pmean : format XX.XXX kW signed   → (int16_t)raw / 1000 = kW
 *                                     → * 1000 = W  → net: raw = W directly
 *   Qmean : same as Pmean
 *   Smean : same, unsigned
 *   Freq  : format XX.XX  → raw / 100  = Hz
 *   PowerF: format X.XXX signed        → (int16_t)raw / 1000
 *   Pangle: format XXX.X signed        → (int16_t)raw / 10  = degrees
 *
 * Energy (APenergy 0x40):
 *   Format XXXX.X pulses, 1 LSB = 0.1 pulse.
 *   Default meter constant = 3200 imp/kWh → 1 pulse = 0.3125 Wh
 *   → 1 LSB = 0.03125 Wh.
 *   CHANGE: original used raw * (1/3600) which is dimensionally wrong.
 * ──────────────────────────────────────────────────────────────────── */
void ATM_ReadAll(void)
{
    uint16_t raw;
    SPI_SetModeATM(&hspi1);
    /* Voltage rms [V] */
    raw = ATM_ReadRaw(ATM_URMS);
    g_energy.voltage = (float)raw * 0.01f;

    /* L-line current rms [A] */
    raw = ATM_ReadRaw(ATM_IRMS);
    g_energy.current = (float)raw * 0.001f;

    /* L-line mean active power [W] — signed, format XX.XXX kW */
    raw = ATM_ReadRaw(ATM_PMEAN);
    g_energy.active_power = (float)(int16_t)raw;  /* 1 LSB = 1 W */
    if (g_energy.active_power < 0.0f) g_energy.active_power = 0.0f;

    /* Voltage frequency [Hz] */
    raw = ATM_ReadRaw(ATM_FREQ);
    g_energy.frequency = (float)raw * 0.01f;


    /* Forward active energy accumulation [Wh]
     * CHANGE: original used raw/3600 which is wrong.
     * 1 LSB = 0.1 pulse, 1 pulse = 0.3125 Wh → 1 LSB = 0.03125 Wh */
    raw = ATM_ReadRaw(ATM_APENERGY);
    g_energy.energy_wh += (float)raw * 0.03125f;
    SPI_SetModeST7789(&hspi1);
    /* Update running min/max */
    if (g_energy.voltage > 1.0f) {
        if (g_energy.voltage < g_energy.v_min) g_energy.v_min = g_energy.voltage;
        if (g_energy.voltage > g_energy.v_max) g_energy.v_max = g_energy.voltage;
    }
    if (g_energy.current > 0.001f) {
        if (g_energy.current < g_energy.i_min) g_energy.i_min = g_energy.current;
        if (g_energy.current > g_energy.i_max) g_energy.i_max = g_energy.current;
    }
    if (g_energy.active_power < g_energy.p_min) g_energy.p_min = g_energy.active_power;
    if (g_energy.active_power > g_energy.p_max) g_energy.p_max = g_energy.active_power;
}
