/***************************************************************************
 * * 
 * *   Driver to read and parse EDID values from SII8332 driver
 * *
 * * 
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License as published by
 * * the Free Software Foundation version 2.
 * *
 * * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * * kind, whether express or implied; without even the implied warranty
 * * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * * GNU General Public License for more details.
 * *
 * *****************************************************************************/
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#include <asm/irq.h>
#include <linux/delay.h>

#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include "edid2.h"

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/input.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/poll.h>
//#include <mach/r8a73734.h>
#include <mach/r8a7373.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#define SYSFS_EVENT_FILENAME "evread"
//#define SFEATURE_HDCP_SUPPORT
#define MIN_VERT_RATE 30
#define MAX_VERT_RATE 60
#define VIRT_ACTIVE_PIXELS_1080 1080
#define	SET_BIT(pdata, deviceID, offset, bitnumber)		I2CReadModify(pdata, deviceID, offset,(1<<bitnumber),(1<<bitnumber))
#define	CLR_BIT(pdata, deviceID, offset, bitnumber)		I2CReadModify(pdata, deviceID, offset,(1<<bitnumber),0x00)

//static int outputformat;
extern int outputformat;/*edid*/

static int minverticalrate;
static int maxverticalrate;
static int minhorizontalrate;
static int maxhorizontalrate;

static struct edid_tx *edid_global;

static int __devinit edidA0_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devinit edid60_probe(struct i2c_client *client, const struct i2c_device_id *id);

static int __devexit edidA0_remove(struct i2c_client *client);
static int __devexit edid60_remove(struct i2c_client *client);

static struct dtd edidreadvalues;
struct dtd supported_dtds[] = {
{2700,720,138,480,45,2},/* CEA 2,3 */
{7425,1280,370,720,30,3},/*720P 60Hz CEA 4 */
{7425,1920,280,1080,22,4},/*1080I 60Hz CEA 5 */
{5400,1440,276,480,45,6},/* 14 15 */
{14850,1920,280,1080,45,7},/*1080P 60Hz CEA 16 */
{2700,720,144,576,49,8},/* CEA 17,18 */
{7425,1280,700,720,30,9},/* 720P 50Hz CEA 19 */
{7425,1920,720,1080,22,10},/* CEA 20 */
{2700,1440,288,576,24,11},/* CEA 21,22 */
{14850,1920,720,1080,45,13},/*1080P 50Hz CEA 31 */
};

#define TH_FUNC_START ////printk("sii8332 %s, %d \n",__func__, __LINE__)

static struct i2c_device_id edidA0_id[] = {
  {"edidA0", 0},
  {}
};

static struct i2c_device_id edid60_id[] = {
  {"edid60", 0},
  {}
};
static struct i2c_driver edidA0_driver = 
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "edidA0",

	},
	.id_table	= edidA0_id,
	.probe	= edidA0_probe,
	.remove	= __devexit_p(edidA0_remove),
	.command = NULL,
};

static struct i2c_driver edid60_driver = 
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "edid60",

	},
	.id_table	= edid60_id,
	.probe	= edid60_probe,
	.remove	= __devexit_p(edid60_remove),
	.command = NULL,
};


struct i2c_client* get_edidI2C_client(struct edid_platform_data *pdata, u8 device_id)
{
	struct i2c_client* client_ptr;
  printk(KERN_INFO"\n get_edidI2C_client \n");
  	if(device_id == 0x60)
		client_ptr = pdata->edid60_tx_client;
	else if(device_id == 0xA0)
		client_ptr = pdata->edidA0_tx_client;  
	else
		client_ptr = NULL;
	printk(KERN_INFO"\n get_edidI2C_client end \n");
	return client_ptr;
  
}

