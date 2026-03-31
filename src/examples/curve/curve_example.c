/***************************************************************************//**
 *   @file   basic_example.c
 *   @brief  BJT Curve tracer example for AD5592R (NPN) and AD5593R (PNP).
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
#include "ad5593r.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DEFINE_AD5592R_IP() { \
 .int_ref = true, \
 .spi_init = &ad5592r_spi_ip, \
 .i2c_init = NULL, \
 .ss_init = &ad5592r_spi_ss_ip, \
 .channel_modes = { \
  CH_MODE_DAC,         /* channel 0 */ \
  CH_MODE_ADC,         /* channel 1 */ \
  CH_MODE_DAC_AND_ADC, /* channel 2 */ \
  CH_MODE_UNUSED,      /* channel 3 */ \
  CH_MODE_UNUSED,      /* channel 4 */ \
  CH_MODE_UNUSED,      /* channel 5 */ \
  CH_MODE_UNUSED,      /* channel 6 */ \
  CH_MODE_UNUSED,      /* channel 7 */ \
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

#define DEFINE_AD5593R_IP() { \
 .int_ref = true, \
 .spi_init = NULL, \
 .i2c_init = &ad5593r_i2c_ip, \
 .ss_init = NULL, \
 .channel_modes = { \
   CH_MODE_DAC, /* channel 0 - Base drive */ \
   CH_MODE_ADC, /* channel 1 - Collector sense */ \
   CH_MODE_DAC_AND_ADC, /* channel 2 - Collector drive */ \
   CH_MODE_DAC, /* channel 3 - Emitter drive (PNP needs high-side supply) */ \
   CH_MODE_UNUSED, /* channel 4 */ \
   CH_MODE_UNUSED, /* channel 5 */ \
   CH_MODE_UNUSED, /* channel 6 */ \
   CH_MODE_UNUSED, /* channel 7 */ \
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

#define NUM_CURVES 5
#define NUM_POINTS 50

/*ASCII VALUES*/

#define GRAPH_HEIGHT 20
#define GRAPH_WIDTH  60


int ad5592r_curve_example(struct no_os_uart_desc *uart_desc)
{
 struct ad5592r_dev *ad5592r_dev = NULL;
 uint16_t dac_value; // Mid-scale value
 float vc;
 float ib;
 float ic;
 uint32_t vref_mv;
 float mV_per_lsb;  // Scale factor - matches Python's scale property
 int ret;

 /* Circuit parameters */
 uint16_t Vcsense_raw;
 uint16_t Vcdrive_meas_raw;

 /* Resistor values (matching Python exactly) */
 float Rsense = 47.0f;      // Collector sense resistor (47 ohms)
 float Rbase = 47.0e3f;     // Base resistor (47 kOhms)
 float Vbe = 0.7f;          // Estimated base-emitter voltage drop

 char msg_title[] = "\n\r========== AD5592R (SPI) NPN Curve Tracer ==========\n\r";
 no_os_uart_write(uart_desc, msg_title, sizeof(msg_title) - 1);

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

 /* Calculate scale factor (mV per LSB) - matches Python's .scale property */
 /* In IIO: scale_factor = vref_mv / (1 << AD559XR_ADC_RESOLUTION) where resolution=12 */
 mV_per_lsb = (float)vref_mv / 4096.0f;

 char scale_msg[64];
 sprintf(scale_msg, "Vref: %u mV, Scale: %.4f mV/LSB\n\r", vref_mv, mV_per_lsb);
 no_os_uart_write(uart_desc, scale_msg, strlen(scale_msg));

 /* Read temperature from channel 8 */
 uint16_t temp_raw;
 ret = ad5592r_read_adc(ad5592r_dev, 8, &temp_raw);
 if (ret == 0) {
  temp_raw &= 0x0FFF;
  /* Temperature calculation using IIO formula: T = (raw + offset) * scale */
  /* From Linux kernel ad5592r-base.c and IIO driver */
  /* scale = (vref_mv * 150715900.52) / 1e9 in milli-degrees per LSB */
  /* offset = (-75365 * 25) / vref_mv for ZERO_TO_VREF range */
  float scale = ((float)vref_mv * 150715900.52f) / 1000000000.0f;
  float offset = (-75365.0f * 25.0f) / (float)vref_mv;
  float temperature = ((float)temp_raw + offset) * scale / 1000.0f; /* Convert mC to C */
  char temp_msg[64];
  sprintf(temp_msg, "AD5592R Temperature: %.2f°C (raw=%u)\n\r", temperature, temp_raw);
  no_os_uart_write(uart_desc, temp_msg, strlen(temp_msg));
 } else {
  char temp_err[] = "Failed to read temperature\n\r";
  no_os_uart_write(uart_desc, temp_err, sizeof(temp_err) - 1);
 }

 /* Initialize DACs to 500mV (matching Python: Vbdrive.raw = 500.0 / mV_per_lsb) */
 dac_value = (uint16_t)(500.0f / mV_per_lsb);

 ret = ad5592r_write_dac(ad5592r_dev, 0, dac_value);
 if (ret) {
  char msg_err[] = "Failed to write to DAC channel 0\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }

 ret = ad5592r_write_dac(ad5592r_dev, 2, dac_value);
 if (ret) {
  char msg_err[] = "Failed to write to DAC channel 2\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }

 /* Arrays to store curve data */
 /* VB = CHANNEL 0 (DAC), VC = CHANNEL 2 (DAC+ADC), Vsense = CHANNEL 1 (ADC) */
 float curve_vcs[NUM_CURVES][NUM_POINTS];
 float curve_ics[NUM_CURVES][NUM_POINTS];
 int curve_idx = 0;

 char msg_sweep[] = "\n\rStarting curve sweep...\n\r";
 no_os_uart_write(uart_desc, msg_sweep, sizeof(msg_sweep) - 1);

 /* Sweep base voltage: 499 to 2500 mV in 500 mV steps (matching Python) */
 for (uint16_t vb_mv = 499; vb_mv < 2500; vb_mv += 500) {

  /* Convert mV to DAC code using scale factor (matches Python: Vbdrive.raw = vb / mV_per_lsb) */
  uint16_t vbdrive_raw = (uint16_t)(vb_mv / mV_per_lsb);

  /* Write base drive to DAC */
  ret = ad5592r_write_dac(ad5592r_dev, 0, vbdrive_raw);
  if (ret) {
   char msg_err[] = "Failed to write to DAC channel 0\n\r";
   no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
   goto cleanup;
  }

  /* Allow base current to settle */
  no_os_mdelay(50);

  /* Calculate base voltage and current (matching Python logic) */
  float vb_voltage = (vbdrive_raw * mV_per_lsb) / 1000.0f;  // Convert to volts
  ib = (vb_voltage - Vbe) / Rbase;  // Base current in amps

  char curveBuffer[128];
  float parsed_ib = ib * 1e6;  // Convert to microamps
  sprintf(curveBuffer, "Base Drive:  %.11f  Volts,  %.10f  uA\n\r", vb_voltage, parsed_ib);
  int curvelen = strlen(curveBuffer);
  no_os_uart_write(uart_desc, curveBuffer, curvelen);

  int point_idx = 0;

  /* Sweep collector voltage: 0 to 2500 mV in 50 mV steps (matching Python) */
  for (uint16_t vc_mv = 0; vc_mv < 2500; vc_mv += 50) {

   /* Convert mV to DAC code (matches Python: Vcdrive.raw = vcv / mV_per_lsb) */
   uint16_t vcdrive_raw = (uint16_t)(vc_mv / mV_per_lsb);

   ret = ad5592r_write_dac(ad5592r_dev, 2, vcdrive_raw);
   if (ret) {
    char msg_err[] = "Failed to write to DAC channel 2\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
   }

   /* Small delay for settling */
   no_os_mdelay(10);

   /* Read ADC channels */
   ret = ad5592r_read_adc(ad5592r_dev, 1, &Vcsense_raw);
   if (ret) {
    char msg_err[] = "Failed to read ADC channel 1\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
   }

   ret = ad5592r_read_adc(ad5592r_dev, 2, &Vcdrive_meas_raw);
   if (ret) {
    char msg_err[] = "Failed to read ADC channel 2\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
   }

   /* Mask to 12-bit values */
   Vcsense_raw &= 0x0FFF;
   Vcdrive_meas_raw &= 0x0FFF;

   /* Calculate collector current using scale factor (matches Python exactly) */
   /* Python: ic = (Vcdrive_meas.raw - Vcsense.raw) * mV_per_lsb / Rsense */
   /* Result is in mA because mV/ohm = mA */
   ic = ((float)(Vcdrive_meas_raw - Vcsense_raw) * mV_per_lsb) / Rsense;

   /* Collector voltage is the sense voltage */
   /* Python: vc = Vcsense.raw * mV_per_lsb / 1000.0 */
   vc = (Vcsense_raw * mV_per_lsb) / 1000.0f;  // Convert to volts

   curve_vcs[curve_idx][point_idx] = vc;
   curve_ics[curve_idx][point_idx] = ic;

   /* Output every point to match Python output */
   char curveBuffer2[128];
   sprintf(curveBuffer2, "coll voltage:  %.11f   coll curre:  %.16f\n\r", vc, ic);
   int curvelen2 = strlen(curveBuffer2);
   no_os_uart_write(uart_desc, curveBuffer2, curvelen2);

   point_idx++;
  }

  /* Curve completed */
  char msg_curve_done[] = "\n\r";
  no_os_uart_write(uart_desc, msg_curve_done, sizeof(msg_curve_done) - 1);

  curve_idx++;
 }
 /* ======================
  * ASCII ART FUNCTION
  * ====================== */

 char grid[GRAPH_HEIGHT + 2][GRAPH_WIDTH + 2];
 char ASCII_Buffer[128];

 /* Initialize grid with spaces */
 for (int y = 0; y < GRAPH_HEIGHT + 2; y++) {
  for (int x = 0; x < GRAPH_WIDTH + 2; x++) {
   grid[y][x] = ' ';
  }
 }

 /* Draw borders */
 for (int x = 0; x < GRAPH_WIDTH + 2; x++) {
  grid[0][x] = '-';
  grid[GRAPH_HEIGHT + 1][x] = '-';
 }
 for (int y = 0; y < GRAPH_HEIGHT + 2; y++) {
  grid[y][0] = '|';
  grid[y][GRAPH_WIDTH + 1] = '|';
 }

 /* Draw corners */
 grid[0][0] = '+';
 grid[0][GRAPH_WIDTH + 1] = '+';
 grid[GRAPH_HEIGHT + 1][0] = '+';
 grid[GRAPH_HEIGHT + 1][GRAPH_WIDTH + 1] = '+';

 /* Find max voltage and current */
 float max_v = 0.0f;
 float max_i = 0.0f;

 for (int c = 0; c < NUM_CURVES; c++) {
  for (int p = 0; p < NUM_POINTS; p++) {
   if (curve_vcs[c][p] > max_v) {
    max_v = curve_vcs[c][p];
   }
   if (curve_ics[c][p] > max_i) {
    max_i = curve_ics[c][p];
   }
  }
 }

 /* Ensure non-zero ranges */
 if (max_v < 0.01f) max_v = 1.0f;
 if (max_i < 0.001f) max_i = 0.01f;

 /* Plot curves */
 for (int c = 0; c < NUM_CURVES; c++) {
  for (int p = 0; p < NUM_POINTS; p++) {
   float v = curve_vcs[c][p];
   float i = curve_ics[c][p];

   /* Map to grid coordinates */
   int x = 1 + (int)((v / max_v) * (GRAPH_WIDTH - 1));
   int y = 1 + (int)((i / max_i) * (GRAPH_HEIGHT - 1));

   /* Invert y-axis (0 at bottom) */
   y = GRAPH_HEIGHT - y;

   if (x >= 1 && x <= GRAPH_WIDTH && y >= 1 && y <= GRAPH_HEIGHT) {
    grid[y][x] = '*';
   }
  }
 }

 /* Print the graph */
 no_os_mdelay(100);
 sprintf(ASCII_Buffer, "\r\n\r\n === AD5592R (SPI) - NPN Curve Tracer (Ic vs Vc) ===\r\n");
 no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));

 sprintf(ASCII_Buffer, "Y-axis: Ic (0 to %.2f mA)\r\n", max_i);
 no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));

 sprintf(ASCII_Buffer, "X-axis: Vc (0 to %.2f V)\r\n\r\n", max_v);
 no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));

 /* Print grid row by row */
 for (int y = 0; y < GRAPH_HEIGHT + 2; y++) {
  no_os_uart_write(uart_desc, (uint8_t*)grid[y], GRAPH_WIDTH + 2);
  no_os_uart_write(uart_desc, (uint8_t*)"\r\n", 2);
 }

 /* Print X-axis labels */
 sprintf(ASCII_Buffer, "0.0");
 no_os_uart_write(uart_desc, (uint8_t*)ASCII_Buffer, strlen(ASCII_Buffer));

 float step = max_v / 5.0f;
 for (int k = 1; k <= 5; k++) {
  no_os_uart_write(uart_desc, (uint8_t*)"       ", 7);
  sprintf(ASCII_Buffer, "%.2f", (double)(step * k));
  no_os_uart_write(uart_desc, (uint8_t*)ASCII_Buffer, strlen(ASCII_Buffer));
 }
 no_os_uart_write(uart_desc, (uint8_t*)" V\r\n\r\n", 6);

 char msg_complete[] = "===== AD5592R Curve Trace Complete =====\n\r\n\r";
 no_os_uart_write(uart_desc, (const uint8_t *)msg_complete, sizeof(msg_complete) - 1);

 /* Output CSV data for Excel */
 char csv_header[] = "\n\r=== CSV DATA START: AD5592R_NPN ===\n\r";
 no_os_uart_write(uart_desc, csv_header, sizeof(csv_header) - 1);

 char csv_col_header[] = "Curve,Ib_uA,Vc_V,Ic_mA\n\r";
 no_os_uart_write(uart_desc, csv_col_header, sizeof(csv_col_header) - 1);

 /* Output all curve data in CSV format */
 for (int c = 0; c < NUM_CURVES; c++) {
  /* Calculate Ib for this curve */
  uint16_t vb_mv = 499 + (c * 500);
  float vb_voltage = (((uint16_t)(vb_mv / mV_per_lsb)) * mV_per_lsb) / 1000.0f;
  float ib_ua = ((vb_voltage - Vbe) / Rbase) * 1e6;

  for (int p = 0; p < NUM_POINTS; p++) {
   char csv_line[128];
   float vc_v = curve_vcs[c][p];
   float ic_ma = curve_ics[c][p];  // Already in mA
   sprintf(csv_line, "%d,%.4f,%.4f,%.4f\n\r", c + 1, ib_ua, vc_v, ic_ma);
   no_os_uart_write(uart_desc, csv_line, strlen(csv_line));
  }
 }

 char csv_footer[] = "=== CSV DATA END ===\n\r\n\r";
 no_os_uart_write(uart_desc, csv_footer, sizeof(csv_footer) - 1);

 no_os_mdelay(1000);


