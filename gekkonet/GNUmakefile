gekkonet.path ?= .

gekkonet.src := \
    src/gekkonet.cpp \
    src/backend.cpp \
    src/event.cpp \
    src/gekko.cpp \
    src/input.cpp \
    src/net.cpp \
    src/player.cpp \
    src/storage.cpp \
    src/sync.cpp

gekkonet.include := \
    -I$(gekkonet.path)/include \
    -I$(gekkonet.path)/thirdparty \
    -I$(gekkonet.path)/thirdparty/asio

gekkonet.objects := $(gekkonet.src:src/%.cpp=obj/gekkonet-%.o)

gekkonet.flags := $(gekkonet.include) -DGEKKONET_STATIC

obj/gekkonet-%.o: $(gekkonet.path)/src/%.cpp
	@echo Compiling ../gekkonet/$(subst $(gekkonet.path)/,,$<) ...
	@$(compiler) -c -o $@ $< $(gekkonet.flags)

ifndef compiler
    $(error "compiler variable must be defined by the including Makefile")
endif

gekkonet.options :=

ifeq ($(platform),windows)
    gekkonet.options += -lws2_32
endif

ifeq ($(platform),macos)
endif

ifeq ($(platform),linux)
endif