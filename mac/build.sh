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
pushd () {
    command pushd "$@" > /dev/null
}
popd () {
    command popd "$@" > /dev/null
}

build_gbemu() {
    if pushd .. && make $1; then 
       if [[ $1 != "clean" ]]; then 
           local version=`cat version.txt`
           popd &&
               cp ../build/gbemu $APP_DIR/Contents/MacOS && 
               cp resources/Info.plist $APP_DIR/Contents/ &&
               sed -i .bak -e "s/@GBEmuVersion@/$version/g" $APP_DIR/Contents/Info.plist &&
               rm $APP_DIR/Contents/Info.plist.bak &&
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

