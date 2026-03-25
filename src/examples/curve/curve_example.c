/***************************************************************************//**
 *   @file   curve_example.c
 *   @brief  Curve tracer example for AD5592R.
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
#include <stdint.h>
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


int curve_example(void)
{
 struct ad5592r_dev *ad5592r_dev = NULL;
 struct no_os_uart_desc *uart_desc = NULL;
 uint16_t dac_value; // Mid-scale value
 float vc;
 float ib;
 float ic;
 uint32_t vref_mv;
 int ret;

 /*   */

 uint32_t Vcsense;
 uint32_t Vcdrive_meas;
 /* Resistor Values / Other stuffs */
 float Rsense = 47.0f;
 float Rbase = 47.0e3f;
 float Vbe = 0.7f;
 float scale =  2.5f;

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





 

 while(1){ 
    /* Intitalize Vb and Vc Drives by 500 / Scale */

    dac_value = 500 / (uint16_t) scale;

    ret = ad5592r_write_dac(ad5592r_dev, 0, dac_value);
    if (ret) {
    char msg_err[] = "Failed to write to DAC\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
    }


    ret = ad5592r_write_dac(ad5592r_dev, 2, dac_value);
    if (ret) {
    char msg_err[] = "Failed to write to DAC\n\r";
    no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
    goto cleanup;
    }

    
    /* VB/RAW = CHANNEL 0, VC.RAW = CHANNEL 2*/
    /* I = FLOAT R = FLOAT V DEPENDS*/
    float curve_vcs[NUM_CURVES][NUM_POINTS];
    float curve_ics[NUM_CURVES][NUM_POINTS];
    int curve_idx = 0;
    
    for (uint16_t vb = 819; vb < 4096; vb = vb + 819){

        /*ITERATION OF MY CALCULATION FOR FORMULA*/
        // uint32_t vbdrive = vb / (uint32_t)scale;
        uint32_t vbdrive = vb;
        // uint32_t vbdrive = (vb * vref_mv) / 4096;

        /*Write Values in the DAC*/
        ret = ad5592r_write_dac(ad5592r_dev, 0, vbdrive);
        if (ret) {
            char msg_err[] = "Failed to write to DAC\n\r";
            no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
            goto cleanup;
        }
        
        ib = ((((float)vbdrive * vref_mv / 1000) - Vbe) / Rbase);
        char curveBuffer[128];
        no_os_mdelay(1000);
        float parsed_ib = ib * 1e6;
        float parsed_base_drive = ((float)vbdrive * scale) / 4096.0f;
        sprintf(curveBuffer,"Base Drive: %0.4f Volts | %0.4f uA | vref: %d \n\r ", parsed_base_drive, parsed_ib, vref_mv);
        int curvelen = strlen(curveBuffer);
        no_os_uart_write(uart_desc, curveBuffer, curvelen);

        int point_idx = 0;

        for(uint16_t vcv = 0; vcv < 4096; vcv = vcv + 81){

            uint32_t Vcsense_raw;
            uint32_t Vcdrive_meas_raw;

            /*ITERATION OF MY CALCULATION FOR FORMULA*/
            // uint32_t vcdrive = vcv / (uint32_t)scale;
            uint32_t vcdrive = vcv;
            // uint32_t vcdrive = (vcv * vref_mv) / 4096;

            ret = ad5592r_write_dac(ad5592r_dev, 2, vcdrive);
            if (ret) {
                char msg_err[] = "Failed to write to DAC\n\r";
                no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
                goto cleanup;
            }


            ret = ad5592r_read_adc(ad5592r_dev, 1, &Vcsense_raw);
            if (ret) {
            char msg_err[] = "Failed to read from ADC\n\r";
            no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
            goto cleanup;
            }

            ret = ad5592r_read_adc(ad5592r_dev, 2, &Vcdrive_meas_raw);
            if (ret) {
            char msg_err[] = "Failed to read from ADC\n\r";
            no_os_uart_write(uart_desc, msg_err, sizeof(msg_err) - 1);
            goto cleanup;
            }
            /*Pad the value*/
            Vcsense = Vcsense_raw & 0x0FFF;
            // Vcdrive_meas = Vcdrive_meas_raw & 0x0FFF;
            Vcdrive_meas = Vcdrive_meas_raw & 0x0FFF;

            
            ic = ((((float)Vcdrive_meas - (float)Vcsense) * vref_mv) / Rsense) / 4096.0f;
            vc = ((float)Vcsense * scale) / 4096.0f;

            curve_vcs[curve_idx][point_idx] = vc;
            curve_ics[curve_idx][point_idx] = ic;
            char curveBuffer2[128];
            no_os_mdelay(100);
            sprintf(curveBuffer2,"coll voltage: %f, coll curre: %f \n\r", vc , ic);
            int curvelen2 = strlen(curveBuffer2);
            no_os_uart_write(uart_desc, curveBuffer2, curvelen2);

            point_idx++;

            }

        curve_idx++;

    }
    // ======================
    /* ASCII ART FUNCTION */
    // ======================


    // char grid[GRAPH_HEIGHT][GRAPH_WIDTH];
    // char ASCII_Buffer[100];
    
    // int grid_h = GRAPH_HEIGHT + 2;
    // int grid_w = GRAPH_WIDTH + 2;
    

    // // LOOP FOR BORDERS
    // for (int y = 0; y < GRAPH_HEIGHT + 2; y++){
    //     for(int x = 0; x < GRAPH_WIDTH + 2; x++){
    //         if (y == 0 || y == GRAPH_WIDTH + 1){
    //             grid[y][x] = '-';
    //         }
    //         if (x == 0 || x == GRAPH_HEIGHT + 1){
    //             grid[y][x] = '|';
    //         } else {
    //             grid[y][x] = ' ';
    //         }
    //     no_os_mdelay(100);
    //     }
    // }

    // // CORNERS
    // grid[0][0] = '+';
    // grid[0][GRAPH_WIDTH + 1] = '+';
    // grid[GRAPH_HEIGHT + 1][0] = '+';
    // grid[grid_h][grid_w] = '+';

    // float max_v = 0.0f;
    // float max_i = 0.0f;

    // // STORE MAX VOLTAGES AND CURRENT
    // for(int c = 0; c < NUM_CURVES; c++){
    //     for(int p = 0; p < NUM_POINTS; p++){
    //         if (curve_vcs[c][p] > max_v){
    //             max_v = curve_vcs[c][p];
    //         }
    //         if (curve_ics[c][p] > max_i){
    //             max_i = curve_ics[c][p];
    //         }
    //     }
    // }
    
    // // CHECK VALUE IF POSITIVE
    // if(max_v < 0.1f){
    //     max_v = 1.0f;
    // }
    // if(max_i < 0.1f){
    //     max_i = 1.0f;
    // }

    // for(int c = 0; c < NUM_CURVES; c++){
    //     for(int p = 0; p < NUM_POINTS; p++){
    //         float v = curve_vcs[c][p];
    //         float i = curve_ics[c][p];

    //         int x = 1 + (int)((v / max_v) * (GRAPH_WIDTH - 1));
    //         int y = 1 + (int)((i / max_i) * (GRAPH_HEIGHT - 1));
    //         int invert_y = GRAPH_HEIGHT - y;

    //         if ((x >= 1 && x <= GRAPH_WIDTH) && (y >= 1 && y <= GRAPH_HEIGHT)){
    //             grid[y][x] = '*';
    //         }
    //     }
    // }

    // no_os_mdelay(100);
    // sprintf(ASCII_Buffer, "\r\n == ASCII Curve Tracer ==\r\n");
    // no_os_uart_write(uart_desc, ASCII_Buffer, strlen(ASCII_Buffer));
    

    // // Print GRID
    // for(int y = 0; y < GRAPH_WIDTH; y++){
    //     no_os_mdelay(100);
    //     no_os_uart_write(uart_desc, (uint8_t*)grid[y], GRAPH_WIDTH + 2);
    //     no_os_uart_write(uart_desc, (uint8_t*)"\r\n", 2);
    // }

    // no_os_mdelay(100);
    // sprintf(ASCII_Buffer, " 0.0");
    // no_os_uart_write(uart_desc, (uint8_t*)ASCII_Buffer, strlen(ASCII_Buffer));

    // float step = max_v / 5.0f;
    // for(int k = 1; k <= 5; k++){
    //     no_os_mdelay(100);
    //     no_os_uart_write(uart_desc, (uint8_t*)"            ", 9);
    //     sprintf(ASCII_Buffer, "%.1f", step * k);
    //     no_os_uart_write(uart_desc, (uint8_t*)ASCII_Buffer, strlen(ASCII_Buffer));

    // }
    // no_os_mdelay(100);
    // no_os_uart_write(uart_desc, (uint8_t*)"\r\n", 2);
    

 no_os_mdelay(1000);
 }


cleanup:
 if (ad5592r_dev)
  ad5592r_remove(ad5592r_dev);
 if (uart_desc)
  no_os_uart_remove(uart_desc);

 return ret;
}

example_main("curve_example");
 