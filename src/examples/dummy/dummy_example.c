/***************************************************************************//**
 *   @file   dummy_example.c
 *   @brief  Simple loopback example for AD5592R.
 *   @author 
********************************************************************************
 * Copyright 2026(c) Analog Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ANALOG DEVICES, INC. BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#include "example.h"
#include "common_data.h"
#include "no_os_delay.h"
#include "no_os_print_log.h"
#include "no_os_uart.h"
#include "ad5592r.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEFINE_AD5592R_IP() { \
 .int_ref = true, \
 .spi_init = &ad5592r_spi_ip, \
 .i2c_init = NULL, \
 .ss_init = &ad5592r_spi_ss_ip, \
 .channel_modes = { \
  CH_MODE_DAC, /* channel 0 */ \
  CH_MODE_ADC, /* channel 1 */ \
  CH_MODE_DAC_AND_ADC, /* channel 2 */ \
  CH_MODE_UNUSED, /* channel 3 */ \
  CH_MODE_UNUSED, /* channel 4 */ \
  CH_MODE_UNUSED, /* channel 5 */ \
  CH_MODE_ADC, /* channel 6 */ \
  CH_MODE_DAC, /* channel 7 */ \
 }, \
 .channel_offstate = { \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 0 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 1 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 2 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 3 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 4 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 5 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 6 */ \
  CH_OFFSTATE_OUT_TRISTATE, /* channel 7 */ \
 }, \
 .adc_range = ZERO_TO_VREF, \
 .dac_range = ZERO_TO_VREF, \
 .adc_buf = false, \
}

int dummy_example(void)
{
 struct ad5592r_dev *ad5592r_dev = NULL;
 struct no_os_uart_desc *uart_desc = NULL;
 uint16_t dac_value = 0; // Mid-scale value
 uint16_t adc_value;
 uint16_t adc_value2;
 uint32_t vref_mv;
 int ret;

 /* Initialize UART for logging */
 ret = no_os_uart_init(&uart_desc, &uart_ip);
 if (ret) {
  return ret;
 }

 char msg_clear[] = "\e[2J\e[H";
 no_os_uart_write(uart_desc, msg_clear, sizeof(msg_clear) - 1);

 char msg_title[] = "AD5592R Loopback Example\n\r";
 no_os_uart_write(uart_desc, msg_title, sizeof(msg_title) - 1);

 char msg_separator[] = "==========================================\n\r\n\r";
 no_os_uart_write(uart_desc, msg_separator, sizeof(msg_separator) - 1);

 /* Initialize AD5592R (SPI device) */
 struct ad5592r_init_param ad5592r_ip = DEFINE_AD5592R_IP();
 ret = ad5592r_init(&ad5592r_dev, &ad5592r_ip);
 if (ret) {
  char msg_err[] = "Failed to initialize AD5592R (SPI)\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }
 char msg_init[] = "AD5592R (SPI) initialized successfully\n\r";
 no_os_uart_write(uart_desc, msg_init, sizeof(msg_init) - 1);

 /* Get reference voltage */
 ret = ad5592r_get_ref(ad5592r_dev, &vref_mv);
 if (ret) {
  char msg_err[] = "Failed to get reference voltage\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }

 uint32_t scale = (vref_mv / (1 << 12) - 1) * 1000;


 while (1) {
  /* Write to DAC on channel 0 */
  ret = ad5592r_write_dac(ad5592r_dev, 0, dac_value);
  if (ret) {
   char msg_err[] = "Failed to write to DAC\n\r";
   no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
   goto cleanup;
  }

  /* Read from ADC on channel 1 */
  ret = ad5592r_read_adc(ad5592r_dev, 1, &adc_value);
  if (ret) {
   char msg_err[] = "Failed to read from ADC\n\r";
   no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
   goto cleanup;
  }
  
  adc_value = adc_value & 0xFFF; // PADD THE VALUE OF THE ADC


  /* Convert ADC value to voltage */
  /* NOTE THAT VREF = 2.5*/
  uint32_t voltage_mv = (((adc_value * vref_mv)) / (4095));

  char buffer[128];
  sprintf(buffer, "CH1 CH0 DAC Value: 0x%04X | Voltage VAL: %d (%ld.%03ld V) | ADC VAL: %d | DAC VAL : %d | Scale: %d |\n\r",
   dac_value, voltage_mv, voltage_mv / 1000, voltage_mv % 1000, adc_value, dac_value, scale);
  no_os_uart_write(uart_desc, buffer, strlen(buffer));


  char seperator[] = "=========================================================================\n\r\n\r";
  no_os_uart_write(uart_desc, seperator, sizeof(seperator) - 1);


  ret = ad5592r_write_dac(ad5592r_dev, 2, dac_value);
  if (ret) {
   char msg_err[] = "Failed to write to DAC\n\r";
   no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
   goto cleanup;
  }

  /* Read from ADC on channel 1 */
  ret = ad5592r_read_adc(ad5592r_dev, 2, &adc_value2);
  if (ret) {
   char msg_err[] = "Failed to read from ADC\n\r";
   no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
   goto cleanup;
  }

  adc_value2 = adc_value2 & 0xFFF; // PADD THE VALUE OF THE ADC

  uint32_t voltage_mv2 = (((adc_value2 * vref_mv)) / (4096));


  char buffer2[128];
  sprintf(buffer2, "CH2 DAC Value: 0x%04X | Voltage VAL: %d (%ld.%03ld V) | ADC VAL: %d | DAC VAL : %d |\n\r",
   dac_value, voltage_mv2, voltage_mv2 / 1000, voltage_mv2 % 1000, adc_value, dac_value);
  no_os_uart_write(uart_desc, buffer2, strlen(buffer2));

  char seperator2[] = "=========================================================================\n\r\n\r";
  no_os_uart_write(uart_desc, seperator2, sizeof(seperator2) - 1);
  /* Increment DAC value for next iteration */
  dac_value = (dac_value + 100) % 4096;

  no_os_mdelay(1000);
 }

cleanup:
 if (ad5592r_dev)
  ad5592r_remove(ad5592r_dev);
 if (uart_desc)
  no_os_uart_remove(uart_desc);

 return ret;
}

example_main("dummy_example");
 