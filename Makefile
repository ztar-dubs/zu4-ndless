# Ultima IV for TI-Nspire CX CAS
# Based on zu4 (Zesty Ultima IV) - https://github.com/0ldsk00l/zu4
# Build system adapted from retrospire (MAME TI-Nspire port)

DEBUG = FALSE

GCC = nspire-gcc
GXX = nspire-g++
LD  = nspire-g++
GENZEHN = genzehn

# Base compiler flags (ARM, no unaligned access, LTO, size optimization)
GCCFLAGS = -Wall -W -marm -mno-unaligned-access \
           -ffunction-sections -fdata-sections \
           -fno-unwind-tables -fno-asynchronous-unwind-tables -flto

FLAGS_C = -std=c99
FLAGS_CXX = -std=gnu++11

# Include paths
# nspire/ MUST come first to intercept libxml2, GL, and SDL headers
INCLUDES = -Inspire -I. -Isrc/src -Isrc/src/lzw -Isrc/deps/yxml -Isrc/deps/miniz

# fopen override is now handled by linker --wrap (see nspire_fopen.c)
FORCE_INCLUDE =

# Defines for TI-Nspire platform
DEFINES = -DNSPIRE \
          -DNDEBUG \
          -DLSB_FIRST=1 \
          -DNO_AUDIO \
          -DSCREEN_SCALE=1 \
          -D_GNU_SOURCE

# Libraries
LIBS = -lSDL -lz -lm -lstdc++

# Linker flags
LDFLAGS = -L/ndless-sdk/lib $(LIBS) -Wl,--gc-sections -Wl,--wrap=fopen -Os -flto -mno-unaligned-access

ZEHNFLAGS = --name "ultima4"

ifeq ($(DEBUG),FALSE)
	GCCFLAGS += -Os
else
	GCCFLAGS += -O0 -g
	DEFINES += -DDEBUG_BUILD
endif

# Combine flags
CFLAGS = $(GCCFLAGS) $(FLAGS_C) $(INCLUDES) $(DEFINES) $(FORCE_INCLUDE)
CXXFLAGS = $(GCCFLAGS) $(FLAGS_CXX) $(INCLUDES) $(DEFINES) $(FORCE_INCLUDE)

# === Source files ===

# zu4 C sources
ZU4_CSRC = \
	src/deps/yxml/yxml.c \
	src/deps/miniz/miniz.c \
	src/src/lzw/hash.c \
	src/src/lzw/lzw.c \
	src/src/lzw/u4decode.c \
	src/src/armor.c \
	src/src/coords.c \
	src/src/direction.c \
	src/src/error.c \
	src/src/image.c \
	src/src/imageloader.c \
	src/src/io.c \
	src/src/moongate.c \
	src/src/names.c \
	src/src/random.c \
	src/src/rle.c \
	src/src/savegame.c \
	src/src/settings.c \
	src/src/u4file.c \
	src/src/u4_sdl.c \
	src/src/weapon.c \
	src/src/xmlparse.c

# zu4 C++ sources
ZU4_CXXSRC = \
	src/src/annotation.cpp \
	src/src/aura.cpp \
	src/src/camp.cpp \
	src/src/cheat.cpp \
	src/src/city.cpp \
	src/src/codex.cpp \
	src/src/combat.cpp \
	src/src/config.cpp \
	src/src/controller.cpp \
	src/src/context.cpp \
	src/src/conversation.cpp \
	src/src/creature.cpp \
	src/src/death.cpp \
	src/src/dialogueloader.cpp \
	src/src/dialogueloader_hw.cpp \
	src/src/dialogueloader_lb.cpp \
	src/src/dialogueloader_tlk.cpp \
	src/src/dungeon.cpp \
	src/src/dungeonview.cpp \
	src/src/event.cpp \
	src/src/game.cpp \
	src/src/imagemgr.cpp \
	src/src/imageview.cpp \
	src/src/intro.cpp \
	src/src/item.cpp \
	src/src/location.cpp \
	src/src/map.cpp \
	src/src/maploader.cpp \
	src/src/mapmgr.cpp \
	src/src/menu.cpp \
	src/src/menuitem.cpp \
	src/src/movement.cpp \
	src/src/object.cpp \
	src/src/person.cpp \
	src/src/player.cpp \
	src/src/portal.cpp \
	src/src/progress_bar.cpp \
	src/src/screen.cpp \
	src/src/script.cpp \
	src/src/shrine.cpp \
	src/src/spell.cpp \
	src/src/stats.cpp \
	src/src/textview.cpp \
	src/src/tile.cpp \
	src/src/tileanim.cpp \
	src/src/tilemap.cpp \
	src/src/tileset.cpp \
	src/src/tileview.cpp \
	src/src/view.cpp \
	src/src/xml.cpp

# Nspire platform layer (replaces SDL2/OpenGL/libxml2)
NSPIRE_CSRC = \
	nspire/nspire_fopen.c \
	nspire/nspire_video.c \
	nspire/nspire_audio_stub.c

NSPIRE_CXXSRC = \
	nspire/nspire_main.cpp \
	nspire/nspire_event.cpp \
	nspire/nspire_xml_shim.cpp

# All sources
CSRC = $(ZU4_CSRC) $(NSPIRE_CSRC)
CXXSRC = $(ZU4_CXXSRC) $(NSPIRE_CXXSRC)
OBJS = $(CSRC:.c=.o) $(CXXSRC:.cpp=.o)

EXE = ultima4

all: $(EXE).tns
	@cp $(EXE).tns prod/

# C compilation
%.o: %.c
	@echo "CC  $<"
	@$(GCC) $(CFLAGS) -c $< -o $@

# C++ compilation
%.o: %.cpp
	@echo "CXX $<"
	@$(GXX) $(CXXFLAGS) -c $< -o $@

# Link
$(EXE).elf: $(OBJS)
	@echo "LD  $@"
	$(LD) $^ -o $@ $(LDFLAGS) -Wl,-Map=$(EXE).map

# Create TI-Nspire executable
$(EXE).tns: $(EXE).elf
	@echo "Creating TI-Nspire executable..."
	$(GENZEHN) --input $^ --output $@.zehn $(ZEHNFLAGS)
	make-prg $@.zehn $@
	rm $@.zehn

# Copy to prod directory
prod: $(EXE).tns
	@echo "Preparing prod directory..."
	cp $(EXE).tns prod/
	@echo "Done! Copy prod/ contents to your TI-Nspire."

clean:
	@echo "Cleaning..."
	rm -f $(OBJS) $(EXE).tns $(EXE).elf $(EXE).zehn $(EXE).map

count:
	@echo "C files:   $$(echo $(CSRC) | wc -w)"
	@echo "C++ files: $$(echo $(CXXSRC) | wc -w)"
	@echo "Total:     $$(echo $(OBJS) | wc -w)"

.PHONY: all clean count prod
