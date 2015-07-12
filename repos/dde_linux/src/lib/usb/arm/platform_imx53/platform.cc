/*
 * \brief  EHCI for imx53 initialisation ocde
 * \author Vladimir Lushnikov
 * \date   2015-05-24
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <irq_session/connection.h>
#include <util/mmio.h>
#include <io_mem_session/connection.h>
#include <platform_session/connection.h>

#include <platform/platform.h>
#include <platform.h>

#include <extern_c_begin.h>
#include <lx_emul.h>
#include <linux/usb/otg.h>
#include <extern_c_end.h>

extern "C" void module_ci_hdrc_driver_init();
extern "C" void module_ci_hdrc_imx_driver_init();
extern "C" void module_ehci_hcd_init();

// FIXME - supplement the lx_emul environment
#if VERBOSE_LX_EMUL
#define TRACE lx_printf("\033[32m%s\033[0m called, not implemented\n", __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

bool queue_work(struct workqueue_struct *wq, struct work_struct *work) { TRACE; return 0; }

struct workqueue_struct *create_singlethread_workqueue(char *)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	return wq;
}

void destroy_workqueue(struct workqueue_struct *wq) { TRACE; }
void flush_workqueue(struct workqueue_struct *wq) { TRACE; }

// FIXME - make this configurable?
unsigned of_usb_get_dr_mode(struct device_node *np) { return USB_DR_MODE_HOST; }


/**
 * Base addresses
 */

enum {
	// FIXME - this isn't the EHCI base because that's offset
	CI_BASE = 0x53f80000,
	CI_SIZE = 0x200,

	CI_IRQ  = 18,
};

static resource _ci_resource[] = 
{
	{ CI_BASE, CI_BASE + CI_SIZE - 1, "ci_hdrc", IORESOURCE_MEM },
	{ CI_IRQ, CI_IRQ, "ci_hdrc_irq", IORESOURCE_IRQ }
};

struct Ehci : Genode::Mmio
{
	Ehci(Genode::addr_t const mmio_base) : Mmio(mmio_base)
	{
		write<UsbDeviceMode::ControllerMode>(0x3);
		if (read<UsbDeviceMode::ControllerMode>() != 0x3) {
			PDBG("WARNING: unable to set controller mode to host");
		}
	}

	struct UsbDeviceMode : Register<0x1a8, 32>
	{
		struct ControllerMode : Bitfield<0, 2> { };
	};
};

void set_usb_host_mode()
{
	/* set ehci controller in host mode (rather than the OTG default) */
	Genode::Io_mem_connection io_ehci(CI_BASE, CI_SIZE);
	Genode::addr_t ehci_base = (Genode::addr_t)Genode::env()->rm_session()->attach(io_ehci.dataspace());

	Ehci ehci(ehci_base);
	Genode::env()->rm_session()->detach(ehci_base);
}

void platform_hcd_init(Services *services)
{
	if (!services->ehci)
		return;

	module_ci_hdrc_driver_init();
	module_ci_hdrc_imx_driver_init();
	module_ehci_hcd_init();
	
	// FIXME temporary before we start with OTG mode
	set_usb_host_mode();

	// FIXME - USB_HCD clock settings seemingly not needed?
	//platform.enable(Platform::Session::USB_HCD);

	/* setup EHCI-controller platform device */
	platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
	pdev->name = (char *)"imx_usb";
	pdev->id   = 0;
	pdev->num_resources = 2;
	pdev->resource = _ci_resource;

	/*
	 * Needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c'
	 */
	static u64 dma_mask = ~(u64)0;
	pdev->dev.dma_mask = &dma_mask;
	pdev->dev.coherent_dma_mask = ~0;
	platform_device_register(pdev);
}

Genode::Irq_session_capability platform_irq_activate(int irq)
{
	try {
		Genode::Irq_connection conn(irq);
		conn.on_destruction(Genode::Irq_connection::KEEP_OPEN);
		return conn;
	} catch (...) { }

	return Genode::Irq_session_capability();
}
