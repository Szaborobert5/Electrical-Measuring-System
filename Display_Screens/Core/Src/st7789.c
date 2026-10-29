/*
 * st7789.c
 *
 *  Created on: Apr 13, 2026
 *      Author: Ljubomir
 */
#include "st7789.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;

void ST7789_SendCmd(uint8_t cmd)
{
	ST7789_Select();
	ST7789_DC_Clr(); // DC low = command
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    ST7789_UnSelect();
}
void ST7789_SendData(uint8_t *data, size_t data_size)
{
	ST7789_Select();
	ST7789_DC_Set();

	//split data in small chunks because HAL can't send more than 64k at once

	while (data_size > 0)
	{
		uint16_t chunk_size = data_size > 65535 ? 65535 : data_size;
		#ifdef USE_DMA
			if (DMA_MIN_SIZE <= data_size)
			{
				HAL_SPI_Transmit_DMA(&hspi1, data, chunk_size);
				while (hspi1.hdmatx->State != HAL_DMA_STATE_READY)
				{}
			}
			else
				HAL_SPI_Transmit(&hspi1, data, chunk_size, HAL_MAX_DELAY);
		#else
			HAL_SPI_Transmit(&hspi1, data, chunk_size, HAL_MAX_DELAY);
		#endif
		data += chunk_size;
		data_size -= chunk_size;
	}
	ST7789_UnSelect();
}
void ST7789_SendSmallData(uint8_t data)
{
	ST7789_Select();
	ST7789_DC_Set();   // DC high = data
    HAL_SPI_Transmit(&hspi1, &data, sizeof(data), HAL_MAX_DELAY);
    ST7789_UnSelect();
}

void ST7789_Init(void)
{
    // Hardware reset
	ST7789_RST_Clr();
    HAL_Delay(10);
    ST7789_RST_Set();
    HAL_Delay(120);

    ST7789_SendCmd(0x11); // Sleep out
    HAL_Delay(120);

    ST7789_SendCmd(0x36); // Memory data access control
    ST7789_SendSmallData(0x00);

    ST7789_SendCmd(0x3A); // Color format: 16-bit RGB565
    ST7789_SendSmallData(0x05);

    ST7789_SendCmd(0xB2); // Porch control
    ST7789_SendSmallData(0x0C);
    ST7789_SendSmallData(0x0C);
    ST7789_SendSmallData(0x00);
    ST7789_SendSmallData(0x33);
    ST7789_SendSmallData(0x33);

    ST7789_SendCmd(0xB7); // Gate control
    ST7789_SendSmallData(0x35);

    ST7789_SendCmd(0xBB); // VCOM setting
    ST7789_SendSmallData(0x19);

    ST7789_SendCmd(0xC0); // LCM control
    ST7789_SendSmallData(0x2C);

    ST7789_SendCmd(0xC2); // VDV and VRH enable
    ST7789_SendSmallData(0x01);

    ST7789_SendCmd(0xC3); // VRH set
    ST7789_SendSmallData(0x12);

    ST7789_SendCmd(0xC4); // VDV set
    ST7789_SendSmallData(0x20);

    ST7789_SendCmd(0xC6); // Frame rate: 60Hz
    ST7789_SendSmallData(0x0F);

    ST7789_SendCmd(0xD0); // Power control
    ST7789_SendSmallData(0xA4);
    ST7789_SendSmallData(0xA1);

    // Positive gamma
    ST7789_SendCmd(0xE0);
    ST7789_SendSmallData(0xD0); ST7789_SendSmallData(0x04);
    ST7789_SendSmallData(0x0D); ST7789_SendSmallData(0x11);
    ST7789_SendSmallData(0x13); ST7789_SendSmallData(0x2B);
    ST7789_SendSmallData(0x3F); ST7789_SendSmallData(0x54);
    ST7789_SendSmallData(0x4C); ST7789_SendSmallData(0x18);
    ST7789_SendSmallData(0x0D); ST7789_SendSmallData(0x0B);
    ST7789_SendSmallData(0x1F); ST7789_SendSmallData(0x23);

    // Negative gamma
    ST7789_SendCmd(0xE1);
    ST7789_SendSmallData(0xD0); ST7789_SendSmallData(0x04);
    ST7789_SendSmallData(0x0C); ST7789_SendSmallData(0x11);
    ST7789_SendSmallData(0x13); ST7789_SendSmallData(0x2C);
    ST7789_SendSmallData(0x3F); ST7789_SendSmallData(0x44);
    ST7789_SendSmallData(0x51); ST7789_SendSmallData(0x2F);
    ST7789_SendSmallData(0x1F); ST7789_SendSmallData(0x1F);
    ST7789_SendSmallData(0x20); ST7789_SendSmallData(0x23);

    ST7789_SendCmd(0x21); // Display inversion on (needed for correct colors)
    ST7789_SendCmd(0x29); // Display on
    HAL_Delay(10);
}

