.PHONY: all app-3ds clean install-local-sd

all: app-3ds

app-3ds:
	$(MAKE) -C app-3ds

clean:
	$(MAKE) -C app-3ds clean

install-local-sd: app-3ds
	mkdir -p local/sdmc/3ds/anki3ds
	cp app-3ds/anki3ds.3dsx local/sdmc/3ds/anki3ds/anki3ds.3dsx
	cp app-3ds/anki3ds.smdh local/sdmc/3ds/anki3ds/anki3ds.smdh
