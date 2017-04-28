#ifndef __ASM_MACH_INTC_H
#define __ASM_MACH_INTC_H
#include <linux/sh_intc.h>

#define INTC_IRQ_PINS_ENUM_16L(p)				\
	p ## _IRQ0, p ## _IRQ1, p ## _IRQ2, p ## _IRQ3,		\
	p ## _IRQ4, p ## _IRQ5, p ## _IRQ6, p ## _IRQ7,		\
	p ## _IRQ8, p ## _IRQ9, p ## _IRQ10, p ## _IRQ11,	\
	p ## _IRQ12, p ## _IRQ13, p ## _IRQ14, p ## _IRQ15

#define INTC_IRQ_PINS_ENUM_16H(p)				\
	p ## _IRQ16, p ## _IRQ17, p ## _IRQ18, p ## _IRQ19,	\
	p ## _IRQ20, p ## _IRQ21, p ## _IRQ22, p ## _IRQ23,	\
	p ## _IRQ24, p ## _IRQ25, p ## _IRQ26, p ## _IRQ27,	\
	p ## _IRQ28, p ## _IRQ29, p ## _IRQ30, p ## _IRQ31

#define INTC_IRQ_PINS_VECT_16L(p, vect, base)				\
	vect(p ## _IRQ0, base + 0x000), vect(p ## _IRQ1, base + 0x020),	\
	vect(p ## _IRQ2, base + 0x040), vect(p ## _IRQ3, base + 0x060),	\
	vect(p ## _IRQ4, base + 0x080), vect(p ## _IRQ5, base + 0x0a0),	\
	vect(p ## _IRQ6, base + 0x0c0), vect(p ## _IRQ7, base + 0x0e0),	\
	vect(p ## _IRQ8, base + 0x100), vect(p ## _IRQ9, base + 0x120),	\
	vect(p ## _IRQ10, base + 0x140), vect(p ## _IRQ11, base + 0x160), \
	vect(p ## _IRQ12, base + 0x180), vect(p ## _IRQ13, base + 0x1a0), \
	vect(p ## _IRQ14, base + 0x1c0), vect(p ## _IRQ15, base + 0x1e0)

#define INTC_IRQ_PINS_VECT_16H(p, vect, base)				\
	vect(p ## _IRQ16, base + 0x000), vect(p ## _IRQ17, base + 0x020), \
	vect(p ## _IRQ18, base + 0x040), vect(p ## _IRQ19, base + 0x060), \
	vect(p ## _IRQ20, base + 0x080), vect(p ## _IRQ21, base + 0x0a0), \
	vect(p ## _IRQ22, base + 0x0c0), vect(p ## _IRQ23, base + 0x0e0), \
	vect(p ## _IRQ24, base + 0x100), vect(p ## _IRQ25, base + 0x120), \
	vect(p ## _IRQ26, base + 0x140), vect(p ## _IRQ27, base + 0x160), \
	vect(p ## _IRQ28, base + 0x180), vect(p ## _IRQ29, base + 0x1a0), \
	vect(p ## _IRQ30, base + 0x1c0), vect(p ## _IRQ31, base + 0x1e0)

#define INTC_IRQ_PINS_MASK_16L(p, base)					\
	{ base + 0x40, base + 0x60, 8, /* INTMSK00A / INTMSKCLR00A */	\
	  { p ## _IRQ0, p ## _IRQ1, p ## _IRQ2, p ## _IRQ3,		\
	    p ## _IRQ4, p ## _IRQ5, p ## _IRQ6, p ## _IRQ7 } },		\
	{ base + 0x44, base + 0x64, 8, /* INTMSK10A / INTMSKCLR10A */	\
	  { p ## _IRQ8, p ## _IRQ9, p ## _IRQ10, p ## _IRQ11,		\
	    p ## _IRQ12, p ## _IRQ13, p ## _IRQ14, p ## _IRQ15 } }

#define INTC_IRQ_PINS_MASK_16H(p, base)					\
	{ base + 0x48, base + 0x68, 8, /* INTMSK20A / INTMSKCLR20A */	\
	  { p ## _IRQ16, p ## _IRQ17, p ## _IRQ18, p ## _IRQ19,		\
	    p ## _IRQ20, p ## _IRQ21, p ## _IRQ22, p ## _IRQ23 } },	\
	{ base + 0x4c, base + 0x6c, 8, /* INTMSK30A / INTMSKCLR30A */	\
	  { p ## _IRQ24, p ## _IRQ25, p ## _IRQ26, p ## _IRQ27,		\
	    p ## _IRQ28, p ## _IRQ29, p ## _IRQ30, p ## _IRQ31 } }

#define INTC_IRQ_PINS_PRIO_16L(p, base)					\
	{ base + 0x10, 0, 32, 4, /* INTPRI00A */			\
	  { p ## _IRQ0, p ## _IRQ1, p ## _IRQ2, p ## _IRQ3,		\
	    p ## _IRQ4, p ## _IRQ5, p ## _IRQ6, p ## _IRQ7 } },		\
	{ base + 0x14, 0, 32, 4, /* INTPRI10A */			\
	  { p ## _IRQ8, p ## _IRQ9, p ## _IRQ10, p ## _IRQ11,		\
	    p ## _IRQ12, p ## _IRQ13, p ## _IRQ14, p ## _IRQ15 } }

#define INTC_IRQ_PINS_PRIO_16H(p, base)					\
	{ base + 0x18, 0, 32, 4, /* INTPRI20A */			\
	  { p ## _IRQ16, p ## _IRQ17, p ## _IRQ18, p ## _IRQ19,		\
	    p ## _IRQ20, p ## _IRQ21, p ## _IRQ22, p ## _IRQ23 } },	\
	{ base + 0x1c, 0, 32, 4, /* INTPRI30A */			\
	  { p ## _IRQ24, p ## _IRQ25, p ## _IRQ26, p ## _IRQ27,		\
	    p ## _IRQ28, p ## _IRQ29, p ## _IRQ30, p ## _IRQ31 } }

#define INTC_IRQ_PINS_SENSE_16L(p, base)				\
	{ base + 0x00, 32, 4, /* ICR1A */				\
	  { p ## _IRQ0, p ## _IRQ1, p ## _IRQ2, p ## _IRQ3,		\
	    p ## _IRQ4, p ## _IRQ5, p ## _IRQ6, p ## _IRQ7 } },		\
	{ base + 0x04, 32, 4, /* ICR2A */				\
	  { p ## _IRQ8, p ## _IRQ9, p ## _IRQ10, p ## _IRQ11,		\
	    p ## _IRQ12, p ## _IRQ13, p ## _IRQ14, p ## _IRQ15 } }

#define INTC_IRQ_PINS_SENSE_16H(p, base)				\
	{ base + 0x08, 32, 4, /* ICR3A */				\
	  { p ## _IRQ16, p ## _IRQ17, p ## _IRQ18, p ## _IRQ19,		\
	    p ## _IRQ20, p ## _IRQ21, p ## _IRQ22, p ## _IRQ23 } },	\
	{ base + 0x0c, 32, 4, /* ICR4A */				\
	  { p ## _IRQ24, p ## _IRQ25, p ## _IRQ26, p ## _IRQ27,		\
	    p ## _IRQ28, p ## _IRQ29, p ## _IRQ30, p ## _IRQ31 } }

#define INTC_IRQ_PINS_ACK_16L(p, base)					\
	{ base + 0x20, 0, 8, /* INTREQ00A */				\
	  { p ## _IRQ0, p ## _IRQ1, p ## _IRQ2, p ## _IRQ3,		\
	    p ## _IRQ4, p ## _IRQ5, p ## _IRQ6, p ## _IRQ7 } },		\
	{ base + 0x24, 0, 8, /* INTREQ10A */				\
	  { p ## _IRQ8, p ## _IRQ9, p ## _IRQ10, p ## _IRQ11,		\
	    p ## _IRQ12, p ## _IRQ13, p ## _IRQ14, p ## _IRQ15 } }

#define INTC_IRQ_PINS_ACK_16H(p, base)					\
	{ base + 0x28, 0, 8, /* INTREQ20A */				\
	  { p ## _IRQ16, p ## _IRQ17, p ## _IRQ18, p ## _IRQ19,		\
	    p ## _IRQ20, p ## _IRQ21, p ## _IRQ22, p ## _IRQ23 } },	\
	{ base + 0x2c, 0, 8, /* INTREQ30A */				\
	  { p ## _IRQ24, p ## _IRQ25, p ## _IRQ26, p ## _IRQ27,		\
	    p ## _IRQ28, p ## _IRQ29, p ## _IRQ30, p ## _IRQ31 } }

#define INTC_IRQ_PINS_16(p, base, base16l, vect, str)			\
									\
static struct resource p ## _resources[] __initdata = {			\
	[0] = {								\
		.start	= base,						\
		.end	= base + 0x64,					\
		.flags	= IORESOURCE_MEM,				\
	},								\
};									\
									\
enum {									\
	p ## _UNUSED = 0,						\
	INTC_IRQ_PINS_ENUM_16L(p),					\
};									\
									\
static struct intc_vect p ## _vectors[] __initdata = {			\
	INTC_IRQ_PINS_VECT_16L(p, vect, base16l),			\
};									\
									\
static struct intc_mask_reg p ## _mask_registers[] __initdata = {	\
	INTC_IRQ_PINS_MASK_16L(p, base),				\
};									\
									\
static struct intc_prio_reg p ## _prio_registers[] __initdata = {	\
	INTC_IRQ_PINS_PRIO_16L(p, base),				\
};									\
									\
static struct intc_sense_reg p ## _sense_registers[] __initdata = {	\
	INTC_IRQ_PINS_SENSE_16L(p, base),				\
};									\
									\
static struct intc_mask_reg p ## _ack_registers[] __initdata = {	\
	INTC_IRQ_PINS_ACK_16L(p, base),					\
};									\
									\
static struct intc_desc p ## _desc __initdata = {			\
	.name = str,							\
	.resource = p ## _resources,					\
	.num_resources = ARRAY_SIZE(p ## _resources),			\
	.hw = INTC_HW_DESC(p ## _vectors, NULL,				\
			     p ## _mask_registers, p ## _prio_registers, \
			     p ## _sense_registers, p ## _ack_registers) \
}

#define INTC_IRQ_PINS_32(p, base, base16l, base16h, vect, str)		\
									\
static struct resource p ## _resources[] __initdata = {			\
	[0] = {								\
		.start	= base,						\
		.end	= base + 0x6c,					\
		.flags	= IORESOURCE_MEM,				\
	},								\
};									\
									\
enum {									\
	p ## _UNUSED = 0,						\
	INTC_IRQ_PINS_ENUM_16L(p),					\
	INTC_IRQ_PINS_ENUM_16H(p),					\
};									\
									\
static struct intc_vect p ## _vectors[] __initdata = {			\
	INTC_IRQ_PINS_VECT_16L(p, vect, base16l),			\
	INTC_IRQ_PINS_VECT_16H(p, vect, base16h),			\
};									\
									\
static struct intc_mask_reg p ## _mask_registers[] __initdata = {	\
	INTC_IRQ_PINS_MASK_16L(p, base),				\
	INTC_IRQ_PINS_MASK_16H(p, base),				\
};									\
									\
static struct intc_prio_reg p ## _prio_registers[] __initdata = {	\
	INTC_IRQ_PINS_PRIO_16L(p, base),				\
	INTC_IRQ_PINS_PRIO_16H(p, base),				\
};									\
									\
static struct intc_sense_reg p ## _sense_registers[] __initdata = {	\
	INTC_IRQ_PINS_SENSE_16L(p, base),				\
	INTC_IRQ_PINS_SENSE_16H(p, base),				\
};									\
									\
static struct intc_mask_reg p ## _ack_registers[] __initdata = {	\
	INTC_IRQ_PINS_ACK_16L(p, base),					\
	INTC_IRQ_PINS_ACK_16H(p, base),					\
};									\
									\
static struct intc_desc p ## _desc __initdata = {			\
	.name = str,							\
	.resource = p ## _resources,					\
	.num_resources = ARRAY_SIZE(p ## _resources),			\
	.hw = INTC_HW_DESC(p ## _vectors, NULL,				\
			     p ## _mask_registers, p ## _prio_registers, \
			     p ## _sense_registers, p ## _ack_registers) \
}

#define INTC_PINT_E_EMPTY
#define INTC_PINT_E_NONE 0, 0, 0, 0, 0, 0, 0, 0,
#define INTC_PINT_E(p)							\
	PINT ## p ## 0, PINT ## p ## 1, PINT ## p ## 2, PINT ## p ## 3,	\
	PINT ## p ## 4, PINT ## p ## 5, PINT ## p ## 6, PINT ## p ## 7,

#define INTC_PINT_V_NONE
#define INTC_PINT_V(p, vect)					\
	vect(PINT ## p ## 0, 0), vect(PINT ## p ## 1, 1),	\
	vect(PINT ## p ## 2, 2), vect(PINT ## p ## 3, 3),	\
	vect(PINT ## p ## 4, 4), vect(PINT ## p ## 5, 5),	\
	vect(PINT ## p ## 6, 6), vect(PINT ## p ## 7, 7),

#define INTC_PINT(p, mask_reg, sense_base, str,				\
	enums_1, enums_2, enums_3, enums_4,				\
	vect_1, vect_2, vect_3, vect_4,					\
	mask_a, mask_b, mask_c, mask_d,					\
	sense_a, sense_b, sense_c, sense_d)				\
									\
enum {									\
	PINT ## p ## _UNUSED = 0,					\
	enums_1 enums_2 enums_3 enums_4 				\
};									\
									\
static struct intc_vect p ## _vectors[] __initdata = {			\
	vect_1 vect_2 vect_3 vect_4					\
};									\
									\
static struct intc_mask_reg p ## _mask_registers[] __initdata = {	\
	{ mask_reg, 0, 32, /* PINTER */					\
	  { mask_a mask_b mask_c mask_d } }				\
};									\
									\
static struct intc_sense_reg p ## _sense_registers[] __initdata = {	\
	{ sense_base + 0x00, 16, 2, /* PINTCR */			\
	  { sense_a } },						\
	{ sense_base + 0x04, 16, 2, /* PINTCR */			\
	  { sense_b } },						\
	{ sense_base + 0x08, 16, 2, /* PINTCR */			\
	  { sense_c } },						\
	{ sense_base + 0x0c, 16, 2, /* PINTCR */			\
	  { sense_d } },						\
};									\
									\
static struct intc_desc p ## _desc __initdata = {			\
	.name = str,							\
	.hw = INTC_HW_DESC(p ## _vectors, NULL,				\
			     p ## _mask_registers, NULL,		\
			     p ## _sense_registers, NULL),		\
}

#endif  /* __ASM_MACH_INTC_H */
