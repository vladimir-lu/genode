/*
 * \brief  Dummy pager support for Genode
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IPC_PAGER_H_
#define _CORE__INCLUDE__IPC_PAGER_H_

#include <base/cache.h>
#include <base/ipc.h>
#include <base/stdint.h>
#include <base/native_types.h>

namespace Genode {

	class Mapping
	{
		private:

			addr_t _from_phys_addr;
			addr_t _to_virt_addr;
			size_t _num_pages;
			bool   _writeable;

			enum { PAGE_SIZE_LOG2 = 12 };

		public:

			/**
			 * Constructor
			 */
			Mapping(addr_t dst_addr, addr_t src_addr,
			        Cache_attribute const cacheability, bool io_mem,
			        unsigned l2size = PAGE_SIZE_LOG2,
			        bool rw = true)
			:
				_from_phys_addr(src_addr),
				_to_virt_addr(dst_addr),
				_num_pages(1 << (l2size - PAGE_SIZE_LOG2)),
				_writeable(rw)
			{ }

			/**
			 * Construct invalid mapping
			 */
			Mapping() : _num_pages(0) { }

			/**
			 * Prepare map operation
			 *
			 * No preparations are needed on Codezero because all mapping
			 * originate from the physical address space.
			 */
			void prepare_map_operation() { }

			addr_t from_phys() const { return _from_phys_addr; }
			addr_t to_virt()   const { return _to_virt_addr; }
			size_t num_pages() const { return _num_pages; }
			bool   writeable() const { return _writeable; }
	};


	/**
	 * Special paging server class
	 */
	class Ipc_pager : public Native_capability
	{
		private:

			Native_thread_id _last;      /* faulted thread                 */
			addr_t           _pf_addr;   /* page-fault address             */
			addr_t           _pf_ip;     /* instruction pointer of faulter */
			bool             _pf_write;  /* true on write fault            */

			Mapping _reply_mapping;

//		protected:
//
//			/**
//			 * Wait for pagefault
//			 */
//			void _wait();
//
//			/**
//			 * Send page-fault reply and wait for next page fault
//			 */
//			void _reply_and_wait();

		public:

			/**
			 * Constructor
			 */
			Ipc_pager();

			/**
			 * Wait for a new page fault received as short message IPC
			 */
			void wait_for_fault();

			/**
			 * Reply current page-fault and wait for a new one
			 */
			void reply_and_wait_for_fault();

			/**
			 * Request instruction pointer of current page fault
			 */
			addr_t fault_ip() { return _pf_ip; }

			/**
			 * Request fault address of current page fault
			 */
			addr_t fault_addr() { return _pf_addr; }

			/**
			 * Set parameters for next reply
			 */
			void set_reply_mapping(Mapping m) { _reply_mapping = m; }

			/**
			 * Set destination for next reply
			 */
			void set_reply_dst(Native_capability pager_object) {
				_last = pager_object.local_name(); }

			/**
			 * Answer call without sending a mapping
			 *
			 * This function is used to acknowledge local calls from one of
			 * core's region-manager sessions.
			 */
			void acknowledge_wakeup();

			/**
			 * Return thread ID of last faulter
			 */
			Native_thread_id last() const { return _last; }

			/**
			 * Return badge for faulting thread
			 */
			unsigned long badge() const { return _last; }

			/**
			 * Return true if page fault was a write fault
			 */
			bool is_write_fault() const { return _pf_write; }

			/**
			 * Return true if last fault was an exception
			 */
			bool is_exception() const
			{
				/*
				 * Reflection of exceptions is not supported on this platform.
				 */
				return false;
			}
	};
}

#endif /* _CORE__INCLUDE__IPC_PAGER_H_ */
