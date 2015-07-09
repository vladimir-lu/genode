/*
 * \brief  Serial output driver for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

/* Genode includes */
#include <drivers/uart/x86_uart_base.h>

namespace Genode { class Serial; }

/**
 * Serial output driver for core
 */
class Genode::Serial : public X86_uart_base
{
	private:

		enum { CLOCK_UNUSED = 0 };

		addr_t _port()
		{
			/**
			 * Read BDA (Bios Data Area) to obtain I/O ports of COM
			 * interfaces. The page must be mapped already,
			 * see crt0_translation_table.s.
			 */

			enum {
				MAP_ADDR_BDA         = 0x1ff000,

				BDA_SERIAL_BASE_COM1 = 0x400,
				BDA_EQUIPMENT_WORD   = 0x410,
				BDA_EQUIPMENT_SERIAL_COUNT_MASK  = 0x7,
				BDA_EQUIPMENT_SERIAL_COUNT_SHIFT = 9,
			};

			char * map_bda = reinterpret_cast<char *>(MAP_ADDR_BDA);
			uint16_t serial_count = *reinterpret_cast<uint16_t *>(map_bda + BDA_EQUIPMENT_WORD);
			serial_count >>= BDA_EQUIPMENT_SERIAL_COUNT_SHIFT;
			serial_count &= BDA_EQUIPMENT_SERIAL_COUNT_MASK;

			if (serial_count > 0)
				return *reinterpret_cast<uint16_t *>(map_bda + BDA_SERIAL_BASE_COM1);

			return 0;
		}

	public:

		Serial(unsigned baud_rate)
		: X86_uart_base(_port(), CLOCK_UNUSED, baud_rate) { }
};
