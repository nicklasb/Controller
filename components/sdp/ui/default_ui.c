
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include <ssd1306.h>
#include <font8x8_basic.h>

#include "qrcode.h"

char * default_ui_log_prefix;

void display_qr_code(SSD1306_t *oled, const char * message, uint16_t x_coord, uint16_t y_coord) 
{
	// The structure to manage the QR code
	QRCode qrcode;

	// Allocate a chunk of memory to store the QR code
	uint16_t bufsize = qrcode_getBufferSize(3);
	uint8_t qrcodeData[bufsize];

	qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, message);

	uint8_t dest[574];
	memset(&dest, 0, 574);
	// Each row 72 pixels -> 9 bytes
	// Want whitespace above
	int pos = 27;
	for (int mody=0; mody < 29; mody++) {


		int rowstartpos = pos;
		// Whitespace to the left
		pos++;
		int modx = 0;	
		while (modx < 29) {
			for (int curr_bit = 7; curr_bit > 0; curr_bit = curr_bit - 2) {

				if (qrcode_getModule(&qrcode, modx, mody)) {
					dest[pos] |= 1 << (curr_bit-1);
					dest[pos] |= 1 << (curr_bit);
				}
				modx++;
				if (modx == 29) {
					break;
				}
				
			}
			pos ++;
		}

		memcpy(&dest[rowstartpos + 9], &dest[rowstartpos], 9);    
		pos = rowstartpos + 18;
		//ESP_LOGI("after", "modx: %i,mody: %i, pos: %i", modx, mody, pos);	
	}
    
	ESP_LOGI("after", "Displaying QR-code");	
	ssd1306_bitmaps(oled, x_coord, y_coord, &dest, 72, 64, true);




}

void init_default_ui() {

/*
 You have to set this config value with menuconfig
 CONFIG_INTERFACE
 for i2c
 CONFIG_MODEL
 CONFIG_SDA_GPIO
 CONFIG_SCL_GPIO
 CONFIG_RESET_GPIO
 for SPI
 CONFIG_CS_GPIO
 CONFIG_DC_GPIO
 CONFIG_RESET_GPIO
*/


	SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];

#if CONFIG_I2C_INTERFACE
	ESP_LOGI(default_ui_log_prefix, "INTERFACE is i2c");
	ESP_LOGI(default_ui_log_prefix, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(default_ui_log_prefix, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(default_ui_log_prefix, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
	ESP_LOGI(default_ui_log_prefix, "INTERFACE is SPI");
	ESP_LOGI(default_ui_log_prefix, "CONFIG_MOSI_GPIO=%d",CONFIG_MOSI_GPIO);
	ESP_LOGI(default_ui_log_prefix, "CONFIG_SCLK_GPIO=%d",CONFIG_SCLK_GPIO);
	ESP_LOGI(default_ui_log_prefix, "CONFIG_CS_GPIO=%d",CONFIG_CS_GPIO);
	ESP_LOGI(default_ui_log_prefix, "CONFIG_DC_GPIO=%d",CONFIG_DC_GPIO);
	ESP_LOGI(default_ui_log_prefix, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_SPI_INTERFACE

#if CONFIG_FLIP
	dev._flip = true;
	ESP_LOGW(default_ui_log_prefix, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(default_ui_log_prefix, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);
#endif // CONFIG_SSD1306_128x64
#if CONFIG_SSD1306_128x32
	ESP_LOGI(default_ui_log_prefix, "Panel is 128x32");
	ssd1306_init(&dev, 128, 32);
#endif // CONFIG_SSD1306_128x32

	ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 128);
	display_qr_code(&dev, "https://github.com/RobustoFramework\0", 0, 0);
	vTaskDelay(3000 / portTICK_PERIOD_MS);


	// Restart module
	//esp_restart();
}


void default_ui_init(char *_log_prefix) {
        default_ui_log_prefix = _log_prefix;
    init_default_ui();

}