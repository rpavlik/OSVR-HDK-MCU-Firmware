/*
 * Copyright 2016 OSVR and contributors.
 * Copyright 2016 Dennis Yeh.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "HDK2.h"
#include "Console.h"
#include "string.h"
#include "ui.h"
#include "uart.h"
#include "delay.h"
#include "nxp/AVRHDMI.h"
#include "my_hardware.h"
#include "DeviceDrivers/Solomon.h"
#include "DeviceDrivers/BNO070.h"

#include <stdio.h>
#include "stdarg.h"

#define NXP_1_ADDR 0x48
#define Waitx1ms delay_ms
#define Waitx1us delay_us
#define AUO_INIT_DELAY_US 1000
#define EDID_ADDR_YEAR 0x11
#define EDID_ADDR_WEEK 0x10
#define EDID_ADDR_SN_0 0x0C
#define EDID_ADDR_SN_1 0x0D
#define EDID_ADDR_SN_2 0x0E
#define EDID_ADDR_SN_3 0x0F
#define EDID_ADDR_CS_0 0x7F
#define EDID_ADDR_CS_1 0xFF

#define CORE_KEY "Ctlh618#"
#define CORE_KEY_LENGTH 8
#define CORE_KEY_RETRY_TIME 3

static uint8_t core_key_pass = 0;

const unsigned char EDID_LUT[256] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x06, 0xAF, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x03, 0x80, 0x0D, 0x0F, 0x78, 0xEA, 0xA8, 0x7D, 0xAB, 0x54, 0x3C, 0xB6, 0x21, 0x10, 0x4C, 0x4F, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xA4, 0x6A, 0x70,
    0x56, 0x81, 0xB0, 0x0C, 0x40, 0x5C, 0x05, 0x63, 0x00, 0x82, 0x96, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xFF,
    0x00, 0x56, 0x30, 0x30, 0x31, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC, 0x00,
    0x4F, 0x53, 0x56, 0x52, 0x20, 0x48, 0x44, 0x4B, 0x32, 0x0A, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x14,
    0x78, 0x01, 0xFF, 0x1D, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xBC,

    0x02, 0x03, 0x0C, 0x30, 0x41, 0x00, 0x65, 0x03, 0x0C, 0x00, 0x10, 0x00, 0xA4, 0x6A, 0x70, 0x56, 0x81, 0xB0, 0x0C,
    0x40, 0x5C, 0x05, 0x63, 0x00, 0x82, 0x96, 0x00, 0x00, 0x00, 0x18, 0xA4, 0x6A, 0x70, 0x56, 0x81, 0xB0, 0x0C, 0x40,
    0x5C, 0x05, 0x63, 0x00, 0x82, 0x96, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70,
};

int TC358870_i2c_Init(void)
{
	static int tmpResult = -1;
	twi_master_options_t opt_TC358870;

	opt_TC358870.speed = TC358870_TWI_SPEED;
	opt_TC358870.chip = TC358870_ADDR;
	tmpResult = twi_master_setup(TWI_TC358870_PORT, &opt_TC358870);
	if (tmpResult != STATUS_OK)
		return TC358870_ERROR;

	return TC358870_OK;
}

int TC358870_i2c_Read(uint16_t RegNum, uint8_t *data)
{
	static status_code_t nResult = 0;
	uint8_t data_received[2] = {0x00, 0x00};

	twi_package_t packet_read = {
	    .addr[0] = (uint8_t)((RegNum >> 8) & 0xFF),  // TWI slave memory address data
	    .addr[1] = (uint8_t)(RegNum & 0xFF),         // TWI slave memory address data
	    .addr_length = sizeof(uint16_t),             // TWI slave memory address data size
	    .chip = TC358870_ADDR,                       // TWI slave bus address
	    .buffer = data_received,                     // transfer data source buffer
	    .length = sizeof(data_received)              // transfer data size (bytes)
	};

	nResult = twi_master_read(TWI_TC358870_PORT, &packet_read);

	if (nResult == STATUS_OK)
	{
		*data = data_received[0];
		return TC358870_OK;
	}

	return TC358870_ERROR;
}

int TC358870_i2c_Write(uint16_t RegNum, uint32_t nValue, int nLength)
{
	static status_code_t nResult = 0;
	uint8_t sendData[4];

	memset(sendData, 0x00, sizeof(uint8_t *) * 4);
	if (nLength == 1)
	{
		sendData[0] = (uint8_t)nValue;
	}
	else if (nLength == 2)
	{
		sendData[0] = (uint8_t)(nValue & 0xFF);
		sendData[1] = (uint8_t)((nValue >> 8) & 0xFF);
	}
	else if (nLength == 4)
	{
		sendData[0] = (uint8_t)(nValue & 0xFF);
		sendData[1] = (uint8_t)((nValue >> 8) & 0xFF);
		sendData[2] = (uint8_t)((nValue >> 16) & 0xFF);
		sendData[3] = (uint8_t)((nValue >> 24) & 0xFF);
	}
	else
		return TC358870_ERROR;

	twi_package_t packet = {
	    .addr[0] = (uint8_t)((RegNum >> 8) & 0xFF),  // TWI slave memory address data
	    .addr[1] = (uint8_t)(RegNum & 0xFF),         // TWI slave memory address data
	    .addr_length = sizeof(uint16_t),             // TWI slave memory address data size
	    .chip = TC358870_ADDR,                       // TWI slave bus address
	    .buffer = sendData,                          // transfer data source buffer
	    .length = nLength,                           // transfer data size (bytes)
	    .no_wait = true                              // return immediately if not available
	};

	nResult = twi_master_write(TWI_TC358870_PORT, &packet);
	if (nResult == STATUS_OK)
		return TC358870_OK;

	return TC358870_ERROR;
}

void debugPrintf(const char *format, ...)
{
	char buffer[256];

	va_list ap;
	va_start(ap, format);
	vsprintf(buffer, format, ap);
	WriteLn(buffer);
	va_end(ap);
}

// LCD Initialization --
int AUO_H381DLN01_Init(int bDisplayON)
{
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x07FE, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xEC00, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xEC0B, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xEC16, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xEC21, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xEC2D, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xBAA9, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x06AB, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x84BB, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x1CBC, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x08FE, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x1A07, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x0AFE, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x1B2A, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x0DFE, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x6502, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x414D, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x0F4B, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xFE53, 2);  // DCSCMD_Q // change command
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x00FE, 2);  // DCSCMD_Q
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x03C2, 2);  // DCSCMD_Q 03C2 -> 0BC2
	delay_us(AUO_INIT_DELAY_US);
	TC358870_i2c_Write(0x0504, 0x0015, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0xFF51, 2);  // DCSCMD_Q
	delay_ms(2);

	// Exit Sleep
	TC358870_i2c_Write(0x0504, 0x0005, 2);  // DCSCMD_Q
	TC358870_i2c_Write(0x0504, 0x0011, 2);  // DCSCMD_Q

	if (bDisplayON != 0)
	{
		delay_ms(200);
		TC358870_i2c_Write(0x0504, 0x0005, 2);  // DCSCMD_Q
		TC358870_i2c_Write(0x0504, 0x0029, 2);  // DCSCMD_Q
		delay_ms(1);
		// bDisplayON = 0;
	}
}

/*
        Function: Checking video is existing or not by polling method.
        In: none
        Out: VIDEO_EXIST / VIDEO_NOT_EXIST.

*/
static void VideoStatusHandler(bool video_status)
{
	static int bTCPwrInit = 1;

	if (video_status == VIDEO_NOT_EXIST)
	{  // NO VIDEO
		AUO_H381DLN01_Reset();

		UpdateResolutionDetection();
		Update_BNO_Report_Header();
		ioport_set_value(Debug_LED, DEBUG_LED_OFF);
	}
	else
	{                                             // VIDEO IS READY
		ioport_set_value(TC358870_Reset_Pin, 0);  // TC358870 reset active
		delay_ms(50);
		ioport_set_value(TC358870_Reset_Pin, 1);  // TC358870 reset active
		delay_ms(5);

		TC358870_Init_Receive_HDMI_Signal();  // re-initial TC358870 if HDMI unplug.

		UpdateResolutionDetection();
		Update_BNO_Report_Header();

		ioport_set_value(Debug_LED, DEBUG_LED_ON);
	}
}

