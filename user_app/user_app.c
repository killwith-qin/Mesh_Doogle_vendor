/********************************************************************************************************
 * @file	user_app.c
 *
 * @brief	for TLSR chips
 *
 * @author	telink
 * @date	Sep. 30, 2010
 *
 * @par     Copyright (c) 2017, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/
#include "proj/tl_common.h"
#if !WIN32
#include "proj/mcu/watchdog_i.h"
#include "proj_lib/mesh_crypto/mesh_md5.h"
#include "vendor/common/myprintf.h"
#endif 
#include "proj_lib/ble/ll/ll.h"
#include "proj_lib/ble/blt_config.h"
#include "vendor/common/user_config.h"
#include "proj_lib/ble/service/ble_ll_ota.h"
#include "vendor/common/app_health.h"
#include "proj_lib/sig_mesh/app_mesh.h"
#include "vendor/common/app_provison.h"
#include "vendor/common/lighting_model.h"
#include "vendor/common/sensors_model.h"
#include "vendor/common/remote_prov.h"
#include "proj_lib/mesh_crypto/sha256_telink.h"
#include "proj_lib/mesh_crypto/le_crypto.h"
#include "vendor/common/lighting_model_LC.h"
#include "vendor/common/mesh_ota.h"
#include "vendor/common/mesh_common.h"
#include "vendor/common/mesh_config.h"
#include "vendor/common/directed_forwarding.h"
#include "vendor/common/certify_base/certify_base_crypto.h"

#if(__TL_LIB_8258__ || (MCU_CORE_TYPE == MCU_CORE_8258))
#include "stack/ble/ble.h"
#elif(MCU_CORE_TYPE == MCU_CORE_8278)
#include "stack/ble_8278/ble.h"
#endif

#if FAST_PROVISION_ENABLE
#include "vendor/common/fast_provision_model.h"
#endif

#if (HCI_ACCESS==HCI_USE_UART)
#include "proj/drivers/uart.h"
#endif

static void Send_One_Ctr_Signal(u32 One_Signal);
static void Sned_24bit_Signal(u8 *bit);
static void Sned_Reset_Signal(void);

void cb_user_factory_reset_additional()
{
    // TODO
}

void cb_user_proc_led_onoff_driver(int on)
{
    // TODO
}




typedef enum RGB_Color
{
	RED    = 0x0000FF00,
	ORAGE  = 0x007DFF00,
	YELLOW = 0x00FFFF00,
    GREEN  = 0x00FF0000,
    CYAN   = 0x00FF00FF,
    BULU   = 0x000000FF,
    PURPLE = 0x0000FFFF,
    DROWN  = 0x00000000
}Seven_Color;


#define EVERY_SEND_COUNT 30

void User_Ctr_LED_Function(u8 Cmd_ON_OFF)
{
	static u32 User_Send_LED_Signal_Tick = 0;
	static U32 Color_Count = 0;
	int j;
	U8 switch_Led_light;
    if(Cmd_ON_OFF != 0)
    {

		//gpio_toggle(GPIO_PB5);
		//gpio_write(GPIO_PB5,1);
    if( clock_time_exceed(User_Send_LED_Signal_Tick, 100*1000))
	{
		User_Send_LED_Signal_Tick = clock_time();
		Color_Count++;
		switch_Led_light = Color_Count%30;

			for(j=0;j<EVERY_SEND_COUNT;j++)
			{
				if(j==switch_Led_light)
				{
					Send_One_Ctr_Signal(RED);
					Send_One_Ctr_Signal(YELLOW);
					Send_One_Ctr_Signal(BULU);
					Send_One_Ctr_Signal(CYAN);
					Send_One_Ctr_Signal(PURPLE);
					Send_One_Ctr_Signal(ORAGE);
					Send_One_Ctr_Signal(GREEN);
				}
				else
				{
					Send_One_Ctr_Signal(DROWN);
				}
            }
    }
    }
	else
	{
        for(j=0;j<EVERY_SEND_COUNT;j++)
		{
			Send_One_Ctr_Signal(DROWN);
		}

	}
}



/**
 * @brief  Send a byte of serial data.
 * @param  byte: Data to send.
 * @retval None
 */
_attribute_ram_code_ static void Send_One_Ctr_Signal(u32 One_Signal)   //0x00 XX XX XX 24bit valid
{
	int i;
	//volatile u32 pcTxReg = SIGNAL_IO_REG_ADDR;//register GPIO output
	u8 bit[24] = {0};
	
	for(i=0;i<24;i++)
	{
        bit[i] = ( (One_Signal>>i) & 0x01)? 0x01 : 0x00;
	}
	
	/*! Minimize the time for interrupts to close and ensure timely 
	    response after interrupts occur. */
	u8 r = 0;
	r = irq_disable();
	    Sned_24bit_Signal(bit);
	irq_restore(r);
	 
}

#define LED_SIGAL_SEND_PIN  (GPIO_PB5)  //register GPIO output
#define SIGNAL_IO_REG_ADDR  (0x58B)    //register GPIO output

#define SINGAL_OUT_LOW      ( write_reg8 (read_reg8(SIGNAL_IO_REG_ADDR) & ( ~(LED_SIGAL_SEND_PIN & 0xff)  ) ) )

#define SINGAL_OUT_HIGH     ( write_reg8 (read_reg8(SIGNAL_IO_REG_ADDR) | ( LED_SIGAL_SEND_PIN & 0xff)) )

_attribute_ram_code_ 
_attribute_no_inline_ 
static void Sned_24bit_Signal(u8 *bit)
{
	int j;
        u8 tmp_out_low  = read_reg8(SIGNAL_IO_REG_ADDR) & (~(LED_SIGAL_SEND_PIN & 0xff));
	    u8 tmp_out_high = read_reg8(SIGNAL_IO_REG_ADDR) | (LED_SIGAL_SEND_PIN & 0xff);
	/*! Make sure the following loop instruction starts at 4-byte alignment: (which is destination address of "tjne") */
	// _ASM_NOP_; 
	for(j = 0;j<24;j++) 
	{
		 if(bit[j]!=0)
		 {
            write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_high);
			CLOCK_DLY_10_CYC;CLOCK_DLY_10_CYC;
			CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
            write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_low);
			CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
		 }
		 else
		 {
            write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_high);
            CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
			write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_low);
			CLOCK_DLY_10_CYC;CLOCK_DLY_10_CYC;
			CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
		 }
	}
}


_attribute_ram_code_ static void Sned_Reset_Signal(void)
{

        u8 tmp_out_low  = read_reg8(SIGNAL_IO_REG_ADDR) & (~(LED_SIGAL_SEND_PIN & 0xff));
	    
        write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_low);
		sleep_us(50);
}
