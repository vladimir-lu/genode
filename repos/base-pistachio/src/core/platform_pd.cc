/*
 * \brief   Pistachio protection domain facility
 * \author  Christian Helmuth
 * \author  Julian Stecklina
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/lock_guard.h>
#include <util.h>
#include <platform_pd.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/thread.h>
#include <l4/sigma0.h>
#include <l4/schedule.h>
}

using namespace Pistachio;
using namespace Genode;


static const bool verbose = false;

#define PT_DBG(args...) if (verbose) { PDBG(args); } else { }


/**************************
 ** Static class members **
 **************************/

L4_Word_t Platform_pd::_core_utcb_ptr = 0;


/****************************
 ** Private object members **
 ****************************/

void Platform_pd::_create_pd(bool syscall)
{
	if (syscall) {
		PT_DBG("_create_pd (_l4_task_id = 0x%08lx)",
		       _l4_task_id.raw);

		/* create place-holder thread representing the PD */
		L4_ThreadId_t l4t = make_l4_id(_pd_id, 0, _version);
		L4_Word_t res = L4_ThreadControl(l4t, l4t, L4_Myself(), L4_nilthread, (void *)-1);
		L4_Set_Priority(l4t, 0);

		if (res == 0)
			panic("Task creation failed");

		_l4_task_id = l4t;

	} else {

		/* core case */
		if (!L4_SameThreads(L4_Myself(), _l4_task_id))
			panic("Core creation is b0rken");
	}

	_setup_address_space();
}


void Platform_pd::_destroy_pd()
{
	using namespace Pistachio;

	PT_DBG("_destroy_pd (_l4_task_id = 0x%08lx)",
	 _l4_task_id.raw);

	// Space Specifier == nilthread -> destroy
	L4_Word_t res = L4_ThreadControl(_l4_task_id, L4_nilthread,
	                                 L4_nilthread, L4_nilthread,
	                                 (void *)-1);

	if (res != 1) {
		panic("Destroying protection domain failed.");
	}

	_l4_task_id = L4_nilthread;
}


int Platform_pd::_alloc_pd(signed pd_id)
{
	if (pd_id == PD_INVALID) {
		unsigned i;

		for (i = PD_FIRST; i < PD_MAX; i++)
			if (_pds()[i].free) break;

		/* no free protection domains available */
		if (i == PD_MAX) return -1;

		pd_id = i;

	} else {
		if (!_pds()[pd_id].reserved || !_pds()[pd_id].free)
			return -1;
	}

	_pds()[pd_id].free = 0;

	_pd_id   = pd_id;
	_version = _pds()[pd_id].version;

	return pd_id;
}


void Platform_pd::_free_pd()
{
	unsigned id = _pd_id;

	if (_pds()[id].free) return;

	/* maximum reuse count reached leave non-free */
	if (_pds()[id].version++ == VERSION_MAX) return;

	_pds()[id].free = 1;
	++_pds()[id].version;
}


void Platform_pd::_init_threads()
{
	unsigned i;

	for (i = 0; i < THREAD_MAX; ++i)
		_threads[i] = 0;
}


Platform_thread* Platform_pd::_next_thread()
{
	unsigned i;

	/* look for bound thread */
	for (i = 0; i < THREAD_MAX; ++i)
		if (_threads[i]) break;

	/* no bound threads */
	if (i == THREAD_MAX) return 0;

	return _threads[i];
}


int Platform_pd::_alloc_thread(int thread_id, Platform_thread *thread)
{
	int i = thread_id;

	/* look for free thread */
	if (thread_id == Platform_thread::THREAD_INVALID) {

		/* start from 1 here, because thread 0 is our placeholder thread */
		for (i = 1; i < THREAD_MAX; ++i)
			if (!_threads[i]) break;

		/* no free threads available */
		if (i == THREAD_MAX) return -1;
	} else {
		if (_threads[i]) return -2;
	}

	_threads[i] = thread;

	return i;
}


void Platform_pd::_free_thread(int thread_id)
{
	if (!_threads[thread_id])
		PWRN("double-free of thread %x.%x detected", _pd_id, thread_id);

	_threads[thread_id] = 0;
}


/***************************
 ** Public object members **
 ***************************/

int Platform_pd::bind_thread(Platform_thread *thread)
{
	using namespace Pistachio;

	/* thread_id is THREAD_INVALID by default - only core is the special case */
	int thread_id = thread->thread_id();
	L4_ThreadId_t l4_thread_id;

	int t = _alloc_thread(thread_id, thread);
	if (t < 0) {
		PERR("thread alloc failed");
		return -1;
	}
	thread_id = t;
	l4_thread_id = make_l4_id(_pd_id, thread_id, _version);

	/* finally inform thread about binding */
	thread->bind(thread_id, l4_thread_id, this);

	if (verbose) _debug_log_threads();
	return 0;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	int thread_id = thread->thread_id();

	/* unbind thread before proceeding */
	thread->unbind();

	_free_thread(thread_id);

	if (verbose) _debug_log_threads();
}


