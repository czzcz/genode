/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <vm_state.h>

/* core includes */
#include <kernel/vm.h>
#include <kernel/thread.h>

extern void *         _mt_nonsecure_entry_pic;
extern Genode::addr_t _tz_client_context;
extern Genode::addr_t _mt_master_context_begin;
extern Genode::addr_t _tz_master_context;

namespace Kernel
{
	Vm_ids * vm_ids() { return unmanaged_singleton<Vm_ids>(); }
	Vm_pool * vm_pool() { return unmanaged_singleton<Vm_pool>(); }
}

using namespace Kernel;


void Kernel::Thread::_call_new_vm()
{
	/* lookup signal context */
	auto const context = Signal_context::pool()->object(user_arg_4());
	if (!context) {
		PWRN("failed to lookup signal context");
		user_arg_0(0);
		return;
	}

	/* create virtual machine */
	typedef Genode::Cpu_state_modes Cpu_state_modes;
	void * const allocator = reinterpret_cast<void *>(user_arg_1());
	void * const table = reinterpret_cast<void *>(user_arg_3());
	Cpu_state_modes * const state =
		reinterpret_cast<Cpu_state_modes *>(user_arg_2());
	Vm * const vm = new (allocator) Vm(state, context, table);

	/* return kernel name of virtual machine */
	user_arg_0(vm->id());
}


Kernel::Vm::Vm(void                   * const state,
               Kernel::Signal_context * const context,
               void                   * const table)
:  Cpu_job(Cpu_priority::min, 0),
  _state((Genode::Vm_state * const)state),
  _context(context), _table(0)
{
	affinity(cpu_pool()->primary_cpu());

	Genode::memcpy(&_tz_master_context, &_mt_master_context_begin,
	               sizeof(Cpu_context));
}


void Vm::exception(unsigned const cpu)
{
	switch(_state->cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST:
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(cpu);
		return;
	case Genode::Cpu_state::DATA_ABORT:
		_state->dfar = Cpu::Dfar::read();
	default:
		pause();
		_context->submit(1);
	}
}


void Vm::proceed(unsigned const cpu)
{
	mtc()->switch_to(reinterpret_cast<Cpu::Context*>(_state), cpu,
	                 (addr_t)&_mt_nonsecure_entry_pic,
	                 (addr_t)&_tz_client_context);
}