/****************************************************************************
*	name	=	edidA0_probe
*	func	=	
*	input	=	struct i2c_client *client, const struct i2c_device_id *id
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __devinit edidA0_probe(struct i2c_client *client,
								const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct edidtx_0xA0 *DDC_A0;
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
	DDC_A0 = kzalloc(sizeof(struct edidtx_0xA0), GFP_KERNEL);
	if (!DDC_A0) {
		dev_err(&client->dev, "edidA0_probe failed to allocate driverdata\n");
		return -ENOMEM;
	}
	DDC_A0->pdata = client->dev.platform_data;
	DDC_A0->pdata->edidA0_tx_client = client;

	if (!DDC_A0->pdata) {
		printk(KERN_INFO"\n edidA0 no platform data\n");
		kfree(DDC_A0);
		return -EINVAL;
	}

	i2c_set_clientdata(client, DDC_A0);

	return 0;
}

static int __devinit edid60_probe
				(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	edid_global = kzalloc(sizeof(struct edid_tx), GFP_KERNEL);
	if (!edid_global) {
 		dev_err(&client->dev, "edid60_probe failed to allocate driverdata\n");
 		return -ENOMEM;
 	}
	
	edid_global->pdata = client->dev.platform_data;	
	if (!edid_global->pdata) {
		printk(KERN_INFO"\n SIM60 no platform data\n");
		kfree(edid_global);
		return -EINVAL;
	}
	
	edid_global->pdata->edid60_tx_client = client;
	mutex_init(&edid_global->i2c_lock);
	edid_global->client = client;

	i2c_set_clientdata(client, edid_global);

	return 0;
}

static int __devexit edidA0_remove(struct i2c_client *client)
{
	i2c_del_driver(&edidA0_driver);
	return 0;
}

static int __devexit edid60_remove(struct i2c_client *client)
{
	i2c_del_driver(&edid60_driver);
	return 0;
}

/****************************************************************************
*	name	=	edid_init
*	func	=	
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __init edid_init(void)
{
	int ret=0;
	printk (KERN_INFO "entering %s\n", __func__);
	ret = i2c_add_driver(&edidA0_driver);
	if (ret !=0)
		goto err_exit5;

	ret = i2c_add_driver(&edid60_driver);
	if (ret !=0)
		goto err_exit6;   
	
	return 0;
		
err_exit6:
	printk(KERN_INFO"edid60_driver fail\n");  
	i2c_del_driver(&edid60_driver);  

err_exit5:
	printk(KERN_INFO"edidA0_driver fail\n");  
	i2c_del_driver(&edidA0_driver);

	return 0;
}

/****************************************************************************
*	name	=	edid_exit
*	func	=	
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
static void __exit edid_exit(void)
{
	i2c_del_driver(&edidA0_driver);
	i2c_del_driver(&edid60_driver);
}

/****************************************************************************
*	name	=	edid_header_check
*	func	=	
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/

static bool edid_header_check(struct edid_tx *edid)
{
	u8 i = 0;
	/*printk(KERN_INFO "edid: %s():%d start!\n",
	__func__, __LINE__);*/
	if (edid->edid_block0[0] != 0x00)
		return false;

	for (i = 1; i <= 6; i++) {
		if (edid->edid_block0[i] != 0xFF) {
			/*printk(KERN_INFO "%s returning false 1\n", __func__);*/
			return false;
		}
	}

	if (edid->edid_block0[7] != 0x00) {
		/*printk(KERN_INFO "%s returning false 2\n", __func__);*/
		return false;
	}

	return true;
}

/****************************************************************************
*	name	=	edid_checksum
*	func	=
*	input	=	struct edid_tx *edid, u8 block_num
*	output	=	None
*	return	=	ret
****************************************************************************/
static bool edid_checksum(struct edid_tx *edid, u8 block_num)
{
	u8 i = 0, checksum = 0;
	printk(KERN_INFO "edid: %s():%d start!\n", __func__, __LINE__);

	for (i = 0; i < EDID_SIZE; i++) {
		if (block_num == 0)
			checksum += edid->edid_block0[i];
		else if (block_num == 1)
			checksum += edid->edid_block1[i];
		else if (block_num == 2)
			checksum += edid->edid_block2[i];
		else if (block_num == 3)
			checksum += edid->edid_block3[i];
	}
	if (checksum)
		return false;
	else
		return true;
}

