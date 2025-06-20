#!/bin/bash

# Script for setting up ESP_IDF environment on Linux
# use it like this: `source sourceme_IDF_ENV.sh`
# Author: Paul Abbott, Lumitec, 2022
SCRIPT_VER="1.0"
SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")

# this only works when sourced (not run as a script), because it is adding variables into the local shell
[[ "${BASH_SOURCE[0]}" == "${0}" ]] && echo "Error, must source this script!  Try: \"source ${BASH_SOURCE[0]}\"" && exit 1

# Don't use IDF_PATH set in the environment.  Use this hardcoded path.
IDF_PATH=~/esp/esp-idf-v4.4.8

# enable ccache to greatly speed-up the build (i.e. fast subsequent builds after a clean)
export IDF_CCACHE_ENABLE=1

source ${IDF_PATH}/export.sh
alias idf="idf.py"

# set the default baud very high for fast programming (can be overridden with -b option)
ESPBAUD=2000000

# These commands will not execute, but will be in the history for convenience.
history -s idf menuconfig
history -s idf build flash -b 2000000 -p /dev/ttyUSB1 monitor
history -s idf build flash monitor
