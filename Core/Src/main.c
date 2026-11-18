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
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "font.h"
#include "stdio.h"
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

/* USER CODE BEGIN PV */
volatile uint32_t TIM_num = 0;                    //定时器计数
volatile uint8_t TIM_num_remainder = 0;           //记录对定时器计数取余结果
volatile uint16_t CCR = 75;                       //记录CCR
volatile float pulse = 0;                         //记录脉冲宽度
volatile uint8_t angle = 0;                       //记录当前角度
char TARGET[50] = {};                             //拼接角度
char ccr[50] = {};                                //拼接CCR
char PULSE[50] = {};                              //拼接脉冲宽度
static char Transmit_Data[50] = {};               //记录发送数据
static char Receive_Data[50] = {};                //记录接受数据

typedef enum {
  BUTTON_PRESS,           //按下
  BUTTON_RELEASE,         //松开
}BUTTON_STATE;            //枚举按键状态

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  BUTTON_STATE state;            // 去抖后的稳定状态
  uint32_t debounce_time;
  uint32_t debounce_ms;
  uint8_t last_read;             // 上次原始采样电平
  uint16_t num_state1;
  volatile uint8_t press_event;  // 稳定按下时置 1
} BUTTON_DATA;

BUTTON_DATA button_list[] = {
  {GPIOB,BUTTON1_Pin,BUTTON_RELEASE,0,15,OFF,0,0},
  {GPIOB,BUTTON2_Pin,BUTTON_RELEASE,0,15,OFF,0,0},
  {GPIOB,BUTTON3_Pin,BUTTON_RELEASE,0,15,OFF,0,0},
};
uint8_t button_num = sizeof(button_list)/sizeof(BUTTON_DATA);  //记录按键个数
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void PWM(uint32_t num1) {               //改变脉冲宽度以到不同角度
  TIM_num_remainder = num1 % 5;
  angle = TIM_num_remainder * (180 / 4);
  CCR = 75 + (375 - 75) * TIM_num_remainder/ 4;
  pulse = (float)angle / 9.00;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, CCR);
}

void BUTTON_debounce(void)
{
  for (uint8_t i = 0; i < button_num; i++) {
    BUTTON_DATA *key = &button_list[i];
    uint8_t now_level = HAL_GPIO_ReadPin(key->port, key->pin);

    if (now_level != key->last_read) {
      key->last_read = now_level;
      key->debounce_time = 0;
      continue;
    }

    if (key->debounce_time < key->debounce_ms) {
      key->debounce_time += 5;
      if (key->debounce_time < key->debounce_ms) {
        continue;
      }
    }

    if (now_level == ON && key->state != BUTTON_PRESS) {
      key->state = BUTTON_PRESS;
      key->press_event = 1;
    } else if (now_level == OFF && key->state != BUTTON_RELEASE) {
      key->state = BUTTON_RELEASE;
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {

  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM1) {
    BUTTON_debounce();
  }
  if (htim->Instance == TIM2) {
    PWM(TIM_num);
    TIM_num ++;
    HAL_UART_Transmit_DMA(&huart1, Transmit_Data, sizeof(Transmit_Data));
  }
}
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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim1);
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_Delay(20);
  OLED_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (angle < 0) {
      angle = 0;
    }
    if (angle > 180) {
      angle = 180;
    }

    sprintf(TARGET, "target: %d deg", angle);
    sprintf(ccr, "CCR: %d", CCR);
    sprintf(PULSE, "pulse: %.4lf", pulse);
    sprintf(Transmit_Data,"target: %d deg，CCR: %d，pulse: %.4lf",angle,CCR,pulse);

    BUTTON_DATA *key1 = &button_list[0];
    BUTTON_DATA *key2 = &button_list[1];
    BUTTON_DATA *key3 = &button_list[2];

    if (key1->press_event) {
      key1->press_event = 0;
      key1->num_state1 ^= 1;
    }

    if (key1->num_state1 == 1) {
      HAL_TIM_Base_Stop_IT(&htim2);
    } else {
      HAL_TIM_Base_Start_IT(&htim2);
    }

    if (key1->num_state1 == 1 && key2->press_event) {
      key2->press_event = 0;
      if (angle >= 15) {
        angle -= 15;
        CCR = 75 + (375 - 75) * angle / 180;
        pulse = (float)angle / 9.00;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, CCR);
      }
      HAL_UART_Transmit_DMA(&huart1, Transmit_Data, sizeof(Transmit_Data));
    }


    if (key1->num_state1 == 1 && key3->press_event) {
      key3->press_event = 0;
      if (angle <= 165) {
        angle += 15;
        CCR = 75 + (375 - 75) * angle / 180;
        pulse = (float)angle / 9.00;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, CCR);
      }
      HAL_UART_Transmit_DMA(&huart1, Transmit_Data, sizeof(Transmit_Data));
    }

    OLED_NewFrame();

    OLED_PrintString(0,0,TARGET,&font16x16,OLED_COLOR_NORMAL);
    OLED_PrintString(0,16,ccr,&font16x16,OLED_COLOR_NORMAL);
    OLED_PrintString(0,32,PULSE,&font16x16,OLED_COLOR_NORMAL);

    OLED_ShowFrame();
    HAL_Delay(100);
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
