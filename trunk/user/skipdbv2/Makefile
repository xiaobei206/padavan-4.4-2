THISDIR = $(shell pwd)
SRC = $(THISDIR)/skipdbv2
BUILD = $(THISDIR)/build/$(CONFIG_CROSS_COMPILE)

all: configure
	$(MAKE) -j$(HOST_NCPU) -C $(BUILD)

configure:
	( mkdir -p $(BUILD); cd $(BUILD) ; \
	CROSS_COMPILE="$(CONFIG_CROSS_COMPILE)-" \
	STAGEDIR="$(STAGEDIR)" \
	ARCH="$(ARCH)" \
	CONFIG_CROSS_COMPILER_ROOT="$(CONFIG_CROSS_COMPILER_ROOT)" \
	LDFLAGS="$(LDFLAGS) -L$(STAGEDIR)/lib" \
	cmake -DCMAKE_TOOLCHAIN_FILE="$(CONFIG_CMAKE_TOOLCHAIN_FILE)" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_BUILD_TYPE=Release \
		$(SRC) \
	)

clean:
	rm -rf $(THISDIR)/build
	rm -f config_done

romfs:
	$(ROMFSINST) -p +x $(BUILD)/bin/skipd /usr/sbin/skipd
	$(ROMFSINST) -p +x $(BUILD)/bin/dbus /usr/sbin/dbus
	
