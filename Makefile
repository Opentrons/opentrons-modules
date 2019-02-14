# opentrons modules makefile
# https://github.com/Opentrons/opentrons-modules

SHELL := /bin/bash

LIBRARIES_DIR := libraries
MODULES_DIR := modules
INO_DIR := $(HOME)/arduino_ide

ARDUINO_VERSION ?= 1.8.5
CI_SYSTEM ?= linux

ifeq ($(CI_SYSTEM), osx)
	ARDUINO := $(INO_DIR)/Arduino.app/Contents/MacOS/Arduino
	ARDUINO_ARCHIVE := arduino-$(ARDUINO_VERSION)-macosx.zip
	UNPACK_CMD := unzip $(ARDUINO_ARCHIVE) -d $(HOME)/arduino_ide/
	ARDUINO_SKETCHBOOK_LOC := $(HOME)/Documents/Arduino
	ARDUINO15_LOC := $(HOME)/Library/Arduino15
else
	ARDUINO := $(INO_DIR)/arduino
	ARDUINO_ARCHIVE := arduino-$(ARDUINO_VERSION)-linux64.tar.xz
	UNPACK_CMD := tar xf $(ARDUINO_ARCHIVE) -C $(HOME)/arduino_ide/ --strip-components=1
	ARDUINO_SKETCHBOOK_LOC := $(HOME)/Arduino
	ARDUINO15_LOC := $(HOME)/.arduino15
endif

BUILD_PATH := $(HOME)/.arduino_builds
DOWNLOAD_LINK := https://downloads.arduino.cc/$(ARDUINO_ARCHIVE)

ifeq ($(wildcard $(ARDUINO)), )
	NO_ARDUINO := true
endif

# setup Arduino
.PHONY: setup-ino
setup-ino:
	# Download Arduino
	mkdir -p $(HOME)/arduino_ide
	# if not already cached, download and install/unpack arduino IDE
	$(if $(NO_ARDUINO), wget --quiet $(DOWNLOAD_LINK), @echo "Arduino already present")
	$(if $(NO_ARDUINO), $(UNPACK_CMD))
	# Link all the libraries that the modules use
	mkdir -p $(ARDUINO_SKETCHBOOK_LOC)/libraries/
	cp -a $(LIBRARIES_DIR)/. $(ARDUINO_SKETCHBOOK_LOC)/libraries/
	echo -n "Libraries installed:"
	ls $(ARDUINO_SKETCHBOOK_LOC)/libraries
	# Add board packages
	rm -rf $(ARDUINO15_LOC)
	$(ARDUINO) --pref "boardsmanager.additional.urls=https://adafruit.github.io/arduino-board-index/package_adafruit_index.json, https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json" --save-prefs
	# Install all required boards
	echo -n "Installing board packages.."
	echo -n "Arduino SAMD"
	$(ARDUINO) --install-boards arduino:samd
	echo -n "Adafruit SAMD"
	$(ARDUINO) --install-boards adafruit:samd
	echo -n "Opentrons modules"
	$(ARDUINO) --install-boards Opentrons:avr

.PHONY: build
build: build-magdeck build-tempdeck build-thermocycler

.PHONY: build-magdeck
build-magdeck:
	$(ARDUINO) --verify --board Opentrons:avr:magdeck32u4cat $(MODULES_DIR)/mag-deck/mag-deck-arduino/mag-deck-arduino.ino --verbose-build

.PHONY: build-tempdeck
build-tempdeck:
	$(ARDUINO) --verify --board Opentrons:avr:tempdeck32u4cat $(MODULES_DIR)/temp-deck/temp-deck-arduino/temp-deck-arduino.ino --verbose-build

.PHONY: build-thermocycler
build-thermocycler:
	$(ARDUINO) --verify --board adafruit:samd:adafruit_feather_m0 $(MODULES_DIR)/thermo-cycler/thermo-cycler-arduino/thermo-cycler-arduino.ino --verbose-build
