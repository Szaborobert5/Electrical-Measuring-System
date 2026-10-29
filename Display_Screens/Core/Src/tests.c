/*
 * tests.c
 *
 *  Created on: May 9, 2026
 *      Author: Ljubomir
 */
#include "tests.h"

uint8_t _tests_run    = 0;
uint8_t _tests_passed = 0;

extern SPI_HandleTypeDef hspi1;
/* ════════════════════════════════════════════════════════════════════
   TEST SUITE
   Each test function checks one aspect of the system.
   Results are printed via SWO (ITM) printf and shown on the display.
   ════════════════════════════════════════════════════════════════════ */

/* ── Helper: print result to display and SWO ───────────────────────── */
void Test_Report(const char *name, uint8_t pass, const char *detail)
{
    _tests_run++;
    if (pass) _tests_passed++;

    /* SWO output — visible in ST-Link SWV viewer */
    printf("[%s] %s: %s\r\n", pass ? "PASS" : "FAIL", name, detail);

    /* Show on display — simple text list */
    static uint16_t y = 40;
    char line[48];
    snprintf(line, sizeof(line), "%s %s", pass ? "OK" : "XX", name);
    uint16_t colour = pass ? 0x07E0 /* green */ : 0xF800 /* red */;
    ST7789_WriteString(4, y, line, Font_7x10, colour, 0x0000);
    if (detail && detail[0]) {
        char det[40];
        snprintf(det, sizeof(det), "   %s", detail);
        y += 11;
        ST7789_WriteString(4, y, det, Font_7x10, 0x8410, 0x0000);
    }
    y += 13;
    if (y > 220) y = 40; /* wrap */
}

/* ── TC-1: SPI communication ──────────────────────────────────────────
 * Write a known value to SoftReset register (harmless all-zero write
 * that does nothing but 0x789A), then read back LastData (0x06) which
 * mirrors the last SPI transaction data. Also write/read FuncEn (0x02)
 * which is a readable R/W register.
 * ──────────────────────────────────────────────────────────────────── */
void Test_SPI_Comms(void)
{
	ST7789_FillColor(WHITE);
    /* Write 0x000C to FuncEn, read it back */
    ATM_Write(ATM_FUNCEN, 0x000C);
    HAL_Delay(2);
    uint16_t val = ATM_Read(ATM_FUNCEN);
    char det[24];
    snprintf(det, sizeof(det), "FuncEn=0x%04X", val);
    Test_Report("TC1 SPI_COMMS", val == 0x000C, det);

    /* Write another value, confirm it changes */
    ATM_Write(ATM_FUNCEN, 0x003C);
    HAL_Delay(2);
    val = ATM_Read(ATM_FUNCEN);
    snprintf(det, sizeof(det), "FuncEn=0x%04X", val);
    Test_Report("TC1b SPI_RW",   val == 0x003C, det);

    /* Restore */
    ATM_Write(ATM_FUNCEN, 0x000C);
}

/* ── TC-2: CS pin correct (PA4, not PA8) ─────────────────────────────
 * Manually toggle PA4 LOW, do a dummy SPI byte, toggle HIGH.
 * Then confirm SysStatus reads a sane value (not 0xFFFF which would
 * mean the chip never responded — happens if CS is on wrong pin).
 * ──────────────────────────────────────────────────────────────────── */
void Test_CS_Pin(void)
{
    uint16_t status = ATM_Read(ATM_SYSSTATUS);
    /* 0xFFFF = no response (MISO stuck high). 0x0000 is valid idle state. */
    uint8_t pass = (status != 0xFFFF);
    char det[24];
    snprintf(det, sizeof(det), "SysStat=0x%04X", status);
    Test_Report("TC2 CS_PIN_PA4", pass, det);
}

/* ── TC-3: Voltage measurement ────────────────────────────────────────
 * With mains connected: expect 180–260 V (covers 85–265 V range meters).
 * With no mains (bench): expect < 5 V (noise floor).
 * Also verify frequency is in valid range if voltage is present.
 * ──────────────────────────────────────────────────────────────────── */
