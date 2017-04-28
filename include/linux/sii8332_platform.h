#ifndef _SII8332_PLATFORM_H
#define _SII8332_PLATFORM_H

/*
 * Copyright (C) 2011 Silicon Image, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define SFEATURE_SII8332_PLATFORM

#ifdef SFEATURE_SII8332_PLATFORM

typedef enum        
{
  NO_MHL_STATUS = 0x00,
  MHL_READY_RGND_DETECT,
  MHL_RX_CONNECTED,
  MHL_USB_CONNECTED,
  MHL_DISCOVERY_SUCCESS, 
  MHL_DISCOVERY_FAIL,
  MHL_DISCOVERY_ON,
  MHL_RX_DISCONNECTED,
}mhl_op_enum_type;

typedef struct{
  mhl_op_enum_type op_status;  
  u8 intr1_mask_value;
  u8 intr2_mask_value;  
  u8 intr3_mask_value;  
  u8 intr4_mask_value;
  u8 intr5_mask_value;
  u8 intr7_mask_value;
  u8 intr8_mask_value;  
  u8 intr_cbus0_mask_value;
  u8 intr_cbus1_mask_value;
  u8 intr_tpi_mask_value;
  bool mhl_rgnd;
  bool cbus_connected;
  u8 linkmode;
  u8 connected_ready;  
}mhl_status_type;


typedef struct{
  u8 mhl_ver;
  u8 dev_type;
  u16 adopter_id;
  u8 vid_link_mode;
  u8 aud_link_mode;
  u8 video_type;
  u8 log_dev_map;
  u8 bandwidth;
  u8 feature_flag;
  u16 device_id;
  u8 scratchpad_size;
  u8 int_stat_size;
  
  bool rcp_support;
  bool rap_support;
  bool sp_support;
}mhl_rx_cap_type;



struct mhl_platform_data {  
	unsigned int mhl_rst;
	unsigned int mhl_int;
	unsigned int mhl_en;
	int irq;
	bool hdcp_support;
	bool intr_call;
	mhl_status_type status;  
	mhl_rx_cap_type rx_cap;

	wait_queue_head_t cbus_intr_thread_wait; 

	struct mutex mhl_status_lock; 
	struct mutex intr_check_lock; 

	struct i2c_client *simg72_tx_client;
	struct i2c_client *simg7A_tx_client;
	struct i2c_client *simg92_tx_client;
	struct i2c_client *simg9A_tx_client;    
	struct i2c_client *simgC8_tx_client;
};
#endif/*SFEATURE_SII8332_PLATFORM*/
extern void sii9234_power_onoff(bool on);
extern void sii9234_reset(void);
extern void isvbus_powered_mhl(int);

#endif/*_SII8240_PLATFORM_H*/
