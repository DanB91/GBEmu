//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#import <AppKit/AppKit.h>
#include "common.h"
#include "gbemu.h"

bool openFileDialogAtPath(const char *path, char *outPath) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        NSString *str = [NSString stringWithUTF8String:path];
        NSURL *url = [NSURL URLWithString:str];
        panel.directoryURL = url;   
        panel.allowedFileTypes = @[@"gb", @"gbc", @"sgb"];
        panel.message = @"Open Game Boy ROM File";
        if ([panel runModal] == NSModalResponseOK) {
            NSURL *url = [panel URLs][0];
            const char * path = [url.path UTF8String];
            strncpy(outPath, path, MAX_PATH_LEN);
            return true;
        }
        return false;
    }
}

bool openDirectoryAtPath(const char *path, char *outPath) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        NSString *str = [NSString stringWithUTF8String:path];
        NSURL *url = [NSURL URLWithString:str];
        panel.directoryURL = url;   
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.canCreateDirectories = YES;
        panel.allowsMultipleSelection = NO;
        
        panel.message = @"Choose GBEmu Home Directory";
        if ([panel runModal] == NSModalResponseOK) {
            NSURL *url = [panel URLs][0];
            const char *pathToCopy = [url.path UTF8String];
            strncpy(outPath, pathToCopy, MAX_PATH_LEN);
            return true;
        }
        return false;
    }
}