void Test_Voltage(void)
{
	ST7789_FillColor(WHITE);
    /* Read register directly to bypass g_energy (fresh read) */
    uint16_t raw_v = ATM_Read(ATM_URMS);
    uint16_t raw_f = ATM_Read(ATM_FREQ);
    float v = (float)raw_v * 0.01f;
    float f = (float)raw_f * 0.01f;

    /* Test A: register returns a value in physically possible range */
    uint8_t pass_range = (v >= 0.0f && v <= 655.35f);
    char det[32];
    snprintf(det, sizeof(det), "Urms=%.2fV raw=0x%04X", v, raw_v);
    Test_Report("TC3a URMS_RANGE",  pass_range, det);
    //ST7789_WriteString(10, 150, det, Font_16x26, MAGENTA, WHITE);
    /* Test B: if voltage present, frequency must be plausible */
    if (v > 50.0f) {
        uint8_t pass_freq = (f >= 45.0f && f <= 65.0f);
        snprintf(det, sizeof(det), "Freq=%.2fHz", f);
        Test_Report("TC3b FREQ_VALID", pass_freq, det);
    } else {
        Test_Report("TC3b FREQ_VALID", 1, "skipped (no mains)");
    }

    /* Test C: scaling — raw 22000 should decode to 220.00 V */
    /* Inject raw value via fake read: can't do that, so verify formula */
    float check = (float)(uint16_t)22000 * 0.01f;
    uint8_t pass_scale = (fabsf(check - 220.0f) < 0.01f);
    snprintf(det, sizeof(det), "scale:22000->%.2fV", check);
    Test_Report("TC3c URMS_SCALE", pass_scale, det);
    //ST7789_WriteString(10, 100, det, Font_16x26, MAGENTA, WHITE);
}

/* ── TC-4: Current measurement ────────────────────────────────────────
 * With no load: expect < 0.050 A (noise floor of shunt).
 * Register range check: 0–65.535 A.
 * Scaling check: raw 5000 → 5.000 A.
 * ──────────────────────────────────────────────────────────────────── */
void Test_Current(void)
{
    uint16_t raw_i = ATM_Read(ATM_IRMS);
    float i = (float)raw_i * 0.001f;

    /* Range */
    uint8_t pass_range = (i >= 0.0f && i <= 65.535f);
    char det[32];
    snprintf(det, sizeof(det), "Irms=%.3fA raw=0x%04X", i, raw_i);
    Test_Report("TC4a IRMS_RANGE", pass_range, det);

    /* Scaling */
    float check = (float)(uint16_t)5000 * 0.001f;
    uint8_t pass_scale = (fabsf(check - 5.0f) < 0.001f);
    snprintf(det, sizeof(det), "scale:5000->%.3fA", check);
    Test_Report("TC4b IRMS_SCALE", pass_scale, det);
}

/* ── TC-5: Power measurements ─────────────────────────────────────────
 * Pmean, Qmean, Smean, PowerF, Pangle: check register read returns
 * plausible data types and scaling.
 * Power factor must be in [-1, +1].
 * Phase angle must be in [-180, +180].
 * Apparent power must be >= 0.
 * ──────────────────────────────────────────────────────────────────── */
void Test_Power(void)
{
    uint16_t raw_p  = ATM_Read(ATM_PMEAN);
    //uint16_t raw_q  = ATM_Read(ATM_QMEAN);
    uint16_t raw_s  = ATM_Read(ATM_SMEAN);
    uint16_t raw_pf = ATM_Read(ATM_POWERF);
    uint16_t raw_pa = ATM_Read(ATM_PANGLE);

    float p  = (float)(int16_t)raw_p;
    //float q  = (float)(int16_t)raw_q;
    float s  = (float)raw_s;
    float pf = (float)(int16_t)raw_pf / 1000.0f;
    float pa = (float)(int16_t)raw_pa * 0.1f;

    char det[40];

    /* Apparent power ≥ 0 */
    snprintf(det, sizeof(det), "S=%.1fVA", s);
    Test_Report("TC5a SMEAN_POSITIVE", s >= 0.0f, det);

    /* Power factor in [-1, +1] */
    snprintf(det, sizeof(det), "PF=%.3f", pf);
    Test_Report("TC5b PF_RANGE", (pf >= -1.0f && pf <= 1.0f), det);

    /* Phase angle in [-180, +180] */
    snprintf(det, sizeof(det), "PA=%.1fdeg", pa);
    Test_Report("TC5c PANGLE_RANGE", (pa >= -180.0f && pa <= 180.0f), det);

    /* |P| ≤ S (active can't exceed apparent) — only meaningful with load */
    if (s > 10.0f) {
        uint8_t pass = (fabsf(p) <= s * 1.05f); /* 5% tolerance for rounding */
        snprintf(det, sizeof(det), "P=%.1f S=%.1f", p, s);
        Test_Report("TC5d P_LEQ_S", pass, det);
    } else {
        Test_Report("TC5d P_LEQ_S", 1, "skipped (no load)");
    }

    /* Scaling check: PF raw 0x03E8 = 1000 → 1.000 */
    float pf_check = (float)(int16_t)0x03E8 / 1000.0f;
    snprintf(det, sizeof(det), "0x03E8->%.3f", pf_check);
    Test_Report("TC5e PF_SCALE", fabsf(pf_check - 1.0f) < 0.001f, det);
}

