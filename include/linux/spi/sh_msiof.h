#ifndef __SPI_SH_MSIOF_H__
#define __SPI_SH_MSIOF_H__

#define DTDL_0		0x00000000
#define DTDL_1		0x00100000
#define DTDL_2		0x00200000
#define DTDL_0_5	0x00500000
#define DTDL_1_5	0x00600000
#define SYNCDL_0	0x00000000
#define SYNCDL_1	0x00010000
#define SYNCDL_2	0x00020000
#define SYNCDL_3	0x00030000
#define SYNCDL_0_5	0x00050000
#define SYNCDL_1_5	0x00060000

struct sh_msiof_spi_cs_info {
	s32 cs_port;
	u32 cs_delay;
};

struct sh_msiof_spi_info {
	int tx_fifo_override;
	int rx_fifo_override;
	u16 num_chipselect;
};

#endif /* __SPI_SH_MSIOF_H__ */
