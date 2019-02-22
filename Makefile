# opentrons modules makefile
# https://github.com/Opentrons/opentrons-modules

SHELL := /bin/bash

CI_SYSTEM ?= $(shell uname)

LIBRARIES_DIR := libraries
MODULES_DIR := modules
INO_DIR := $(HOME)/arduino_ide

ARDUINO_VERSION ?= 1.8.5
ARDUINO_SAMD_VER ?= 1.6.20
ADAFRUIT_SAMD_VER ?= 1.2.9
OPENTRONS_BOARDS_VER ?= 1.2.0

ifeq ($(CI_SYSTEM), Darwin)
	ARDUINO := $(INO_DIR)/Arduino.app/Contents/MacOS/Arduino
	ARDUINO_ARCHIVE := arduino-$(ARDUINO_VERSION)-macosx.zip
	UNPACK_CMD := unzip $(ARDUINO_ARCHIVE) -d $(INO_DIR)
	ARDUINO_SKETCHBOOK_LOC := $(HOME)/Documents/Arduino
	ARDUINO15_LOC := $(HOME)/Library/Arduino15
else
	ARDUINO := $(INO_DIR)/arduino
	ARDUINO_ARCHIVE := arduino-$(ARDUINO_VERSION)-linux64.tar.xz
	UNPACK_CMD := tar xf $(ARDUINO_ARCHIVE) -C $(INO_DIR)/ --strip-components=1
	ARDUINO_SKETCHBOOK_LOC := $(HOME)/Arduino
	ARDUINO15_LOC := $(HOME)/.arduino15
endif

BUILDS_DIR := $(HOME)/.arduino_builds
DOWNLOAD_LINK := https://downloads.arduino.cc/$(ARDUINO_ARCHIVE)

ifeq ($(wildcard $(ARDUINO)), )
	NO_ARDUINO := true
endif

ifeq ($(wildcard $(ARDUINO15_LOC)/packages/arduino/hardware/samd/$(ARDUINO_SAMD_VER)/boards.txt), )
	NO_ARDUINO_SAMD := true
endif

ifeq ($(wildcard $(ARDUINO15_LOC)/packages/adafruit/hardware/samd/$(ADAFRUIT_SAMD_VER)/boards.txt), )
	NO_ADAFRUIT_SAMD := true
endif

ifeq ($(wildcard $(ARDUINO15_LOC)/packages/Opentrons/hardware/avr/$(OPENTRONS_BOARDS_VER)/boards.txt), )
	NO_OPENTRONS_BOARDS := true
endif

# setup Arduino
.PHONY: setup-ino
setup-ino:
	# Download Arduino
	mkdir -p $(INO_DIR)
	# if not already cached, download and install/unpack arduino IDE
	$(if $(NO_ARDUINO), wget --quiet $(DOWNLOAD_LINK), @echo "Arduino already present")
	$(if $(NO_ARDUINO), $(UNPACK_CMD))
	# Change sketchbook location to the repository in order to link library files
	$(ARDUINO) --pref "sketchbook.path=$(CURDIR)" --save-prefs
	# Change build path to a known path
	$(ARDUINO) --pref "build.path=$(BUILDS_DIR)/tmp" --save-prefs
	# Add board packages
	$(ARDUINO) --pref "boardsmanager.additional.urls=https://adafruit.github.io/arduino-board-index/package_adafruit_index.json, https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json" --save-prefs
	# Install all required boards
	echo -n "Installing board packages.."
	echo -n "Arduino SAMD: "
	$(if $(NO_ARDUINO_SAMD), $(ARDUINO) --install-boards arduino:samd, @echo "Arduino SAMD already installed")
	echo -n "Adafruit SAMD: "
	$(if $(NO_ADAFRUIT_SAMD), $(ARDUINO) --install-boards adafruit:samd, @echo "Adafruit SAMD already installed")
	echo -n "Opentrons modules: "
	$(if $(NO_OPENTRONS_BOARDS), $(ARDUINO) --install-boards Opentrons:avr, @echo "Opentrons boards already installed")

.PHONY: build
build: build-magdeck build-tempdeck build-thermocycler

.PHONY: build-magdeck
build-magdeck:
	$(ARDUINO) --verify --board Opentrons:avr:magdeck32u4cat $(MODULES_DIR)/mag-deck/mag-deck-arduino/mag-deck-arduino.ino --verbose-build
	mkdir -p $(BUILDS_DIR)/mag-deck
	cp $(BUILDS_DIR)/tmp/mag-deck-arduino.ino.hex $(BUILDS_DIR)/mag-deck/


.PHONY: build-tempdeck
build-tempdeck:
	$(ARDUINO) --verify --board Opentrons:avr:tempdeck32u4cat $(MODULES_DIR)/temp-deck/temp-deck-arduino/temp-deck-arduino.ino --verbose-build
	mkdir -p $(BUILDS_DIR)/temp-deck
	cp $(BUILDS_DIR)/tmp/temp-deck-arduino.ino.hex $(BUILDS_DIR)/temp-deck/

.PHONY: build-thermocycler
build-thermocycler:
	$(ARDUINO) --verify --board adafruit:samd:adafruit_feather_m0 $(MODULES_DIR)/thermo-cycler/thermo-cycler-arduino/thermo-cycler-arduino.ino --verbose-build
	mkdir -p $(BUILDS_DIR)/thermo-cycler
	cp $(BUILDS_DIR)/tmp/thermo-cycler-arduino.ino.bin $(BUILDS_DIR)/thermo-cycler/