void ST7789_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	// GMT130 common offsets - try these
	    //x1 += 55;
	    //x2 += 55;
	    y1 += 20;
	    y2 += 20;
    // Column address
    ST7789_SendCmd(0x2A);
    ST7789_SendSmallData(x1 >> 8);
    ST7789_SendSmallData(x1 & 0xFF);
    ST7789_SendSmallData(x2 >> 8);
    ST7789_SendSmallData(x2 & 0xFF);

    // Row address
    ST7789_SendCmd(0x2B);
    ST7789_SendSmallData(y1 >> 8);
    ST7789_SendSmallData(y1 & 0xFF);
    ST7789_SendSmallData(y2 >> 8);
    ST7789_SendSmallData(y2 & 0xFF);

    // Start writing
    ST7789_SendCmd(0x2C);
}

void ST7789_FillColor(uint16_t color)
{
    ST7789_SetWindow(0, 0, LCD_W - 1, LCD_H - 1);
    ST7789_Select();
    ST7789_DC_Set();

    uint8_t buf[2] = {color >> 8, color & 0xFF};

    for (uint32_t i = 0; i < LCD_W * LCD_H; i++)
    	HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    ST7789_UnSelect();
}

void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ST7789_SetWindow(x, y, x + w - 1, y + h - 1);
    ST7789_Select();
    ST7789_DC_Set();

    uint8_t buf[2] = {color >> 8, color & 0xFF};

    for (uint32_t i = 0; i < w * h; i++)
    	HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    ST7789_UnSelect();
}
void ST7789_SetRotation(uint8_t m)
{
	ST7789_SendCmd(ST7789_MADCTL);
	switch(m){
	case 0:
		ST7789_SendSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
		break;
	case 1:
		ST7789_SendSmallData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
		break;
	case 2:
		ST7789_SendSmallData(ST7789_MADCTL_RGB);
		break;
	case 3:
		ST7789_SendSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
		break;
	default:
		break;

	}
}
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
	if ((x < 0) || (x >= LCD_W) || (y < 0) || (y >= LCD_H))	return;
	ST7789_SetWindow(x, y, x, y);
	uint8_t data[] = {color >> 8, color & 0xFF};
	ST7789_SendData(data, sizeof(data));
}

void ST7789_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	uint16_t swap;
	uint16_t steep = ABS(y2 - y1) > ABS(x2 - x1);
	if (steep)
	{
		swap = x1;
		x1 = y1;
		y1 = swap;

		swap = x2;
		x2 = y2;
		y2 = swap;
	}
	if(x1 > x2)
	{
		swap = x1;
		x1 = x2;
		x2 = swap;

		swap = y1;
		y1 = y2;
		y2 = swap;
	}
	uint16_t dx, dy;
	dx = x2 - x1;
	dy = ABS(y2 - y1);
	int16_t err = dx / 2;
	int16_t ystep;
	if(y1 < y2)
		ystep = 1;
	else
		ystep = -1;
	for(;x1 <= x2;x1++)
	{
		if(steep)
			ST7789_DrawPixel(y1, x1, color);
		else
			ST7789_DrawPixel(x1, y1, color);
		err -= dy;
		if(err < 0)
		{
			y1 += ystep;
			err += dx;
		}
	}
}
void ST7789_DrawHLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color) {
    ST7789_FillRect(x, y, len, 1, color);
}

void ST7789_DrawVLine(uint16_t x, uint16_t y, uint16_t len, uint16_t color) {
    ST7789_FillRect(x, y, 1, len, color);
}

void ST7789_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	ST7789_DrawLine(x1, y1, x2, y1, color);
	ST7789_DrawLine(x1, y1, x1, y2, color);
	ST7789_DrawLine(x1, y2, x2, y2, color);
	ST7789_DrawLine(x2, y1, x2, y2, color);
}