void Platform_pd::touch_utcb_space()
{
	L4_Word_t utcb_ptr;

	L4_KernelInterfacePage_t *kip = get_kip();
	L4_ThreadId_t mylocalid = L4_MyLocalId();
	utcb_ptr = *(L4_Word_t *) &mylocalid;
	utcb_ptr &= ~(L4_UtcbAreaSize (kip) - 1);

	/* store a pointer to core's utcb area */
	_core_utcb_ptr = utcb_ptr;

	PT_DBG("Core's UTCB area is at 0x%08lx (0x%08lx)",
	 utcb_ptr, L4_UtcbAreaSize(kip));
	PWRN("Core can have %lu threads.",
	 L4_UtcbAreaSize(kip) / L4_UtcbSize(kip));

	/*
	 * We used to touch the UTCB space here, but that was probably not
	 * neccessary.
	 */
}


/* defined in genode.ld linker script */
extern "C" L4_Word_t _kip_utcb_area;


void Platform_pd::_setup_address_space()
{
	L4_ThreadId_t ss = _l4_task_id;

	/*
	 * Check whether the address space we are about to change is Core's.
	 * If it is, we need to do little more than filling in some values.
	 */
	if (L4_SameThreads(ss, L4_Myself())) {
		_kip_ptr  = (L4_Word_t)get_kip();
		_utcb_ptr = _core_utcb_ptr;
		return;
	}

	/* setup a brand new address space */

	L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *)get_kip();
	L4_Fpage_t kip_space = L4_FpageLog2((L4_Word_t)kip, L4_KipAreaSizeLog2(kip));
	PT_DBG("kip_start = %08lx", L4_Address(kip_space));

	/* utcb space follows the kip, but must be aligned */
	L4_Word_t kip_end = L4_Address(kip_space) + L4_KipAreaSize(kip);
	PT_DBG("kip_end = %08lx", kip_end);

	L4_Word_t utcb_start = _core_utcb_ptr;
//	L4_Word_t utcb_start = (L4_Word_t)(&_kip_utcb_area);
	PT_DBG("utcb_start = %08lx", utcb_start);
	L4_Word_t utcb_size = L4_UtcbSize(kip) * THREAD_MAX;
	PT_DBG("utcb_size = %08lx", utcb_size);

	L4_Fpage_t utcb_space = L4_Fpage(utcb_start,
	 // L4_Fpage truncates this.
	 utcb_size + get_page_size() - 1 );

	PT_DBG("Creating address space for %08lx.", ss.raw);

	L4_Word_t old_control;
	int res;

	res = L4_SpaceControl(ss, 0, kip_space, utcb_space, L4_anythread, &old_control);

	if (res != 1 ) {
		PERR("Error while setting up address space: %lu", L4_ErrorCode());
		panic("L4_SpaceControl");
	}

	PT_DBG("Address space for %08lx created!", ss.raw);

	_kip_ptr  = L4_Address(kip_space);
	_utcb_ptr = L4_Address(utcb_space);
}


L4_Word_t Platform_pd::_utcb_location(unsigned int thread_id)
{
  return _utcb_ptr + thread_id*L4_UtcbSize(get_kip());
}


Platform_pd::Platform_pd(bool core) :
	_l4_task_id(L4_MyGlobalId())
{
	/*
	 * Start with version 2 to avoid being mistaken as local or
	 * interrupt ID.
	 */
	Pd_alloc free(false, true, 2);

	_init_threads();

	/* init remainder */
	for (unsigned i = 0 ; i < PD_MAX; ++i) _pds()[i] = free;

	/* mark system threads as reserved */
	_pds()[0].reserved = 1;
	_pds()[0].free = 0;

	_pd_id = _alloc_pd(PD_INVALID);

	_create_pd(false);
}


Platform_pd::Platform_pd(Allocator * md_alloc, char const *,
                         signed pd_id, bool create)
{
	if (!create)
		panic("create must be true.");

	_init_threads();

	_pd_id = _alloc_pd(pd_id);

	if (_pd_id < 0) {
		PERR("pd alloc failed");
	}

	_create_pd(create);
}


Platform_pd::~Platform_pd()
{
	PT_DBG("Destroying all threads of pd %p", this);

	/* unbind all threads */
	while (Platform_thread *t = _next_thread()) unbind_thread(t);

	PT_DBG("Destroying pd %p", this);

	_destroy_pd();
	_free_pd();
}


/***********************
 ** Debugging support **
 ***********************/

void Platform_pd::_debug_log_threads()
{
	PWRN("_debug_log_threads disabled.");
}


void Platform_pd::_debug_log_pds()
{
	PWRN("_debug_log_pds disabled.");
}
