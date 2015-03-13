/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
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
			unsigned count, status;

			outb(PIT_MODE, PIT_READ_BACK);

			status = inb(PIT_CH0);
			count  = inb(PIT_CH0);
			count |= inb(PIT_CH0) << 8;

			/*
			 * If the timer fires, counting does not stop. Instead the counter
			 * is set to 0xffff by the hardware and continues to decrement
			 * (while OUT stays high). Return 0 if status indicates OUT high
			 * (timer fired)
			 *
			 * See the Intel 8254, Programmable Interval Timer document for more
			 * details.
			 */
			if ((status & 0x80) == 0x80)
			{
				return 0;
			}
			return count;
		}

	private:

		enum {
			PIT_TICK_RATE = 1193182ul,

			PIT_CH0       = 0x40,
			PIT_MODE      = 0x43,
			PIT_READ_BACK = 0x2c,
		};
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
