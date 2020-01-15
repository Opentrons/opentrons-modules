#!/bin/bash

BUILDS_DIR="./build"

RELEASE_VERSION=$(cut -d'@' -f2 <<< $TRAVIS_TAG)

case $TRAVIS_TAG in

  tempdeck@v*)
    export RELEASE_LOCAL_DIR="${BUILDS_DIR}/temp-deck"
    export RELEASE_UPLOAD_DIR="temperature-module/${RELEASE_VERSION}"
    ;;

  magdeck@v*)
    export RELEASE_LOCAL_DIR="${BUILDS_DIR}/mag-deck"
    export RELEASE_UPLOAD_DIR="magnetic-module/${RELEASE_VERSION}"
    ;;

  thermocycler@v*)
    export RELEASE_LOCAL_DIR="${BUILDS_DIR}/thermo-cycler"
    export RELEASE_UPLOAD_DIR="thermocycler/${RELEASE_VERSION}"
    ;;

  *)
    echo "No tagged module to prepare for release"
    ;;

esac
