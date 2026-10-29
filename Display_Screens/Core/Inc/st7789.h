#ifndef __ST7789_H
#define __ST7789_H

#include "main.h"
#include "font.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
//#include <string.h>

#define DC_PIN     DC_ST7789_Pin
#define RES_PIN    Reset_ST7789_Pin
#define BLK_PIN    BL_ST7789_Pin
#define CS_PIN     CS_ST7789_Pin

#define DC_PORT    DC_ST7789_GPIO_Port
#define RES_PORT   Reset_ST7789_GPIO_Port
#define BLK_PORT   BL_ST7789_GPIO_Port
#define CS_PORT    CS_ST7789_GPIO_Port

#ifndef CFG_NO_CS
#define ST7789_CS_PORT  ST7789_CS_GPIO_Port
#define ST7789_CS_PIN   ST7789_CS_Pin
#endif



// Display dimensions for GMT130 (240x280)
#define LCD_W 240
#define LCD_H 280

// Common colors (RGB565)
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define DARKGREEN	0x0347
#define CYAN        0x7FFF
#define DARKCYAN	0x0185
#define YELLOW      0xFFE0
#define ORANGE		0xFACA
#define GRAY        0X8430
#define BRED        0XF81F
#define GRED        0XFFE0
#define GBLUE       0X07FF
#define BROWN       0XBC40
#define BRRED       0XFC07
#define DARKBLUE    0X01CF
#define LIGHTBLUE   0X7D7C
#define GRAYBLUE    0X5458
#define PANEL		0x0885
#define DIM			0x324C
#define	PURPLE		0xC85F
#define DARKPURPLE 	0x0824
#define AMBER		0xFFD6
#define DARKAMBER	0x0920

#define LIGHTGREEN  0X841F
#define LGRAY       0XC618
#define LGRAYBLUE   0XA651
#define LBBLUE      0X2B12

/* Control Registers and constant codes */
#define ST7789_NOP     0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID   0x04
#define ST7789_RDDST   0x09

#define ST7789_SLPIN   0x10
#define ST7789_SLPOUT  0x11
#define ST7789_PTLON   0x12
#define ST7789_NORON   0x13

#define ST7789_INVOFF  0x20
#define ST7789_INVON   0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_RAMRD   0x2E

#define ST7789_PTLAR   0x30
#define ST7789_COLMOD  0x3A
#define ST7789_MADCTL  0x36

/* Basic operations */
#define ST7789_RST_Clr() HAL_GPIO_WritePin(RES_PORT, RES_PIN, GPIO_PIN_RESET)
#define ST7789_RST_Set() HAL_GPIO_WritePin(RES_PORT, RES_PIN, GPIO_PIN_SET)

#define ST7789_DC_Clr() HAL_GPIO_WritePin(DC_PORT, DC_PIN, GPIO_PIN_RESET)
#define ST7789_DC_Set() HAL_GPIO_WritePin(DC_PORT, DC_PIN, GPIO_PIN_SET)
#define LCD_CS_LOW()    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET)
#define LCD_CS_HIGH()   HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET)
#define ST7789_Select()     LCD_CS_LOW()
#define ST7789_UnSelect()   LCD_CS_HIGH()

/* Page Address Order ('0': Top to Bottom, '1': the opposite) */
#define ST7789_MADCTL_MY  0x80
/* Column Address Order ('0': Left to Right, '1': the opposite) */
#define ST7789_MADCTL_MX  0x40
/* Page/Column Order ('0' = Normal Mode, '1' = Reverse Mode) */
#define ST7789_MADCTL_MV  0x20
/* Line Address Order ('0' = LCD Refresh Top to Bottom, '1' = the opposite) */
#define ST7789_MADCTL_ML  0x10
/* RGB/BGR Order ('0' = RGB, '1' = BGR) */
#define ST7789_MADCTL_RGB 0x00

#define ABS(x) ((x) > 0 ? (x) : -(x))

void ST7789_SendCmd(uint8_t cmd);
void ST7789_SendData(uint8_t *data, size_t data_size);
void ST7789_SendSmallData(uint8_t data);
void ST7789_Init(void);
void ST7789_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ST7789_FillColor(uint16_t color);
void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
//New functions added
void ST7789_SetRotation(uint8_t m);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
/* Graphical functions. */
void ST7789_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ST7789_DrawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void ST7789_DrawVLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color);

void ST7789_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
/* Text functions. */
void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7789_WriteStringCentered(uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor);
/* Extented Graphical functions. */
void ST7789_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void ST7789_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void ST7789_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ST7789_DrawArc(uint16_t cx, uint16_t cy, uint16_t r, float start_deg, float end_deg, uint16_t color, uint8_t thickness);

/* Command functions */
void ST7789_TearEffect(uint8_t tear);

/* Simple test function. */
void ST7789_Test(void);

#endif
