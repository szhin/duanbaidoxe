/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "i2c-lcd.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// --- QUẢN LÝ BÃI XE ---
int tong_so_cho = 4;
int so_cho_hien_tai = 4; // còn trống

// --- TRẠNG THÁI CỔNG 1 (VÀO) ---
uint8_t trang_thai_vao = 0;  // PA2 cảm biến hồng ngoại: 0 là phát hiện xe, 1 là không
uint32_t thoi_gian_cong_vao = 0;

// --- TRẠNG THÁI CỔNG 2 (RA) ---
uint8_t trang_thai_ra = 0;
uint32_t thoi_gian_cong_ra = 0;

// --- CỜ TRẠNG THÁI & ĐÈN ---
uint8_t co_bao_chay = 0; // 0 là ở trạng thái an toàn, 1 chay
uint8_t che_do_den = 0; // 0 auto, 1 Cưỡng bức bật (MAN ON). 2: Cưỡng bức tắt (MAN OFF).
uint32_t thoi_gian_gui_data = 0;

// --- BLUETOOTH CỜ LỆNH & MẬT KHẨU ---
volatile uint8_t lenh_tu_app = 0;
char ky_tu_nhan;
char chuoi_nhan[15];
uint8_t vi_tri_chuoi = 0; // Đóng vai trò như "con trỏ" để biết ký tự tiếp theo sẽ được ghi vào vị trí nào trong mảng chuoi_nhan

// --- BIẾN NGẮT BLUETOOTH TỰ ĐỘNG ---
uint32_t thoi_gian_ngat_app = 0;
uint8_t trang_thai_ngat = 0;
// 0: Trạng thái bình thường, module Bluetooth đang có điện.
// 1: Đang trong quá trình đếm ngược 10 giây để chuẩn bị ngắt.
// 2: Đã ngắt nguồn module (PB11 xuống mức Low) và đang đợi 1 giây để bật lại (Reset kết nối).


extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void Update_LCD_Display() {
    char row1[16];
    char row2[16];

    lcd_clear();
    lcd_put_cur(0, 0);
    sprintf(row1, "TONG CHO: %d", tong_so_cho);
    lcd_send_string(row1);

    lcd_put_cur(1, 0);
    if (so_cho_hien_tai <= 0) {
        lcd_send_string("BAI XE DA DAY !");
    } else {
        sprintf(row2, "CON TRONG: %d", so_cho_hien_tai);
        lcd_send_string(row2);
    }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
   HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    // Ép 2 Servo ở trạng thái Đóng (1500 cho cổng 1, 500 cho cổng 2 - tuỳ chiều đặt servo của bạn)
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1500);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 500);

    // TẮT CÒI NGAY LÚC KHỞI ĐỘNG: PB5 CÒI
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);

    // Ép chân PB11 lên mức High (3.3V) để bật nguồn module Bluetooth PB11: bluetooth
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);

    // Mở cổng nhận Bluetooth lần đầu tiên
    // Giao thức Uart, Lắng nghe ký tự mật khẩu từ App điện thoại gửi qua Bluetooth.
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&ky_tu_nhan, 1);

    lcd_init();
    Update_LCD_Display();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  // ==========================================
	        // 0. XỬ LÝ LỆNH TỪ APP QUẢN LÝ (ADMIN)
	        // ==========================================
	        if (lenh_tu_app == 4) // Lệnh ADMIN Mở Toàn Bộ (Pass: 888888)
	        {
	            lenh_tu_app = 0;
	            lcd_clear(); lcd_put_cur(0, 0); lcd_send_string("ADMIN: MO 2 CONG");

	            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500);
	            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1500);
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);

	            trang_thai_vao = 2; trang_thai_ra = 2;
	            thoi_gian_cong_vao = HAL_GetTick(); thoi_gian_cong_ra = HAL_GetTick();
	        }
	        else if (lenh_tu_app == 5) // Lệnh ADMIN Mở Cổng Vào (Gate 1)
	        {
	            lenh_tu_app = 0;
	            lcd_clear(); lcd_put_cur(0, 0); lcd_send_string("ADMIN: MO CONG 1");

	            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500);
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);

	            trang_thai_vao = 2;
	            thoi_gian_cong_vao = HAL_GetTick();
	        }
	        else if (lenh_tu_app == 6) // Lệnh ADMIN Mở Cổng Ra (Gate 2)
	        {
	            lenh_tu_app = 0;
	            lcd_clear(); lcd_put_cur(0, 0); lcd_send_string("ADMIN: MO CONG 2");

	            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1500);
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);

	            trang_thai_ra = 2;
	            thoi_gian_cong_ra = HAL_GetTick();
	        }
	        else if (lenh_tu_app == 7) // Lệnh ADMIN Đóng 2 Cổng Khẩn Cấp
	        {
	            lenh_tu_app = 0;
	            lcd_clear(); lcd_put_cur(0, 0); lcd_send_string("ADMIN: DONG CONG");

	            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1500); // Hạ G1
	            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 500);  // Hạ G2
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

	            trang_thai_vao = 0; // Reset trạng thái để nhận xe mới
	            trang_thai_ra = 0;

	            HAL_Delay(1500); // Dừng màn hình 1.5s cho dễ đọc
	            Update_LCD_Display();
	        }
	        else if (lenh_tu_app == 2) // Lệnh ADMIN Chuyển Chế Độ Đèn
	        {
	            lenh_tu_app = 0;
	            che_do_den++;
	            if (che_do_den > 2) che_do_den = 0;

	            lcd_clear(); lcd_put_cur(0, 0);
	            if (che_do_den == 0) lcd_send_string("DEN SAN: AUTO   ");
	            else if (che_do_den == 1) lcd_send_string("DEN SAN: MAN ON ");
	            else if (che_do_den == 2) lcd_send_string("DEN SAN: MAN OFF");

	            HAL_Delay(1500);
	            Update_LCD_Display();
	        }
	      // ==========================================
	      // 1. LOGIC CỔNG 1 - ĐI VÀO (QUYỀN USER)
	      // ==========================================
	      if (trang_thai_vao == 0)
	      {
	    	  // reset: phát hiện vật cản
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET && so_cho_hien_tai > 0)
	          {
	              lcd_put_cur(1, 0);
	              lcd_send_string("NHAP PASS APP...");
	              lenh_tu_app = 0;
	              trang_thai_vao = 1;
	          }
	      }

	      else if (trang_thai_vao == 1)
	      {
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET)
	          {
	              trang_thai_vao = 0;
	              Update_LCD_Display();
	          }
	          else if (lenh_tu_app == 1) // QUYỀN USER: NHẬP ĐÚNG PASS (123456)
	          {
	              lenh_tu_app = 0;
	              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500);
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);

	              lcd_put_cur(1, 0);
	              lcd_send_string("PASS DUNG! MO.. ");

	              // Kích hoạt hẹn giờ 10s tự ngắt Bluetooth
	              thoi_gian_ngat_app = HAL_GetTick();
	              trang_thai_ngat = 1;

	              trang_thai_vao = 2;
	          }
	          else if (lenh_tu_app == 3) // NHẬP SAI MẬT KHẨU HOẶC KHÔNG CÓ QUYỀN
	          {
	              lenh_tu_app = 0;
	              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
	              HAL_Delay(100); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); HAL_Delay(100);
	              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
	              HAL_Delay(100); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);

	              lcd_put_cur(1, 0);
	              lcd_send_string("SAI MAT KHAU!!");
	              HAL_Delay(1500);

	              lcd_put_cur(1, 0);
	              lcd_send_string("NHAP PASS APP...");
	          }
	      }

	      // xác nhận xe đã qua cổng an toàn và thực hiện đóng Barie tự động
	      else if (trang_thai_vao == 2)
	      {
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET)
	          {
	              thoi_gian_cong_vao = HAL_GetTick();
	              trang_thai_vao = 3;
	          }
	      }

	      else if (trang_thai_vao == 3)
	      {
	    	  // đảm bảo việc luôn mở nếu hồng ngoại phát hiện đi lùi
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET)
	          {
	              trang_thai_vao = 2;
	          }
	          else if (HAL_GetTick() - thoi_gian_cong_vao >= 1500)
	          {
	              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1500);
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

	              so_cho_hien_tai--;
	              Update_LCD_Display();
	              trang_thai_vao = 0;
	          }
	      }

	      // ==========================================
	      // 2. LOGIC CỔNG 2 - ĐI RA (TỰ ĐỘNG KHÔNG PASS)
	      // ==========================================
	      if (trang_thai_ra == 0)
	      {
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET && so_cho_hien_tai < tong_so_cho)
	          {
	              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1500);
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
	              trang_thai_ra = 1;
	          }
	      }
	      else if (trang_thai_ra == 1)
	      {
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET)
	          {
	              thoi_gian_cong_ra = HAL_GetTick();
	              trang_thai_ra = 2;
	          }
	      }
	      else if (trang_thai_ra == 2)
	      {
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET)
	          {
	              trang_thai_ra = 1;
	          }
	          else if (HAL_GetTick() - thoi_gian_cong_ra >= 1500)
	          {
	              __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 500);
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	              so_cho_hien_tai++;
	              Update_LCD_Display();
	              trang_thai_ra = 0;
	          }
	      }

	      // ==========================================
	      // 3. LOGIC BÁO CHÁY (PA12) VÀ NÚT TẮT (PB9)
	      // ==========================================
	      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_RESET) co_bao_chay = 1;

	      if (co_bao_chay == 1)
	      {
	          lcd_clear();
	          lcd_put_cur(0, 0); lcd_send_string("!!! CO CHAY !!!");
	          lcd_put_cur(1, 0); lcd_send_string(" PB9 DE TAT... ");

	          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500);
	          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1500);

	          while (co_bao_chay == 1)
	          {
	              HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8); // Nháy Đỏ PA8
	              HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5); // Nháy Còi PB5

	              // 200ms, chia thành 20 khoảng, kiểm tra ở từng khoảng xem nút pb9 có được nhấn hay không
	              for (int i = 0; i < 20; i++)
	              {
	                  if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_RESET)
	                  {
	                      co_bao_chay = 0;
	                      break;
	                  }
	                  HAL_Delay(10);
	              }
	          }
	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); // led đỏ tắt
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); // còi tắt

	          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 1500);
	          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 500);

	          trang_thai_vao = 0;
	          trang_thai_ra = 0;
	          Update_LCD_Display();
	      }

	      // ==========================================
	      // 4. NÚT MỞ CỔNG THỦ CÔNG (PB9)
	      // ==========================================
	      if (co_bao_chay == 0 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_RESET)
	      {
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); // bíp để thông báo đã nhận lệnh
	          HAL_Delay(100);
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);

	          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 500);
	          __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 1500);
	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);

	          trang_thai_vao = 2;
	          trang_thai_ra = 2;
	          // Chừng nào người quản lý còn nhấn giữ nút PB9, chương trình sẽ "treo" tại vòng lặp này.
	          // nếu bật lên thì tính toán thời gian ra vào cổng
	          while(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_RESET)
	          {
	              thoi_gian_cong_vao = HAL_GetTick();
	              thoi_gian_cong_ra = HAL_GetTick();
	              HAL_Delay(10);
	          }
	      }

	      // ==========================================
	      // 5. LOGIC ĐÈN SÂN BÃI (PB10) VÀ LDR (PA4)
	      // ==========================================
	      if (che_do_den == 0) // AUTO
	      {
	          if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_SET) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
	          else HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
	      }
	      else if (che_do_den == 1) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);   // MAN ON
	      else if (che_do_den == 2) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET); // MAN OFF

	      // ==========================================
	      // 6. GỬI DATA LÊN APP SAU MỖI 30S
	      // ==========================================
	      if (HAL_GetTick() - thoi_gian_gui_data >= 30000)
	      {
	          thoi_gian_gui_data = HAL_GetTick();
	          char bt_msg[60];
	          sprintf(bt_msg, "=> TONG CHO: %d | CON TRONG: %d \r\n", tong_so_cho, so_cho_hien_tai);
	          HAL_UART_Transmit(&huart1, (uint8_t *)bt_msg, strlen(bt_msg), 100);
	      }

	      // ==========================================
	      // 7. TỰ ĐỘNG "ĐÁ VĂNG" BLUETOOTH SAU 10 GIÂY
	      // ==========================================
	      if (trang_thai_ngat == 1 && (HAL_GetTick() - thoi_gian_ngat_app >= 10000))
	      {
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET); // Cúp điện module BT

	          thoi_gian_ngat_app = HAL_GetTick();
	          trang_thai_ngat = 2;

	          lcd_put_cur(1, 0);
	          lcd_send_string(" DA NGAT BLUET! ");
	      }
	      else if (trang_thai_ngat == 2 && (HAL_GetTick() - thoi_gian_ngat_app >= 1000))
	      {
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET); // Bật điện lại sau 1s
	          trang_thai_ngat = 0;

	          Update_LCD_Display();
	      }

	      HAL_Delay(20);
  }
}
  /* USER CODE END 3 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV8;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 15;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);

  /*Configure GPIO pins : PA2 PA3 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA6 PA7 PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (ky_tu_nhan == '*')
        {
            vi_tri_chuoi = 0;
            memset(chuoi_nhan, 0, sizeof(chuoi_nhan));
        }
        else if (ky_tu_nhan == '#')
        {
            chuoi_nhan[vi_tri_chuoi] = '\0';

            // --- KIỂM TRA MÃ TỪ APP ---
            if (strcmp(chuoi_nhan, "123456") == 0) lenh_tu_app = 1;         // USER: Mở cổng 1
            else if (strcmp(chuoi_nhan, "888888") == 0) lenh_tu_app = 4;    // ADMIN: Mở 2 cổng
            else if (strcmp(chuoi_nhan, "888888DEN") == 0) lenh_tu_app = 2; // ADMIN: Chỉnh đèn
            else if (strcmp(chuoi_nhan, "888888IN") == 0) lenh_tu_app = 5;  // ADMIN: Mở Cổng Vào
            else if (strcmp(chuoi_nhan, "888888OUT") == 0) lenh_tu_app = 6; // ADMIN: Mở Cổng Ra
            else if (strcmp(chuoi_nhan, "888888CLOSE") == 0) lenh_tu_app = 7;// ADMIN: Đóng 2 cổng
            else lenh_tu_app = 3; // SAI PASS (Tự động còi kêu và báo lỗi ở mục 1)

            vi_tri_chuoi = 0;
        }
        else
        {
            if (vi_tri_chuoi < 14)
            {
                chuoi_nhan[vi_tri_chuoi] = ky_tu_nhan;
                vi_tri_chuoi++;
            }
        }
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&ky_tu_nhan, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UART_Receive_IT(huart, (uint8_t *)&ky_tu_nhan, 1);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