/****************************************************************************
*	name	=	ParseDetailedTiming
*	func	=	
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/ 
static bool ParseDetailedTiming(struct edid_tx *edid, u8 DetailedTimingOffset, u8 Block)
{
	u8 TmpByte;
	u8 i;
	u16 TmpWord;
    u8 edidBlockData [EDID_BLOCK_SIZE];
	printk(KERN_INFO "edid: %s():%d start!\n", __func__, __LINE__);
  memset(edidBlockData, 0x00, EDID_BLOCK_SIZE);
  if(Block == EDID_BLOCK_0){
    memcpy(edidBlockData, edid->edid_block0, EDID_BLOCK_SIZE);
  }
 /* else if(Block == EDID_BLOCK_0){
    memcpy(edidBlockData, edid->edid_block1, EDID_BLOCK_SIZE);
  }*/

	TmpWord = edidBlockData[DetailedTimingOffset + PIX_CLK_OFFSET] +
		256 * edidBlockData[DetailedTimingOffset + PIX_CLK_OFFSET + 1];

	if (TmpWord == 0x00)            // 18 byte partition is used as either for Monitor Name or for Monitor Range Limits or it is unused
	{
		if (Block == EDID_BLOCK_0)      // if called from Block #0 and first 2 bytes are 0 => either Monitor Name or for Monitor Range Limits
		{
			if (edidBlockData[DetailedTimingOffset + 3] == 0xFC) // these 13 bytes are ASCII coded monitor name
			{
				////printk("Monitor Name: ");

				for (i = 0; i < 13; i++)
				{
					////printk("%c", edidBlockData[DetailedTimingOffset + 5 + i]); // Display monitor name
				}
				////printk("\n");
			}

			else if (edidBlockData[DetailedTimingOffset + 3] == 0xFD) // these 13 bytes contain Monitor Range limits, binary coded
			{
				////printk("Monitor Range Limits:\n\n");

				i = 0;
				minverticalrate = (int) edidBlockData[DetailedTimingOffset + 5 + i++];
				printk("Min Vertical Rate in Hz: %d\n", minverticalrate); //
				maxverticalrate = (int) edidBlockData[DetailedTimingOffset + 5 + i++];
				printk("Max Vertical Rate in Hz: %d\n", maxverticalrate); //
				minhorizontalrate = (int) edidBlockData[DetailedTimingOffset + 5 + i++];
				printk("Min Horizontal Rate in Hz: %d\n", minhorizontalrate); //
				maxhorizontalrate = (int) edidBlockData[DetailedTimingOffset + 5 + i++];
				printk("Max Horizontal Rate in Hz: %d\n", maxhorizontalrate); //
				printk("Max Supported pixel clock rate in MHz/10: %d\n", (int) edidBlockData[DetailedTimingOffset + 5 + i++]); //
				printk("Tag for secondary timing formula (00h=not used): %d\n", (int) edidBlockData[DetailedTimingOffset + 5 + i++]); //
				printk("Min Vertical Rate in Hz %d\n", (int) edidBlockData[DetailedTimingOffset + 5 + i]); //
				printk("\n");
			}
		}

		else if (Block == EDID_BLOCK_2_3)                          // if called from block #2 or #3 and first 2 bytes are 0x00 (padding) then this
		{                                                                                          // descriptor partition is not used and parsing should be stopped
			////printk("No More Detailed descriptors in this block\n");
			////printk("\n");
			return false;
		}
	}

	else                                            // first 2 bytes are not 0 => this is a detailed timing descriptor from either block
	{
		if((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x36))
		{
			printk("\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 1:\n");
			printk("===========================================================\n\n");
		}
		else if((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x48))
		{
			printk("\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 2:\n");
			printk("===========================================================\n\n");
		}
		
		printk("Pixel Clock (MHz * 100): %d\n", (int)TmpWord);
		edidreadvalues.pixelFrequency = TmpWord;
		TmpWord = edidBlockData[DetailedTimingOffset + H_ACTIVE_OFFSET] +
			256 * ((edidBlockData[DetailedTimingOffset + H_ACTIVE_OFFSET + 2] >> 4) & FOUR_LSBITS);
		printk("Horizontal Active Pixels: %d\n", (int)TmpWord);
		edidreadvalues.hActive = TmpWord;
		TmpWord = edidBlockData[DetailedTimingOffset + H_BLANKING_OFFSET] +
			256 * (edidBlockData[DetailedTimingOffset + H_BLANKING_OFFSET + 1] & FOUR_LSBITS);
		printk("Horizontal Blanking (Pixels): %d\n", (int)TmpWord);
		edidreadvalues.hBlank = TmpWord;
		TmpWord = (edidBlockData[DetailedTimingOffset + V_ACTIVE_OFFSET] )+
			256 * ((edidBlockData[DetailedTimingOffset + (V_ACTIVE_OFFSET) + 2] >> 4) & FOUR_LSBITS);
		printk("Vertical Active (Lines): %d\n", (int)TmpWord);
		edidreadvalues.vActive = TmpWord;
		TmpWord = edidBlockData[DetailedTimingOffset + V_BLANKING_OFFSET] +
			256 * (edidBlockData[DetailedTimingOffset + V_BLANKING_OFFSET + 1] & FOUR_LSBITS);
		printk("Vertical Blanking (Lines): %d\n", (int)TmpWord);
		edidreadvalues.vBlank = TmpWord;
		TmpWord = edidBlockData[DetailedTimingOffset + H_SYNC_OFFSET] +
			256 * ((edidBlockData[DetailedTimingOffset + (H_SYNC_OFFSET + 3)] >> 6) & TWO_LSBITS);
		printk("Horizontal Sync Offset (Pixels): %d\n", (int)TmpWord);

		TmpWord = edidBlockData[DetailedTimingOffset + H_SYNC_PW_OFFSET] +
			256 * ((edidBlockData[DetailedTimingOffset + (H_SYNC_PW_OFFSET + 2)] >> 4) & TWO_LSBITS);
		printk("Horizontal Sync Pulse Width (Pixels): %d\n", (int)TmpWord);

		TmpWord = ((edidBlockData[DetailedTimingOffset + V_SYNC_OFFSET] >> 4) & FOUR_LSBITS) +
			256 * ((edidBlockData[DetailedTimingOffset + (V_SYNC_OFFSET + 1)] >> 2) & TWO_LSBITS);
		printk("Vertical Sync Offset (Lines): %d\n", (int)TmpWord);

		TmpWord = ((edidBlockData[DetailedTimingOffset + V_SYNC_PW_OFFSET]) & FOUR_LSBITS) +
			256 * (edidBlockData[DetailedTimingOffset + (V_SYNC_PW_OFFSET + 1)] & TWO_LSBITS);
		printk("Vertical Sync Pulse Width (Lines): %d\n", (int)TmpWord);

		TmpWord = edidBlockData[DetailedTimingOffset + H_IMAGE_SIZE_OFFSET] +
			256 * (((edidBlockData[DetailedTimingOffset + (H_IMAGE_SIZE_OFFSET + 2)]) >> 4) & FOUR_LSBITS);
		printk("Horizontal Image Size (mm): %d\n", (int)TmpWord);

		TmpWord = edidBlockData[DetailedTimingOffset + V_IMAGE_SIZE_OFFSET] +
			256 * (edidBlockData[DetailedTimingOffset + (V_IMAGE_SIZE_OFFSET + 1)] & FOUR_LSBITS);
		printk("Vertical Image Size (mm): %d\n", (int)TmpWord);

		TmpByte = edidBlockData[DetailedTimingOffset + H_BORDER_OFFSET];
		printk("Horizontal Border (Pixels): %d\n", (int)TmpByte);

		TmpByte = edidBlockData[DetailedTimingOffset + V_BORDER_OFFSET];
		printk("Vertical Border (Lines): %d\n", (int)TmpByte);

		TmpByte = edidBlockData[DetailedTimingOffset + FLAGS_OFFSET];
		if (TmpByte & BIT7)
		{
			//printk("Interlaced\n");
		}
		else
		{
			//printk("Non-Interlaced\n");
		}

		if (!(TmpByte & BIT5) && !(TmpByte & BIT6))
		{
			//printk("Normal Display, No Stereo\n");
		}
		else
		{
		  //printk("Refer to VESA E-EDID Release A, Revision 1, table 3.17\n");
		}

		if (!(TmpByte & BIT3) && !(TmpByte & BIT4))
		{
			//printk("Analog Composite\n");
		}
		else if ((TmpByte & BIT3) && !(TmpByte & BIT4))
		{
			//printk("Bipolar Analog Composite\n");
		}
		else if (!(TmpByte & BIT3) && (TmpByte & BIT4))
		{
			//printk("Digital Composite\n");
		}
		else if ((TmpByte & BIT3) && (TmpByte & BIT4))
		{
			//printk("Digital Separate\n");
		}

		////printk("\n");
	}
	return true;
}

