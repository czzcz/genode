/*
 * \brief  x86 static kernel configuration
 * \author Martin Stein
 * \author Reto Buerki
 * \date   2015-03-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__CONFIGURATION_H_
#define _KERNEL__CONFIGURATION_H_

namespace Kernel
{
	enum {
		DEFAULT_STACK_SIZE   = 16 * 1024,
		MAX_PDS              = 256,
		MAX_THREADS          = 256,
		MAX_SIGNAL_RECEIVERS = 2048,
		MAX_SIGNAL_CONTEXTS  = 4096,
		MAX_VMS              = 4,
	};

	/* amount of priority bands amongst quota owners in CPU scheduling */
	constexpr unsigned cpu_priorities = 4;

	/* super period in CPU scheduling and the overall allocatable CPU time */
	constexpr unsigned cpu_quota_ms = 1000;

	/* time slice for the round-robin mode and the idle in CPU scheduling */
	constexpr unsigned cpu_fill_ms = 54;
}

#endif /* _KERNEL__CONFIGURATION_H_ */