cleanup:
 if (ad5592r_dev)
 ad5592r_remove(ad5592r_dev);

 return ret;
}

int ad5593r_curve_example(struct no_os_uart_desc *uart_desc)
{
 struct ad5592r_dev *ad5593r_dev = NULL;
 uint16_t dac_value;
 float vc;
 float ib;
 float ic;
 uint32_t vref_mv;
 float mV_per_lsb;
 int ret;

 /* Circuit parameters */
 uint16_t Vcsense_raw;
 uint16_t Vcdrive_meas_raw;

 /* Resistor values (matching Python exactly) */
 float Rsense = 47.0f;
 float Rbase = 47.0e3f;
 float Vbe = 0.7f;

 char msg_title[] = "\n\r========== AD5593R (I2C) PNP Curve Tracer ==========\n\r";
 no_os_uart_write(uart_desc, msg_title, sizeof(msg_title) - 1);

 /* Initialize AD5593R (I2C device) */
 struct ad5592r_init_param ad5593r_ip = DEFINE_AD5593R_IP();
 ret = ad5593r_init(&ad5593r_dev, &ad5593r_ip);
 if (ret) {
  char msg_err[] = "Failed to initialize AD5593R (I2C)\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }
 char msg_init[] = "AD5593R (I2C) initialized successfully\n\r";
 no_os_uart_write(uart_desc, msg_init, sizeof(msg_init) - 1);

 /* Get reference voltage */
 ret = ad5592r_get_ref(ad5593r_dev, &vref_mv);
 if (ret) {
  char msg_err[] = "Failed to get reference voltage\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }

 /* Calculate scale factor (mV per LSB) */
 mV_per_lsb = (float)vref_mv / 4096.0f;

 char scale_msg[64];
 sprintf(scale_msg, "Vref: %u mV, Scale: %.4f mV/LSB\n\r", vref_mv, mV_per_lsb);
 no_os_uart_write(uart_desc, scale_msg, strlen(scale_msg));

 /* Read temperature from channel 8 */
 uint16_t temp_raw;
 ret = ad5593r_read_adc(ad5593r_dev, 8, &temp_raw);
 if (ret == 0) {
  temp_raw &= 0x0FFF;
  /* Temperature calculation using IIO formula: T = (raw + offset) * scale */
  /* From Linux kernel ad5592r-base.c and IIO driver */
  /* scale = (vref_mv * 150715900.52) / 1e9 in milli-degrees per LSB */
  /* offset = (-75365 * 25) / vref_mv for ZERO_TO_VREF range */
  float scale = ((float)vref_mv * 150715900.52f) / 1000000000.0f;
  float offset = (-75365.0f * 25.0f) / (float)vref_mv;
  float temperature = ((float)temp_raw + offset) * scale / 1000.0f; /* Convert mC to C */
  char temp_msg[64];
  sprintf(temp_msg, "AD5593R Temperature: %.2f°C (raw=%u)\n\r", temperature, temp_raw);
  no_os_uart_write(uart_desc, temp_msg, strlen(temp_msg));
 } else {
  char temp_err[] = "Failed to read temperature\n\r";
  no_os_uart_write(uart_desc, temp_err, sizeof(temp_err) - 1);
 }

 /* Set Emitter drive to 2.5V for PNP operation (channel 3) */
 uint16_t vedrive_raw = (uint16_t)(2499.0f / mV_per_lsb);
 ret = ad5593r_write_dac(ad5593r_dev, 3, vedrive_raw);
 if (ret) {
  char msg_err[] = "Failed to write to DAC channel 3 (Emitter)\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }
 char msg_vedrive[128];
 sprintf(msg_vedrive, "Emitter drive (CH3) set to %u counts (%.2f mV)\n\r", vedrive_raw, vedrive_raw * mV_per_lsb);
 no_os_uart_write(uart_desc, msg_vedrive, strlen(msg_vedrive));

 /* Initialize DACs to 500mV */
 dac_value = (uint16_t)(500.0f / mV_per_lsb);

 char debug_init[128];
 sprintf(debug_init, "Initializing Base/Collector DACs to %u counts (%.2f mV)\n\r", dac_value, dac_value * mV_per_lsb);
 no_os_uart_write(uart_desc, debug_init, strlen(debug_init));

 ret = ad5593r_write_dac(ad5593r_dev, 0, dac_value);
 if (ret) {
  char msg_err[] = "Failed to write to DAC channel 0\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }
 char msg_dac0[] = "DAC channel 0 written successfully\n\r";
 no_os_uart_write(uart_desc, msg_dac0, sizeof(msg_dac0) - 1);

 ret = ad5593r_write_dac(ad5593r_dev, 2, dac_value);
 if (ret) {
  char msg_err[] = "Failed to write to DAC channel 2\n\r";
  no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
  goto cleanup;
 }
 char msg_dac2[] = "DAC channel 2 written successfully\n\r";
 no_os_uart_write(uart_desc, msg_dac2, sizeof(msg_dac2) - 1);

 /* Verify by reading back channel 2 (DAC+ADC mode) */
 uint16_t readback_raw;
 no_os_mdelay(10);  // Let DAC settle
 ret = ad5593r_read_adc(ad5593r_dev, 2, &readback_raw);
 if (ret == 0) {
 readback_raw &= 0x0FFF;
 float readback_v = (readback_raw * mV_per_lsb) / 1000.0f;
 sprintf(debug_init, "DAC CH2 readback: raw=%u, voltage=%.4fV\n\r", readback_raw, readback_v);
 no_os_uart_write(uart_desc, debug_init, strlen(debug_init));
 }

 /* DAC Test Sweep - verify DAC is responding */
 char msg_dac_test[] = "\n\rDAC Test Sweep (verifying DAC functionality):\n\r";
 no_os_uart_write(uart_desc, msg_dac_test, sizeof(msg_dac_test) - 1);
 for (uint16_t test_val = 0; test_val <= 4095; test_val += 1024) {
 ret = ad5593r_write_dac(ad5593r_dev, 2, test_val);
 if (ret) continue;
 no_os_mdelay(10);
 ret = ad5593r_read_adc(ad5593r_dev, 2, &readback_raw);
 if (ret == 0) {
  readback_raw &= 0x0FFF;
  float expected_v = (test_val * mV_per_lsb) / 1000.0f;
  float actual_v = (readback_raw * mV_per_lsb) / 1000.0f;
  sprintf(debug_init, "  DAC=%u (%.3fV) -> ADC=%u (%.3fV) | diff=%.3fV\n\r",
    test_val, expected_v, readback_raw, actual_v, actual_v - expected_v);
  no_os_uart_write(uart_desc, debug_init, strlen(debug_init));
 }
 }

 /* Arrays to store curve data */
 float curve_vcs[NUM_CURVES][NUM_POINTS];
 float curve_ics[NUM_CURVES][NUM_POINTS];
 int curve_idx = 0;

 char msg_sweep[] = "\n\rStarting PNP curve sweep (sweeping HIGH to LOW)...\n\r";
 no_os_uart_write(uart_desc, msg_sweep, sizeof(msg_sweep) - 1);

 /* Sweep base voltage: 2000mV down to 500mV in -500mV steps (PNP operation) */
 for (int16_t vb_mv = 2000; vb_mv > 0; vb_mv -= 500) {

  uint16_t vbdrive_raw = (uint16_t)(vb_mv / mV_per_lsb);

  ret = ad5593r_write_dac(ad5593r_dev, 0, vbdrive_raw);
  if (ret) {
   char msg_err[] = "Failed to write to DAC channel 0\n\r";
   no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
   goto cleanup;
  }

  no_os_mdelay(50);

  float vb_voltage = (vbdrive_raw * mV_per_lsb) / 1000.0f;
  ib = (vb_voltage - Vbe) / Rbase;

  char curveBuffer[128];
  float parsed_ib = ib * 1e6;
  sprintf(curveBuffer, "Base Drive:  %.11f  Volts,  %.10f  uA\n\r", vb_voltage, parsed_ib);
  int curvelen = strlen(curveBuffer);
  no_os_uart_write(uart_desc, curveBuffer, curvelen);

  /* For PNP, fill array in reverse order so graph displays Vc increasing left-to-right */
  int point_idx = NUM_POINTS - 1;

  /* Sweep collector voltage: 2499mV down to 50mV in -50mV steps (PNP operation) */
  for (int16_t vc_mv = 2499; vc_mv > 0; vc_mv -= 50) {

   uint16_t vcdrive_raw = (uint16_t)(vc_mv / mV_per_lsb);

   ret = ad5593r_write_dac(ad5593r_dev, 2, vcdrive_raw);
   if (ret) {
    char msg_err[] = "Failed to write to DAC channel 2\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
   }

   no_os_mdelay(10);

   ret = ad5593r_read_adc(ad5593r_dev, 1, &Vcsense_raw);
   if (ret) {
    char msg_err[] = "Failed to read ADC channel 1\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
   }

   ret = ad5593r_read_adc(ad5593r_dev, 2, &Vcdrive_meas_raw);
   if (ret) {
    char msg_err[] = "Failed to read ADC channel 2\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
   }

   Vcsense_raw &= 0x0FFF;
   Vcdrive_meas_raw &= 0x0FFF;

   /* Use same formula as Python: (Vcdrive_meas - Vcsense) for both NPN and PNP */
   /* Result is in mA because mV/ohm = mA */
   ic = ((float)(Vcdrive_meas_raw - Vcsense_raw) * mV_per_lsb) / Rsense;
   vc = (Vcsense_raw * mV_per_lsb) / 1000.0f;

   curve_vcs[curve_idx][point_idx] = vc;
   curve_ics[curve_idx][point_idx] = ic;

   /* Output every point to match Python output */
   char curveBuffer2[128];
   sprintf(curveBuffer2, "coll voltage:  %.11f   coll curre:  %.16f\n\r", vc, ic);
   int curvelen2 = strlen(curveBuffer2);
   no_os_uart_write(uart_desc, curveBuffer2, curvelen2);

   point_idx--;
  }

  char msg_curve_done[] = "\n\r";
  no_os_uart_write(uart_desc, msg_curve_done, sizeof(msg_curve_done) - 1);

  curve_idx++;
 }

 /* ASCII Graph */
 char grid[GRAPH_HEIGHT + 2][GRAPH_WIDTH + 2];
 char ASCII_Buffer[128];

 for (int y = 0; y < GRAPH_HEIGHT + 2; y++) {
 for (int x = 0; x < GRAPH_WIDTH + 2; x++) {
  grid[y][x] = ' ';
 }
 }

 for (int x = 0; x < GRAPH_WIDTH + 2; x++) {
 grid[0][x] = '-';
 grid[GRAPH_HEIGHT + 1][x] = '-';
 }
 for (int y = 0; y < GRAPH_HEIGHT + 2; y++) {
 grid[y][0] = '|';
 grid[y][GRAPH_WIDTH + 1] = '|';
 }

 grid[0][0] = '+';
 grid[0][GRAPH_WIDTH + 1] = '+';
 grid[GRAPH_HEIGHT + 1][0] = '+';
 grid[GRAPH_HEIGHT + 1][GRAPH_WIDTH + 1] = '+';

 float max_v = 0.0f;
 float max_i = 0.0f;

 /* Find max using absolute values for PNP (negative currents) */
 for (int c = 0; c < NUM_CURVES; c++) {
 for (int p = 0; p < NUM_POINTS; p++) {
  if (curve_vcs[c][p] > max_v) {
 max_v = curve_vcs[c][p];
  }
  float abs_i = curve_ics[c][p] < 0 ? -curve_ics[c][p] : curve_ics[c][p];
  if (abs_i > max_i) {
 max_i = abs_i;
  }
 }
 }

 if (max_v < 0.01f) max_v = 1.0f;
 if (max_i < 0.001f) max_i = 0.01f;

 /* Plot using absolute values for PNP display */
 for (int c = 0; c < NUM_CURVES; c++) {
 for (int p = 0; p < NUM_POINTS; p++) {
  float v = curve_vcs[c][p];
  float i = curve_ics[c][p] < 0 ? -curve_ics[c][p] : curve_ics[c][p];

  int x = 1 + (int)((v / max_v) * (GRAPH_WIDTH - 1));
  int y = 1 + (int)((i / max_i) * (GRAPH_HEIGHT - 1));

  

  if (x >= 1 && x <= GRAPH_WIDTH && y >= 1 && y <= GRAPH_HEIGHT) {
 grid[y][x] = '*';
  }
 }
 }

 no_os_mdelay(100);
 sprintf(ASCII_Buffer, "\r\n\r\n === AD5593R (I2C) - PNP Curve Tracer (Ic vs Vc) ===\r\n");
 no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));

 sprintf(ASCII_Buffer, "Y-axis: |Ic| (0 to %.2f mA)\r\n", max_i);
 no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));

 sprintf(ASCII_Buffer, "X-axis: Vc (0 to %.2f V)\r\n\r\n", max_v);
 no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));

 for (int y = 0; y < GRAPH_HEIGHT + 2; y++) {
 no_os_uart_write(uart_desc, (uint8_t*)grid[y], GRAPH_WIDTH + 2);
 no_os_uart_write(uart_desc, (uint8_t*)"\r\n", 2);
 }

 sprintf(ASCII_Buffer, "0.0");
 no_os_uart_write(uart_desc, (uint8_t*)ASCII_Buffer, strlen(ASCII_Buffer));

 float step = max_v / 5.0f;
 for (int k = 1; k <= 5; k++) {
 no_os_uart_write(uart_desc, (uint8_t*)"       ", 7);
 sprintf(ASCII_Buffer, "%.2f", step * k);
 no_os_uart_write(uart_desc, (uint8_t*)ASCII_Buffer, strlen(ASCII_Buffer));
 }
 no_os_uart_write(uart_desc, (uint8_t*)" V\r\n\r\n", 6);

 char msg_complete[] = "===== AD5593R Curve Trace Complete =====\n\r\n\r";
 no_os_uart_write(uart_desc, msg_complete, sizeof(msg_complete) - 1);

 /* Output CSV data for Excel */
 char csv_header[] = "\n\r=== CSV DATA START: AD5593R_PNP ===\n\r";
 no_os_uart_write(uart_desc, csv_header, sizeof(csv_header) - 1);

 char csv_col_header[] = "Curve,Ib_uA,Vc_V,Ic_mA\n\r";
 no_os_uart_write(uart_desc, csv_col_header, sizeof(csv_col_header) - 1);

 /* Output all curve data in CSV format */
 for (int c = 0; c < NUM_CURVES; c++) {
 /* Calculate Ib for this curve (PNP sweeps from 2000mV down in -500mV steps) */
 int16_t vb_mv = 2000 - (c * 500);
 float vb_voltage = (((uint16_t)(vb_mv / mV_per_lsb)) * mV_per_lsb) / 1000.0f;
 float ib_ua = ((vb_voltage - Vbe) / Rbase) * 1e6;

 for (int p = 0; p < NUM_POINTS; p++) {
  char csv_line[128];
  float vc_v = curve_vcs[c][p];
  float ic_ma = curve_ics[c][p];  // Already in mA
  sprintf(csv_line, "%d,%.4f,%.4f,%.4f\n\r", c + 1, ib_ua, vc_v, ic_ma);
  no_os_uart_write(uart_desc, csv_line, strlen(csv_line));
 }
 }

 char csv_footer[] = "=== CSV DATA END ===\n\r\n\r";
 no_os_uart_write(uart_desc, csv_footer, sizeof(csv_footer) - 1);

 no_os_mdelay(1000);