/*
        Function: Checking video is existing or not by polling method.
        In: none
        Out: VIDEO_EXIST / VIDEO_NOT_EXIST.

*/
bool IsVideoExistingPolling(void)
{
	uint16_t new_cnt;
	static uint16_t cnt = 0;
	static bool last_video_status = VIDEO_NOT_EXIST;
	bool status;

	new_cnt = tc_read_count(&VIDEO_POLLING_TIMER);

	if (new_cnt - cnt > VIDEO_POLLING_PERIOD)
	{
		if ((status = HDMI_IsVideoExisting()) != last_video_status)
		{  // status is changed...
			last_video_status = status;
			VideoStatusHandler(status);
		}
		cnt = new_cnt;
	}

	return status;
}

/*
        Function: Power on sequence.
        In: 0: sucess / 1: fail
        Out: none

*/
bool PowerOnSeq(void)
{
	while (ioport_get_value(PWR_GOOD_2V5) == 0)
	{  // waiting for power good.
		// time out
		delay_us(50);
	}

	delay_ms(100);

	ioport_set_value(TC358870_Reset_Pin, 0);  // TC358870 reset active
	ioport_set_value(PANEL_RESET, 0);         // AUO panel reset active

	delay_ms(50);

	ioport_set_value(TC358870_Reset_Pin, 1);  // TC358870 reset active
	ioport_set_value(PANEL_RESET, 1);         // AUO panel reset active

	delay_ms(5);
	// AUO_H381DLN01_Reset();
	TC358870_Init_Receive_HDMI_Signal();

	return 0;
}

void AUO_H381DLN01_Reset(void)
{
	ioport_set_pin_low(PANEL_RESET);
	delay_ms(10);
	ioport_set_pin_high(PANEL_RESET);
	delay_ms(1);
}

void Toshiba_TC358870_Reset(void)
{
	ioport_set_value(TC358870_Reset_Pin, 0);  // TC358870 reset active
	delay_ms(50);
	ioport_set_value(TC358870_Reset_Pin, 1);  // TC358870 reset active
	delay_ms(1);
}