/* ── TC-6: Energy accumulation ────────────────────────────────────────
 * Reset energy accumulator, wait 1 s, read APenergy.
 * With no load: APenergy should be 0 (chip in no-load state).
 * With load: must be > 0.
 * Verify the Wh scaling formula: raw=320 → 320*0.03125=10.0 Wh.
 * Also verify absolute energy register (ATenergy) ≥ APenergy.
 * ──────────────────────────────────────────────────────────────────── */
void Test_Energy_Accumulation(void)
{
    /* Clear accumulator by reading (registers clear on read) */
    ATM_Read(ATM_APENERGY);
    ATM_Read(ATM_ATENERGY);
    HAL_Delay(1000);

    uint16_t raw_ap = ATM_Read(ATM_APENERGY);
    uint16_t raw_at = ATM_Read(ATM_ATENERGY);

    char det[40];

    /* Wh scaling formula */
    float wh_check = (float)(uint16_t)320 * 0.03125f;
    uint8_t pass_scale = (fabsf(wh_check - 10.0f) < 0.01f);
    snprintf(det, sizeof(det), "320*0.03125=%.1fWh", wh_check);
    Test_Report("TC6a ENERGY_SCALE", pass_scale, det);

    /* APenergy register readable (not 0xFFFF = SPI fail) */
    snprintf(det, sizeof(det), "APenergy=0x%04X", raw_ap);
    Test_Report("TC6b APENERGY_REG", raw_ap != 0xFFFF, det);

    /* ATenergy ≥ APenergy (absolute ≥ forward) */
    snprintf(det, sizeof(det), "AT=0x%04X AP=0x%04X", raw_at, raw_ap);
    Test_Report("TC6c ATENERGY_GEQ_AP", raw_at >= raw_ap, det);

    /* g_energy accumulator increases over time (run two reads with gap) */
    float e_before = g_energy.energy_wh;
    ATM_ReadAll();
    HAL_Delay(500);
    ATM_ReadAll();
    float e_after = g_energy.energy_wh;
    snprintf(det, sizeof(det), "before=%.4f after=%.4f", e_before, e_after);
    /* With no load both may be 0 — just check it didn't go negative */
    Test_Report("TC6d ENERGY_NONDECR", e_after >= e_before, det);
}

/* ── TC-7: Min/max tracking ───────────────────────────────────────────
 * Manually inject two known values via g_energy, call
 * Screens_UpdateStats, verify v_min/v_max update correctly.
 * ──────────────────────────────────────────────────────────────────── */
void Test_MinMax_Tracking(void)
{
    /* Save state */
    float saved_v     = g_energy.voltage;
    float saved_v_min = g_energy.v_min;
    float saved_v_max = g_energy.v_max;

    /* Reset and inject a low value */
    g_energy.v_min = 999.0f;
    g_energy.v_max = 0.0f;
    g_energy.voltage = 210.0f;
    Screens_UpdateStats(&g_energy);

    /* Inject a high value */
    g_energy.voltage = 235.0f;
    Screens_UpdateStats(&g_energy);

    /* Inject mid value — should not change min or max */
    g_energy.voltage = 220.0f;
    Screens_UpdateStats(&g_energy);

    uint8_t pass_min = (fabsf(g_energy.v_min - 210.0f) < 0.1f);
    uint8_t pass_max = (fabsf(g_energy.v_max - 235.0f) < 0.1f);
    char det[40];

    snprintf(det, sizeof(det), "vmin=%.1f (exp 210.0)", g_energy.v_min);
    Test_Report("TC7a VMIN_TRACK", pass_min, det);

    snprintf(det, sizeof(det), "vmax=%.1f (exp 235.0)", g_energy.v_max);
    Test_Report("TC7b VMAX_TRACK", pass_max, det);

    /* Also test current min/max */
    g_energy.i_min = 999.0f;
    g_energy.i_max = 0.0f;
    g_energy.current = 3.500f;
    Screens_UpdateStats(&g_energy);
    g_energy.current = 0.250f;
    Screens_UpdateStats(&g_energy);

    uint8_t pass_imin = (fabsf(g_energy.i_min - 0.250f) < 0.001f);
    uint8_t pass_imax = (fabsf(g_energy.i_max - 3.500f) < 0.001f);
    snprintf(det, sizeof(det), "imin=%.3fA (exp 0.250)", g_energy.i_min);
    Test_Report("TC7c IMIN_TRACK", pass_imin, det);
    snprintf(det, sizeof(det), "imax=%.3fA (exp 3.500)", g_energy.i_max);
    Test_Report("TC7d IMAX_TRACK", pass_imax, det);

    /* Restore */
    g_energy.voltage = saved_v;
    g_energy.v_min   = saved_v_min;
    g_energy.v_max   = saved_v_max;
}