/****************************************************************************
*	name	=	edid_set_output_format
*	func	=	Based on the values read from EDID and the formats supported by edid edid
				Output format is set
*	input	=	
*	output	=	None
*	return	=	ret
****************************************************************************/ 
int edid_set_output_format(void)
{
	u8 i;
	int ret = 0;
	u8 len_supported_dtds = sizeof(supported_dtds) / sizeof(struct dtd);
	printk(KERN_INFO "set_output_format\n");
	for (i = 0; i < len_supported_dtds; i++) {
		/*printk(KERN_INFO "%s supported_dtds[i].outputformat = %d\n",*/
		/*__func__,supported_dtds[i].outputformat);*/
		if ((supported_dtds[i].pixelFrequency ==
			edidreadvalues.pixelFrequency) &&
			(supported_dtds[i].hActive == edidreadvalues.hActive) &&
			(supported_dtds[i].hBlank == edidreadvalues.hBlank) &&
			(supported_dtds[i].vActive == edidreadvalues.vActive) &&
			(supported_dtds[i].vBlank == edidreadvalues.vBlank)) {

			outputformat = supported_dtds[i].outputformat;
			printk(KERN_INFO"%s outputformat = %d\n", __func__, outputformat);
			if (edidreadvalues.vActive == VIRT_ACTIVE_PIXELS_1080) {
				/*printk(KERN_INFO "minverticalrate %d MIN_VERT_RATE %d\n",*/
				/*minverticalrate,MIN_VERT_RATE);*/
				if (minverticalrate <= MIN_VERT_RATE) {
					printk(KERN_INFO "%s setting 1080p 24Hz\n", __func__);
					outputformat = edid_CEA32_1920X1080P_24HZ;
					} else {
						printk(KERN_INFO "%s setting 720p 60Hz\n", __func__);
						outputformat = edid_CEA4_1280X720P_60HZ;
						/*1080p 50Hz/60Hz not supported so settinf 720p60Hz.*/
				}
			}
			return ret;
		}
	}
	outputformat = edid_CEA4_1280X720P_60HZ; /*	AV7100_CEA4_1280X720P_60HZ;*/
	printk(KERN_INFO "%s outputformat = %d\n", __func__, outputformat);
	return ret;
}

