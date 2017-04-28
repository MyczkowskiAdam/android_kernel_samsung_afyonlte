#ifndef __SH_TIMER_H__
#define __SH_TIMER_H__

struct sh_timer_clock {
	const char *name;
	unsigned int divisor;
};

struct sh_timer_config {
	char *name;
	long channel_offset;
	long channel_offset_p;
	int timer_bit;
	unsigned long clockevent_rating;
	unsigned long clocksource_rating;

	struct sh_timer_clock *cks_table;
	unsigned short cks_num;
	unsigned short cks;
	unsigned long cmcsr_init;
};

#endif /* __SH_TIMER_H__ */
