ERTM_VERSION ?= 1.0.0-rc1
DEPLOY_TARGET ?= /acc/local/L867/drv/ertm/$(ERTM_VERSION)
TOOLS = ../tools/uart-bootloader/usb-bootloader.py ertm-cli udev-find

deploy: $(LIBS) libertm.h $(TOOLS) ../wrc.bin ertm-setup
	mkdir -p $(DEPLOY_TARGET)/lib $(DEPLOY_TARGET)/include \
		$(DEPLOY_TARGET)/tools $(DEPLOY_TARGET)/bin
	ln -s ../pyenv $(DEPLOY_TARGET)
	install -b ../wrc.bin $(LIBS) -C $(DEPLOY_TARGET)/lib
	install -b libertm.h -C $(DEPLOY_TARGET)/include
	install -b $(TOOLS) -C $(DEPLOY_TARGET)/tools
	(cd $(DEPLOY_TARGET)/bin ; ln -sf ../tools/* .)
	install -b ertm-setup -C $(DEPLOY_TARGET)/bin