/****************************************************************************
*	name	=	parse861longdescriptors
*	func	=	
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/ 
static bool parse861longdescriptors(struct edid_tx *edid)
{
    u8 LongDescriptorsOffset;
    u8 DescriptorNum = 1;

    LongDescriptorsOffset = edid->edid_block1[LONG_DESCR_PTR_IDX];   // EDID block offset 2 holds the offset

    if (!LongDescriptorsOffset)                         // per CEA-861-D, table 27
    {
        printk("EDID -> No Detailed Descriptors\n");
        return false;
    }

    // of the 1st 18-byte descriptor
    while (LongDescriptorsOffset + LONG_DESCR_LEN < EDID_BLOCK_SIZE)
    {
        printk("Parse Results - CEA-861 Long Descriptor #%d:\n", (int) DescriptorNum);
        printk("===============================================================\n");


        if (!ParseDetailedTiming(edid,LongDescriptorsOffset, EDID_BLOCK_2_3))
    		{
    			return false;
    		}

        LongDescriptorsOffset +=  LONG_DESCR_LEN;
        DescriptorNum++;
    }

    return true;
}  
/****************************************************************************
*	name	=	parse861shortdescriptors
*	func	=	
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/ 
static bool parse861shortdescriptors (struct edid_tx *edid)
{
  u8 LongDescriptorOffset;
  u8 DataBlockLength;
  u8 DataIndex;
  u8 ExtendedTagCode;
  u8 VSDB_BaseOffset = 0;

  u8 V_DescriptorIndex = 0;  // static to support more than one extension
  u8 A_DescriptorIndex = 0;  // static to support more than one extension

  u8 TagCode;

  u8 i;
  u8 j;
	printk("%s start\n",__func__);
  if (edid->edid_block1[EDID_TAG_ADDR] != EDID_EXTENSION_TAG){
      printk("EDID -> Extension Tag Error\n");
      return false;
  }

  if (edid->edid_block1[EDID_REV_ADDR] != EDID_REV_THREE){
      printk("EDID -> Revision Error\n");
      return false;
  }

  LongDescriptorOffset = edid->edid_block1[LONG_DESCR_PTR_IDX];    // block offset where long descriptors start

  edid->EDID_Data.UnderScan = ((edid->edid_block1[MISC_SUPPORT_IDX]) >> 7) & BIT0;  // byte #3 of CEA extension version 3
  edid->EDID_Data.BasicAudio = ((edid->edid_block1[MISC_SUPPORT_IDX]) >> 6) & BIT0;
  edid->EDID_Data.YCbCr_4_4_4 = ((edid->edid_block1[MISC_SUPPORT_IDX]) >> 5) & BIT0;
  edid->EDID_Data.YCbCr_4_2_2 = ((edid->edid_block1[MISC_SUPPORT_IDX]) >> 4) & BIT0;

  if (edid->EDID_Data.YCbCr_4_4_4 == 1)
	{
		////printk("EDID -> EDID_Data.YCbCr_4_4_4 \n");

		//outputColorSpace = BIT_TPI_OUTPUT_FORMAT_YCbCr444;
	}
	else if (edid->EDID_Data.YCbCr_4_2_2 == 1)
	{
		////printk("EDID -> EDID_Data.YCbCr_4_2_2 \n");

		//outputColorSpace = BIT_TPI_OUTPUT_FORMAT_YCbCr422;
	}
	else
	{
		////printk("EDID -> EDID_Data.RGB \n");

		//outputColorSpace = BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB;
	}

  DataIndex = EDID_DATA_START;            // 4

  while (DataIndex < LongDescriptorOffset)
  {
      TagCode = (edid->edid_block1[DataIndex] >> 5) & THREE_LSBITS;
      DataBlockLength = edid->edid_block1[DataIndex++] & FIVE_LSBITS;
      if ((DataIndex + DataBlockLength) > LongDescriptorOffset)
      {
          ////printk("EDID -> V Descriptor Overflow\n");
          return false;
      }

      i = 0;                                  // num of short video descriptors in current data block

      switch (TagCode)
      {
          case VIDEO_D_BLOCK:
			////printk("EDID -> Video data block DataBlockLength = %d \n",DataBlockLength);
            while ((i < DataBlockLength) && (i < MAX_V_DESCRIPTORS))        // each SVD is 1 byte long
            {
                edid->EDID_Data.VideoDescriptor[V_DescriptorIndex++] = edid->edid_block1[DataIndex++];
                i++;
            }
            DataIndex += DataBlockLength - i;   // if there are more STDs than MAX_V_DESCRIPTORS, skip the last ones. Update DataIndex

            ////printk("EDID -> Short Descriptor Video Block\n");
            break;

          case AUDIO_D_BLOCK:
			////printk("EDID -> Audio data block DataBlockLength = %d \n",DataBlockLength);
            while (i < DataBlockLength/3)       // each SAD is 3 bytes long
            {
                j = 0;
                while (j < AUDIO_DESCR_SIZE)    // 3
                {
                    edid->EDID_Data.AudioDescriptor[A_DescriptorIndex][j++] = edid->edid_block1[DataIndex++];
                }
                A_DescriptorIndex++;
                i++;
            }
            ////printk("EDID -> Short Descriptor Audio Block\n");
            break;

          case SPKR_ALLOC_D_BLOCK:
				////printk("EDID -> speaker alloc  data block DataBlockLength = %d \n",DataBlockLength);
              edid->EDID_Data.SpkrAlloc[i++] = edid->edid_block1[DataIndex++];       // although 3 bytes are assigned to Speaker Allocation, only
              DataIndex += 2;                                     // the first one carries information, so the next two are ignored by this code.
              ////printk("EDID -> Short Descriptor Speaker Allocation Block\n");
            break;

          case USE_EXTENDED_TAG:
				////printk("EDID -> rxtended tag data block DataBlockLength = %d \n",DataBlockLength);
              ExtendedTagCode = edid->edid_block1[DataIndex++];

              switch (ExtendedTagCode)
              {
                  case VIDEO_CAPABILITY_D_BLOCK:

  					        ////printk("EDID -> Short Descriptor Video Capability Block\n");

          					// TO BE ADDED HERE: Save "video capability" parameters in EDID_Data data structure
          					// Need to modify that structure definition
          					// In the meantime: just increment DataIndex by 1

  					        DataIndex += 1;    // replace with reading and saving the proper data per CEA-861 sec. 7.5.6 while incrementing DataIndex

					        break;

                  case COLORIMETRY_D_BLOCK:
                        edid->EDID_Data.ColorimetrySupportFlags = edid->edid_block1[DataIndex++] & TWO_LSBITS;
                        edid->EDID_Data.MetadataProfile = edid->edid_block1[DataIndex++] & THREE_LSBITS;

  					        ////printk("EDID -> Short Descriptor Colorimetry Block\n");
					        break;
              }
			    break;

          case VENDOR_SPEC_D_BLOCK:
              VSDB_BaseOffset = DataIndex - 1;
				////printk("EDID -> VENDOR_SPEC_D_BLOCK\n");
              if ((edid->edid_block1[DataIndex++] == 0x03) &&    // check if sink is HDMI compatible
                  (edid->edid_block1[DataIndex++] == 0x0C) &&
                  (edid->edid_block1[DataIndex++] == 0x00)){
                  edid->EDID_Data.HDMI_Sink = true;
                  edid->hdmi_sink = true;
              }
              else{
                  edid->EDID_Data.HDMI_Sink = false;
                  edid->hdmi_sink = false;
              }
              
              edid->EDID_Data.CEC_A_B = edid->edid_block1[DataIndex++];  // CEC Physical address
              edid->EDID_Data.CEC_C_D = edid->edid_block1[DataIndex++];

              if ((DataIndex + 7) > VSDB_BaseOffset + DataBlockLength){        // Offset of 3D_Present bit in VSDB
                      edid->EDID_Data._3D_Supported = false;
              }
              else if (edid->edid_block1[DataIndex + 7] >> 7){
                      edid->EDID_Data._3D_Supported = true;
              }
              else{
                      edid->EDID_Data._3D_Supported = false;
              }
              DataIndex += DataBlockLength - HDMI_SIGNATURE_LEN - CEC_PHYS_ADDR_LEN; // Point to start of next block
              ////printk("EDID -> Short Descriptor Vendor Block\n");
              ////printk("\n");
            break;

          default:
              ////printk("EDID -> Unknown Tag Code\n");
              return false;

      }                   // End, Switch statement
  }                       // End, while (DataIndex < LongDescriptorOffset) statement

    return true;
}

static void parsebock0_timingdescriptors(struct edid_tx *edid)
{
  u8 i=0;
  u8 offset = 0, ar_code = 0;

  printk("Parsing Established Timing:\n");
  printk("===========================\n");
  
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT7)
      printk("720 x 400 @ 70Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT6)
      printk("720 x 400 @ 88Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT5)
      printk("640 x 480 @ 60Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT4)
      printk("640 x 480 @ 67Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT3)
      printk("640 x 480 @ 72Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT2)
      printk("640 x 480 @ 75Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT1)
      printk("800 x 600 @ 56Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX] & BIT0)
      printk("800 x 400 @ 60Hz\n");

  // Parse Established Timing Byte #1:

  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT7)
      printk("800 x 600 @ 72Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT6)
      printk("800 x 600 @ 75Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT5)
      printk("832 x 624 @ 75Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT4)
      //printk("1024 x 768 @ 87Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT3)
      //printk("1024 x 768 @ 60Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT2)
      //printk("1024 x 768 @ 70Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT1)
      //printk("1024 x 768 @ 75Hz\n");
  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  & BIT0)
      //printk("1280 x 1024 @ 75Hz\n");

  // Parse Established Timing Byte #2:

  if(edid->edid_block0[ESTABLISHED_TIMING_INDEX + 2] & 0x80)
      //printk("1152 x 870 @ 75Hz\n");

  if((!edid->edid_block0[0])&&(!edid->edid_block0[ESTABLISHED_TIMING_INDEX + 1]  )&&(!edid->edid_block0[2]))
      //printk("No established video modes\n");

  //printk("Parsing Standard Timing:\n");
  //printk("========================\n");  


  for (i = 0; i < NUM_OF_STANDARD_TIMINGS; i += 2)
  {
    if ((edid->edid_block0[STANDARD_TIMING_OFFSET + i] == 0x01) && ((edid->edid_block0[STANDARD_TIMING_OFFSET + i +1]) == 1))
    {
      //printk("Standard Timing Undefined\n"); 
    }
    else
    {
      //printk("Horizontal Active pixels: %i\n", (int)((edid->edid_block0[STANDARD_TIMING_OFFSET + i] + 31)*8));

    ar_code = (edid->edid_block0[STANDARD_TIMING_OFFSET + i +1] & TWO_MSBITS) >> 6;
    //printk("Aspect Ratio: ");

    switch(ar_code)
    {
      case AR16_10:
      //printk("16:10\n");
      break;

      case AR4_3:
      //printk("4:3\n");
      break;

      case AR5_4:
      //printk("5:4\n");
      break;

      case AR16_9:
      //printk("16:9\n");
      break;
    }
    }
  }

  for (i = 0; i < NUM_OF_DETAILED_DESCRIPTORS; i++)
  {
      offset = DETAILED_TIMING_OFFSET + (LONG_DESCR_LEN * i);
      ParseDetailedTiming(edid, offset, EDID_BLOCK_0);
  }
    
}

#if 0
{
	int ret = 0;
	u8 dev_addr;
	u8 buf1[] = { reg >> 8, reg & 0xFF };
	u8 buf2[] = { data };
	struct i2c_msg msg1 = { .flags = 0, .buf = buf1, .len = 2 };
	struct i2c_msg msg2 = { .flags = 0, .buf = buf2, .len = 1 };

	dev_addr = priv->config->demod_address;
	msg1.addr = dev_addr;
	msg2.addr = dev_addr;

	if (debug >= 2)
		////printk(KERN_DEBUG "%s: reg=0x%04X, data=0x%02X\n",
			__func__, reg, data);

	ret = i2c_transfer(priv->i2c, &msg1, 1);
	if (ret != 1)
		return -EIO;

	ret = i2c_transfer(priv->i2c, &msg2, 1);
	return (ret != 1) ? -EIO : 0;
}
#endif
/****************************************************************************
*	name	=	read_edid_block0
*	func	=	
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/ 
static int  read_edid_block0(struct edid_tx *edid)
{
	int ret, retries = 10;
	struct i2c_client *client_ptr;
	unsigned char data[1];
	data[0] = 0x00;
	struct i2c_msg msg1[] = {
		{ .addr = 0xA0 >> 1,
		  .flags = 0,
		  .len = 1,
		  .buf = data },
		{ .addr = 0xA0 >> 1,
		  .flags = I2C_M_RD,
		  .len = 0x80,
		  .buf = &edid->edid_block0[0] } 
	};
	/*printk(KERN_INFO "edid: %s():%d !\n", __func__, __LINE__);*/
	client_ptr = get_edidI2C_client(edid->pdata, DDC_0xA0);
	
	do {
		ret = i2c_transfer(client_ptr->adapter, msg1, 2);
	} while (ret != 2 && --retries);
	
	/*printk(KERN_INFO "edid: %s():%d !\n", __func__, ret);*/
	return ret;
}
/****************************************************************************
*	name	=	read_edid_block1
*	func	=
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/
static int  read_edid_block1(struct edid_tx *edid)
{
	int ret, retries = 10;
	unsigned char data[1];
	struct i2c_client *client_ptr;
	data[0] = 0x80;		
	struct i2c_msg msg1[] = {
		{ .addr = 0xA0 >> 1,
		  .flags = 0,
		  .len = 1,
		  .buf = data },
		{ .addr = 0xA0 >> 1,
		  .flags = I2C_M_RD,
		  .len = 0x80,
		  .buf = &edid->edid_block1[0] } 
	};
	/*printk(KERN_INFO "edid: %s():%d !\n", __func__, __LINE__);*/
	client_ptr = get_edidI2C_client(edid->pdata, DDC_0xA0);
	
	do {
		ret = i2c_transfer(client_ptr->adapter, msg1, 2);
	} while (ret != 2 && --retries);
	/*printk(KERN_INFO "edid: %s():%d !\n", __func__, ret);*/
	return ret;
}
/****************************************************************************
*	name	=	read_edid_block2
*	func	=
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/
static int  read_edid_block2(struct edid_tx *edid)
{
	int ret, retries = 10;
	unsigned char data[1], data1[1] ;
	struct i2c_client *client_ptr1;

	data[0] = 0x01;
	data1[0] = 0x00;

	struct i2c_msg msg1[] = {
	{ .addr = 0x60 >> 1,
		.flags = 0,
		.len = 1,
		.buf = data },
		{ .addr = 0xA0 >> 1,
		.flags = 0,
		.len = 1,
		.buf = data1 },
		{ .addr = 0xA0 >> 1,
		.flags = I2C_M_RD,
		.len = 0x80,
		.buf = &edid->edid_block2[0] } };
		
	client_ptr1 = get_edidI2C_client(edid->pdata, DDC_0x60);
	/*struct i2c_client* client_ptr2 =
					get_edidI2C_client(edid->pdata, DDC_0xA0);*/
	do {
		ret = i2c_transfer(client_ptr1->adapter, msg1, 3);
	} while (ret != 3 && --retries);
	/*printk(KERN_INFO "edid: %s():%d !\n", __func__, ret);*/
	return ret;
}
/****************************************************************************
*	name	=	read_edid_block3
*	func	=
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/
static int  read_edid_block3(struct edid_tx *edid)
{
	int ret, retries = 10;
	unsigned char data[1], data1[1] ;
	struct i2c_client *client_ptr1;

	data[0] = 0x01;
	data1[0] = 0x80;

	struct i2c_msg msg1[] = {
	{ .addr = 0x60 >> 1,
		.flags = 0,
		.len = 1,
		.buf = data },
		{ .addr = 0xA0 >> 1,
		.flags = 0,
		.len = 1,
		.buf = data1 },
		{ .addr = 0xA0 >> 1,
		.flags = I2C_M_RD,
		.len = 0x80,
		.buf = &edid->edid_block3[0] } };
		
	client_ptr1 = get_edidI2C_client(edid->pdata, DDC_0x60);
	/*struct i2c_client* client_ptr2 =
				get_edidI2C_client(edid->pdata, DDC_0xA0);*/
	do {
	ret = i2c_transfer(client_ptr1->adapter, msg1, 3);
	} while (ret != 3 && --retries);
	/*printk(KERN_INFO "edid: %s():%d !\n", __func__, ret);*/
	return ret;
}
/****************************************************************************
*	name	=	edid_read
*	func	=	
*	input	=	struct edid_tx *edid
*	output	=	None
*	return	=	ret
****************************************************************************/ 
//int edid_read(struct edid_tx *edid)
int edid_read()
{
	int ret = 0;
	u8 block_cnt = 0;

	TH_FUNC_START;
	mutex_lock(&edid_global->i2c_lock);

	memset(&edid_global->edid_block0[0], 0x00, EDID_SIZE);
	memset(&edid_global->edid_block1[0], 0x00, EDID_SIZE);
	memset(&edid_global->edid_block2[0], 0x00, EDID_SIZE);
	memset(&edid_global->edid_block3[0], 0x00, EDID_SIZE);
	
	ret = read_edid_block0(edid_global);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
		__func__, __LINE__);
		mutex_unlock(&edid_global->i2c_lock);
		return ret;
	}
	
	if (!edid_header_check(edid_global)) {
		edid_global->hdmi_sink = false;
		/*mutex_unlock(&edid->i2c_lock);*/
		goto ddc_release;
	}
	if (!edid_checksum(edid_global, 0)) {
		edid_global->hdmi_sink = false;
		/*mutex_unlock(&edid->i2c_lock);*/
		goto ddc_release;
	}
	
	parsebock0_timingdescriptors(edid_global);
	block_cnt = edid_global->edid_block0[0x7E];
	printk(KERN_INFO "%s():%d  Block CNT:%d\n",
		__func__, __LINE__, block_cnt);

	if (!block_cnt) {
		edid_global->hdmi_sink = false;
		/*mutex_unlock(&edid->i2c_lock);*/
		goto ddc_release;
	}


	switch (block_cnt) {
	case 1:
		printk(KERN_INFO "%s():%d\n", __func__, __LINE__);
		ret = read_edid_block1(edid_global); /*edid_read_block1(edid);*/
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
				__func__, __LINE__);
			goto ddc_release;
		}

	/* commenting #if 0 */
	/* #if 0 */
	if (!parse861shortdescriptors(edid_global)) {
		edid_global->hdmi_sink = false;
		printk(KERN_INFO "edid: %s():%d failed !\n",
				__func__, __LINE__);
		/*mutex_unlock(&edid->i2c_lock);*/
		goto ddc_release;
	}

	if (!parse861longdescriptors(edid_global)) {
		edid_global->hdmi_sink = false;
				goto ddc_release;
			}
			/*#endif*/
			break;

	case 2:
		ret = read_edid_block1(edid_global); /*edid_read_block1(edid_global);*/
		if (ret < 0) {
				printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
				__func__, __LINE__);
				goto ddc_release;
			}

		/* commenting #if 0 */
		/* #if 0*/
		if (!parse861shortdescriptors(edid_global)) {
			edid_global->hdmi_sink = false;
			goto ddc_release;
		}
		if (!parse861longdescriptors(edid_global)) {
			edid_global->hdmi_sink = false;
			goto ddc_release;
		}
		/*#endif*/

		ret = read_edid_block2(edid_global);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
			__func__, __LINE__);
			goto ddc_release;
		}

	break;

	case 3:
		ret = read_edid_block1(edid_global); /*edid_read_block1(edid);*/
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
			__func__, __LINE__);
			goto ddc_release;
		}

	/* commenting #if 0 */
	/*#if 0*/
	if (!parse861shortdescriptors(edid_global)) {
		edid_global->hdmi_sink = false;
		goto ddc_release;
	}
	if (!parse861longdescriptors(edid_global)) {
		edid_global->hdmi_sink = false;
		goto ddc_release;
	}
	/*#endif*/
	ret = read_edid_block2(edid_global);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
		__func__, __LINE__);
		goto ddc_release;
	}

	ret = read_edid_block3(edid_global);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]edid: %s():%d failed !\n",
		__func__, __LINE__);
		goto ddc_release;
	}

	break;

		default:
			break;

		}

ddc_release:
		
	mutex_unlock(&edid_global->i2c_lock);
	return ret;
}

module_init(edid_init);
module_exit(edid_exit);

MODULE_DESCRIPTION("EDID Read and parse driver for sii8332");
MODULE_AUTHOR("Renesas Mobile");
MODULE_LICENSE("GPL");
