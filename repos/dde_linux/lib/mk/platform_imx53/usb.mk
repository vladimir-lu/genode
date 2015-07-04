# TODO - enable CONFIG_USB_CHIPIDEA_UDC later
# 				udc.c

# TODO - don't think usbmisc_imx.c is needed at the moment

SRC_C   += $(addprefix usb/chipidea/, ci_hdrc_imx.c core.c debug.c host.c otg.c usbmisc_imx.c)

include $(REP_DIR)/lib/mk/usb.inc
include $(REP_DIR)/lib/mk/armv7/usb.inc

CC_OPT  += -DCONFIG_USB_EHCI_TT_NEWSCHED -DCONFIG_USB_PHY -DCONFIG_USB_CHIPIDEA_HOST -DCONFIG_USB_CHIPIDEA_DEBUG -DVERBOSE_DEBUG

# FIXME - enable DEBUG_TRACE on the C build later

SRC_CC  += platform.cc
vpath platform.cc $(LIB_DIR)/arm/platform_imx53
