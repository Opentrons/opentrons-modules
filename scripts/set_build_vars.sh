#!/bin/bash

DIST_DIR="./dist"

RELEASE_VERSION=$(cut -d'@' -f2 <<< $TRAVIS_TAG)
THIS_BUILD_TAG=$(git describe --tags)

case $TRAVIS_TAG in

  tempdeck@v*)
    export RELEASE_LOCAL_DIR="${DIST_DIR}/temp-deck"
    export RELEASE_UPLOAD_DIR="temperature-module/${RELEASE_VERSION}"
    ;;

  magdeck@v*)
    export RELEASE_LOCAL_DIR="${DIST_DIR}/mag-deck"
    export RELEASE_UPLOAD_DIR="magnetic-module/${RELEASE_VERSION}"
    ;;

  thermocycler@v*)
    export RELEASE_LOCAL_DIR="${DIST_DIR}/thermo-cycler"
    export RELEASE_UPLOAD_DIR="thermocycler/${RELEASE_VERSION}"
    ;;

  heater-shaker@v*)
    export RELEASE_LOCAL_DIR="${DIST_DIR}/heater-shaker"
    export RELEASE_UPLOAD_DIR="heater-shaker/${RELEASE_VERSION}"
    ;;

  thermocycler-gen2@v*)
    export RELEASE_LOCAL_DIR="${DIST_DIR}/thermocycler-gen2"
    export RELEASE_UPLOAD_DIR="thermocycler-gen2/${RELEASE_VERSION}"
    ;;

  *)
    export RELEASE_LOCAL_DIR=$DIST_DIR
    export RELEASE_UPLOAD_DIR="modules-${THIS_BUILD_TAG}-${TRAVIS_BRANCH}"
    ;;

esac
