# opentrons modules makefile
# https://github.com/Opentrons/opentrons-modules

SHELL := /bin/bash

CI_SYSTEM ?= $(shell uname)

LIBRARIES_DIR := libraries
MODULES_DIR := modules
INO_DIR := $(HOME)/arduino_ide
EEPROM_DIR := eepromWriter

ARDUINO_VERSION ?= 1.8.10
ARDUINO_SAMD_VER ?= 1.6.21
OPENTRONS_BOARDS_VER ?= 1.2.0
OPENTRONS_SAMD_BOARDS_VER ?= 1.1.0

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
	$(if $(NO_ARDUINO_SAMD), $(ARDUINO) --install-boards arduino:samd:$(ARDUINO_SAMD_VER), @echo "Arduino SAMD already installed")
	echo -n "Opentrons SAMD: "
	$(if $(NO_OPENTRONS_SAMD_BOARDS), $(ARDUINO) --install-boards Opentrons:samd:$(OPENTRONS_SAMD_BOARDS_VER), @echo "Opentrons SAMD already installed")
	echo -n "Opentrons modules: "
	$(if $(NO_OPENTRONS_BOARDS), $(ARDUINO) --install-boards Opentrons:avr, @echo "Opentrons boards already installed")

.PHONY: build
build: build-magdeck build-tempdeck build-thermocycler

# Magdeck flags
MD_FW_VERSION := $(shell git describe --tags --match "magdeck*" | cut -d'@' -f2)

# Tempdeck flags
TD_FW_VERSION := $(shell git describe --tags --match "tempdeck*" | cut -d'@' -f2)

# Thermocycler flags
TC_FW_VERSION := $(shell git describe --tags --match "thermocycler*" | cut -d'@' -f2)

MAGDECK_BUILD_DIR := $(BUILDS_DIR)/mag-deck
TEMPDECK_BUILD_DIR := $(BUILDS_DIR)/temp-deck
TC_BUILD_DIR := $(BUILDS_DIR)/thermo-cycler

DUMMY_BOARD ?= false
LID_WARNING ?= false
HFQ_PWM ?= false
HW_VERSION ?= 4
LID_TESTING ?= false
RGBW_NEO ?= true
VOLUME_DEPENDENCY ?= true

.PHONY: build-magdeck
build-magdeck:
	echo "compiler.cpp.extra_flags=-DMD_FW_VERSION=\"$(MD_FW_VERSION)\"" > $(ARDUINO15_LOC)/packages/Opentrons/hardware/avr/$(OPENTRONS_BOARDS_VER)/platform.local.txt
	$(ARDUINO) --verify --board Opentrons:avr:magdeck32u4cat $(MODULES_DIR)/mag-deck/mag-deck-arduino/mag-deck-arduino.ino --verbose-build
	mkdir -p $(MAGDECK_BUILD_DIR)
	cp $(BUILDS_DIR)/tmp/mag-deck-arduino.ino.hex $(MAGDECK_BUILD_DIR)

.PHONY: build-tempdeck
build-tempdeck:
	echo "compiler.cpp.extra_flags=-DTD_FW_VERSION=\"$(TD_FW_VERSION)\"" > $(ARDUINO15_LOC)/packages/Opentrons/hardware/avr/$(OPENTRONS_BOARDS_VER)/platform.local.txt
	$(ARDUINO) --verify --board Opentrons:avr:tempdeck32u4cat $(MODULES_DIR)/temp-deck/temp-deck-arduino/temp-deck-arduino.ino --verbose-build
	mkdir -p $(TEMPDECK_BUILD_DIR)
	cp $(BUILDS_DIR)/tmp/temp-deck-arduino.ino.hex $(TEMPDECK_BUILD_DIR)
	cp $(EEPROM_DIR)/eepromWriter.hex $(TEMPDECK_BUILD_DIR)
	cp $(EEPROM_DIR)/avrdude.conf $(TEMPDECK_BUILD_DIR)
	cp $(EEPROM_DIR)/write_module_memory.py $(TEMPDECK_BUILD_DIR)

.PHONY: build-thermocycler
build-thermocycler:
	echo "compiler.cpp.extra_flags=-DDUMMY_BOARD=$(DUMMY_BOARD) \
	-DLID_WARNING=$(LID_WARNING) -DHFQ_PWM=$(HFQ_PWM) \
	-DHW_VERSION=$(HW_VERSION) -DLID_TESTING=$(LID_TESTING) \
	-DRGBW_NEO=$(RGBW_NEO) -DVOLUME_DEPENDENCY=$(VOLUME_DEPENDENCY) \
	-DTC_FW_VERSION=\"$(TC_FW_VERSION)\"" \
	> $(ARDUINO15_LOC)/packages/Opentrons/hardware/samd/$(OPENTRONS_SAMD_BOARDS_VER)/platform.local.txt

	# build firmware
	$(ARDUINO) --verify --board Opentrons:samd:thermocycler_m0 $(MODULES_DIR)/thermo-cycler/thermo-cycler-arduino/thermo-cycler-arduino.ino --verbose-build
	mkdir -p $(TC_BUILD_DIR)
	cp $(BUILDS_DIR)/tmp/thermo-cycler-arduino.ino.bin $(TC_BUILD_DIR)
	cp $(MODULES_DIR)/thermo-cycler/production/firmware_uploader.py $(TC_BUILD_DIR)

	# build eeprom writer
	$(ARDUINO) --verify --board Opentrons:samd:thermocycler_m0 $(MODULES_DIR)/thermo-cycler/production/eepromWriter/eepromWriter.ino --verbose-build
	cp $(BUILDS_DIR)/tmp/eepromWriter.ino.bin $(TC_BUILD_DIR)
	cp $(MODULES_DIR)/thermo-cycler/production/serial_and_firmware_uploader.py $(TC_BUILD_DIR)

.PHONY: zip-all
zip-all:
	cd $(BUILDS_DIR) && zip -r mag-deck-$(MD_FW_VERSION).zip mag-deck \
	&& zip -r temp-deck-$(TD_FW_VERSION).zip temp-deck \
	&& zip -r thermo-cycler-$(TC_FW_VERSION).zip thermo-cycler \

.PHONY: clean
clean:
	rm -rf $(BUILDS_DIR)/tmp
	rm -rf $(MAGDECK_BUILD_DIR)
	rm -rf $(TEMPDECK_BUILD_DIR)
	rm -rf $(TC_BUILD_DIR)

.PHONY: teardown
teardown:
	rm -rf $(INO_DIR)
	rm -rf $(ARDUINO15_LOC)
