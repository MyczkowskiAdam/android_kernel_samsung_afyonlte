/*
 * rtds_memory_drv_cma.c
 *	 RT CMA device driver function file.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <asm/page.h>

#include "log_kernel.h"
#include "iccom_drv.h"
#include "rtds_memory_drv_cma.h"
#include <mach/memory-r8a7373.h>

struct device	*rt_cma_dev[CMA_DEV_CNT];

static int rt_cma_drv_probe(struct platform_device *pdev);
static int rt_cma_drv_remove(struct platform_device *pdev);
static int rt_cma_drv_init(void);
static void rt_cma_drv_exit(void);

static struct platform_driver rt_cma_driver = {
	.probe		= rt_cma_drv_probe,
	.remove		= __devexit_p(rt_cma_drv_remove),
	.driver		= {
		.name	= "rt_cma_dev",
		.owner	= THIS_MODULE,
	},
};

struct page *rt_cma_drv_alloc(unsigned int size, int id)
{
	struct page *pages = NULL;
	int	i;
	bool	flag;
	int	page_num = size / PAGE_SIZE;
	MSG_HIGH("[CMA DRV]IN |%s\n", __func__);
	MSG_MED("[CMA DRV]   |size:%dMiB\n", size / SZ_1M);
	MSG_MED("[CMA DRV]   |id:%d\n", id);

	for (i = 0; i < DRM_ALLOC_COUNT; i++) {
		pages = dma_alloc_from_contiguous(rt_cma_dev[id], page_num, 0);
		MSG_MED("[CMA DRV]   | pages:0x%08x\n", (u32)pages);
		if (pages) {
			MSG_MED("[CMA DRV]   | phys_addr:0x%08x\n",
				(u32)page_to_phys(pages));
			if (page_to_phys(pages) == SDRAM_DISPLAY_START_ADDR ||
			    page_to_phys(pages) ==
					SDRAM_OMX_RT_SHARED_START_ADDR) {
				break;
			} else {
				/* start addr not collect */
				/* free allocate & retry allocate */
				flag = dma_release_from_contiguous(
								rt_cma_dev[id],
								pages,
								page_num);
				if (!flag) {
					MSG_ERROR("[CMA DRV]ERR|" \
						  "[%s] L.%d\n",
						  __func__, __LINE__);
					panic("Memory release error[%s][%d]" \
						"err_code[%d]",
						__func__, __LINE__, flag);
					break;
				}
			}
		}
	}

	MSG_HIGH("[CMA DRV]OUT|%s\n", __func__);
	return pages;
}

int rt_cma_drv_free(struct page *pages, unsigned int size, int id)
{
	int ret = SMAP_OK;
	bool flag;
	MSG_HIGH("[CMA DRV]IN |%s\n", __func__);
	MSG_MED("[CMA DRV]   |page:0x%08x\n", (u32)pages);
	MSG_MED("[CMA DRV]   |size:%dMiB\n", size / SZ_1M);
	MSG_MED("[CMA DRV]   |id:%d\n", id);

	flag = dma_release_from_contiguous(rt_cma_dev[id],
					   pages,
					   (size / PAGE_SIZE));
	if (!flag)
		ret = SMAP_NG;

	MSG_HIGH("[CMA DRV]OUT|%s ret:%d\n", __func__, ret);
	return ret;
}

static int __devinit rt_cma_drv_probe(struct platform_device *pdev)
{
	MSG_HIGH("[CMA DRV]IN |%s\n", __func__);
	MSG_MED("[CMA DRV]   | id:%d\n", pdev->id);
	MSG_MED("[CMA DRV]   | &pdev->dev:0x%x\n", (u32)&pdev->dev);

	switch (pdev->id) {
	case OMX_MDL_ID:
	case DISPLAY_MDL_ID:
		rt_cma_dev[pdev->id] = &pdev->dev;
		MSG_LOW("[CMA DRV]   | id:%d dev:0x%08x\n",
			pdev->id, (u32)rt_cma_dev[pdev->id]);
		MSG_LOW("[CMA DRV]   | id:%d cma_area:0x%08x\n",
			pdev->id, (u32)rt_cma_dev[pdev->id]->cma_area);
		break;
	default:
		MSG_ERROR("[CMA DRV]ERR|id illeagal[%d]\n", pdev->id);
		break;
	}
	MSG_HIGH("[CMA DRV]OUT|%s\n", __func__);
	return 0;
}

static int __devexit rt_cma_drv_remove(struct platform_device *pdev)
{
	MSG_HIGH("[CMA DRV]IN |%s\n", __func__);
	MSG_HIGH("[CMA DRV]OUT|%s\n", __func__);
	return 0;
}

static int __init rt_cma_drv_init(void)
{
	int ret;
	MSG_HIGH("[CMA DRV]IN |%s\n", __func__);
	ret = platform_driver_register(&rt_cma_driver);
	MSG_HIGH("[CMA DRV]OUT|%s ret:%d\n", __func__, ret);
	return ret;
}

static void __exit rt_cma_drv_exit(void)
{
	MSG_HIGH("[CMA DRV]IN |%s\n", __func__);
	platform_driver_unregister(&rt_cma_driver);
	MSG_HIGH("[CMA DRV]OUT|%s\n", __func__);
	return;
}

module_init(rt_cma_drv_init);
module_exit(rt_cma_drv_exit);

