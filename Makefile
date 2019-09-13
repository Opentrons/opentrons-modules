# opentrons modules makefile
# https://github.com/Opentrons/opentrons-modules

SHELL := /bin/bash

CI_SYSTEM ?= $(shell uname)

# this may be set as an environment variable to select the version of
# python to run if pyenv is not available. it should always be set to
# point to a python3.6.
OT_PYTHON ?= python

LIBRARIES_DIR := libraries
MODULES_DIR := modules
INO_DIR := $(HOME)/arduino_ide

ARDUINO_VERSION ?= 1.8.5
ARDUINO_SAMD_VER ?= 1.6.20
OPENTRONS_BOARDS_VER ?= 1.2.0
OPENTRONS_SAMD_BOARDS_VER ?= 1.0.3

OPENTRONS_BOARD_URL := https://s3.us-east-2.amazonaws.com/opentrons-modules/package_opentrons_index.json

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

BUILDS_DIR := ./build
DOWNLOAD_LINK := https://downloads.arduino.cc/$(ARDUINO_ARCHIVE)

ifeq ($(wildcard $(ARDUINO)), )
	NO_ARDUINO := true
endif

ifeq ($(wildcard $(ARDUINO15_LOC)/packages/arduino/hardware/samd/$(ARDUINO_SAMD_VER)/boards.txt), )
	NO_ARDUINO_SAMD := true
endif

ifeq ($(wildcard $(ARDUINO15_LOC)/packages/Opentrons/hardware/avr/$(OPENTRONS_BOARDS_VER)/boards.txt), )
	NO_OPENTRONS_BOARDS := true
endif

ifeq ($(wildcard $(ARDUINO15_LOC)/packages/Opentrons/hardware/samd/$(OPENTRONS_SAMD_BOARDS_VER)/boards.txt), )
	NO_OPENTRONS_SAMD_BOARDS := true
endif


# setup Arduino
.PHONY: setup
setup:
	# Download Arduino
	mkdir -p $(INO_DIR)
	# if not already cached, download and install/unpack arduino IDE
	$(if $(NO_ARDUINO), curl -O --get $(DOWNLOAD_LINK), @echo "Arduino already present")
	$(if $(NO_ARDUINO), $(UNPACK_CMD))
	rm -rf $(ARDUINO_ARCHIVE)
	# Change sketchbook location to the repository in order to link library files
	$(ARDUINO) --pref "sketchbook.path=$(CURDIR)" --save-prefs
	# Change build path to a known path
	$(ARDUINO) --pref "build.path=$(CURDIR)/$(BUILDS_DIR)/tmp" --save-prefs
	# Add board packages
	$(ARDUINO) --pref "boardsmanager.additional.urls=$(ADAFRUIT_BOARD_URL), $(OPENTRONS_BOARD_URL)" --save-prefs
	# Install all required boards
	echo -n "Installing board packages.."
	echo -n "Arduino SAMD: "
	$(if $(NO_ARDUINO_SAMD), $(ARDUINO) --install-boards arduino:samd, @echo "Arduino SAMD already installed")
	echo -n "Opentrons SAMD: "
	$(if $(NO_OPENTRONS_SAMD_BOARDS), $(ARDUINO) --install-boards Opentrons:samd, @echo "Opentrons SAMD already installed")
	echo -n "Opentrons modules: "
	$(if $(NO_OPENTRONS_BOARDS), $(ARDUINO) --install-boards Opentrons:avr, @echo "Opentrons boards already installed")

.PHONY: build
build: build-magdeck build-tempdeck build-thermocycler build-tc-eeprom

TC_DIR := $(MODULES_DIR)/thermo-cycler
TC_BUILD_DIR := $(BUILDS_DIR)/thermo-cycler
TC_FW_VERSION ?= 1.0.3
TC_FW_FILENAME := thermo-cycler-v$(TC_FW_VERSION).uf2
DUMMY_BOARD ?= false
USE_GCODES ?= true
LID_WARNING ?= false
HFQ_PWM ?= false
OLD_PID_INTERVAL ?= true
HW_VERSION ?= 4
LID_TESTING ?= false
RGBW_NEO ?= true

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
	echo "compiler.cpp.extra_flags=-DDUMMY_BOARD=$(DUMMY_BOARD) \
	-DUSE_GCODES=$(USE_GCODES) -DLID_WARNING=$(LID_WARNING) \
	-DHFQ_PWM=$(HFQ_PWM) -DOLD_PID_INTERVAL=$(OLD_PID_INTERVAL) \
	-DHW_VERSION=$(HW_VERSION) -DLID_TESTING=$(LID_TESTING) \
	-DRGBW_NEO=$(RGBW_NEO) -DTC_FW_VERSION=\"$(TC_FW_VERSION)\"" \
	> $(ARDUINO15_LOC)/packages/Opentrons/hardware/samd/$(OPENTRONS_SAMD_BOARDS_VER)/platform.local.txt
	$(ARDUINO) --verify --board Opentrons:samd:thermocycler_m0 $(TC_DIR)/thermo-cycler-arduino/thermo-cycler-arduino.ino --verbose-build
	mkdir -p $(TC_BUILD_DIR)
	mkdir -p $(TC_BUILD_DIR)/firmware
	$(OT_PYTHON) $(TC_DIR)/production/uf2conv.py \
		$(BUILDS_DIR)/tmp/thermo-cycler-arduino.ino.bin -c -f SAMD21 -o $(TC_BUILD_DIR)/firmware/$(TC_FW_FILENAME)
	cp -r $(TC_DIR)/thermo-cycler-arduino $(TC_BUILD_DIR)
	cp $(TC_DIR)/production/serial_and_firmware_uploader.py $(TC_BUILD_DIR)
	cp $(TC_DIR)/production/firmware_uploader.py $(TC_BUILD_DIR)
	cp $(TC_DIR)/production/uf2conv.py $(TC_BUILD_DIR)

.PHONY: build-tc-eeprom
build-tc-eeprom:
	$(ARDUINO) --verify --board Opentrons:samd:thermocycler_m0 $(MODULES_DIR)/thermo-cycler/production/eepromWriter/eepromWriter.ino --verbose-build
	mkdir -p $(TC_BUILD_DIR)
	mkdir -p $(TC_BUILD_DIR)/firmware
	$(OT_PYTHON) $(TC_DIR)/production/uf2conv.py \
		$(BUILDS_DIR)/tmp/eepromWriter.ino.bin -c -f SAMD21 -o $(TC_BUILD_DIR)/firmware/eepromWriter.uf2

.PHONY: teardown
teardown:
	rm -rf $(INO_DIR)
	rm -rf $(ARDUINO15_LOC)
