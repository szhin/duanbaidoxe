#ifndef I2C_LCD_H
#define I2C_LCD_H

#include "main.h"

void lcd_init(void);   // Khởi tạo LCD
void lcd_send_cmd(char cmd);  // Gửi lệnh (ví dụ: xóa màn hình, về đầu dòng)
void lcd_send_data(char data); // Gửi 1 ký tự
void lcd_send_string(char *str); // Gửi 1 chuỗi ký tự
void lcd_put_cur(int row, int col); // Đặt con trỏ (hàng, cột)
void lcd_clear(void);  // Xóa màn hình

#endif