void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	ST7789_DrawPixel(x0, y0 + r, color);
	ST7789_DrawPixel(x0, y0 - r, color);
	ST7789_DrawPixel(x0 + r, y0, color);
	ST7789_DrawPixel(x0 - r, y0, color);

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		ST7789_DrawPixel(x0 + x, y0 + y, color);
		ST7789_DrawPixel(x0 - x, y0 + y, color);
		ST7789_DrawPixel(x0 + x, y0 - y, color);
		ST7789_DrawPixel(x0 - x, y0 - y, color);

		ST7789_DrawPixel(x0 + y, y0 + x, color);
		ST7789_DrawPixel(x0 - y, y0 + x, color);
		ST7789_DrawPixel(x0 + y, y0 - x, color);
		ST7789_DrawPixel(x0 - y, y0 - x, color);
	}
}

void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
	if ((x >= LCD_W) || (y >= LCD_H))
		return;
	if ((x + w - 1) >= LCD_W)
		return;
	if ((y + h - 1) >= LCD_H)
		return;

	ST7789_SetWindow(x, y, x + w - 1, y + h - 1);
	ST7789_SendData((uint8_t *)data, sizeof(uint16_t) * w * h);
}

void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor)
{
	uint32_t i, b, j;
	ST7789_SetWindow(x, y, x + font.width - 1, y + font.height - 1);

	for (i = 0; i < font.height; i++) {
		b = font.data[(ch - 32) * font.height + i];
		for (j = 0; j < font.width; j++) {
			if ((b << j) & 0x8000) {
				uint8_t data[] = {color >> 8, color & 0xFF};
				ST7789_SendData(data, sizeof(data));
			}
			else {
				uint8_t data[] = {bgcolor >> 8, bgcolor & 0xFF};
				ST7789_SendData(data, sizeof(data));
			}
		}
	}
}

void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor)
{
	while (*str) {
		if (x + font.width >= LCD_W) {
			x = 0;
			y += font.height;
			if (y + font.height >= LCD_H) {
				break;
			}

			if (*str == ' ') {
				// skip spaces in the beginning of the new line
				str++;
				continue;
			}
		}
		ST7789_WriteChar(x, y, *str, font, color, bgcolor);
		x += font.width;
		str++;
	}
}
void ST7789_WriteStringCentered(uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor) {
	uint16_t w = strlen(str) * font.width;
    uint16_t x = (LCD_W - w) / 2;
    ST7789_WriteString(x, y, str, font, color, bgcolor);
}

void ST7789_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
	/* Draw lines */
	ST7789_DrawLine(x1, y1, x2, y2, color);
	ST7789_DrawLine(x2, y2, x3, y3, color);
	ST7789_DrawLine(x3, y3, x1, y1, color);
}

void ST7789_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color)
{
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
			yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
			curpixel = 0;

	deltax = ABS(x2 - x1);
	deltay = ABS(y2 - y1);
	x = x1;
	y = y1;

	if (x2 >= x1) {
		xinc1 = 1;
		xinc2 = 1;
	}
	else {
		xinc1 = -1;
		xinc2 = -1;
	}

	if (y2 >= y1) {
		yinc1 = 1;
		yinc2 = 1;
	}
	else {
		yinc1 = -1;
		yinc2 = -1;
	}

	if (deltax >= deltay) {
		xinc1 = 0;
		yinc2 = 0;
		den = deltax;
		num = deltax / 2;
		numadd = deltay;
		numpixels = deltax;
	}
	else {
		xinc2 = 0;
		yinc1 = 0;
		den = deltay;
		num = deltay / 2;
		numadd = deltax;
		numpixels = deltay;
	}

	for (curpixel = 0; curpixel <= numpixels; curpixel++) {
		ST7789_DrawLine(x, y, x3, y3, color);

		num += numadd;
		if (num >= den) {
			num -= den;
			x += xinc1;
			y += yinc1;
		}
		x += xinc2;
		y += yinc2;
	}
}

void ST7789_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	ST7789_DrawPixel(x0, y0 + r, color);
	ST7789_DrawPixel(x0, y0 - r, color);
	ST7789_DrawPixel(x0 + r, y0, color);
	ST7789_DrawPixel(x0 - r, y0, color);
	ST7789_DrawLine(x0 - r, y0, x0 + r, y0, color);

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		ST7789_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
		ST7789_DrawLine(x0 + x, y0 - y, x0 - x, y0 - y, color);

		ST7789_DrawLine(x0 + y, y0 + x, x0 - y, y0 + x, color);
		ST7789_DrawLine(x0 + y, y0 - x, x0 - y, y0 - x, color);
	}
}

