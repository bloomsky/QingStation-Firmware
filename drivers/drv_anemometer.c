/*
 * Copyright (c) 2020-2021, Jianjia Ma
 * majianjia@live.com
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author           Notes
 * 2021-02-06     Jianjia Ma       the first version
 */

#include <rtthread.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include <rtdevice.h>
#include <board.h>
#include "drv_anemometer.h"

#define DBG_TAG "drv_ane"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

}

// ADC init
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc2;
DMA_HandleTypeDef hdma_tim3_ch1_trig;

// when use cubemx to configure, copy the MX function in cubemx/core/main.c to here after every configuration.
static void MX_ADC2_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  hadc2.Instance = ADC2;
   hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
   hadc2.Init.Resolution = ADC_RESOLUTION_12B;
   hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
   hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
   hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
   hadc2.Init.LowPowerAutoWait = DISABLE;
   hadc2.Init.ContinuousConvMode = ENABLE;
   hadc2.Init.NbrOfConversion = 1;
   hadc2.Init.DiscontinuousConvMode = DISABLE;
   hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
   hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
   hadc2.Init.DMAContinuousRequests = ENABLE;
   hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
   hadc2.Init.OversamplingMode = DISABLE;
   if (HAL_ADC_Init(&hadc2) != HAL_OK)
   {
     Error_Handler();
   }
   /** Configure Regular Channel
   */
   sConfig.Channel = ADC_CHANNEL_1;
   sConfig.Rank = ADC_REGULAR_RANK_1;
   sConfig.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
   sConfig.SingleDiff = ADC_SINGLE_ENDED;
   sConfig.OffsetNumber = ADC_OFFSET_NONE;
   sConfig.Offset = 0;
   if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
   {
     Error_Handler();
   }
}

// timer initialization
TIM_HandleTypeDef htim3;

void TIM3_Init(uint32_t frequency)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = SystemCoreClock/frequency/100 - 1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100;              // this is not 100-1
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 100;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH; // must not change
  sConfigOC.OCFastMode = TIM_OCFAST_ENABLE; //TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim3);

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

}



// enable the power for all analog sections.
// user code need to wait until signal and power supplies are stable.
void ane_pwr_control(uint32_t freq, bool flag)
{
    if(flag)
    {
        MX_DMA_Init();

        // set up the analog switch
        rt_pin_mode(SW_PIN_EN, PIN_MODE_OUTPUT);
        rt_pin_mode(SW_PIN_A, PIN_MODE_OUTPUT);
        rt_pin_mode(SW_PIN_B, PIN_MODE_OUTPUT);

        rt_pin_write(SW_PIN_EN, 1);
        rt_pin_write(SW_PIN_EN, 0);

        // ADC setting.
        MX_ADC2_Init();

        HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

        // timer
        TIM3_Init(freq);

    }else {
        // disable ADC
        HAL_ADC_DeInit(&hadc2);
        //timer
        HAL_TIM_Base_DeInit(&htim3);
    }
    rt_pin_write(SW_PIN_EN, !flag);
    rt_pin_write(SW_PIN_A, GPIO_PIN_RESET);
    rt_pin_write(SW_PIN_B, GPIO_PIN_RESET);
}

// analog switch
void set_output_channel(ULTRASONIC_CHANNEL ch)
{
    // Although we can map the ch to the pin directly, we do an obvious way.
    // Again, when the receiver is also selected on the opposite of transmitter
    switch(ch){
    case NORTH:
        rt_pin_write(SW_PIN_A, GPIO_PIN_RESET);
        rt_pin_write(SW_PIN_B, GPIO_PIN_RESET);
        break;
    case EAST:
        rt_pin_write(SW_PIN_A, GPIO_PIN_SET);
        rt_pin_write(SW_PIN_B, GPIO_PIN_RESET);
        break;
    case SOUTH:
        rt_pin_write(SW_PIN_A, GPIO_PIN_RESET);
        rt_pin_write(SW_PIN_B, GPIO_PIN_SET);
        break;
    case WEST:
        rt_pin_write(SW_PIN_A, GPIO_PIN_SET);
        rt_pin_write(SW_PIN_B, GPIO_PIN_SET);
        break;
    default: break;
    }
}

// send a sequence of pulse at frequency and the num of size.
static void send_pulse(uint16_t* pulses, uint16_t length)
{
    // the length here is in bytes.
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t*)pulses, length * sizeof(uint16_t));
}
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM3)
         HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_1);
}

// start ADC sampling.
static void start_sampling(uint16_t* buffer, uint32_t length)
{
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)buffer, length);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC2)
        HAL_ADC_Stop_DMA(&hadc2);
}

bool ane_check_busy()
{
    return ((HAL_ADC_GetState(&hadc2) & HAL_ADC_STATE_REG_BUSY) != 0UL);
}

// input: the RECEIVER side that willing to take the sample.
int adc_sample(ULTRASONIC_CHANNEL ch, uint16_t* adc_buf, uint32_t adc_len)
{
    // convert the receiver channel to transmitter channel
    switch(ch){
    case NORTH: ch=SOUTH; break;
    case SOUTH: ch=NORTH; break;
    case WEST: ch=EAST; break;
    case EAST: ch=WEST; break;
    }
    set_output_channel(ch);
    rt_thread_delay(10);
    start_sampling(adc_buf, adc_len);
    return 0;
}



// test functions
// send a pulse from CH side, then sample the opposite side ADC data. NORTH -> SOUTH
int ane_measure_ch(ULTRASONIC_CHANNEL ch, uint16_t *pulse, uint16_t pulse_len, uint16_t* adc_buf, uint32_t adc_len)
{
    set_output_channel(ch);
    // see if delay needed. and see if need to perform zero_level sampling here.
    rt_thread_delay(10);

    rt_enter_critical();
    send_pulse(pulse, pulse_len);
    start_sampling(adc_buf, adc_len);
    rt_exit_critical();

    // user need to wait for the ADC sampling.
    rt_thread_delay(2);
    return 0;
}



void DMA1_Channel2_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel2_IRQn 0 */

  /* USER CODE END DMA1_Channel2_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc2);
  /* USER CODE BEGIN DMA1_Channel2_IRQn 1 */

  /* USER CODE END DMA1_Channel2_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel6 global interrupt.
  */
void DMA1_Channel6_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel6_IRQn 0 */

  /* USER CODE END DMA1_Channel6_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_tim3_ch1_trig);
  /* USER CODE BEGIN DMA1_Channel6_IRQn 1 */

  /* USER CODE END DMA1_Channel6_IRQn 1 */
}

/**
  * @brief This function handles ADC1 and ADC2 interrupts.
  */
void ADC1_2_IRQHandler(void)
{
    extern ADC_HandleTypeDef hadc1;
  /* USER CODE BEGIN ADC1_2_IRQn 0 */

  /* USER CODE END ADC1_2_IRQn 0 */
  HAL_ADC_IRQHandler(&hadc1);
  HAL_ADC_IRQHandler(&hadc2);
  /* USER CODE BEGIN ADC1_2_IRQn 1 */

  /* USER CODE END ADC1_2_IRQn 1 */
}

void TIM3_IRQHandler(void)
{
  /* USER CODE BEGIN TIM3_IRQn 0 */

  /* USER CODE END TIM3_IRQn 0 */
  HAL_TIM_IRQHandler(&htim3);
  /* USER CODE BEGIN TIM3_IRQn 1 */

  /* USER CODE END TIM3_IRQn 1 */
}

