#!/bin/bash
cd $(dirname "$0")
TARGET=${1-release}

APP_DIR=build/GBEmu.app

pushd() {
    command pushd "$@" > /dev/null
}
popd() {
    command popd "$@" > /dev/null
}

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
build_gbemu() {
        if [[ $1 == "clean" ]]; then 
           if make $1; then
               echo "Cleaned build dir!"
               exit 0
           else
               exit 1
           fi
        fi
        local version=`cat ../version.txt`

        mkdir -p $APP_DIR/Contents
        mkdir -p $APP_DIR/Contents/MacOS
        mkdir -p $APP_DIR/Contents/Frameworks
        mkdir -p $APP_DIR/Contents/Resources
        make $1 && cp build/gbemu $APP_DIR/Contents/MacOS && 
            cp resources/Info.plist $APP_DIR/Contents/ &&
            sed -i .bak -e "s/@GBEmuVersion@/$version/g" $APP_DIR/Contents/Info.plist &&
            rm $APP_DIR/Contents/Info.plist.bak &&
            cp resources/GBEmuIcon.icns $APP_DIR/Contents/Resources &&
            cp -r resources/SDL2.framework $APP_DIR/Contents/Frameworks &&
            install_name_tool $APP_DIR/Contents/MacOS/gbemu -add_rpath "@loader_path/../Frameworks"
}
if [[ $1 == "help" ]]; then
    printUsage
elif build_gbemu $TARGET; then
    echo "Success! App located at $APP_DIR"
else
    echo 'Build error!'
fi
