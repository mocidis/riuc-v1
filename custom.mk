CROSS_COMPILE:=1
ARMV7L:=1
LINUX_X86:=2
LINUX_X86_64:=3
MINGW_X86:=4
MACOS_X86_64:=5
ifeq ($(CROSS_COMPILE),$(ARM))
	CROSS_TOOL:=arm-linux-gnueabi-gcc
	LIBS_DIR:=../libs/linux-armv7l
	LIBS:= -L$(LIBS_DIR)/lib $(LIBS_DIR)/lib/libjson-c.a -lpjsua2-arm-none-linux-gnueabi -lstdc++ -lpjsua-arm-none-linux-gnueabi -lpjsip-ua-arm-none-linux-gnueabi -lpjsip-simple-arm-none-linux-gnueabi -lpjsip-arm-none-linux-gnueabi -lpjmedia-codec-arm-none-linux-gnueabi -lpjmedia-arm-none-linux-gnueabi -lpjmedia-videodev-arm-none-linux-gnueabi -lpjmedia-audiodev-arm-none-linux-gnueabi -lpjmedia-arm-none-linux-gnueabi -lpjnath-arm-none-linux-gnueabi -lpjlib-util-arm-none-linux-gnueabi -lsrtp-arm-none-linux-gnueabi -lresample-arm-none-linux-gnueabi -lgsmcodec-arm-none-linux-gnueabi -lspeex-arm-none-linux-gnueabi -lilbccodec-arm-none-linux-gnueabi -lg7221codec-arm-none-linux-gnueabi -lportaudio-arm-none-linux-gnueabi -lpj-arm-none-linux-gnueabi -lm -lrt -lpthread -lsqlite3 
endif
ifeq ($(CROSS_COMPILE),$(LINUX_X86))
	CROSS_TOOL:=gcc
	LIBS_DIR:=../libs/linux-i686
	LIBS:= -L$(LIBS_DIR)/lib $(LIBS_DIR)/lib/libjson-c.a -lpjsua2-i686-pc-linux-gnu -lstdc++ -lpjsua-i686-pc-linux-gnu -lpjsip-ua-i686-pc-linux-gnu -lpjsip-simple-i686-pc-linux-gnu -lpjsip-i686-pc-linux-gnu -lpjmedia-codec-i686-pc-linux-gnu -lpjmedia-i686-pc-linux-gnu -lpjmedia-videodev-i686-pc-linux-gnu -lpjmedia-audiodev-i686-pc-linux-gnu -lpjmedia-i686-pc-linux-gnu -lpjnath-i686-pc-linux-gnu -lpjlib-util-i686-pc-linux-gnu -lsrtp-i686-pc-linux-gnu -lresample-i686-pc-linux-gnu -lgsmcodec-i686-pc-linux-gnu -lspeex-i686-pc-linux-gnu -lilbccodec-i686-pc-linux-gnu -lg7221codec-i686-pc-linux-gnu -lportaudio-i686-pc-linux-gnu  -lpj-i686-pc-linux-gnu -lm -lrt -lpthread  -lasound -lpthread  -pthread -lm -lsqlite3
endif
ifeq ($(CROSS_COMPILE),$(LINUX_X86_64))
	CROSS_TOOL:=gcc
	LIBS_DIR:=../libs/linux-x86_64
	#LIBS:= -L$(LIBS_DIR)/lib $(LIBS_DIR)/lib/libjson-c.a -lpjsua2-i686-pc-linux-gnu -lstdc++ -lpjsua-i686-pc-linux-gnu -lpjsip-ua-i686-pc-linux-gnu -lpjsip-simple-i686-pc-linux-gnu -lpjsip-i686-pc-linux-gnu -lpjmedia-codec-i686-pc-linux-gnu -lpjmedia-i686-pc-linux-gnu -lpjmedia-videodev-i686-pc-linux-gnu -lpjmedia-audiodev-i686-pc-linux-gnu -lpjmedia-i686-pc-linux-gnu -lpjnath-i686-pc-linux-gnu -lpjlib-util-i686-pc-linux-gnu -lsrtp-i686-pc-linux-gnu -lresample-i686-pc-linux-gnu -lgsmcodec-i686-pc-linux-gnu -lspeex-i686-pc-linux-gnu -lilbccodec-i686-pc-linux-gnu -lg7221codec-i686-pc-linux-gnu -lportaudio-i686-pc-linux-gnu  -lpj-i686-pc-linux-gnu -lm -lrt -lpthread  -lasound -lpthread  -pthread -lm -lsqlite3
endif
ifeq ($(CROSS_COMPILE),$(MINGW_X86))
	CROSS_TOOL:=gcc
	LIBS_DIR:=../libs/mingw32-i586
endif
ifeq ($(CROSS_COMPILE),$(MACOS_X86_64))
	CROSS_TOOL:=gcc
	LIBS_DIR:=../libs/darwin-x86_64
endif
