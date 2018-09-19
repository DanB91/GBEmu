#!/bin/bash
cd $(dirname "$0")
rm -rf build
TARGET=${1-release}

APP_DIR=build/GBEmu.app
mkdir -p $APP_DIR
mkdir $APP_DIR/Contents
mkdir $APP_DIR/Contents/MacOS
mkdir $APP_DIR/Contents/Frameworks
mkdir $APP_DIR/Contents/Resources

build_gbemu() {
    if pushd .. && make $1; then 
       if [[ $1 != "clean" ]]; then 
           popd &&
               cp ../build/gbemu $APP_DIR/Contents/MacOS && 
               cp resources/Info.plist $APP_DIR/Contents/ &&
               cp resources/GBEmuIcon.icns $APP_DIR/Contents/Resources &&
               cp -r resources/SDL2.framework $APP_DIR/Contents/Frameworks &&
               install_name_tool $APP_DIR/Contents/MacOS/gbemu -add_rpath "@loader_path/../Frameworks"
       else
           return 0
       fi
   fi
}
if build_gbemu $TARGET; then
    echo "Success! App located at $APP_DIR"
else
    echo 'Build error!'
fi