cleanup:
 if (ad5593r_dev)
 ad5593r_remove(ad5593r_dev);

 return ret;
}

int curve_example(void)
{
 struct no_os_uart_desc *uart_desc = NULL;
 int ret;

 /* Initialize UART for logging */
 ret = no_os_uart_init(&uart_desc, &uart_ip);
 if (ret) {
 return ret;
 }

 /* Add delay to ensure UART connection is fully established */
 no_os_mdelay(2000);

 char msg_clear[] = "\e[2J\e[H";
 no_os_uart_write(uart_desc, msg_clear, sizeof(msg_clear) - 1);

 char msg_header[] = "========================================\n\r";
 no_os_uart_write(uart_desc, msg_header, sizeof(msg_header) - 1);
 char msg_title[] = "  Dual Device BJT Curve Tracer Demo\n\r";
 no_os_uart_write(uart_desc, msg_title, sizeof(msg_title) - 1);
 no_os_uart_write(uart_desc, msg_header, sizeof(msg_header) - 1);

 /* Run AD5592R curve tracer (SPI) */
 ret = ad5592r_curve_example(uart_desc);
 if (ret) {
 char msg_err[] = "\n\rAD5592R curve tracer failed!\n\r";
 no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
 goto cleanup;
 }

 /* Add delay between tests */
 no_os_mdelay(2000);

 /* Run AD5593R curve tracer (I2C) */
 ret = ad5593r_curve_example(uart_desc);
 if (ret) {
 char msg_err[] = "\n\rAD5593R curve tracer failed!\n\r";
 no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
 goto cleanup;
 }

cleanup:
 if (uart_desc)
 no_os_uart_remove(uart_desc);

 return ret;
}

example_main("curve_example");
 