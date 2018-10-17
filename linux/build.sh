#!/bin/bash
cd $(dirname "$0")
TARGET=${1-release}
BUILD_DIR=$(dirname "$0")"/build"

printUsage() {
	echo "Builds GBEmu for Linux. By default, the 'release' build is built."
	echo "Usage $0 [help | release | profile | debug | test]"
	echo -e "\thelp -- Prints this help message."
	echo -e "\trelease -- Builds release build."
	echo -e "\tprofile -- Builds GBEmu where the profiler enabled and is accessible in the GBEmu debugger."
	echo -e "\tdebug -- Builds debuggable build."
	echo -e "\ttest -- Runs unit tests."
        echo -e "\tclean -- Cleans the build directory."
} 
if [[ $1 == "help" ]]; then
    printUsage
else 
    if [[ $TARGET == "clean" ]]; then 
       if make $1; then
           echo "Cleaned build dir!"
           exit 0
       else
           exit 1
       fi
    elif make $TARGET; then
        echo "Success! App located at $BUILD_DIR/gbemu"
    else
        echo 'Build error!'
    fi
fi
