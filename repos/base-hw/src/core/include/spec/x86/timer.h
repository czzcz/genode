/*
 * \brief  Timer driver for core
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <base/stdint.h>
#include <base/printf.h>
#include <port_io.h>

namespace Genode
{
	/**
	 * Timer driver for core
	 */
	class Timer;
}

class Genode::Timer
{
	public:

		Timer()
		{
			/*
			 * Init PIT with lobyte/hibyte access, mode 0 (Interrupt on
			 * Terminal Count)
			 */
			outb(PIT_MODE, 0x30);
		}

		static unsigned interrupt_id(int)
		{
			PDBG("not implemented");
			return 0;
		}

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			outb(PIT_CH0, tics & 0xff);
			outb(PIT_CH0, tics >> 8);
		}

		static uint32_t ms_to_tics(unsigned const ms)
		{
			return (PIT_TICK_RATE / 1000) * ms;
		}

		unsigned value(unsigned)
		{
			PDBG("not implemented");
			return 0;
		}

	private:

		enum {
			PIT_TICK_RATE = 1193182ul,

			PIT_CH0  = 0x40,
			PIT_MODE = 0x43,
		};
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
