# path to RETRO68
RETRO68=../../../Retro68-build/toolchain

PREFIX=$(RETRO68)/m68k-unknown-elf
CC=$(RETRO68)/bin/m68k-unknown-elf-gcc
CXX=$(RETRO68)/bin/m68k-unknown-elf-g++
REZ=$(RETRO68)/bin/Rez

BUILDFLAGS=-Ofast -ffloat-store -funsafe-math-optimizations -fsingle-precision-constant -mcpu=68000 -mtune=68000 -m68000 -msoft-float -malign-int
LDFLAGS=-lRetroConsole $(BUILDFLAGS)
RINCLUDES=$(PREFIX)/RIncludes
REZFLAGS=-I$(RINCLUDES)

# all resource file help is from https://github.com/clehner/Browsy/blob/master/Makefile
RSRC_HEX=$(wildcard rsrc/*/*.hex)
RSRC_TXT=$(wildcard rsrc/*/*.txt)
RSRC_JS=$(wildcard rsrc/*/*.js)
RSRC_JSON=$(wildcard rsrc/*/*.json)
RSRC_DAT=$(RSRC_HEX:.hex=.dat) $(RSRC_TXT:.txt=.dat) $(RSRC_JS:.js=.dat) $(RSRC_JSON:.json=.dat)

NuklearQuickDraw.bin NuklearQuickDraw.APPL NuklearQuickDraw.dsk: NuklearQuickDraw.flt rsrc-args compile_js
	$(REZ) $(REZFLAGS) \
		-DFLT_FILE_NAME="\"NuklearQuickDraw.flt\"" "$(RINCLUDES)/Retro68APPL.r" \
		-t "APPL" \
		 $(BUILDFLAGS) -o NuklearQuickDraw.bin --cc NuklearQuickDraw.APPL --cc NuklearQuickDraw.dsk -C WWW6 $(shell cat rsrc-args)

NuklearQuickDraw.flt: hello.o
	$(CXX) $< -o $@ $(LDFLAGS)	# C++ used for linking because RetroConsole needs it

.PHONY: clean
clean:
	rm -f NuklearQuickDraw.bin NuklearQuickDraw.APPL NuklearQuickDraw.dsk NuklearQuickDraw.flt NuklearQuickDraw.flt.gdb hello.o rsrc/*/*.dat rsrc-args

compile_js:
	./compile_js.sh

rsrc: $(RSRC_DAT) rsrc-args

rsrc/%.dat: rsrc/%.hex
	$(QUIET_RSRC)$(FROM_HEX) $< > $@

rsrc/TEXT/%.dat: rsrc/TEXT/%.txt
	$(QUIET_RSRC)tr '\n' '\r' < $< > $@

rsrc/JS/%.dat: rsrc/TEXT/%.txt
	$(QUIET_RSRC)tr '\n' '\r' < $< > $@

rsrc-args: $(RSRC_DAT)
	@cd rsrc && for code in $$(ls); do \
		echo -n "-t $$code "; \
		cd "$$code" && for file in *.dat; do \
			echo -n "-r $${file%.dat} rsrc/$$code/$$file "; \
		done; \
		cd ..; \
	done > ../$@
