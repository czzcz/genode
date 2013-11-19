/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__THREAD_H_
#define _KERNEL__THREAD_H_

/* core includes */
#include <kernel/configuration.h>
#include <kernel/scheduler.h>
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <cpu_support.h>
#include <cpu.h>

namespace Genode
{
	class Platform_thread;
}

namespace Kernel
{
	class Thread;
	class Pd;

	typedef Genode::Cpu           Cpu;
	typedef Genode::Native_utcb   Native_utcb;

	void reset_lap_time();

	/**
	 * Kernel backend for userland execution-contexts
	 */
	class Thread;

	class Thread_ids : public Id_allocator<MAX_THREADS> { };
	typedef Object_pool<Thread> Thread_pool;

	Thread_ids  * thread_ids();
	Thread_pool * thread_pool();
}

class Kernel::Thread
:
	public Cpu::User_context,
	public Object<Thread, MAX_THREADS, Thread_ids, thread_ids, thread_pool>,
	public Execution_context,
	public Ipc_node,
	public Signal_context_killer,
	public Signal_receiver_killer,
	public Signal_handler,
	public Thread_cpu_support
{
	friend class Thread_event;

	private:

		enum { START_VERBOSE = 0 };

		enum State
		{
			SCHEDULED                   = 1,
			AWAITS_START                = 2,
			AWAITS_IPC                  = 3,
			AWAITS_RESUME               = 4,
			AWAITS_SIGNAL               = 5,
			AWAITS_SIGNAL_CONTEXT_KILL  = 6,
			AWAITS_SIGNAL_RECEIVER_KILL = 7,
			STOPPED                     = 8,
		};

		Platform_thread * const _platform_thread;
		State                   _state;
		Pd *                    _pd;
		Native_utcb *           _phys_utcb;
		Native_utcb *           _virt_utcb;
		Signal_receiver *       _signal_receiver;

		/**
		 * Notice that another thread yielded the CPU to this thread
		 */
		void _receive_yielded_cpu();

		/**
		 * Attach or detach the handler of a thread-triggered event
		 *
		 * \param event_id           kernel name of the thread event
		 * \param signal_context_id  kernel name signal context or 0 to detach
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _route_event(unsigned const event_id,
		                 unsigned const signal_context_id);

		/**
		 * Map kernel name of thread event to the corresponding member
		 *
		 * \param id  kernel name of targeted thread event
		 *
		 * \retval  0  failed
		 * \retval >0  targeted member pointer
		 */
		Thread_event Thread::* _event(unsigned const id) const;

		/**
		 * Return wether this is a core thread
		 */
		bool _core() const;

		/**
		 * Resume execution rawly
		 */
		void _schedule();

		/**
		 * Pause execution
		 */
		void _pause();

		/**
		 * Suspend unrecoverably from execution
		 */
		void _stop();

		/**
		 * Try to escape from blocking state, if in any, and resume execution
		 *
		 * \retval -1  failed
		 * \retval  0  succeeded, execution was paused
		 * \retval  1  succeeded, execution was not paused
		 */
		int _resume();

		/**
		 * Handle an exception thrown by the memory management unit
		 */
		void _mmu_exception();

		/**
		 * Handle kernel-call request of this thread
		 */
		void _call();

		/**
		 * Read a thread register
		 *
		 * \param id     kernel name of targeted thread register
		 * \param value  read-value buffer
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _read_reg(addr_t const id, addr_t & value) const;

		/**
		 * Override a thread register
		 *
		 * \param id     kernel name of targeted thread register
		 * \param value  write-value buffer
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _write_reg(addr_t const id, addr_t const value);

		/**
		 * Map kernel names of thread registers to the corresponding data
		 *
		 * \param id  kernel name of thread register
		 *
		 * \retval  0  failed
		 * \retval >0  pointer to register content
		 */
		addr_t Thread::* _reg(addr_t const id) const;


		/****************************************************************
		 ** Kernel-call backends, for details see 'kernel/interface.h' **
		 ****************************************************************/

		void _call_new_pd();
		void _call_kill_pd();
		void _call_new_thread();
		void _call_delete_thread();
		void _call_start_thread();
		void _call_pause_thread();
		void _call_resume_thread();
		void _call_yield_thread();
		void _call_current_thread_id();
		void _call_get_thread();
		void _call_wait_for_request();
		void _call_request_and_wait();
		void _call_reply();
		void _call_update_pd();
		void _call_update_region();
		void _call_print_char();
		void _call_new_signal_receiver();
		void _call_new_signal_context();
		void _call_await_signal();
		void _call_signal_pending();
		void _call_submit_signal();
		void _call_ack_signal();
		void _call_kill_signal_context();
		void _call_kill_signal_receiver();
		void _call_new_vm();
		void _call_run_vm();
		void _call_pause_vm();
		void _call_access_thread_regs();
		void _call_route_thread_event();


		/***************************
		 ** Signal_context_killer **
		 ***************************/

		void _signal_context_kill_pending();
		void _signal_context_kill_done();


		/****************************
		 ** Signal_receiver_killer **
		 ****************************/

		void _signal_receiver_kill_pending();
		void _signal_receiver_kill_done();


		/********************
		 ** Signal_handler **
		 ********************/

		void _await_signal(Signal_receiver * const receiver);
		void _receive_signal(void * const base, size_t const size);


		/**************
		 ** Ipc_node **
		 **************/

		void _received_ipc_request(size_t const s);
		void _await_ipc();
		void _await_ipc_succeeded(size_t const s);
		void _await_ipc_failed();

	public:

		/**
		 * Constructor
		 *
		 * \param platform_thread  corresponding userland object
		 */
		Thread(Platform_thread * const platform_thread);

		/**
		 * Prepare thread to get scheduled the first time
		 *
		 * \param ip         initial instruction pointer
		 * \param sp         initial stack pointer
		 * \param cpu_id     target cpu
		 * \param pd_id      target protection-domain
		 * \param utcb_phys  physical UTCB pointer
		 * \param utcb_virt  virtual UTCB pointer
		 * \param main       wether the thread is the first one in its PD
		 * \param start      wether to start execution
		 */
		void init(void * const ip, void * const sp, unsigned const cpu_id,
		          unsigned const pd_id, Native_utcb * const utcb_phys,
		          Native_utcb * const utcb_virt, bool const main,
		          bool const start);


		/***********************
		 ** Execution_context **
		 ***********************/

		void handle_exception();
		void proceed();


		/***************
		 ** Accessors **
		 ***************/

		Platform_thread * platform_thread() const { return _platform_thread; }
		unsigned          id() const { return Object::id(); }
		char const *      label() const;
		unsigned          pd_id() const;
		char const *      pd_label() const;
};

#endif /* _KERNEL__THREAD_H_ */
