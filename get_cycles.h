
/**
 * Copyright © 2012 Tino Reichardt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * taken from linux, we can add more arch specific stuff here
 */

#ifndef M_GET_CYCLES_H
#define M_GET_CYCLES_H

/**
 * when the current architecture has some cycle counter, it should
 * be implemented here
 *
 * /TR 2012-12-01
 */

typedef unsigned long long cycles_t;

#if defined(__i386__)		/* tested /TR */
static inline cycles_t get_cycles(void)
{
	cycles_t tick;
	asm volatile ("rdtsc":"=A" (tick));

	return tick;
}
#elif defined(__x86_64__)	/* tested /TR */
static inline cycles_t get_cycles(void)
{
	unsigned int tickl, tickh;
	asm volatile ("rdtsc":"=a" (tickl), "=d"(tickh));

	return ((cycles_t) tickh << 32) | tickl;
}
#elif defined(__s390__)		/* tested /TR */
static inline cycles_t get_cycles(void)
{
	cycles_t tick;
	asm volatile ("stck %0":"=Q" (tick)::"cc");

	return tick >> 2;
}
#elif defined(__alpha__)
static inline cycles_t get_cycles(void)
{
	unsigned int tick;
	asm volatile ("rpcc %0":"=r" (tick));

	return tick;
}
#elif defined(__sparc__)
static inline cycles_t get_cycles(void)
{
	long long tick;
	asm volatile ("rd %%tick,%0":"=r" (tick));

	return tick;
}
#elif defined(_MSC_VER)
static inline cycles_t get_cycles(void)
{
	return __rdtsc();
}
#else
/* arm / mips maybe? */
#warning "debugging with get_cycles() not supported..."
static inline cycles_t get_cycles(void)
{
	return 0;
}
#endif

#endif
