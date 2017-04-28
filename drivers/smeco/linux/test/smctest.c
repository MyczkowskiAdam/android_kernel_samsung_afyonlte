/*
* Copyright (c) 2013, Renesas Mobile Corporation.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if 0
/*
Change history:

Version:       1    20-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  File created, Linux Kernel specific code added

-------------------------------------------------------------------------------
*/
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/gfp.h>

#include <linux/platform_device.h>
#include <linux/netdevice.h>

MODULE_AUTHOR("Renesas Mobile Europe / Heikki Siikaluoma <heikki.siikaluoma@renesasmobile.com>");
MODULE_DESCRIPTION("SMeCo/SMC Test Application");
MODULE_LICENSE("Dual BSD/GPL");

#define VERSION "0.0.4"

#define SMC_TEST_PASSED  1
#define SMC_TEST_FAILED  0

#define SMC_TEST_PRINT(...)  printk( KERN_ALERT __VA_ARGS__ )

    /* Test Interface in Smeco Kernel Module */
extern uint8_t smc_test_linux_start(uint16_t test_case_id, uint16_t test_data_input_len, uint8_t* test_data_input);

    /* Test startup parameters */
static ushort    testdata[16] = {0,0,0,0,0,0,0,0,0,0,
                                 0,0,0,0,0,0};
static int       testdatalen  = 0;
static int       instantiate  = 0;

module_param(instantiate, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(instantiate, "Instantiate SMC Platform Driver");

module_param_array(testdata, ushort, &testdatalen, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(testdata, "SMC test case input data");

struct platform_device smc_netdevice0 =
{
  .name = "smc_net_device",
  .id = 0,
};

static struct platform_device *devices[] __initdata =
{
    &smc_netdevice0,
};

static int smc_test_init(void)
{
    uint8_t  test_ret        = SMC_TEST_FAILED;
    uint8_t* test_data_array = NULL;
    uint16_t test_data_len   = 0;
    uint8_t  test_case_id    = 0;
    int      i               = 0;

    SMC_TEST_PRINT("smc_test_init: SMC Test v.%s", VERSION);

    if( instantiate == 1 )
    {
        SMC_TEST_PRINT("smc_test_init: Instantiate SMC platform driver...");
        platform_add_devices(devices, ARRAY_SIZE(devices));

        SMC_TEST_PRINT("smc_test_init: SMC platform driver instantiated");
    }
    else
    {
        test_data_len = (uint16_t)( sizeof(testdata) / sizeof(ushort)) - 1;

        test_data_array = (uint8_t*)kmalloc(test_data_len, GFP_ATOMIC);

        test_case_id = testdata[0];

        for(i = 1; i < test_data_len; i++)
        {
            test_data_array[i-1] = testdata[i];
        }

        SMC_TEST_PRINT("smc_test_init: Run SMC Test case %d...", test_case_id);

        test_ret = smc_test_linux_start(test_case_id, test_data_len, test_data_array);

        SMC_TEST_PRINT("smc_test_init: SMC Test case %d completed. Test %s\n", test_case_id, test_ret?"PASSED":"FAILED");
    }

    return 0;
}

static void smc_test_exit(void)
{
    SMC_TEST_PRINT("smc_test_exit: SMC Test v.%s", VERSION);
}

module_init(smc_test_init);
module_exit(smc_test_exit);


/* EOF */