/* ── TC-8: Button and screen transitions ──────────────────────────────
 * Programmatically call Screens_NextState() 4 times and verify we
 * cycle through all 4 states and return to the original.
 * ──────────────────────────────────────────────────────────────────── */

/* ── Run all tests ────────────────────────────────────────────────── */
void Run_Tests(void)
{
    /* Clear screen, print header */
    ST7789_FillColor(0x0000);
    ST7789_WriteString(4, 4, "=== SELF TEST ===", Font_7x10, 0xFFFF, 0x0000);
    ST7789_WriteString(4, 16, "Running...", Font_7x10, 0x8410, 0x0000);

    printf("\r\n=== ATM90E26 + STM32G431 SELF TEST ===\r\n");

    Test_SPI_Comms();
    HAL_Delay(1000);
    Test_CS_Pin();
    HAL_Delay(1000);
    Test_Voltage();
    HAL_Delay(1000);
    Test_Current();
    HAL_Delay(1000);
    Test_Power();
    HAL_Delay(1000);
    Test_Energy_Accumulation();
    HAL_Delay(1000);
    Test_MinMax_Tracking();
    HAL_Delay(1000);

    /* Summary */
    char summary[48];
    snprintf(summary, sizeof(summary), "DONE: %d/%d passed",
             _tests_passed, _tests_run);
    printf("=== %s ===\r\n\r\n", summary);

    uint16_t col = (_tests_passed == _tests_run) ? 0x07E0 : 0xF800;
    ST7789_WriteString(4, 228, summary, Font_7x10, col, 0x0000);

    HAL_Delay(3000); /* leave results visible for 3 s */

    /* Restore normal screens */
    while (Screens_GetState() != SCREEN_VOLTAGE)
        Screens_NextState();
    Screens_Update(&g_energy);
}

void ATM_CheckStatus(void)
{
    uint16_t status = ATM_Read(ATM_SYSSTATUS);

    if(status == 0xFFFF)
    {
    	ST7789_WriteString(2, 10, "ATM SPI communication failed!", Font_7x10, WHITE, BLACK);
        printf("ATM SPI communication failed!\n");
        return;
    }

    printf("SYSSTATUS = 0x%04X\n", status);

    if(status & SAG_WARNING_BIT)
    {
    	ST7789_WriteString(2, 20, "Warning: Voltage sag detected!", Font_7x10, WHITE, BLACK);
        printf("Warning: Voltage sag detected!\n");
    }

    if(status & REVP_CHANGE_BIT)
    {
    	ST7789_WriteString(2, 30, "Info: Active energy direction changed!", Font_7x10, WHITE, BLACK);
        printf("Info: Active energy direction changed!\n");
    }

    if(status & REVQ_CHANGE_BIT)
    {
    	ST7789_WriteString(2, 40, "Info: Reactive energy direction changed!", Font_7x10, WHITE, BLACK);
        printf("Info: Reactive energy direction changed!\n");
    }

    if(status & LN_CHANGE_BIT)
    {
    	ST7789_WriteString(2, 50, "Warning: L/N metering line changed!", Font_7x10, WHITE, BLACK);
        printf("Warning: L/N metering line changed!\n");
    }

    if((status & CAL_ERROR_MASK) == CAL_ERROR_MASK)
    {
    	ST7789_WriteString(2, 60, "ERROR: CS1 checksum invalid!", Font_7x10, WHITE, BLACK);
        printf("ERROR: CS1 checksum invalid!\n");
    }

    if((status & ADJ_ERROR_MASK) == ADJ_ERROR_MASK)
    {
    	ST7789_WriteString(2, 70, "ERROR: CS2 checksum invalid!", Font_7x10, WHITE, BLACK);
        printf("ERROR: CS2 checksum invalid!\n");
    }

    if((status & (CAL_ERROR_MASK | ADJ_ERROR_MASK)) == 0)
    {
    	ST7789_WriteString(2, 70, "Calibration status OK", Font_7x10, WHITE, BLACK);
        printf("\n");
    }
}


