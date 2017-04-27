/**
 * Samsung debug inform.
 *
 * @author kumhyun.cho@samsung.com
 * @since 2012.05.29
 */

#include <asm/io.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <linux/kernel.h>
#include <mach/sec_debug_inform.h>

static struct sec_debug_inform sec_debug_inform = {
	.phys = SEC_DEBUG_INFORM_PHYS,
	.virt = (void __iomem*)SEC_DEBUG_INFORM_VIRT,
	.size = SZ_32,
};

static void sec_debug_inform_dump(const char* prefix) {
	pr_info("%s: sec_debug_inform={{phys=0x%lx virt=0x%p pfn=%lu}}},size=%d\n",
			prefix ? prefix : "",
			sec_debug_inform.phys,
			sec_debug_inform.virt,
			__phys_to_pfn(sec_debug_inform.phys),
			sec_debug_inform.size);
}

void sec_debug_inform_write(unsigned magic, unsigned index) {
	iowrite32(magic, 
			sec_debug_inform.virt 
			+ SEC_DEBUG_INFORM_OFFSET 
			+ (0x00000004 * index));
}

void sec_debug_inform_magic_write(unsigned magic) {
	iowrite32(magic, sec_debug_inform.virt);
}

void __iomem *sec_debug_inform_init(void) {
#if defined(CONFIG_SEC_DEBUG_INFORM_IOREMAP)
	sec_debug_inform.virt = ioremap_nocache(sec_debug_inform.phys, sec_debug_inform.size);
#else
	/** do nothing */
#endif

	sec_debug_inform_dump(__func__);

	return sec_debug_inform.virt;
}