void ST7789_TearEffect(uint8_t tear)
{
	ST7789_SendCmd(tear ? 0x35 /* TEON */ : 0x34 /* TEOFF */);
}

void ST7789_Test(void)
{
	ST7789_FillColor(WHITE);
	HAL_Delay(1000);
	ST7789_WriteString(10, 20, "Speed Test", Font_11x18, RED, WHITE);
	HAL_Delay(1000);
	ST7789_FillColor(CYAN);
    HAL_Delay(500);
	ST7789_FillColor(RED);
    HAL_Delay(500);
	ST7789_FillColor(BLUE);
    HAL_Delay(500);
	ST7789_FillColor(GREEN);
    HAL_Delay(500);
	ST7789_FillColor(YELLOW);
    HAL_Delay(500);
	ST7789_FillColor(BROWN);
    HAL_Delay(500);
	ST7789_FillColor(DARKBLUE);
    HAL_Delay(500);
	ST7789_FillColor(MAGENTA);
    HAL_Delay(500);
	ST7789_FillColor(LIGHTGREEN);
    HAL_Delay(500);
	ST7789_FillColor(LGRAY);
    HAL_Delay(500);
	ST7789_FillColor(LBBLUE);
    HAL_Delay(500);
	ST7789_FillColor(WHITE);
	HAL_Delay(500);

	ST7789_WriteString(10, 10, "Font test.", Font_16x26, GBLUE, WHITE);
	ST7789_WriteString(10, 50, "Hello Steve!", Font_7x10, RED, WHITE);
	ST7789_WriteString(10, 75, "Hello Steve!", Font_11x18, YELLOW, WHITE);
	ST7789_WriteString(10, 100, "Hello Steve!", Font_16x26, MAGENTA, WHITE);
	HAL_Delay(1000);

	ST7789_FillColor(RED);
	ST7789_WriteString(10, 10, "Rect./Line.", Font_11x18, YELLOW, BLACK);
	ST7789_DrawRectangle(30, 30, 100, 100, WHITE);
	HAL_Delay(1000);

	ST7789_FillColor(RED);
	ST7789_WriteString(10, 10, "Circle.", Font_11x18, YELLOW, BLACK);
	ST7789_DrawCircle(60, 60, 25, WHITE);
	HAL_Delay(1000);

	ST7789_FillColor(RED);
	ST7789_WriteString(10, 10, "Filled Cir.", Font_11x18, YELLOW, BLACK);
	ST7789_DrawFilledCircle(60, 60, 25, WHITE);
	HAL_Delay(1000);

	ST7789_FillColor(RED);
	ST7789_WriteString(10, 10, "Triangle", Font_11x18, YELLOW, BLACK);
	ST7789_DrawTriangle(30, 30, 30, 70, 60, 40, WHITE);
	HAL_Delay(1000);

	ST7789_FillColor(RED);
	ST7789_WriteString(10, 10, "Filled Tri", Font_11x18, YELLOW, BLACK);
	ST7789_DrawFilledTriangle(30, 30, 30, 70, 60, 40, WHITE);
	HAL_Delay(1000);

	//	If FLASH cannot storage anymore datas, please delete codes below.
	ST7789_FillColor(WHITE);
	ST7789_DrawImage(0, 0, 128, 128, (uint16_t *)saber);
	HAL_Delay(3000);
}
/* Draw an arc from start_deg to end_deg (0=right, 90=down),
   thickness = number of concentric circles to draw              */
void ST7789_DrawArc(uint16_t cx, uint16_t cy, uint16_t r, float start_deg, float end_deg, uint16_t color, uint8_t thickness) {
    float step = 1.0f;
    for (float a = start_deg; a <= end_deg; a += step) {
        float rad = a * (float)M_PI / 180.0f;
        for (uint8_t t = 0; t < thickness; t++) {
            uint16_t rr = r + t;
            int16_t x = (int16_t)(cx + rr * cosf(rad));
            int16_t y = (int16_t)(cy + rr * sinf(rad));
            if (x >= 0 && x < LCD_W && y >= 0 && y < LCD_H)
                ST7789_DrawPixel((uint16_t)x, (uint16_t)y, color);
        }
    }
}