int TC358870_Reset_MIPI(void)
{
	uint8_t tc_data;

	AUO_H381DLN01_Reset();

	if (TC358870_i2c_Read(0x0004, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	tc_data = tc_data & 0xFFFC;
	TC358870_i2c_Write(0x0004, tc_data, 2);
	TC358870_i2c_Write(0x0002, 0x0200, 2);
	TC358870_i2c_Write(0x0002, 0x0000, 2);

	delay_ms(150);

	// DSI-TX0 Transition Timing
	TC358870_i2c_Write(0x0108, 0x00000001, 4);  // DSI_TX_CLKEN
	TC358870_i2c_Write(0x010C, 0x00000001, 4);  // DSI_TX_CLKSEL
	TC358870_i2c_Write(0x02A0, 0x00000001, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x02AC, 0x000090B0, 4);  // MIPI_PLL_CNF
	delay_ms(1);

	TC358870_i2c_Write(0x02A0, 0x00000003, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x0118, 0x00000014, 4);  // LANE_ENABLE
	TC358870_i2c_Write(0x0120, 0x00001770, 4);  // LINE_INIT_COUNT
	TC358870_i2c_Write(0x0124, 0x00000000, 4);  // HSTX_TO_COUNT
	TC358870_i2c_Write(0x0128, 0x00000101, 4);  // FUNC_ENABLE
	TC358870_i2c_Write(0x0130, 0x00010000, 4);  // DSI_TATO_COUNT
	TC358870_i2c_Write(0x0134, 0x00005000, 4);  // DSI_PRESP_BTA_COUNT
	TC358870_i2c_Write(0x0138, 0x00010000, 4);  // DSI_PRESP_LPR_COUNT
	TC358870_i2c_Write(0x013C, 0x00010000, 4);  // DSI_PRESP_LPW_COUNT
	TC358870_i2c_Write(0x0140, 0x00010000, 4);  // DSI_PRESP_HSR_COUNT
	TC358870_i2c_Write(0x0144, 0x00010000, 4);  // DSI_PRESP_HSW_COUNT
	TC358870_i2c_Write(0x0148, 0x00001000, 4);  // DSI_PR_TO_COUNT
	TC358870_i2c_Write(0x014C, 0x00010000, 4);  // DSI_LRX-H_TO_COUNT
	TC358870_i2c_Write(0x0150, 0x00000160, 4);  // FUNC_MODE
	TC358870_i2c_Write(0x0154, 0x00000001, 4);  // DSI_RX_VC_ENABLE
	TC358870_i2c_Write(0x0158, 0x000000C8, 4);  // IND_TO_COUNT
	TC358870_i2c_Write(0x0168, 0x0000002A, 4);  // DSI_HSYNC_STOP_COUNT
	TC358870_i2c_Write(0x0170, 0x000003CB, 4);  // APF_VDELAYCNT
	TC358870_i2c_Write(0x017C, 0x00000081, 4);  // DSI_TX_MODE
	TC358870_i2c_Write(0x018C, 0x00000001, 4);  // DSI_HSYNC_WIDTH
	TC358870_i2c_Write(0x0190, 0x0000016C, 4);  // DSI_HBPR
	TC358870_i2c_Write(0x01A4, 0x00000000, 4);  // DSI_RX_STATE_INT_MASK
	TC358870_i2c_Write(0x01C0, 0x00000015, 4);  // DSI_LPRX_THRESH_COUNT
	TC358870_i2c_Write(0x0214, 0x00000000, 4);  // APP_SIDE_ERR_INT_MASK
	TC358870_i2c_Write(0x021C, 0x00000080, 4);  // DSI_RX_ERR_INT_MASK
	TC358870_i2c_Write(0x0224, 0x00000000, 4);  // DSI_LPTX_INT_MASK
	TC358870_i2c_Write(0x0254, 0x00000006, 4);  // LPTXTIMECNT
	TC358870_i2c_Write(0x0258, 0x00240204, 4);  // TCLK_HEADERCNT
	TC358870_i2c_Write(0x025C, 0x000D0008, 4);  // TCLK_TRAILCNT
	TC358870_i2c_Write(0x0260, 0x00140006, 4);  // THS_HEADERCNT
	TC358870_i2c_Write(0x0264, 0x00004268, 4);  // TWAKEUPCNT
	TC358870_i2c_Write(0x0268, 0x0000000F, 4);  // TCLK_POSTCNT
	TC358870_i2c_Write(0x026C, 0x000D0008, 4);  // THS_TRAILCNT
	TC358870_i2c_Write(0x0270, 0x00000020, 4);  // HSTXVREGCNT
	TC358870_i2c_Write(0x0274, 0x0000001F, 4);  // HSTXVREGEN
	TC358870_i2c_Write(0x0278, 0x00060005, 4);  // BTA_COUNT
	TC358870_i2c_Write(0x027C, 0x00000002, 4);  // DPHY_TX ADJUST
	TC358870_i2c_Write(0x011C, 0x00000001, 4);  // DSITX_START

	// DSI-TX1 Transition Timing
	TC358870_i2c_Write(0x0308, 0x00000001, 4);  // DSI_TX_CLKEN
	TC358870_i2c_Write(0x030C, 0x00000001, 4);  // DSI_TX_CLKSEL
	TC358870_i2c_Write(0x04A0, 0x00000001, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x04AC, 0x000090B0, 4);  // MIPI_PLL_CNF
	Waitx1ms(1);

	TC358870_i2c_Write(0x04A0, 0x00000003, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x0318, 0x00000014, 4);  // LANE_ENABLE
	TC358870_i2c_Write(0x0320, 0x00001770, 4);  // LINE_INIT_COUNT
	TC358870_i2c_Write(0x0324, 0x00000000, 4);  // HSTX_TO_COUNT
	TC358870_i2c_Write(0x0328, 0x00000101, 4);  // FUNC_ENABLE
	TC358870_i2c_Write(0x0330, 0x00010000, 4);  // DSI_TATO_COUNT
	TC358870_i2c_Write(0x0334, 0x00005000, 4);  // DSI_PRESP_BTA_COUNT
	TC358870_i2c_Write(0x0338, 0x00010000, 4);  // DSI_PRESP_LPR_COUNT
	TC358870_i2c_Write(0x033C, 0x00010000, 4);  // DSI_PRESP_LPW_COUNT
	TC358870_i2c_Write(0x0340, 0x00010000, 4);  // DSI_PRESP_HSR_COUNT
	TC358870_i2c_Write(0x0344, 0x00010000, 4);  // DSI_PRESP_HSW_COUNT
	TC358870_i2c_Write(0x0348, 0x00001000, 4);  // DSI_PR_TO_COUNT
	TC358870_i2c_Write(0x034C, 0x00010000, 4);  // DSI_LRX-H_TO_COUNT
	TC358870_i2c_Write(0x0350, 0x00000160, 4);  // FUNC_MODE
	TC358870_i2c_Write(0x0354, 0x00000001, 4);  // DSI_RX_VC_ENABLE
	TC358870_i2c_Write(0x0358, 0x000000C8, 4);  // IND_TO_COUNT
	TC358870_i2c_Write(0x0368, 0x0000002A, 4);  // DSI_HSYNC_STOP_COUNT
	TC358870_i2c_Write(0x0370, 0x000003CB, 4);  // APF_VDELAYCNT
	TC358870_i2c_Write(0x037C, 0x00000081, 4);  // DSI_TX_MODE
	TC358870_i2c_Write(0x038C, 0x00000001, 4);  // DSI_HSYNC_WIDTH
	TC358870_i2c_Write(0x0390, 0x0000016C, 4);  // DSI_HBPR
	TC358870_i2c_Write(0x03A4, 0x00000000, 4);  // DSI_RX_STATE_INT_MASK
	TC358870_i2c_Write(0x03C0, 0x00000015, 4);  // DSI_LPRX_THRESH_COUNT
	TC358870_i2c_Write(0x0414, 0x00000000, 4);  // APP_SIDE_ERR_INT_MASK
	TC358870_i2c_Write(0x041C, 0x00000080, 4);  // DSI_RX_ERR_INT_MASK
	TC358870_i2c_Write(0x0424, 0x00000000, 4);  // DSI_LPTX_INT_MASK
	TC358870_i2c_Write(0x0454, 0x00000006, 4);  // LPTXTIMECNT
	TC358870_i2c_Write(0x0458, 0x00240204, 4);  // TCLK_HEADERCNT
	TC358870_i2c_Write(0x045C, 0x000D0008, 4);  // TCLK_TRAILCNT
	TC358870_i2c_Write(0x0460, 0x00140006, 4);  // THS_HEADERCNT
	TC358870_i2c_Write(0x0464, 0x00004268, 4);  // TWAKEUPCNT
	TC358870_i2c_Write(0x0468, 0x0000000F, 4);  // TCLK_POSTCNT
	TC358870_i2c_Write(0x046C, 0x000D0008, 4);  // THS_TRAILCNT
	TC358870_i2c_Write(0x0470, 0x00000020, 4);  // HSTXVREGCNT
	TC358870_i2c_Write(0x0474, 0x0000001F, 4);  // HSTXVREGEN
	TC358870_i2c_Write(0x0478, 0x00060005, 4);  // BTA_COUNT
	TC358870_i2c_Write(0x047C, 0x00000002, 4);  // DPHY_TX ADJUST
	TC358870_i2c_Write(0x031C, 0x00000001, 4);  // DSITX_START

	// Command Transmission Before Video Start
	TC358870_i2c_Write(0x0500, 0x0004, 2);      // CMD_SEL
	TC358870_i2c_Write(0x0110, 0x00000016, 4);  // MODE_CONFIG
	TC358870_i2c_Write(0x0310, 0x00000016, 4);  // MODE_CONFIG

	AUO_H381DLN01_Init(1);

	if (TC358870_i2c_Read(0x0004, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	tc_data = tc_data | 0x0003;
	TC358870_i2c_Write(0x0004, tc_data, 2);
}

int TC358870_CheckLANEStatus(void)
{
	uint8_t tc_data;

	if (TC358870_i2c_Read(0x0290, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	if (!(tc_data & 0x8F))
		return TC358870_ERROR;

	if (TC358870_i2c_Read(0x0490, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	if (!(tc_data & 0x8F))
		return TC358870_ERROR;

	return TC358870_OK;
}

int TC358870_Check0x0294tatus(void)
{
	uint8_t tc_data;

	delay_ms(20);

	if (TC358870_i2c_Read(0x0294, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	if (tc_data)
		return TC358870_ERROR;

	if (TC358870_i2c_Read(0x0494, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	if (tc_data)
		return TC358870_ERROR;

	return TC358870_OK;
}

void OSVR_HDK_EDID(void)
{
	uint8_t cs = 0;
	int i, j;
	unsigned short idx = 0;
	int sn_status;
	uint8_t edid_week;
	uint8_t edid_year;
	uint8_t edid_sn_hex[4];
	uint32_t edid_sn_dec;
	uint8_t hex[2], buf[20];

	if (sn_status = eep_read_sn(sn) != 0)
	{
		// WriteLn ("Read S/N NG.");

		edid_year = 26;  // 26 => 2016.
		edid_week = 1;
		memset(edid_sn_hex, 0, sizeof(uint8_t) * 4);
	}
	else
	{                                               // parsing s/n info
		edid_year = (ascii_to_dec_8(sn + 2) + 10);  // counting from 1990. year - 1990
		edid_week = ascii_to_dec_8(sn + 4);
		if (edid_week < 1 || edid_week > 53)
		{
			edid_week = 1;
			// WriteLn("Week Out Of Range.\n");
		}
		edid_sn_dec =
		    (uint32_t)(((uint32_t)(sn[10] - '0') * 10000) + ((uint32_t)(sn[11] - '0') * 1000) +
		               ((uint32_t)(sn[12] - '0') * 100) + ((uint32_t)(sn[13] - '0') * 10) + (uint32_t)(sn[14] - '0'));
		edid_sn_hex[0] = (uint8_t)(edid_sn_dec & 0x000000FF);
		edid_sn_hex[1] = (uint8_t)((edid_sn_dec & 0x0000FF00) >> 8);
		edid_sn_hex[2] = (uint8_t)((edid_sn_dec & 0x00FF0000) >> 16);
		edid_sn_hex[3] = (uint8_t)((edid_sn_dec & 0xFF000000) >> 24);
	}

	if (DebugLevel == 0xAA)
	{  // debug message.
		WriteLn("S/N Info...\n");
		sprintf(buf, "Year: %d", edid_year);
		WriteLn(buf);
		sprintf(buf, "Week: %d", edid_week);
		WriteLn(buf);

		sprintf(buf, "int32=%d", sizeof(uint32_t));
		WriteLn(buf);

		sprintf(buf, "S/N: %d", (uint32_t)edid_sn_dec);
		WriteLn(buf);
		sprintf(buf, "%02X-%02X-%02X-%02X", edid_sn_hex[3], edid_sn_hex[2], edid_sn_hex[1], edid_sn_hex[0]);
		WriteLn(buf);
	}

	// feeding EDID data to TC358870
	for (i = 0; i < 2; i++)
	{
		for (j = 0, cs = 0; j < 0x80; j++, idx++)
		{
			switch (idx)
			{
			case EDID_ADDR_WEEK:
				cs += edid_week;
				TC358870_i2c_Write(0x8C00 + idx, edid_week, 1);
				break;

			case EDID_ADDR_YEAR:
				cs += edid_year;
				TC358870_i2c_Write(0x8C00 + idx, edid_year, 1);
				break;

			case EDID_ADDR_SN_0:
			case EDID_ADDR_SN_1:
			case EDID_ADDR_SN_2:
			case EDID_ADDR_SN_3:
				cs += edid_sn_hex[idx - EDID_ADDR_SN_0];
				TC358870_i2c_Write(0x8C00 + idx, edid_sn_hex[idx - EDID_ADDR_SN_0], 1);
				break;

			case EDID_ADDR_CS_0:
			case EDID_ADDR_CS_1:
				cs = (unsigned char)(0x100 - cs);  // Making EDID page check sum. The 1-byte sum of all 128 bytes in
				                                   // this EDID block shall equal zero.
				TC358870_i2c_Write(0x8C00 + idx, (unsigned char)cs, 1);
				break;

			default:
				cs += EDID_LUT[idx];
				TC358870_i2c_Write(0x8C00 + idx, EDID_LUT[idx], 1);
				break;
			}
		}
	}
}

int TC358870_Init_Receive_HDMI_Signal(void)
{
	uint8_t tc_data;
	static int InitFlag = 0;

	// Initialization to receive HDMI signal
	// Software Reset
	TC358870_i2c_Write(0x0004, 0x0004, 2);  // ConfCtl0
	TC358870_i2c_Write(0x0002, 0x3F01, 2);  // SysCtl
	TC358870_i2c_Write(0x0002, 0x0000, 2);  // SysCtl
	TC358870_i2c_Write(0x0006, 0x0008, 2);  // ConfCtl1

	// DSI-TX0 Transition Timing
	TC358870_i2c_Write(0x0108, 0x00000001, 4);  // DSI_TX_CLKEN
	TC358870_i2c_Write(0x010C, 0x00000001, 4);  // DSI_TX_CLKSEL
	TC358870_i2c_Write(0x02A0, 0x00000001, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x02AC, 0x00003047, 4);  // MIPI_PLL_CNF
	delay_ms(1);

	TC358870_i2c_Write(0x02A0, 0x00000003, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x0118, 0x00000014, 4);  // LANE_ENABLE
	TC358870_i2c_Write(0x0120, 0x00001B58, 4);  // LINE_INIT_COUNT
	TC358870_i2c_Write(0x0124, 0x00000000, 4);  // HSTX_TO_COUNT
	TC358870_i2c_Write(0x0128, 0x00000101, 4);  // FUNC_ENABLE
	TC358870_i2c_Write(0x0130, 0x00010000, 4);  // DSI_TATO_COUNT
	TC358870_i2c_Write(0x0134, 0x00005000, 4);  // DSI_PRESP_BTA_COUNT
	TC358870_i2c_Write(0x0138, 0x00010000, 4);  // DSI_PRESP_LPR_COUNT
	TC358870_i2c_Write(0x013C, 0x00010000, 4);  // DSI_PRESP_LPW_COUNT
	TC358870_i2c_Write(0x0140, 0x00010000, 4);  // DSI_PRESP_HSR_COUNT
	TC358870_i2c_Write(0x0144, 0x00010000, 4);  // DSI_PRESP_HSW_COUNT
	TC358870_i2c_Write(0x0148, 0x00001000, 4);  // DSI_PR_TO_COUNT
	TC358870_i2c_Write(0x014C, 0x00010000, 4);  // DSI_LRX-H_TO_COUNT
	TC358870_i2c_Write(0x0150, 0x00000161, 4);  // FUNC_MODE
	TC358870_i2c_Write(0x0154, 0x00000001, 4);  // DSI_RX_VC_ENABLE
	TC358870_i2c_Write(0x0158, 0x000000C8, 4);  // IND_TO_COUNT
	TC358870_i2c_Write(0x0168, 0x0000002A, 4);  // DSI_HSYNC_STOP_COUNT
	TC358870_i2c_Write(0x0170, 0x000003CC, 4);  // APF_VDELAYCNT
	TC358870_i2c_Write(0x017C, 0x00000081, 4);  // DSI_TX_MODE
	TC358870_i2c_Write(0x018C, 0x00000001, 4);  // DSI_HSYNC_WIDTH
	TC358870_i2c_Write(0x0190, 0x0000018C, 4);  // DSI_HBPR
	TC358870_i2c_Write(0x01A4, 0x00000000, 4);  // DSI_RX_STATE_INT_MASK
	TC358870_i2c_Write(0x01C0, 0x00000015, 4);  // DSI_LPRX_THRESH_COUNT
	TC358870_i2c_Write(0x0214, 0x00000000, 4);  // APP_SIDE_ERR_INT_MASK
	TC358870_i2c_Write(0x021C, 0x00000080, 4);  // DSI_RX_ERR_INT_MASK
	TC358870_i2c_Write(0x0224, 0x00000000, 4);  // DSI_LPTX_INT_MASK
	TC358870_i2c_Write(0x0254, 0x00000006, 4);  // LPTXTIMECNT
	TC358870_i2c_Write(0x0258, 0x001C0206, 4);  // TCLK_HEADERCNT
	TC358870_i2c_Write(0x025C, 0x000E0008, 4);  // TCLK_TRAILCNT
	TC358870_i2c_Write(0x0260, 0x000B0006, 4);  // THS_HEADERCNT
	TC358870_i2c_Write(0x0264, 0x00004650, 4);  // TWAKEUPCNT
	TC358870_i2c_Write(0x0268, 0x00000010, 4);  // TCLK_POSTCNT
	TC358870_i2c_Write(0x026C, 0x000C0008, 4);  // THS_TRAILCNT
	TC358870_i2c_Write(0x0270, 0x00000020, 4);  // HSTXVREGCNT
	TC358870_i2c_Write(0x0274, 0x0000001F, 4);  // HSTXVREGEN
	TC358870_i2c_Write(0x0278, 0x00060006, 4);  // BTA_COUNT
	TC358870_i2c_Write(0x027C, 0x00000002, 4);  // DPHY_TX ADJUST
	TC358870_i2c_Write(0x011C, 0x00000001, 4);  // DSITX_START

	// DSI-TX1 Transition Timing
	TC358870_i2c_Write(0x0308, 0x00000001, 4);  // DSI_TX_CLKEN
	TC358870_i2c_Write(0x030C, 0x00000001, 4);  // DSI_TX_CLKSEL
	TC358870_i2c_Write(0x04A0, 0x00000001, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x04AC, 0x00003047, 4);  // MIPI_PLL_CNF
	delay_ms(1);

	TC358870_i2c_Write(0x04A0, 0x00000003, 4);  // MIPI_PLL_CONTROL
	TC358870_i2c_Write(0x0318, 0x00000014, 4);  // LANE_ENABLE
	TC358870_i2c_Write(0x0320, 0x00001B58, 4);  // LINE_INIT_COUNT
	TC358870_i2c_Write(0x0324, 0x00000000, 4);  // HSTX_TO_COUNT
	TC358870_i2c_Write(0x0328, 0x00000101, 4);  // FUNC_ENABLE
	TC358870_i2c_Write(0x0330, 0x00010000, 4);  // DSI_TATO_COUNT
	TC358870_i2c_Write(0x0334, 0x00005000, 4);  // DSI_PRESP_BTA_COUNT
	TC358870_i2c_Write(0x0338, 0x00010000, 4);  // DSI_PRESP_LPR_COUNT
	TC358870_i2c_Write(0x033C, 0x00010000, 4);  // DSI_PRESP_LPW_COUNT
	TC358870_i2c_Write(0x0340, 0x00010000, 4);  // DSI_PRESP_HSR_COUNT
	TC358870_i2c_Write(0x0344, 0x00010000, 4);  // DSI_PRESP_HSW_COUNT
	TC358870_i2c_Write(0x0348, 0x00001000, 4);  // DSI_PR_TO_COUNT
	TC358870_i2c_Write(0x034C, 0x00010000, 4);  // DSI_LRX-H_TO_COUNT
	TC358870_i2c_Write(0x0350, 0x00000161, 4);  // FUNC_MODE
	TC358870_i2c_Write(0x0354, 0x00000001, 4);  // DSI_RX_VC_ENABLE
	TC358870_i2c_Write(0x0358, 0x000000C8, 4);  // IND_TO_COUNT
	TC358870_i2c_Write(0x0368, 0x0000002A, 4);  // DSI_HSYNC_STOP_COUNT
	TC358870_i2c_Write(0x0370, 0x000003CC, 4);  // APF_VDELAYCNT
	TC358870_i2c_Write(0x037C, 0x00000081, 4);  // DSI_TX_MODE
	TC358870_i2c_Write(0x038C, 0x00000001, 4);  // DSI_HSYNC_WIDTH
	TC358870_i2c_Write(0x0390, 0x0000018C, 4);  // DSI_HBPR
	TC358870_i2c_Write(0x03A4, 0x00000000, 4);  // DSI_RX_STATE_INT_MASK
	TC358870_i2c_Write(0x03C0, 0x00000015, 4);  // DSI_LPRX_THRESH_COUNT
	TC358870_i2c_Write(0x0414, 0x00000000, 4);  // APP_SIDE_ERR_INT_MASK
	TC358870_i2c_Write(0x041C, 0x00000080, 4);  // DSI_RX_ERR_INT_MASK
	TC358870_i2c_Write(0x0424, 0x00000000, 4);  // DSI_LPTX_INT_MASK
	TC358870_i2c_Write(0x0454, 0x00000006, 4);  // LPTXTIMECNT
	TC358870_i2c_Write(0x0458, 0x001C0206, 4);  // TCLK_HEADERCNT
	TC358870_i2c_Write(0x045C, 0x000E0008, 4);  // TCLK_TRAILCNT
	TC358870_i2c_Write(0x0460, 0x000B0006, 4);  // THS_HEADERCNT
	TC358870_i2c_Write(0x0464, 0x00004650, 4);  // TWAKEUPCNT
	TC358870_i2c_Write(0x0468, 0x00000010, 4);  // TCLK_POSTCNT
	TC358870_i2c_Write(0x046C, 0x000C0008, 4);  // THS_TRAILCNT
	TC358870_i2c_Write(0x0470, 0x00000020, 4);  // HSTXVREGCNT
	TC358870_i2c_Write(0x0474, 0x0000001F, 4);  // HSTXVREGEN
	TC358870_i2c_Write(0x0478, 0x00060006, 4);  // BTA_COUNT
	TC358870_i2c_Write(0x047C, 0x00000002, 4);  // DPHY_TX ADJUST
	TC358870_i2c_Write(0x031C, 0x00000001, 4);  // DSITX_START

	// Command Transmission Before Video Start
	TC358870_i2c_Write(0x0500, 0x0004, 2);      // CMD_SEL
	TC358870_i2c_Write(0x0110, 0x00000016, 4);  // MODE_CONFIG
	TC358870_i2c_Write(0x0310, 0x00000016, 4);  // MODE_CONFIG

	// LCD Initialization
	AUO_H381DLN01_Init(0);

#if 1  // Dennis Yeh 2016/04/18
	// Split Control
	TC358870_i2c_Write(0x5000, 0x0000, 2);  // STX0_CTL
	TC358870_i2c_Write(0x500C, 0x84E0, 2);  // STX0_FPX
	TC358870_i2c_Write(0x5080, 0x0000, 2);  // STX1_CTL
#else                                       // shift 28 pixels.
	TC358870_i2c_Write(0x5000, 0x0000, 2);  // STX0_CTL
	TC358870_i2c_Write(0x500C, 0x0000, 2);  // STX0_FPX
	TC358870_i2c_Write(0x500E, 0x041B, 2);  // STX0_LPX
	TC358870_i2c_Write(0x5080, 0x0000, 2);  // STX1_CTL
	TC358870_i2c_Write(0x508C, 0x0454, 2);  // STX1_FPX
	TC358870_i2c_Write(0x508E, 0x086F, 2);  // STX1_LPX
	TC358870_i2c_Write(0x500A, 0x001C, 2);  // STX0_DPX
	// TC358870_i2c_Write(0x5010,0x0000, 2); // STX0_DRPX
	// TC358870_i2c_Write(0x5012,0x0000, 2); // STX0_DGPX
	// TC358870_i2c_Write(0x5014,0x0000, 2); // STX0_DBPX
	TC358870_i2c_Write(0x508A, 0x1C00, 2);  // STX1_DPX
                                            // TC358870_i2c_Write(0x5090,0x0000, 2); // STX1_DRPX
                                            // TC358870_i2c_Write(0x5092,0x0000, 2); // STX1_DGPX
                                            // TC358870_i2c_Write(0x5094,0x0000, 2); // STX1_DBPX
#endif

	// HDMI PHY
	TC358870_i2c_Write(0x8410, 0x03, 1);  // PHY CTL
	TC358870_i2c_Write(0x8413, 0x3F, 1);  // PHY_ENB
	TC358870_i2c_Write(0x8420, 0x06, 1);  // EQ_BYPS
	TC358870_i2c_Write(0x84F0, 0x31, 1);  // APLL_CTL
	TC358870_i2c_Write(0x84F4, 0x01, 1);  // DDCIO_CTL

	// HDMI Clock
	TC358870_i2c_Write(0x8540, 0x12C0, 2);  // SYS_FREQ0_1
	TC358870_i2c_Write(0x8630, 0x00, 1);    // LOCKDET_FREQ0
	TC358870_i2c_Write(0x8631, 0x0753, 2);  // LOCKDET_REF1_2
	TC358870_i2c_Write(0x8670, 0x02, 1);    // NCO_F0_MOD
	TC358870_i2c_Write(0x8A0C, 0x12C0, 2);  // CSC_SCLK0_1

	// HDMI Interrupt Mask, Clear
	TC358870_i2c_Write(0x850B, 0xFF, 1);  // MISC_INT
	TC358870_i2c_Write(0x851B, 0xFD, 1);  // MISC_INTM

	// Interrupt Control (TOP level)
	TC358870_i2c_Write(0x0014, 0x0FBF, 2);  // IntStatus
	TC358870_i2c_Write(0x0016, 0x0DBF, 2);  // IntMask

	// EDID
	TC358870_i2c_Write(0x85E0, 0x01, 1);    // EDID_MODE
	TC358870_i2c_Write(0x85E3, 0x0100, 2);  // EDID_LEN1_2

	// EDID Data
	OSVR_HDK_EDID();

	// HDCP Setting
	// TC358870_i2c_Write(0x8840, 0xC0, 1); // HDCP Repeater enable
	TC358870_i2c_Write(0x85EC, 0x01, 1);  // key loading command

	// Video Color Format Setting
	TC358870_i2c_Write(0x8A02, 0x42, 1);  // VOUT_SYNC0

	// HDMI SYSTEM
	TC358870_i2c_Write(0x8543, 0x02, 1);  // DDC_CTL
	TC358870_i2c_Write(0x8544, 0x10, 1);  // HPD_CTL

	// HDMI Audio Setting
	TC358870_i2c_Write(0x8600, 0x00, 1);        // AUD_Auto_Mute
	TC358870_i2c_Write(0x8602, 0xF3, 1);        // Auto_CMD0
	TC358870_i2c_Write(0x8603, 0x02, 1);        // Auto_CMD1
	TC358870_i2c_Write(0x8604, 0x0C, 1);        // Auto_CMD2
	TC358870_i2c_Write(0x8606, 0x05, 1);        // BUFINIT_START
	TC358870_i2c_Write(0x8607, 0x00, 1);        // FS_MUTE
	TC358870_i2c_Write(0x8652, 0x02, 1);        // SDO_MODE1
	TC358870_i2c_Write(0x8671, 0x020C49BA, 4);  // NCO_48F0A_D
	TC358870_i2c_Write(0x8675, 0x01E1B089, 4);  // NCO_44F0A_D
	TC358870_i2c_Write(0x8680, 0x00, 1);        // AUD_MODE

	// Let HDMI Source start access
	TC358870_i2c_Write(0x854A, 0x01, 1);  // INIT_END

	// HDMI Signal Detection Wait until HDMI sync is established
	// By Interrupt   In Interrupt Service Routine.
	// Sequence: Check bit7 of 8x8520
	int times = InitFlag ? 100 : 10;
	for (int idx = 0; idx < times; idx++)
	{
		if (TC358870_VideoSyncSignalStatus() == TC358870_Sync)
			break;
		else
			delay_ms(20);
	}

	TC358870_i2c_Write(0x850B, 0xFF, 1);    // MISC_INT
	TC358870_i2c_Write(0x0014, 0x0FBF, 2);  // IntStatus

	// By Polling
	// Sequence: Check bit7 of 8x8520
	for (int idx = 0; idx < times; idx++)
	{
		if (TC358870_VideoSyncSignalStatus() == TC358870_Sync)
			break;
		else
			delay_ms(20);
	}

	// Start Video TX
	TC358870_i2c_Write(0x0004, 0x0C37, 2);  // ConfCtl0
	TC358870_i2c_Write(0x0006, 0x0000, 2);  // ConfCtl1

	// Command Transmission After Video Start.
	// TC358870_i2c_Write(0x0110, 0x00000006, 4); // MODE_CONFIG
	// TC358870_i2c_Write(0x0310, 0x00000006, 4); // MODE_CONFIG
	if (InitFlag == 1)
	{
		TC358870_Reset_MIPI();

		if (TC358870_Check0x0294tatus() == TC358870_ERROR)
			TC358870_Reset_MIPI();
	}

	InitFlag = 1;
}

int TC358870_VideoSyncSignalStatus(void)
{
	uint8_t tc_data;

	if (TC358870_i2c_Read(0x8520, &tc_data) != TC358870_OK)  // get SYS_STATUS
		return TC358870_ERROR;

	if (tc_data & 0x80)
		return TC358870_Sync;
	else
		return TC358870_NoSync;
}

/*    ==============================================================
        Implement for EDID date code and S/E function.
        1. Access S/N from MCU EEPROM.
        2. Create command to access S/N
        3. Write a function to parse date code and S/N from S/N, offers for EDID
        4. Write a function to calculate EDID check sum sinc date code and S/N are replace.
        5. Insert date code and S/N when feed EDID to TC358870.

        ==============================================================
*/

unsigned char sn[SN_LENGTH];

// If CRC 16 is fine for the performnace, replace it...
static unsigned char check_sum_gen(unsigned char *buf, int length)
{
	int i = 0;
	unsigned char cs = 0;

	for (i = 0; i < length; i++)
	{
		cs += buf[i];
	}

	return cs;
}

void eep_write_sn(void)
{
	sn[SN_LENGTH - 1] = check_sum_gen(sn, SN_LENGTH - 1);  // fill check sum.
	nvm_eeprom_erase_and_write_buffer(EEP_ADDR_SN, sn, SN_LENGTH);
}

int eep_read_sn(unsigned char *buf)
{
	int i = 0;

	for (i = 0; i < SN_LENGTH; i++)
	{
		buf[i] = nvm_eeprom_read_byte(EEP_ADDR_SN + i);
	}

	// check sum verify.
	if (check_sum_gen(buf, SN_LENGTH - 1) != buf[SN_LENGTH - 1])
		return -1;  // check sum verify fail.

	return 0;
}

extern char CommandToExecute[MaxCommandLength + 1];

void ProcessFactoryCommand(void)
{
	int i;
	char OutString[SN_LENGTH];
	static int core_key_retry_time = 0;

	switch (CommandToExecute[1])
	{
	case 's':  // 20160520, fctu, S/N read, write.
	case 'S':
		switch (CommandToExecute[2])
		{
		case 'w':  // Write S/N to eeprom.
		case 'W':
			if (core_key_pass == 0)
				break;
			// write...
			memcpy(sn, CommandToExecute + 3, SN_LENGTH - 1);
			eep_write_sn();

			// verify...
			i = eep_read_sn(OutString);
			OutString[SN_LENGTH - 1] = '\0';
			sn[SN_LENGTH - 1] = '\0';

			if (i)
				WriteLn("NG");  // check sum error.
			else if (strcmp(OutString, sn) != 0)
				WriteLn("NG");  // s/n does not match.
			else
				WriteLn("OK");  // s/n match.

			break;

		case 'r':  // Read S/N from eeprom.
		case 'R':
			if (eep_read_sn(OutString))
				WriteLn("NG");  // send NG to host.
			else
			{
				OutString[SN_LENGTH - 1] = '\0';
				WriteLn(OutString);
			}
			break;
		}
		break;

	case 't':  // for testing purpose.
	case 'T':
		OSVR_HDK_EDID();
		break;

	case 'k':
	case 'K':
		switch (CommandToExecute[2])
		{
		case 'r':  // get key.
		case 'R':
			// WriteLn(CORE_KEK);
			break;

		case 'c':  // match key.
		case 'C':
			if (core_key_retry_time >= CORE_KEY_RETRY_TIME)
			{
				// WriteLn("LOCK");  // for debug.
				WriteLn("NG");  // for someone whom unauthority.
				break;
			}

			memcpy(OutString, CommandToExecute + 3, CORE_KEY_LENGTH);

			if (memcmp(OutString, CORE_KEY, CORE_KEY_LENGTH) == 0)
			{
				core_key_retry_time = 0;
				core_key_pass = 1;
				WriteLn("OK");
			}
			else
			{
				core_key_retry_time++;
				core_key_pass = 0;
				WriteLn("NG");
			}

			break;
		}
		break;
	}
}

void nvm_eeprom_write_byte_(eeprom_addr_t address, uint8_t value)
{
	if (address >= EEP_ADDR_SN && address <= 0x1F)  // 0x08 ~0x1F
		if (core_key_pass == 0)                     // no right to access this area.
			return;

	nvm_eeprom_write_byte(address, value);
}
