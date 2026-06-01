AZAHAR_APP ?= $(HOME)/Applications/azahar-macos-arm64-2125.1.2/Azahar.app

.PHONY: all app-3ds clean install-local-sd check-emulator run-emulator

all: app-3ds

app-3ds:
	$(MAKE) -C app-3ds

clean:
	$(MAKE) -C app-3ds clean

install-local-sd: app-3ds
	mkdir -p local/sdmc/3ds/anki3ds
	cp app-3ds/anki3ds.3dsx local/sdmc/3ds/anki3ds/anki3ds.3dsx
	cp app-3ds/anki3ds.smdh local/sdmc/3ds/anki3ds/anki3ds.smdh

check-emulator:
	@test -d "$(AZAHAR_APP)" || \
		(echo "Azahar not found at $(AZAHAR_APP). Set AZAHAR_APP=/path/to/Azahar.app"; exit 1)

run-emulator: app-3ds check-emulator
	open -a "$(AZAHAR_APP)" app-3ds/anki3ds.3dsx
