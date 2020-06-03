//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#include "version.h"
#include "3rdparty/GL/gl3w.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

//unity

#define CO_IMPL
#include "common.h"

#define GB_IMPL
#ifdef CO_DEBUG
#   include "gbemu.h"
#   include "debugger.h"
#   define CALL_DYN(fn,...)gbEmuCode->fn(__VA_ARGS__)
#else
#   include "gbemu.cpp"
#   include "debugger.cpp"
#   define CALL_DYN(fn,...)fn(__VA_ARGS__)
#endif

#include "3rdparty/gl3w.c"
#include "config.cpp"
#if defined(LINUX) || defined(WINDOWS)
#   include "sdl_debugger.cpp"
#endif

#ifdef WINDOWS
#include <io.h>
#include <ShlObj_core.h>
#define F_OK 0
#define access(a1,a2) _access(a1,a2)
#endif

#define RUN_BOOT_SCREEN_FLAG "-b" //unused for now
#define DEBUG_ON_BOOT_FLAG "-d"

#define DEFAULT_GBEMU_HOME_PATH "gbemu_home"
#define HOME_PATH_ENV_VARIABLE "GBEMU_HOME"
#define HOME_PATH_CONFIG_FILE "gbemu_home_path.txt"

#define GBEMU_CONFIG_FILENAME "config.txt"
#define GBEMU_CONFIG_FILE_PATH (".." FILE_SEPARATOR GBEMU_CONFIG_FILENAME)

#define CART_RAM_FILE_EXTENSION "sav"
#define RTC_FILE_LEN 48


#define FRAME_TIME_US 16666
#define CYCLES_PER_SLEEP 60000;

#define AUDIO_SAMPLE_RATE 44100   
#define MAX_AUDIO_SAMPLES_TO_QUEUE 4410 //100 ms

#define DEBUG_WINDOW_MIN_HEIGHT 800
#define DEBUG_WINDOW_MIN_WIDTH 800

#ifdef MAC
#   define CTRL_KEY KMOD_GUI
#else
#   define CTRL_KEY KMOD_CTRL
#endif

    
#define ANALOG_STICK_DEADZONE 8000
#define ANALOG_TRIGGER_DEADZONE 8000
    
struct LinuxAndWindowsPlatformState : PlatformState{
    SDL_Rect textureRect;
    SDL_Texture *screenTexture;
};
void renderMainScreen(SDL_Renderer *renderer, PlatformState *platformState, 
    const PaletteColor *screen, int windowW, int windowH);
PlatformState *initPlatformState(SDL_Renderer *renderer, int windowW, int windowH);
void windowResized(int w, int h, PlatformState *platformState);
bool setFullScreen(SDL_Window *window, bool isFullScreen);

#ifdef MAC 
#   define CTRL "Command-"
#else
#   define CTRL "Ctrl-"
#endif

static const char *resultCodeToString[] = {
    [(int)FileSystemResultCode::OK] =  "",
    [(int)FileSystemResultCode::PermissionDenied] = "Permission denied",
    [(int)FileSystemResultCode::OutOfSpace] = "Out of disk space",
    [(int)FileSystemResultCode::OutOfMemory] = "Out of memory",
    [(int)FileSystemResultCode::AlreadyExists] = "File already exists",
    [(int)FileSystemResultCode::IOError] = "Error reading or writing to disk",
    [(int)FileSystemResultCode::NotFound] = "File not found",
    [(int)FileSystemResultCode::Unknown] = "Unknown error"
};

static const char *movementKeyMappingToString[] = {
  [(int)MovementKeyMappingValue::Backspace] = "Backspace", 
  [(int)MovementKeyMappingValue::Enter] = "Enter", 
  [(int)MovementKeyMappingValue::Down] = "Down Arrow", 
  [(int)MovementKeyMappingValue::Up] = "Up Arrow", 
  [(int)MovementKeyMappingValue::Left] = "Left Arrow", 
  [(int)MovementKeyMappingValue::Right] = "Right Arrow", 
  [(int)MovementKeyMappingValue::SpaceBar] = "Space Bar", 
  [(int)MovementKeyMappingValue::Tab] = "Tab", 
};

static const SDL_Keycode movementKeyMappingToSDLKeyCode[] = {
  [(int)MovementKeyMappingValue::Backspace] = SDLK_BACKSPACE, 
  [(int)MovementKeyMappingValue::Enter] = SDLK_RETURN, 
  [(int)MovementKeyMappingValue::Down] = SDLK_DOWN, 
  [(int)MovementKeyMappingValue::Up] = SDLK_UP, 
  [(int)MovementKeyMappingValue::Left] = SDLK_LEFT, 
  [(int)MovementKeyMappingValue::Right] = SDLK_RIGHT, 
  [(int)MovementKeyMappingValue::SpaceBar] = SDLK_SPACE, 
  [(int)MovementKeyMappingValue::Tab] = SDLK_TAB, 
};

static const int gamepadMappingToSDLButton[] = {
  [(int)GamepadMappingValue::A] = SDL_CONTROLLER_BUTTON_A,  
  [(int)GamepadMappingValue::B] = SDL_CONTROLLER_BUTTON_B,  
  [(int)GamepadMappingValue::X] = SDL_CONTROLLER_BUTTON_X,  
  [(int)GamepadMappingValue::Y] = SDL_CONTROLLER_BUTTON_Y,  
    
  [(int)GamepadMappingValue::Up] = SDL_CONTROLLER_BUTTON_DPAD_UP,  
  [(int)GamepadMappingValue::Down] = SDL_CONTROLLER_BUTTON_DPAD_DOWN,  
  [(int)GamepadMappingValue::Left] = SDL_CONTROLLER_BUTTON_DPAD_LEFT,  
  [(int)GamepadMappingValue::Right] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,  
  
  [(int)GamepadMappingValue::LeftBumper] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  
  [(int)GamepadMappingValue::RightBumper] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,  
    
  [(int)GamepadMappingValue::Start] = SDL_CONTROLLER_BUTTON_START,  
  [(int)GamepadMappingValue::Back] = SDL_CONTROLLER_BUTTON_BACK,  
    
  [(int)GamepadMappingValue::Guide] = SDL_CONTROLLER_BUTTON_GUIDE,  
    
//using negative numbers as to not conflict with SDL codes
  [(int)GamepadMappingValue::LeftTrigger] = NO_INPUT_MAPPING - 1,
  [(int)GamepadMappingValue::RightTrigger] = NO_INPUT_MAPPING - 2,
};
static const char *gamepadMappingToString[] = {
  [(int)GamepadMappingValue::A] = "A Button",  
  [(int)GamepadMappingValue::B] = "B Button",  
  [(int)GamepadMappingValue::X] = "X Button",  
  [(int)GamepadMappingValue::Y] = "Y Button",  
    
  [(int)GamepadMappingValue::Up] = "D-Pad Up",  
  [(int)GamepadMappingValue::Down] = "D-Pad Down",  
  [(int)GamepadMappingValue::Left] = "D-Pad Left",  
  [(int)GamepadMappingValue::Right] = "D-Pad Right",  
  
  [(int)GamepadMappingValue::LeftBumper] = "Left Bumper",  
  [(int)GamepadMappingValue::RightBumper] = "Right Bumper",  
    
  [(int)GamepadMappingValue::Start] = "Start Button",  
  [(int)GamepadMappingValue::Back] = "Back Button",  
    
  [(int)GamepadMappingValue::Guide] = "Guide Button",  
    
  [(int)GamepadMappingValue::LeftTrigger] = "Left Trigger",  
  [(int)GamepadMappingValue::RightTrigger] = "Right Trigger",  
};

static const char *actionToString[(int)Input::Action::NumActions] = {
    [(int)Input::Action::A] = "Game Boy Button A", 
    [(int)Input::Action::B] = "Game Boy Button B", 
    [(int)Input::Action::Start] = "Game Boy Button Start", 
    [(int)Input::Action::Select] = "Game Boy Button Select", 
    [(int)Input::Action::Up] = "Game Boy D-Pad Up",  
    [(int)Input::Action::Down] = "Game Boy D-Pad Down",  
    [(int)Input::Action::Left] = "Game Boy D-Pad Left",  
    [(int)Input::Action::Right] = "Game Boy D-Pad Right",  
    [(int)Input::Action::Rewind] = "Emulator Rewind",  
    [(int)Input::Action::DebuggerContinue] = "Debugger Continue",  
    [(int)Input::Action::DebuggerStep] = "Debugger Step",  
    [(int)Input::Action::ShowHomePath] = "Show GBEmu Home Path",  
    [(int)Input::Action::Mute] = "Mute Sound",
    [(int)Input::Action::Pause] = "Pause Emulator",
    [(int)Input::Action::Reset] = "Reset Emulator",  
    [(int)Input::Action::ShowDebugger] = "Show Debugger",  
    [(int)Input::Action::FullScreen] = "Make Full Screen",  
    [(int)Input::Action::ShowControls] = "Show This Message"  
};

bool openFileDialogAtPath(const char *path, char *outPath);
bool openDirectoryAtPath(const char *path, char *outPath);

void alertDialogSDL(const char *message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, nullptr);
}
#ifdef CO_DEBUG

GBEmuCode loadGBEmuCode(const char *gbemuCodePath) {
    GBEmuCode ret = {};

    ret.gbEmuCodeHandle = SDL_LoadObject(gbemuCodePath);
    CO_ASSERT_MSG(ret.gbEmuCodeHandle, "Error opening shared obj: %s", SDL_GetError());

    ret.runFrame = (RunFrameFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "runFrame");
    CO_ASSERT(ret.runFrame);

    ret.reset = (ResetFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "reset");
    CO_ASSERT(ret.runFrame);
    
    ret.setPlatformContext = (SetPlatformContextFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "setPlatformContext");
    CO_ASSERT(ret.setPlatformContext);
    
    ret.rewindState = (RewindStateFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "rewindState");
    CO_ASSERT(ret.rewindState);
    
    ret.machineStep = (StepFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "machineStep");
    CO_ASSERT(ret.machineStep);
    
    ret.readByte = (ReadByteFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "readByte");
    CO_ASSERT(ret.readByte);
    
    ret.readWord = (ReadWordFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "readWord");
    CO_ASSERT(ret.readWord);
    
    ret.drawDebugger = (DrawDebuggerFn*) SDL_LoadFunction(ret.gbEmuCodeHandle, "drawDebugger");
    CO_ASSERT(ret.drawDebugger);
    
    struct stat fileStats;
    
    stat(gbemuCodePath, &fileStats);
    ret.gbEmuTimeLastModified = fileStats.st_mtime;
    ret.setPlatformContext(getMemoryContext(), alertDialogSDL);

    return ret;
}

#else 
#define loadGBEmuCode(fileName) 0;(void)gbEmuCode;
void runFrame(CPU *, MMU *, GameBoyDebug *gbDebug, ProgramState *, TimeUS dt);
void reset(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug, ProgramState *programState);
#endif

enum class HomeDirectoryOption {
    DEFAULT = 0,
    CUSTOM,
    EXIT 
};

//returns true if ok is pressed
static HomeDirectoryOption showHomeDirectoryDialog(const char *defaultHomeDirPath, const char *configFilePath) {
    SDL_MessageBoxButtonData defaultHomeButton;
    defaultHomeButton.buttonid = (int)HomeDirectoryOption::DEFAULT;
    defaultHomeButton.text = "Default";
    defaultHomeButton.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    
    SDL_MessageBoxButtonData customPathButton;
    customPathButton.buttonid = (int)HomeDirectoryOption::CUSTOM;
    customPathButton.text = "Custom";
    customPathButton.flags = 0;
    
    SDL_MessageBoxButtonData exitButton;
    exitButton.buttonid = (int)HomeDirectoryOption::EXIT;
    exitButton.text = "Exit";
    exitButton.flags = 0;
    
    
    SDL_MessageBoxButtonData buttons[] = {
      defaultHomeButton, customPathButton, exitButton   
    };
    
    SDL_MessageBoxData data = {};
    data.buttons = buttons;
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.numbuttons = 3;
    char *message = nullptr;
    
    buf_gen_memory_printf(message, "GBEmu has no home directory set. This is a required directory that will be used for storage of ROMs, save states, and other files.\n"
        "To set the GBEmu home directory, you have 3 options:\n"
        "\n"
		"1) 'Default': Use and, if needed, create the default GBEmu home directory at %s.\n"
		"2) 'Custom': Select a custom GBEmu home directory.\n"
		"3) 'Exit': Exit GBEmu and set the %s environment variable to a directory of your choice.\n"
        "\n"
        "If options 1) or 2) are selected, a small text file with the GBEmu home directory path will be created at %s.\nThe path in this file can be changed at any time. "
        "You can avoid creation of this text file by choosing option 3).", defaultHomeDirPath, HOME_PATH_ENV_VARIABLE, configFilePath);
    data.message = message;
    data.title = "Attention!";
    
    int pressedButton = -1;
    bool success = SDL_ShowMessageBox(&data, &pressedButton) == 0;
    buf_gen_memory_free(message);
    
    return (success && pressedButton != -1) ? (HomeDirectoryOption)pressedButton : HomeDirectoryOption::EXIT;
}

static void processKeyUp(SDL_Keycode key, Input::State *input, 
                       Input::CodeToActionMap *keysMap, Input::CodeToActionMap *ctrlKeysMap) {
   
    profileStart("process key up", profileState);
    Input::Action action;
    if (retrieveActionForInputCode(key, &action, ctrlKeysMap)) {
        input->actionsHit[(int)action] = false; 
    }
    if (retrieveActionForInputCode(key, &action, keysMap)) {
        input->actionsHit[(int)action] = false; 
    }

    switch (key) {
    case SDLK_0: 
    case SDLK_1:   
    case SDLK_2:   
    case SDLK_3:   
    case SDLK_4:   
    case SDLK_5:   
    case SDLK_6:   
    case SDLK_7:   
    case SDLK_8:   
    case SDLK_9: {
        input->slotToRestoreOrSave = (key == SDLK_0) ? 0 : (key - SDLK_1 + 1);
        input->saveState = false;
        input->restoreState = false;
    } break;
    case SDLK_ESCAPE: {
        input->escapeFullScreen = false;
    } break;
    case SDLK_KP_ENTER:
    case SDLK_RETURN: {
       input->enterPressed = false; 
    } break;
    default:
        //do nothing
        break;
    }
    profileEnd(profileState);
}
static void processKeyDown(SDL_Keycode key,  bool isCtrlDown, Input::State *input, 
                       Input::CodeToActionMap *keysMap, Input::CodeToActionMap *ctrlKeysMap) {
   
    profileStart("process key", profileState);
    //TODO: don't test for ctrl down if lifting up
    if (isCtrlDown) {
        Input::Action action;
        if (retrieveActionForInputCode(key, &action, ctrlKeysMap)) {
            input->actionsHit[(int)action] = true; 
        }
    }
    else {
        Input::Action action;
        if (retrieveActionForInputCode(key, &action, keysMap)) {
            input->actionsHit[(int)action] = true; 
        }
    }

    switch (key) {
    case SDLK_0: 
    case SDLK_1:   
    case SDLK_2:   
    case SDLK_3:   
    case SDLK_4:   
    case SDLK_5:   
    case SDLK_6:   
    case SDLK_7:   
    case SDLK_8:   
    case SDLK_9: {
        input->slotToRestoreOrSave = (key == SDLK_0) ? 0 : (key - SDLK_1 + 1);
        if (isCtrlDown) {
            input->saveState = true;
        }
        else {
            input->restoreState = true;
        }
    } break;
    case SDLK_ESCAPE: {
        input->escapeFullScreen = true;
    } break;
    case SDLK_KP_ENTER:
    case SDLK_RETURN: {
       input->enterPressed = true; 
    } break;
    default:
        //do nothing
        break;
    }
    profileEnd(profileState);
}


static void processButton(int button, bool isDown, Input::State *input, Input::CodeToActionMap *gamepadMap) {
    Input::Action action;
    if (retrieveActionForInputCode(button, &action, gamepadMap)) {
        input->actionsHit[(int)action] = isDown; 
    }
}



static void
cleanUp(CartRAMPlatformState *crps, DebuggerPlatformContext *debuggerContext) {
    closeMemoryMappedFile(crps->cartRAMFileHandle);
    if (debuggerContext) {
        closeDebugger(debuggerContext);
    }
}

//returns true succeeds. else false and program should exit
static bool createBatteryBackedRAMFile(MMU *mmu, const char *filePath) {
    i64 totalSize = mmu->cartRAMSize; 
    if (mmu->hasRTC) totalSize += sizeof(RTCFileState);
    u8 *dataToSave = PUSHMCLR(totalSize, u8);
    auto res = writeDataToFile(dataToSave, totalSize, filePath);
    POPM(dataToSave);
    switch (res) {
    case FileSystemResultCode::OK: return true;
    case FileSystemResultCode::OutOfSpace: {
        ALERT("Could not create battery backed RAM file because disk is out of space. GBEmu will now exit.");
        return false;
    } break;
    case FileSystemResultCode::PermissionDenied: {
        ALERT("Could not create battery backed RAM file because permission is denied. GBEmu will now exit.");
        return false;
    } break;
    case FileSystemResultCode::IOError: {
        ALERT("Could not create battery backed RAM file because of an IO error. GBEmu will now exit.");
        return false;
    } break;
    default: {
        ALERT("Could not create battery backed RAM file because of an unknown error. GBEmu will now exit.");
        return false;
    } break;
    }
}

static bool backupCartRAMFile(const char *fileNameToBackup, const char *extension, const char *homeDirectory, MemoryStack *fileMemory) {
    char newPath[MAX_PATH_LEN];
    timestampFileName(fileNameToBackup, extension, newPath);
    char *srcFileName = nullptr;
    buf_gen_memory_printf(srcFileName, "%s.%s",fileNameToBackup, extension);
    ALERT("Invalid cart RAM file: %s%s! This file will be copied to %s%s and a new file will be created.", homeDirectory, srcFileName, homeDirectory, newPath);
    auto copyResult = copyFile(srcFileName, newPath, fileMemory);
    buf_gen_memory_free(srcFileName);
    switch (copyResult) {
    case FileSystemResultCode::OK: break;
    case FileSystemResultCode::OutOfSpace:  {
        ALERT("Could not copy file. Out of space.  GBEmu will now exit."); 
        return false;
    } break;
    case FileSystemResultCode::PermissionDenied:  {
        ALERT("Could not copy file. Permission denied.  GBEmu will now exit."); 
        return false;
    } break;
    default: {
        ALERT("Could not copy file. Unknown error.  GBEmu will now exit."); 
        return false;
    } break;
    }
    return true;
}

static bool handleInputMappingFromConfig(Input *input, Input::Action action,
                                         ConfigKey *configKey, ConfigValue *configValues, isize numConfigValues) {
    int code;
    fori (numConfigValues) {
        ConfigValue *configValue = &configValues[i];
        switch (configValue->type) {
        case ConfigValueType::GamepadMapping: {
            code = gamepadMappingToSDLButton[(int)configValue->gamepadMapping.value];
            
            Input::Action boundAction;
            if (retrieveActionForInputCode(code, &boundAction, &input->gamepadMap)) {
                char *configValueString = PUSHMCLR(configValue->textFromFile.len + 1, char);
                AutoMemory am(configValueString);
                copyMemory(configValue->textFromFile.data, configValueString, configValue->textFromFile.len);
                ALERT_EXIT("In %s: '%s' cannot be bound multiple to emulator actions ('%s' and '%s').",
                           GBEMU_CONFIG_FILENAME, configValueString,
                           inputActionToStr[(int)boundAction], inputActionToStr[(int)action]);
                return false;
            }
            registerInputMapping(code, action, &input->gamepadMap);
        } break;
        case ConfigValueType::KeyMapping: {
            switch (configValue->keyMapping.type) {
            case KeyMappingType::Character: {
                code = utf32FromUTF8(configValue->keyMapping.characterValue);
            } break;
            case KeyMappingType::MovementKey: {
                SDL_Keycode sdlCode = movementKeyMappingToSDLKeyCode[(int)configValue->keyMapping.movementKeyValue];
                code = sdlCode;
            } break;
            }
            auto map = (configValue->keyMapping.isCtrlHeld) ?
                                    &input->ctrlKeysMap : &input->keysMap;
            Input::Action boundAction;
            if (retrieveActionForInputCode(code, &boundAction, map)) {
                char *configValueString = PUSHMCLR(configValue->textFromFile.len + 1, char);
                AutoMemory am(configValueString);
                copyMemory(configValue->textFromFile.data, configValueString, configValue->textFromFile.len);
                ALERT_EXIT("In %s: '%s' cannot be bound multiple to emulator actions ('%s' and '%s').",
                           GBEMU_CONFIG_FILENAME, configValueString,
                           inputActionToStr[(int)boundAction], inputActionToStr[(int)action]);
                return false;
            }
            registerInputMapping(code, action, map );
        } break;
        default: {
            char *configKeyString = PUSHMCLR(configKey->textFromFile.len + 1, char);
            AutoMemory am(configKeyString);
            copyMemory(configKey->textFromFile.data, configKeyString, configKey->textFromFile.len);
            ALERT_EXIT("'%s' at line: %d, column %d in %s must be bound to a gamepad or key mapping.",
                       configKeyString, configKey->line, configKey->posInLine, GBEMU_CONFIG_FILENAME);
            return false;
        } break;
        }
    }

    return true;

}
static void showConfigError(const char *message,  const ParserResult *result) {
    char *errorLine = PUSHMCLR(result->errorLineLen + 5, char);
    isize errorTokenLen = result->stringAfterErrorToken - result->errorToken;
    char *errorToken = PUSHMCLR(errorTokenLen + 1, char);
    strncpy(errorToken, result->errorToken, (usize)errorTokenLen); 
    
    CO_ERR("Parser config status: %d", result->status);
    char *tmp = strncpy(errorLine, result->errorLine, (usize)(result->errorToken - result->errorLine));
    CO_ASSERT_EQ(result->errorToken - result->errorLine, (isize)strlen(tmp));
    tmp += result->errorToken - result->errorLine;
    *tmp++ = '<';
    *tmp++ = '<';
    strncpy(tmp, errorToken, (usize)errorTokenLen); 
    CO_ASSERT_EQ(result->stringAfterErrorToken - result->errorToken, (isize)strlen(tmp));
    tmp += errorTokenLen;
    *tmp++ = '>';
    *tmp++ = '>';
    strncpy(tmp, result->stringAfterErrorToken, (usize)(result->errorLine + result->errorLineLen - result->stringAfterErrorToken ));
    CO_ASSERT_EQ(strlen(result->errorLine) + 3, strlen(errorLine));
    //ALERT_EXIT has a space before "Exiting...", so we will just hard code it here. 
    ALERT("Invalid value '%s' in %s on line %d -- %s." ENDL ENDL "%s" ENDL ENDL "Exiting...", errorToken, GBEMU_CONFIG_FILENAME, result->errorLineNumber, message, errorLine);
    POPM(errorLine);
}

static inline UTF8Character utf8CharFromScancode(SDL_Scancode scancode, char defaultChar) {
   SDL_Keycode ret = SDL_GetKeyFromScancode(scancode);
   if (ret & SDLK_SCANCODE_MASK) {
      return utf8FromUTF32(defaultChar); 
   }
   
   return utf8FromUTF32(ret < 0x80 ? tolower(ret) : ret);
}

struct PrintableControl {
    UTF32Character keyCode;  
    bool isCtrlDown;
    bool isGamepadControl;
    char *stringToPrint; 
    const char *sectionHeader;
};
static void concatKeyString(char **targetString, const PrintableControl *mapping) {
    if (mapping->isCtrlDown) {
        buf_malloc_strcat(*targetString, CTRL);
    }
    if ((mapping->keyCode & SDLK_SCANCODE_MASK) || iscntrl(mapping->keyCode)) {
        foriarr (movementKeyMappingToSDLKeyCode) {
            if (movementKeyMappingToSDLKeyCode[i] == mapping->keyCode) {
                buf_malloc_strcat(*targetString, movementKeyMappingToString[i]);
                break;
            }
        }
    }
    else {
        buf_malloc_strcat(*targetString, utf8FromUTF32(tolower(mapping->keyCode)).string);
    }
    if (!mapping->isCtrlDown) {
        buf_malloc_strcat(*targetString, " Key");
    }
}
 
static void showInputMapDialog(SDL_Window *mainWindow,
                                Input::CodeToActionMap *keyMap,
                                 Input::CodeToActionMap *ctrlKeyMap,
                                 Input::CodeToActionMap *gamepadMap) { 
    
    PrintableControl *mappingsToPrint = nullptr;
    PrintableControl *showDialogMapping = nullptr;
    
    {
        PrintableControl pm;
        pm.sectionHeader = ENDL "[[Keyboard]]" ENDL;
        pm.stringToPrint = nullptr;
        buf_malloc_push(mappingsToPrint, pm);
    }
    foribuf (keyMap->mappings) {
        Input::Mapping *mapping = &keyMap->mappings[i];
        PrintableControl pm;
        pm.isCtrlDown = false;
        pm.keyCode = mapping->code;
        pm.isGamepadControl = false;
        pm.stringToPrint = buf_malloc_string(actionToString[(int)mapping->action]);
        pm.sectionHeader = nullptr;
        buf_malloc_push(mappingsToPrint, pm);
        if (mapping->action == Input::Action::ShowControls) {
            showDialogMapping = buf_end(mappingsToPrint) - 1;
        }
    }
    {
        PrintableControl pm;
        pm.stringToPrint = nullptr;
        pm.sectionHeader = ENDL;
        buf_malloc_push(mappingsToPrint, pm);
    }
    foribuf (ctrlKeyMap->mappings) {
        Input::Mapping *mapping = &ctrlKeyMap->mappings[i];
        PrintableControl pm;
        pm.isCtrlDown = true;
        pm.keyCode = mapping->code;
        pm.isGamepadControl = false;
        pm.stringToPrint = buf_malloc_string(actionToString[(int)mapping->action]);
        pm.sectionHeader = nullptr;
        buf_malloc_push(mappingsToPrint, pm);
        if (mapping->action == Input::Action::ShowControls) {
            showDialogMapping = buf_end(mappingsToPrint) - 1;
        }
    }
    {
        PrintableControl pm;
        pm.sectionHeader = ENDL "[[Gamepad]]" ENDL;
        pm.stringToPrint = nullptr;
        buf_malloc_push(mappingsToPrint, pm);
    }
    foribuf (gamepadMap->mappings) {
        Input::Mapping *mapping = &gamepadMap->mappings[i];
        PrintableControl pm;
        pm.isCtrlDown = false;
        pm.keyCode = mapping->code;
        pm.isGamepadControl = true;
        pm.stringToPrint = buf_malloc_string(actionToString[(int)mapping->action]);
        pm.sectionHeader = nullptr;
        buf_malloc_push(mappingsToPrint, pm);
    }
    usize longestStringSize = 0;
    foribuf (mappingsToPrint) {
       if(buf_len(mappingsToPrint[i].stringToPrint) > longestStringSize) {
           longestStringSize = buf_len(mappingsToPrint[i].stringToPrint);
       }
    }
    char *keymapDialog = buf_malloc_string("Below are the keyboard and gamepad controls for GBEmu." ENDL
                                           );
    if (showDialogMapping) {
        buf_malloc_strcat(keymapDialog, "You can pull this message up at any time with "); 
        concatKeyString(&keymapDialog, showDialogMapping);
        buf_malloc_strcat(keymapDialog, "." ENDL); 
    }
    foribuf (mappingsToPrint) {
        if (mappingsToPrint[i].sectionHeader) {
            buf_malloc_strcat(keymapDialog, mappingsToPrint[i].sectionHeader);
            continue;
        }
        buf_malloc_strcat(keymapDialog, mappingsToPrint[i].stringToPrint);
        isize numPaddingSpaces = (isize)longestStringSize - (isize)buf_len(mappingsToPrint[i].stringToPrint);
        if (numPaddingSpaces > 0) {
            char *spaces = PUSHM(numPaddingSpaces + 1, char);
            AutoMemory _am(spaces);
            
            fillMemory(spaces, ' ', numPaddingSpaces);
            spaces[numPaddingSpaces] = '\0';
            buf_malloc_strcat(keymapDialog, spaces);
        }
        
        buf_malloc_strcat(keymapDialog, " ->  ");
        if (mappingsToPrint[i].isGamepadControl) {
            forjarr (gamepadMappingToSDLButton) {
                if (gamepadMappingToSDLButton[j] == mappingsToPrint[i].keyCode) {
                    buf_malloc_strcat(keymapDialog, gamepadMappingToString[j]);
                    break;
                }
            }
        }
        else {
            concatKeyString(&keymapDialog, &mappingsToPrint[i]);
        }
        
        buf_malloc_strcat(keymapDialog, ENDL);
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "GBEmu Controls",
                             keymapDialog, mainWindow);
    foribuf (mappingsToPrint) {
         buf_malloc_free(mappingsToPrint[i].stringToPrint);
    }
    buf_malloc_free(mappingsToPrint);
    buf_malloc_free(keymapDialog);
}
static bool doConfigFileParsing(const char *configFilePath, ProgramState *programState) {

    auto result = parseConfigFile(configFilePath); 
    bool isNewConfig = false;
    switch (result.fsResultCode) {
    case FileSystemResultCode::OK:  {
        //do nothing and continue 
    } break;
    case FileSystemResultCode::NotFound: {
        freeParserResult(&result);
        
        profileStart("Save new file", profileState);
        const char defaultConfigFileContents[] = 
            "//See README.md for more about config.txt"
            ENDL
            "//Control Mappings" ENDL 
            ENDL
            "Up = Key %s, Gamepad Up" ENDL 
            "Down = Key %s,  Gamepad Down" ENDL
            "Left = Key %s, Gamepad Left" ENDL
            "Right = Key %s, Gamepad Right" ENDL
            ENDL 
            "Start = Key Enter, Gamepad Start" ENDL 
            "Select = Key %s, Gamepad Back" ENDL
            ENDL 
            "A = Key %s, Gamepad A" ENDL 
            "B = Key %s, Gamepad B" ENDL
            ENDL
            "Rewind = Key Left, Gamepad LeftBumper" ENDL
            "DebuggerStep = Key %s" ENDL
            "DebuggerContinue = Key %s" ENDL
            ENDL
            "Mute = Key " CTRL "%s" ENDL
            "Pause = Key " CTRL "%s" ENDL
            "Reset = Key " CTRL "%s" ENDL
            "ShowDebugger = Key " CTRL "%s" ENDL
            "FullScreen = Key " CTRL "%s" ENDL
            "ShowHomePath = Key %s" ENDL
            "ShowControls = Key " CTRL "%s" ENDL
            ENDL
            "//Misc" ENDL
            "ScreenScale = 4";
        char *fileContents = nullptr;
        buf_gen_memory_printf(fileContents, defaultConfigFileContents, 
                              utf8CharFromScancode(SDL_SCANCODE_W, 'w').string,
                              utf8CharFromScancode(SDL_SCANCODE_S, 's').string, 
                              utf8CharFromScancode(SDL_SCANCODE_A, 'a').string,
                              utf8CharFromScancode(SDL_SCANCODE_D, 'd').string,
                              utf8CharFromScancode(SDL_SCANCODE_BACKSLASH, '\\').string,
                              utf8CharFromScancode(SDL_SCANCODE_SLASH, '/').string,
                              utf8CharFromScancode(SDL_SCANCODE_PERIOD, '.').string,
                              utf8CharFromScancode(SDL_SCANCODE_N, 'n').string,
                              utf8CharFromScancode(SDL_SCANCODE_C, 'c').string,
                              utf8CharFromScancode(SDL_SCANCODE_U, 'u').string,
                              utf8CharFromScancode(SDL_SCANCODE_P, 'p').string,
                              utf8CharFromScancode(SDL_SCANCODE_R, 'r').string,
                              utf8CharFromScancode(SDL_SCANCODE_B, 'b').string,
                              utf8CharFromScancode(SDL_SCANCODE_F, 'f').string,
                              utf8CharFromScancode(SDL_SCANCODE_H, 'h').string,
                              utf8CharFromScancode(SDL_SCANCODE_N, 'n').string);
                                                     
        auto writeResult = writeDataToFile(fileContents, (isize)strlen(fileContents), configFilePath);
        switch (writeResult) {
        case FileSystemResultCode::OK: {
            //continue 
        } break;
        default: {
            buf_gen_memory_free(configFilePath);
            ALERT_EXIT("Failed to save default config file. Reason: %s.", resultCodeToString[(int)writeResult]); 
            return false;
        } break;
        }
        profileEnd(profileState);
        
        profileStart("Parse new config file", profileState); 
        result = parseConfigFile(configFilePath); 

        switch (result.fsResultCode) {
        case FileSystemResultCode::OK:  {
            //do nothing and continue 
        } break;

        default: {
            ALERT_EXIT("Failed to read the newly created" GBEMU_CONFIG_FILENAME " file. Reason: %s.",  resultCodeToString[(int)result.fsResultCode]);
            return false;
        } break;
        }

        profileEnd(profileState);
        isNewConfig = true;

    } break;
    default: {
       ALERT_EXIT("Failed to read " GBEMU_CONFIG_FILENAME ". Reason: %s.",  resultCodeToString[(int)result.fsResultCode]);
       return false;
    } break;
    }
    
#define CASE_ERROR(errCase, msg) case ParserStatus::errCase: {\
       showConfigError(msg, &result);\
       return false;\
} break
    
    switch (result.status) {
    case ParserStatus::OK: {
        //do nothing and continue 
    } break;
    CASE_ERROR(BadTokenStartOfLine, "Invalid config option");
    CASE_ERROR(UnknownConfigValue, "Invalid config value");
    CASE_ERROR(UnknownConfigKey, "Unrecognized config option");
    CASE_ERROR(UnrecognizedKeyMapping, "Unrecognized key to map to");
    CASE_ERROR(NumberKeyMappingNotAllowed, "Mapping to a number key is not supported");
    CASE_ERROR(UnrecognizedGamepadMapping, "Unrecognized gamepad button to map to");
    CASE_ERROR(MissingEquals, "Equals sign expected here");
    CASE_ERROR(ExtraneousToken, "Extraneous token; new line expected");
    CASE_ERROR(MissingComma, "Comma expected here");
    case ParserStatus::UnexpectedNewLine: {
        ALERT("Unexpected new line in %s on line %d:" ENDL ENDL "%s" ENDL ENDL "Exiting...", 
              GBEMU_CONFIG_FILENAME, result.errorLineNumber, result.errorLine);
        return false;
    } break;
    }
#undef CASE_ERROR
    
    Input *input = &programState->input;
    
    fori (result.numConfigPairs) {
#define CASE_MAPPING(mapping)  case ConfigKeyType::mapping: {\
            if (!handleInputMappingFromConfig(input,\
                Input::Action::mapping, &cp->key, cp->values, cp->numValues)) {\
               return false;\
            }\
        } break
        ConfigPair *cp = result.configPairs + i;  
                             
        switch (cp->key.type) {
            //do nothing
            break;
        CASE_MAPPING(Start);
        CASE_MAPPING(Select);
        CASE_MAPPING(A);
        CASE_MAPPING(B);
        CASE_MAPPING(Up);
        CASE_MAPPING(Down);
        CASE_MAPPING(Left);
        CASE_MAPPING(Right);
        CASE_MAPPING(Pause);
        CASE_MAPPING(Mute);
        CASE_MAPPING(ShowDebugger);
        CASE_MAPPING(ShowHomePath);
        CASE_MAPPING(Rewind);
        CASE_MAPPING(DebuggerStep);
        CASE_MAPPING(DebuggerContinue);
        CASE_MAPPING(Reset);
        CASE_MAPPING(FullScreen);
        CASE_MAPPING(ShowControls);
        case ConfigKeyType::ScreenScale: {
           if (cp->numValues != 1) {
               ALERT_EXIT("ScreenScale config option must only take one value.");
               return false;
           }
           ConfigValue *value = cp->values;
           switch (value->type) {
           case ConfigValueType::Integer: {
              int screenScale = value->intValue; 
              //validate screenScale
              if (screenScale <= 0) {
                  char *configKeyString = PUSHMCLR(cp->key.textFromFile.len + 1, char);
                  AutoMemory am(configKeyString);
                  copyMemory(cp->key.textFromFile.data, configKeyString, cp->key.textFromFile.len);
                  ALERT_EXIT("'%s' at line: %d, column %d in %s must be bound to a number greater than 0.", 
                             configKeyString, cp->key.line, cp->key.posInLine, GBEMU_CONFIG_FILENAME);
                  return false;
              }
              programState->screenScale = screenScale;
           } break;
           default:  {
               char *configKeyString = PUSHMCLR(cp->key.textFromFile.len + 1, char);
               AutoMemory am(configKeyString);
               copyMemory(cp->key.textFromFile.data, configKeyString, cp->key.textFromFile.len);
               ALERT_EXIT("'%s' at line: %d, column %d in %s must be bound to a number greater than 0.", 
                          configKeyString, cp->key.line, cp->key.posInLine, GBEMU_CONFIG_FILENAME);
               return false;
           } break;
           }
        } break;
        }
#undef CASE_MAPPING
    }

    //defaults
    if (programState->screenScale <= 0) {
        ALERT("ScreenScale not found in %s.  Defaulting to a ScreenScale of %d.", GBEMU_CONFIG_FILENAME, DEFAULT_SCREEN_SCALE);
        programState->screenScale = DEFAULT_SCREEN_SCALE;
    }
    
    freeParserResult(&result);
    
    if (isNewConfig) {
        showInputMapDialog(nullptr, &input->keysMap, &input->ctrlKeysMap, &input->gamepadMap);
    }
    return true;
}

//returns true for yes, else false
static bool showYesNoDialog(const char *message, const char *title) {
    SDL_MessageBoxButtonData noButton;
    noButton.buttonid = 0;
    noButton.text = "No";
    noButton.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;

    SDL_MessageBoxButtonData yesButton;
    yesButton.buttonid = 1;
    yesButton.text = "Yes";
    yesButton.flags = 0;

    SDL_MessageBoxButtonData buttons[] = {
        noButton, yesButton 
    };

    SDL_MessageBoxData data = {};
    data.buttons = buttons;
    data.flags = SDL_MESSAGEBOX_INFORMATION;
    data.numbuttons = ARRAY_LEN(buttons);
            
    data.message = message;
    data.title = title;

    int isYesPressed = 0;
    bool success = SDL_ShowMessageBox(&data, &isYesPressed) == 0;
    
    return success && isYesPressed;
}

static void 
mainLoop(SDL_Window *window, SDL_Renderer *renderer, PlatformState *platformState,
         SDL_AudioDeviceID audioDeviceID, SDL_GameController **gamepad, const char *romFileName,
         bool shouldEnableDebugMode, DebuggerPlatformContext *debuggerContext, SDL_Window **debuggerWindow, GameBoyDebug *gbDebug, ProgramState *programState) {
    char filePath[MAX_PATH_LEN];
    
#define GAME_LIB_PATH "gbemu.so"
    char gbemuCodePath[MAX_PATH_LEN + 1];
    //used for hot loading
    getCurrentWorkingDirectory(gbemuCodePath);
    strncat(gbemuCodePath, FILE_SEPARATOR GAME_LIB_PATH, MAX_PATH_LEN);
#undef GAME_LIB_PATH
    
    SDL_PauseAudioDevice(audioDeviceID, 0);

    bool isRunning = true;
    bool isPaused = false;
    CPU *cpu = PUSHMCLR(1, CPU);

    MMU *mmu = PUSHMCLR(1, MMU);
    
    mmu->lcd.screen = mmu->lcd.screenStorage;
    mmu->lcd.backBuffer = mmu->lcd.backBufferStorage;


    gbDebug->isEnabled = shouldEnableDebugMode;

    cpu->isPaused = shouldEnableDebugMode;
    programState->shouldUpdateTitleBar = true;
    MemoryStack *fileMemory = &programState->fileMemory;


    /**********
     * Load ROM
     **********/
    char windowTitle[128] = {};
    char romTitle[MAX_ROM_NAME_LEN + 1] = {};
    {
        auto fileResult = readEntireFile(romFileName, fileMemory);
        switch (fileResult.resultCode) {
        case FileSystemResultCode::OK: break;
        case FileSystemResultCode::NotFound: {
            ALERT("Could not open ROM: %s. File not found.", romFileName);
            return;
        } break;
        case FileSystemResultCode::PermissionDenied: {
            ALERT("Could not open ROM: %s. Permission denied.", romFileName);
            return;
        } break;
        case FileSystemResultCode::IOError: {
            ALERT("Could not open ROM: %s. IO Error.", romFileName);
            return;
        } break;
        case FileSystemResultCode::OutOfMemory: {
            ALERT("Could not open ROM: %s. File is too big!", romFileName);
            return;
        } break;
        default: {
            ALERT("Could not open ROM: %s. Unknown Error.", romFileName);
            return;
        } break;
        }
        mmu->romData = fileResult.data;
        mmu->romSize = fileResult.size;
        if (mmu->romSize < 0x150) {
            ALERT("This file is not a valid Game Boy ROM file. File is too small.");
            return;
        }
        
        bool shouldContinue = false;
		{
			u8 computedChecksum = 0;
			for (int i = 0x134; i <= 0x14C; i++) {
				computedChecksum -= mmu->romData[i] + 1;
			}
			if (computedChecksum != mmu->romData[0x14D]) {
                bool isYesPressed = 
                        showYesNoDialog(
                            "Header checksum does not match. This file may not be a valid Game Boy ROM file.\nAre you sure you want to continue?",
                            "Warning!");
                if (!isYesPressed) {
                   return; 
                }
                else {
                    shouldContinue = true;
                }
			}
		}
		if (!shouldContinue) {
			u16 computedChecksum = 0;
            u16 globalChecksum = (u16)((mmu->romData[0x14E] << 8) | mmu->romData[0x14F]);
			for (int i = 0; i < mmu->romSize ; i++) {
                if (i == 0x14E || i == 0x14F) {
                    continue;
                }
				computedChecksum += mmu->romData[i];
			}
			if (computedChecksum != globalChecksum) {
                bool isYesPressed = 
                        showYesNoDialog(
                            "Global checksum does not match. This file may not be a valid Game Boy ROM file.\nAre you sure you want to continue?",
                            "Warning!");
                if (!isYesPressed) {
                   return; 
                }
			}
		}
        u8 mbcType = mmu->romData[0x147];

        switch (mbcType) {

        case 0: {
            mmu->mbcType = MBCType::MBC0;
        } break; 
        case 1: {
            mmu->mbcType = MBCType::MBC1;
        } break; 
        case 2 ... 3: {
            mmu->mbcType = MBCType::MBC1;
            mmu->hasRAM = true;
            if (mbcType == 3) {
                mmu->hasBattery = true;       
            }
        } break; 
        case  8 ... 9: {
            mmu->mbcType = MBCType::MBC0;
            mmu->hasRAM = true;
        } break;
        case 0xF ... 0x13: {
            mmu->mbcType = MBCType::MBC3;
            if (mbcType == 0xF || mbcType == 0x10) {
                mmu->hasRTC = true;
            }
            if (mbcType == 0x10 || mbcType == 0x12 || mbcType == 0x13) {
                mmu->hasRAM = true;
            }
            
            if (mbcType == 0xF || mbcType == 0x10 || mbcType == 0x13) {
                mmu->hasBattery = true;
            }
        } break;
        case 0x19 ... 0x1E: {
            mmu->mbcType = MBCType::MBC5;
            if (mbcType == 0x1A || mbcType == 0x1B || mbcType == 0x1D || mbcType == 0x1E) {
                mmu->hasRAM = true;
            }
            
            if (mbcType == 0x1B || mbcType == 0x1E) {
                mmu->hasBattery = true;
            }
            
            //TODO: rumble
            
            
        } break;
        default: {
            ALERT("This game is not yet supported.");   
            //            ALERT("Cannot load game.\nReason: Unsupported MBC: 0x%X.\nHopefully it will be supported in the future.", mbcType);
            return;
        } break;
        }
        
        //RAM size
        if (mmu->hasRAM) {
            switch (mmu->romData[0x149]) {
            case 0: mmu->cartRAMSize = 0; break;
            case 1: mmu->cartRAMSize = KB(2); break;
            case 2: mmu->cartRAMSize = KB(8); break;
            case 3: mmu->cartRAMSize = KB(32); break;
            case 4: mmu->cartRAMSize = KB(128); break;
            case 5: mmu->cartRAMSize = KB(64); break;
            }
            mmu->maxCartRAMBank = calculateMaxBank(mmu->cartRAMSize);
        }

        if (mmu->hasRAM) {
            //TODO: be smarter than this for creating cart ram for carts smaller than a bank
            mmu->cartRAM = PUSHM(mmu->cartRAMSize < KB(8) ? KB(8) : mmu->cartRAMSize, u8);
            mmu->hasRAM = true;
        }
        
        mmu->romNameLen = (mmu->romData[0x143] == 0x80 || mmu->romData[0x143] == 0xC0) ? 15 : 16;
        
        copyMemory(mmu->romData + 0x134, romTitle, mmu->romNameLen);
        mmu->romName = (char*)mmu->romData + 0x134;
        foriarr (romTitle) {
            if ((u8)romTitle[i] > 0x7E) {
                romTitle[i] = '\0';
            }
        }
        
        if (!doesDirectoryExistAndIsWritable(programState->homeDirectoryPath)) {
            ALERT_EXIT("Cannot write to GBEmu home directory: %s. Permission Denied.", programState->homeDirectoryPath);
            return;
        }
        
        snprintf(programState->loadedROMName, ARRAY_LEN(programState->loadedROMName), "%s", romTitle);
        snprintf(programState->romSpecificPath, MAX_PATH_LEN, "%s" FILE_SEPARATOR "%s", programState->homeDirectoryPath, programState->loadedROMName);
        //home dir
        if (!doesDirectoryExistAndIsWritable(programState->romSpecificPath)) {
            auto res = createDir(programState->romSpecificPath);
            switch (res) {
            case FileSystemResultCode::OK: {
                if (!doesDirectoryExistAndIsWritable(programState->romSpecificPath)) { 
                    ALERT_EXIT("Cannot create save files at (%s).", programState->romSpecificPath);
                    return;
                }

            } break; 
            case FileSystemResultCode::PermissionDenied: {
                ALERT_EXIT("Cannot create save files at (%s). Permission denied.", programState->romSpecificPath);
                return;
            } break;
            case FileSystemResultCode::AlreadyExists: {
                ALERT_EXIT("A file already exists at %s. Please move or delete this file so GBEmu can use this path for save files.", programState->romSpecificPath);
                return;
            } break;
            default:  {
                ALERT_EXIT("Cannot create save files at (%s).", programState->romSpecificPath);
                return;
            } break;
            }
        }
        
        auto res = changeDir(programState->romSpecificPath);
        switch (res) {
        case FileSystemResultCode::OK: {
        } break;
            
        case FileSystemResultCode::PermissionDenied:  {
            ALERT_EXIT("Cannot use required directory: %s. Permission Denied.", programState->romSpecificPath);
            return;
        } break;
        default: {
            ALERT_EXIT("Cannot use required directory: %s.", programState->romSpecificPath);
            return;
        } break;
            
        }
        /******************************************
         * Everything after here is relative to the rom directory!!
         *******************************************/
        if (mmu->hasBattery) {
            snprintf(filePath, ARRAY_LEN(filePath), "%s." CART_RAM_FILE_EXTENSION, programState->loadedROMName);
            
            auto crps = &mmu->cartRAMPlatformState;

            crps->cartRAMFileHandle = mapFileToMemory(filePath, &crps->cartRAMFileMap, &crps->ramLen);
            
            i64 expectedSize = mmu->cartRAMSize;
            if (mmu->hasRTC) expectedSize += RTC_FILE_LEN;
            
            if (crps->cartRAMFileHandle && crps->ramLen == (usize)expectedSize) {
                copyMemory(crps->cartRAMFileMap, mmu->cartRAM, mmu->cartRAMSize);
                
                if (mmu->hasRTC) {
                    /*
                      BGB format: 
                       0       4       time seconds
                       4       4       time minutes
                       8       4       time hours
                       12      4       time days
                       16      4       time days high
                       20      4       latched time seconds
                       24      4       latched time minutes
                       28      4       latched time hours
                       32      4       latched time days
                       36      4       latched time days high
                       40      8       unix timestamp when saving (64 bits little endian)
                       */
                    
                    RTCFileState *rtcFS = (RTCFileState*)(crps->cartRAMFileMap + mmu->cartRAMSize);
                    
                    zeroMemory(&mmu->rtc, sizeof(RTC));
                    mmu->rtc.seconds = (u8)rtcFS->seconds;
                    mmu->rtc.minutes = (u8)rtcFS->minutes;
                    mmu->rtc.hours = (u8)rtcFS->hours;
                    mmu->rtc.days = (u8)rtcFS->days;
                    mmu->rtc.daysHigh = (u8)rtcFS->daysHigh;
                    mmu->rtc.wallClockTime = rtcFS->unixTimestamp;
                    
                    mmu->rtc.latchedSeconds = (u8)rtcFS->latchedSeconds;
                    mmu->rtc.latchedMinutes = (u8)rtcFS->latchedMinutes;
                    mmu->rtc.latchedHours = (u8)rtcFS->latchedHours;
                    mmu->rtc.latchedDays = (u8)rtcFS->latchedDays;
                    mmu->rtc.latchedMisc = (u8)rtcFS->latchedDaysHigh;
                    
                    syncRTCTime(&mmu->rtc, rtcFS);
                    crps->rtcFileMap = rtcFS;
                }
                
            }
            else {
                if (crps->cartRAMFileHandle) {
                    closeMemoryMappedFile(crps->cartRAMFileHandle);
                    if (!backupCartRAMFile(programState->loadedROMName, CART_RAM_FILE_EXTENSION, programState->homeDirectoryPath, fileMemory)) {
                        return;
                    }
                }
                if (!createBatteryBackedRAMFile(mmu, filePath)) {
                    return;
                }
                
                crps->cartRAMFileHandle = mapFileToMemory(filePath, &crps->cartRAMFileMap, &crps->ramLen);
                if (!crps->cartRAMFileHandle) {
                    ALERT("Could not open cart RAM file: %s!", filePath); 
                    return;
                }
                if (crps->ramLen != (usize)expectedSize) {
                    ALERT("Error while loading cart RAM file: %s!", filePath); 
                    return;
                }
                mmu->cartRAMPlatformState.rtcFileMap = (RTCFileState*)(crps->cartRAMFileMap + mmu->cartRAMSize);
            }
            
        }



        SDL_SetWindowTitle(window, romTitle);

    }

    Input *input = &programState->input;

    
#ifdef CO_DEBUG
    programState->gbEmuCode = loadGBEmuCode(gbemuCodePath);
    auto gbEmuCode = &programState->gbEmuCode;
#endif
    
    //Notification init
    NotificationState *notifications = &programState->notifications;
    char currentNotification[MAX_NOTIFICATION_LEN + 1] = {};
    TimeUS timeNotificationIsShown = 0;
    TimeUS startNotificationTime = nowInMicroseconds();
    
    //gamepad init
    SDL_JoystickID gamepadID = 0;
    {
        fori (SDL_NumJoysticks()) {
            if (SDL_IsGameController((int)i)) {
                *gamepad = SDL_GameControllerOpen((int)i);
                if (*gamepad) {
                    gamepadID = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*gamepad));
                    CO_LOG("%s attached", SDL_GameControllerNameForIndex((int)i));
                    NOTIFY(notifications, "%s attached", SDL_GameControllerNameForIndex((int)i));
                }
                else {
                    ALERT("Could not use gamepad!");
                }
                break;
            }
        }

    }
    
    //Sound init
    SoundState *platformSoundState = &programState->soundState;
    platformSoundState->volume = 50;

    {
#define GB_SAMPLES_PER_SEC 4194304/60
        auto *soundFramesBuffer = &mmu->soundFramesBuffer;
        soundFramesBuffer->len = GB_SAMPLES_PER_SEC; //1 second
        soundFramesBuffer->data = PUSHM(soundFramesBuffer->len, SoundFrame);
#undef GB_SAMPLES_PER_SEC 

    }


    gbDebug->nextFreeGBStateIndex = 0;
    
    foriarr (gbDebug->recordedGBStates) {
        auto state = &gbDebug->recordedGBStates[i]; 
        
        if (mmu->cartRAMSize > 0) {
            state->mmu.cartRAMSize = mmu->cartRAMSize;
            state->mmu.cartRAM = PUSHM(state->mmu.cartRAMSize,u8);
        }
        
        auto *srcSoundBuffer = &mmu->soundFramesBuffer;
        auto *destSoundBuffer = &state->mmu.soundFramesBuffer;
        destSoundBuffer->len = srcSoundBuffer->len; 
        destSoundBuffer->data = PUSHM(destSoundBuffer->len, SoundFrame);
    }
    CALL_DYN(reset,cpu, mmu, gbDebug, programState);

    TimeUS startTime = nowInMicroseconds(), dt = 0;
    TimeUS startMeasureTime = startTime;

    /*******************
     * Start main loop
     * *****************/
    while (isRunning) {
        /********************
         * load game code
         ********************/
#ifdef CO_DEBUG
        {
            struct stat fileStats;
            stat(gbemuCodePath, &fileStats);
            time_t gbEmuFileTimeModified = fileStats.st_mtime;
            
            //if the .so is new and the .so isn't locked, reload the code
            if (gbEmuFileTimeModified > gbEmuCode->gbEmuTimeLastModified &&
                    access("./soLock", F_OK) != 0) {
                
                SDL_UnloadObject(gbEmuCode->gbEmuCodeHandle);
                programState->gbEmuCode = loadGBEmuCode(gbemuCodePath);
                gbEmuCode = &programState->gbEmuCode;
            }
        }
#endif

        /******************
         * SDL Events     *
         *****************/
        {
            SDL_Event e;

            while (SDL_PollEvent(&e)) {
                auto keyMod = SDL_GetModState();
                
                switch (e.type) {
                case SDL_WINDOWEVENT: {
                    
                    switch (e.window.event) {
                    case SDL_WINDOWEVENT_CLOSE: {
                        if (e.window.windowID == SDL_GetWindowID(window)) {
                            cleanUp(&mmu->cartRAMPlatformState, debuggerContext);
                            return;
                        }
                        else if (e.window.windowID == SDL_GetWindowID(*debuggerWindow)) {
                            closeDebugger(debuggerContext);
                        }
                    } break;
                    case SDL_WINDOWEVENT_RESIZED: { 
                        if (e.window.windowID == SDL_GetWindowID(*debuggerWindow)) {
                            auto &io = ImGui::GetIO(); 
                            io.DisplaySize.x = e.window.data1;
                            io.DisplaySize.y = e.window.data2;
                        }
                        else if (e.window.windowID == SDL_GetWindowID(window)) {
                            
                            windowResized(e.window.data1, e.window.data2, platformState);
                        }

                    } break;
                    }

                } break;
                case SDL_QUIT:
                    cleanUp(&mmu->cartRAMPlatformState, debuggerContext);
                    return;

                case SDL_KEYDOWN: {
                    processKeyDown(e.key.keysym.sym, (keyMod & CTRL_KEY) != 0, &input->newState, 
                               &input->keysMap, &input->ctrlKeysMap);
                } break;

                case SDL_KEYUP: {
                    processKeyUp(e.key.keysym.sym, &input->newState, &input->keysMap, &input->ctrlKeysMap);
                } break;

                case SDL_CONTROLLERBUTTONUP:  
                case SDL_CONTROLLERBUTTONDOWN:  {
                    if (*gamepad && e.cbutton.which == gamepadID) {
                        processButton((SDL_GameControllerButton)e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN, &input->newState,
                                      &input->gamepadMap);
                    }
                    
                } break;
                case SDL_CONTROLLERAXISMOTION: {
                    if (*gamepad && e.caxis.which == gamepadID) {
                        switch (e.caxis.axis) {
                        case SDL_CONTROLLER_AXIS_LEFTX: {
                            input->newState.xAxis = e.caxis.value;
                            if (e.caxis.value < ANALOG_STICK_DEADZONE && e.caxis.value > -ANALOG_STICK_DEADZONE) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, false, &input->newState, &input->gamepadMap);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false, &input->newState, &input->gamepadMap);
                            }
                            else if (e.caxis.value >= ANALOG_STICK_DEADZONE && 
                                     e.caxis.value > abs(input->newState.yAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, false, &input->newState, &input->gamepadMap);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, true, &input->newState, &input->gamepadMap);
                            }
                            else if (e.caxis.value <= -ANALOG_STICK_DEADZONE && 
                                     abs(e.caxis.value) > abs(input->newState.yAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, true, &input->newState, &input->gamepadMap);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false, &input->newState, &input->gamepadMap);
                            }
                        } break;
                        case SDL_CONTROLLER_AXIS_LEFTY: {
                            input->newState.yAxis = e.caxis.value;
                            if (e.caxis.value < ANALOG_STICK_DEADZONE && e.caxis.value > -ANALOG_STICK_DEADZONE) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_UP, false, &input->newState, &input->gamepadMap);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false, &input->newState, &input->gamepadMap);
                            }
                            else if (e.caxis.value >= ANALOG_STICK_DEADZONE && 
                                     e.caxis.value > abs(input->newState.xAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_UP, false, &input->newState, &input->gamepadMap);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, true, &input->newState, &input->gamepadMap);
                            }
                            else if (e.caxis.value <= -ANALOG_STICK_DEADZONE && 
                                     abs(e.caxis.value) > abs(input->newState.xAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_UP, true, &input->newState, &input->gamepadMap);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false, &input->newState, &input->gamepadMap);
                            }
                        } break;
                        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: {
                            if (e.caxis.value > ANALOG_TRIGGER_DEADZONE) {
                                processButton(gamepadMappingToSDLButton[(int)GamepadMappingValue::LeftTrigger], true, &input->newState, &input->gamepadMap);
                            }
                            else if (e.caxis.value <= ANALOG_TRIGGER_DEADZONE) {
                                processButton(gamepadMappingToSDLButton[(int)GamepadMappingValue::LeftTrigger], false, &input->newState, &input->gamepadMap);
                            }
                        } break;
                        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: {
                            if (e.caxis.value > ANALOG_TRIGGER_DEADZONE) {
                                processButton(gamepadMappingToSDLButton[(int)GamepadMappingValue::RightTrigger], true, &input->newState, &input->gamepadMap);
                            }
                            else if (e.caxis.value <= ANALOG_TRIGGER_DEADZONE) {
                                processButton(gamepadMappingToSDLButton[(int)GamepadMappingValue::RightTrigger], false, &input->newState, &input->gamepadMap);
                            }
                        } break;
                        }
                    }
                    
                } break;

                case SDL_CONTROLLERDEVICEADDED: {
                    if (*gamepad) {
                        break;
                    }
                    int i = e.cdevice.which;
                    *gamepad = SDL_GameControllerOpen(i);
                    gamepadID = i;
                    if (gamepad) {
                        NOTIFY(notifications, "%s attached", SDL_GameControllerNameForIndex(i));
                        CO_LOG("%s attached at index %d", SDL_GameControllerNameForIndex(i), i);
                    }
                    else {
                        CO_ERR("Could not open gamepad");
                    }
                    break;
                } break;

                case SDL_CONTROLLERDEVICEREMOVED: {
                    auto gamepadToClose = SDL_GameControllerFromInstanceID(e.cdevice.which);

                    if (*gamepad == gamepadToClose) {
                        NOTIFY(notifications, "Controller detached");
                        CO_LOG("Controller detached");
                        SDL_GameControllerClose(*gamepad);
                        *gamepad = nullptr;
                    }

                } break;

                }

                if (gbDebug->isEnabled) {
                    switch (e.type) {
                    case SDL_KEYUP:
                    case SDL_KEYDOWN: {
                        gbDebug->key = e.key.keysym.sym & ~SDLK_SCANCODE_MASK;
                        gbDebug->isKeyDown = (e.type == SDL_KEYDOWN);
                        gbDebug->isShiftDown = ((SDL_GetModState() & KMOD_SHIFT) != 0);
                        gbDebug->isCtrlDown = ((SDL_GetModState() & KMOD_CTRL) != 0);
                        gbDebug->isAltDown = ((SDL_GetModState() & KMOD_ALT) != 0);
                        gbDebug->isSuperDown = ((SDL_GetModState() & KMOD_GUI) != 0);
                    } break;
                    case SDL_MOUSEWHEEL: {
                        if (e.wheel.y > 0) {
                            gbDebug->mouseScrollY = 1;
                        }
                        else if (e.wheel.y < 0) {
                            gbDebug->mouseScrollY = -1;
                        }

                    } break;
                    case SDL_MOUSEBUTTONDOWN: {
                        switch (e.button.button) {
                        case SDL_BUTTON_LEFT: gbDebug->mouseDownState[0] = true; break;
                        case SDL_BUTTON_RIGHT: gbDebug->mouseDownState[1] = true; break;
                        case SDL_BUTTON_MIDDLE: gbDebug->mouseDownState[2] = true; break;
                        }

                    } break;
                    case SDL_MOUSEBUTTONUP: {
                        switch (e.button.button) {
                        case SDL_BUTTON_LEFT: gbDebug->mouseDownState[0] = false; break;
                        case SDL_BUTTON_RIGHT: gbDebug->mouseDownState[1] = false; break;
                        case SDL_BUTTON_MIDDLE: gbDebug->mouseDownState[2] = false; break;
                        }

                    } break;
                    case SDL_TEXTINPUT: {
                        strcpy(gbDebug->nextTextPos, e.text.text);
                        gbDebug->nextTextPos += stringLength(e.text.text);
                    } break;

                    }
                }
            }

            if (isActionPressed(Input::Action::ShowDebugger, input) && !gbDebug->isEnabled && !programState->isFullScreen) {
                int windowX, windowY;
                SDL_GetWindowPosition(window, &windowX, &windowY);
                if ((debuggerContext = initDebugger(gbDebug, programState, debuggerWindow, windowX, windowY))) {
                    gbDebug->isEnabled = true;
                }
            }
            if (isActionPressed(Input::Action::ShowControls, input)) {
                showInputMapDialog(window, &input->keysMap, &input->ctrlKeysMap, &input->gamepadMap);
            }
            if (isActionPressed(Input::Action::FullScreen, input)) {
                if (setFullScreen(window, !programState->isFullScreen)) {
                    SDL_ShowCursor(programState->isFullScreen);
                    programState->isFullScreen = !programState->isFullScreen;
                }
            }
            if (input->newState.escapeFullScreen && !input->oldState.escapeFullScreen && programState->isFullScreen) {
                if (setFullScreen(window, false)) {
                    programState->isFullScreen = false;
                    SDL_ShowCursor(true);
                }
            }
        }
        if (gbDebug->isEnabled) {
            //TODO: Why do we have this?
//            if (!debuggerContext->window) {
//                int windowX, windowY;
//                SDL_GetWindowPosition(window, &windowX, &windowY);
//                initDebugger(debuggerContext, gbDebug, programState, windowX, windowY);
//            }

            SDL_GetMouseState(&gbDebug->mouseX, &gbDebug->mouseY);

            // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.

            gbDebug->isWindowInFocus =
                    SDL_GetWindowFlags(*debuggerWindow) & SDL_WINDOW_MOUSE_FOCUS;


            input->newState.mouseX = gbDebug->mouseX;
            input->newState.mouseY = gbDebug->mouseY;


        }

        profileStart("run frame", profileState);
        {
            TimeUS now = nowInMicroseconds();
            dt = now - startTime;
            startTime = now;
        }
        
        {
            int numFrames = (int)(dt/FRAME_TIME_US);
            if (numFrames > 1) {
                gbDebug->numFramesSkipped += numFrames - 1;
            }
            if(!isPaused) {
                CALL_DYN(runFrame, cpu, mmu, gbDebug, programState, dt);
            }
        }
        profileEnd(profileState);
        
        if (gbDebug->isEnabled) {
            auto &io = ImGui::GetIO();
            io.DeltaTime = (float)(nowInMicroseconds() - startTime / 1000000.);
            io.KeysDown[gbDebug->key] = gbDebug->isKeyDown;
            io.KeyShift = gbDebug->isShiftDown;
            io.KeyCtrl = gbDebug->isCtrlDown;
            io.KeyAlt = gbDebug->isAltDown;
            io.KeySuper = gbDebug->isSuperDown;
            fori (3) {
                io.MouseDown[i] = gbDebug->mouseDownState[i]; 
            }

            // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.

            io.MouseWheel = gbDebug->mouseScrollY;
            gbDebug->mouseScrollY = 0;

            io.MousePos.x = gbDebug->mouseX;
            io.MousePos.y = gbDebug->mouseY;

            if (gbDebug->inputText[0]) {
                io.AddInputCharactersUTF8(gbDebug->inputText);
                gbDebug->nextTextPos = gbDebug->inputText;
                *gbDebug->nextTextPos = '\0';
            }
            newDebuggerFrame(debuggerContext);
            profileStart("Draw Debug window", profileState);
            CALL_DYN(drawDebugger, gbDebug, mmu, cpu, programState, dt);
            profileEnd(profileState);
            signalRenderDebugger(debuggerContext);
        }

        /***************
         * Play Audio
         **************/
        if (mmu->soundFramesBuffer.numItemsQueued > 0) {
#define MAX_FRAMES_TO_PLAY 1000
#define VOLUME_AMPLIFIER 4
            SoundFrame *framesToPlay = PUSHM(mmu->soundFramesBuffer.numItemsQueued, SoundFrame);
            AutoMemory _am(framesToPlay);

            u32 numFramesToQueue = (u32)popn(mmu->soundFramesBuffer.numItemsQueued, &mmu->soundFramesBuffer, framesToPlay);

            if (!platformSoundState->isMuted) {
                if (SDL_GetAudioDeviceStatus(audioDeviceID) != SDL_AUDIO_PLAYING)  {
                    SDL_PauseAudio(0);
                }
                i64 startIndex = (i64)numFramesToQueue - MAX_FRAMES_TO_PLAY;
                if (startIndex < 0) startIndex = 0;
                if (startIndex > 0) SDL_ClearQueuedAudio(audioDeviceID);
                SDL_QueueAudio(audioDeviceID, framesToPlay + startIndex, numFramesToQueue * (u32)sizeof(SoundFrame));
            }
            else if (SDL_GetAudioDeviceStatus(audioDeviceID) == SDL_AUDIO_PLAYING)  {
                SDL_ClearQueuedAudio(audioDeviceID);
                SDL_PauseAudio(1);
            }
#undef MAX_FRAMES_TO_PLAY
#undef VOLUME_AMPLIFIER
        }
        

        /****************
         * Draw GB screen
         ****************/
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);

            LCD *lcd = &mmu->lcd;

            profileStart("Draw Screen", profileState);
            if (lcd->isEnabled) {
                int w,h;
                SDL_GetWindowSize(window, &w, &h);
                renderMainScreen(renderer, platformState, lcd->screen, w, h);
                
            }
            profileEnd(profileState);
            profileStart("Flip to screen", profileState);
            SDL_RenderPresent(renderer);
            profileEnd(profileState);
        }

        //draw notifications and new title
        {
            TimeUS now = nowInMicroseconds();
            bool isNewNotification = popNotification(notifications, currentNotification);

            if (isNewNotification) {
                programState->shouldUpdateTitleBar = true;
                timeNotificationIsShown = 0;
                startNotificationTime = now;
            }
            if (timeNotificationIsShown >= MAX_NOTIFICATION_TIME_US) {
                programState->shouldUpdateTitleBar = true;
                currentNotification[0] = '\0';
                timeNotificationIsShown = 0;
            }
            if (!isEmptyString(currentNotification)) {
                timeNotificationIsShown += now - startNotificationTime;
                startNotificationTime = now;
            }

            profileStart("Set Window", profileState);
            if (gbDebug->isEnabled) {
                gbDebug->frameTimeMS = (now - startMeasureTime) / 1000.;
                startMeasureTime = now;
            }
            if (programState->shouldUpdateTitleBar) {
                const char *pausedOrMutedStatus = (cpu->isPaused) ? " -- Paused" : 
                                                                    (platformSoundState->isMuted) ? " -- Muted"
                                                                                                  : "";
                if (!isEmptyString(currentNotification)) {
                    snprintf(windowTitle, ARRAY_LEN(windowTitle), "%s %s -- %s", romTitle, pausedOrMutedStatus,  currentNotification);
                }
                else {
                    snprintf(windowTitle, ARRAY_LEN(windowTitle), "%s %s", romTitle, pausedOrMutedStatus);
                }
                SDL_SetWindowTitle(window, windowTitle);
                programState->shouldUpdateTitleBar = false;
            }
            profileEnd(profileState);
        }
 
        input->oldState = input->newState;
    }
}

int main(int argc, char **argv) {
    bool shouldEnableDebugMode = false;
    char romFileName[MAX_PATH_LEN] = {};
    if (argc > 1 && argc <= 4) {
        forirange (1, argc) {
            if (argv[i][0] == '-') {
                if (strcmp(DEBUG_ON_BOOT_FLAG, argv[i]) == 0) {
                    shouldEnableDebugMode = true;
                }
                else {
                    PRINT_ERR("Unknown option %s", argv[i]);
                }
            }
            else {
                strncpy(romFileName, argv[i], MAX_PATH_LEN);
            }
        }
    }
    else if (argc != 1) {
       PRINT("GBEmu -- Version %s", GBEMU_VERSION);
       PRINT("Usage :gbemu " DEBUG_ON_BOOT_FLAG " path_to_ROM");
       PRINT("\t" DEBUG_ON_BOOT_FLAG " -- Start with the debugger screen up and emulator paused");
       return 1; 
    }
    PRINT("GBEmu -- Version %s", GBEMU_VERSION);
    
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_AudioSpec as;
    SDL_AudioDeviceID audioDeviceID = 0;
    SDL_GameController *gamepad = nullptr;
	ProgramState *programState;
    PlatformState *platformState = nullptr; //only for mac
    char *romsDir = nullptr;
    const char *openDialogPath = nullptr;
    const char *gbEmuHomePath; 
    char homePathConfigFilePath[MAX_PATH_LEN] = {};
    bool isHomePathValid = false;
    char *configFilePath = nullptr;
    {
        initPlatformFunctions(alertDialogSDL);
        if (!initMemory(GENERAL_MEMORY_SIZE + FILE_MEMORY_SIZE /*+ MB(10)*/, GENERAL_MEMORY_SIZE)) { //reserve 10 MB
            //Do not use ALERT since it uses general memory which failed to init
            alertDialog("Not enough memory to run the emulator!");
            goto exit;
        }
        programState = PUSHMCLR(1, ProgramState);
        //TODO: default mappings
//        foriarr (programState->input.inputMappingConfig.inputMappings) {
//            InputMapping *im = programState->input.inputMappingConfig.inputMappings + i;
//            *im = {NO_INPUT_MAPPING, NO_INPUT_MAPPING, NO_INPUT_MAPPING};
//        }
        makeMemoryStack(FILE_MEMORY_SIZE, "fileMem", &programState->fileMemory);

    }

    profileInit(&programState->profileState);
#ifdef CO_PROFILE
    profileState = &programState->profileState;
#endif

    gbEmuHomePath = getenv(HOME_PATH_ENV_VARIABLE);
    if (gbEmuHomePath) {
        copyString(gbEmuHomePath, programState->homeDirectoryPath, MAX_PATH_LEN);
        if (doesDirectoryExistAndIsWritable(programState->homeDirectoryPath)) {
           isHomePathValid = true;
        }
        else {
            ALERT_EXIT("The specified GBEmu home directory, %s, either does not exist or is not writable.\n"
                  "Please correct or unset the %s environment variable and rerun GBEmu.", programState->homeDirectoryPath, HOME_PATH_ENV_VARIABLE);
            return 1;
        }
    }
    if (!isHomePathValid) {
        if (!getFilePathInHomeDir(HOME_PATH_CONFIG_FILE, homePathConfigFilePath)) {
            ALERT_EXIT("Could not get user home directory!");        
            return 1;
        }
        auto readResult = readEntireFile(homePathConfigFilePath, &programState->fileMemory);
        switch (readResult.resultCode) {
        case FileSystemResultCode::OK: {
            if (readResult.size <= MAX_PATH_LEN) {
                copyMemory(readResult.data, programState->homeDirectoryPath, readResult.size);
                programState->homeDirectoryPath[readResult.size] = '\0';
                if (doesDirectoryExistAndIsWritable(programState->homeDirectoryPath)) {
                    isHomePathValid = true;
                }
                else {
                    ALERT_EXIT("The specified GBEmu home directory, %s, either does not exist or is not writable.\n"
                          "Please correct the contents or delete the %s file and rerun GBEmu.", 
                          programState->homeDirectoryPath, homePathConfigFilePath);
                    return 1;
                }
            }
            
        } break;
        default: {
            //do nothing and continue
            //TODO: handle not found and other errors differently?
        } break;
        }
        freeFileBuffer(&readResult, &programState->fileMemory);
    }
    if (!isHomePathValid) {
		//NOTE: interestingly, SDL_Init must be called after opening an open dialog.  else new folder dialog doesn't work in macOS
        if (!getFilePathInHomeDir(DEFAULT_GBEMU_HOME_PATH, programState->homeDirectoryPath)) {
            ALERT_EXIT("Could not get user home directory!");        
            return 1;
        }
        auto result = showHomeDirectoryDialog(programState->homeDirectoryPath, homePathConfigFilePath);
        switch (result) {
        case HomeDirectoryOption::EXIT: {
            return 1;
        } break;
        case HomeDirectoryOption::DEFAULT: {
            if (!doesDirectoryExistAndIsWritable(programState->homeDirectoryPath)) {
                auto res = createDir(programState->homeDirectoryPath);
                switch (res) {
                case FileSystemResultCode::OK: {
                    if (!doesDirectoryExistAndIsWritable(programState->homeDirectoryPath)) { 
                        ALERT_EXIT("Cannot create GBEmu home directory (%s).", programState->homeDirectoryPath);
                        return 1;
                    }

                } break; 
                default:  {
                    ALERT_EXIT("Cannot create GBEmu home directory (%s). Reason: %s.", programState->homeDirectoryPath, resultCodeToString[(int)res]);
                    return 1;
                } break;
                }
            }
        } break;
        case HomeDirectoryOption::CUSTOM: {
            char home[MAX_PATH_LEN];
            *home = '\0';
            if (!getFilePathInHomeDir("", home)) {
                ALERT_EXIT("Could not get user home directory!");        
                return 1;
            }
            for (;;) {
                if (!openDirectoryAtPath(home, programState->homeDirectoryPath)) {
                    return 1;
                }

                if (doesDirectoryExistAndIsWritable(programState->homeDirectoryPath)) {
                    break;
                }
                else {
                    ALERT("Selected directory (%s) is not writable.  Please choose a writable directory for the GBEmu home directory.", programState->homeDirectoryPath);
                }
            }
        } break;
        }
        //TODO: save down new path to text file
        auto res = writeDataToFile(programState->homeDirectoryPath, stringLength(programState->homeDirectoryPath, MAX_PATH_LEN), homePathConfigFilePath);
        switch (res) {
        case FileSystemResultCode::OK: {
           //do nothing 
        } break;
        case FileSystemResultCode::PermissionDenied: {
           ALERT_EXIT("Could not persist the GBEmu home directory path to %s. Permission Denied. Please make sure your user home directory is writable.",
                 homePathConfigFilePath);
           return 1;
        } break;
        default: {
           ALERT_EXIT("Could not persist the GBEmu home directory path to %s.",
                 homePathConfigFilePath);
           return 1;
        }
        }
    }
    profileStart("init sdl", &programState->profileState);
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        ALERT("Error initing the emulator!  Reason: %s", SDL_GetError());
        goto exit;
    }
    profileEnd(&programState->profileState);
    /**********************
     * Parse Config file
     *********************/
    profileStart("Parse Config file", profileState);
    configFilePath = nullptr;
    buf_gen_memory_printf(configFilePath, "%s" FILE_SEPARATOR GBEMU_CONFIG_FILENAME, programState->homeDirectoryPath);
    if (!doConfigFileParsing(configFilePath, programState)) {
        return 1;
    }
    buf_gen_memory_free(configFilePath);
    profileEnd(profileState);
    
    //ROMs dir
    buf_gen_memory_printf(romsDir, "%s" FILE_SEPARATOR "ROMs" FILE_SEPARATOR,programState->homeDirectoryPath);
    if (!doesDirectoryExistAndIsWritable(romsDir)) {
        auto res = createDir(romsDir);
        switch (res) {
        case FileSystemResultCode::OK: {
           openDialogPath = romsDir; 
        } break; 
        default:  {
            openDialogPath = programState->homeDirectoryPath;
        } break;
        }
    }
    else {
        openDialogPath = romsDir; 
    }
	if (isEmptyString(romFileName)){
		//NOTE: interestingly, SDL_Init must be called before opening an open dialog.  else an app instance won't be created in macOS
        if (!openFileDialogAtPath(openDialogPath, romFileName)) {
            return 1;
        }
	}
    buf_gen_memory_free(romsDir);

    window = SDL_CreateWindow("GBEmu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             SCREEN_WIDTH*programState->screenScale, SCREEN_HEIGHT*programState->screenScale, 
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        ALERT("Could not create window. Reason %s", SDL_GetError());
        goto exit;
    }
#ifdef MAC
    if (!SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal")) {
        ALERT_EXIT("Could not init renderer. Reason: could not use Metal renderer.");
        goto exit;
    }
#endif
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        ALERT_EXIT("Could not init renderer. Reason %s", SDL_GetError());
        goto exit;
    }
    
    //Check to see if screen scale is too big
    {
        int windowW, windowH, windowX, windowY, displayIndex; 
        SDL_DisplayMode mode;
        if ((displayIndex = SDL_GetWindowDisplayIndex(window)) < 0) {
            ALERT_EXIT("Could not get display index. Reason %s", SDL_GetError());
            goto exit;
        }
        
        if (SDL_GetCurrentDisplayMode(displayIndex, &mode)) {
            ALERT_EXIT("Could not get display mode. Reason %s", SDL_GetError());
            goto exit;
        }
        int displayW = mode.w, displayH = mode.h; 
        
        SDL_GetWindowPosition(window, &windowX, &windowY);       
        SDL_GetWindowSize(window, &windowW, &windowH);
        
        if (windowW > displayW || windowH > displayH) {
            //TODO: handle with metal
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            int originalScreenScale = programState->screenScale;
            do {
                programState->screenScale--;
            } while (SCREEN_WIDTH*programState->screenScale > displayW || SCREEN_HEIGHT*programState->screenScale > displayH);
            
            ALERT("The GBEmu window is too big for your display.  We have shrank the window size from %d times to %d times the scale of the GameBoy screen.",
                  originalScreenScale, programState->screenScale);

            window = SDL_CreateWindow("GBEmu", windowX, windowY,
                                      SCREEN_WIDTH*programState->screenScale, SCREEN_HEIGHT*programState->screenScale, SDL_WINDOW_SHOWN);

            if (!window) {
                ALERT("Could not create window. Reason %s", SDL_GetError());
                goto exit;
            }

            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );

            if (!renderer) {
                ALERT("Could not init renderer. Reason %s", SDL_GetError());
                goto exit;
            }
        }
    }

    {
        int w,h;
        SDL_GetWindowSize(window, &w, &h);
        platformState = initPlatformState(renderer, w, h);

        if (!platformState) {
            goto exit;
        }
    }
    zeroMemory(&as, sizeof(as));
    as.freq = AUDIO_SAMPLE_RATE;
    as.format = AUDIO_S16LSB;
    as.channels = 2;
    as.samples = 4096;

    audioDeviceID =
            SDL_OpenAudioDevice(nullptr, false, &as, &as, 0);


    if (!audioDeviceID) {
        ALERT("Could not open sound device. Reason: %s", SDL_GetError());
        goto exit;
    }



    {
        SDL_Window *debuggerWindow = nullptr;
        DebuggerPlatformContext *debuggerContext = nullptr;
        auto gbDebug = PUSHMCLR(1, GameBoyDebug);
        AutoMemory _am1(debuggerContext), _am2(gbDebug);
        if (shouldEnableDebugMode) {
            int windowX, windowY;
            SDL_GetWindowPosition(window, &windowX, &windowY);
            if (!(debuggerContext = initDebugger(gbDebug, programState, &debuggerWindow, windowX, windowY))) {
                ALERT("Could not init debugger!");
                goto exit;
            }
        }

        mainLoop(window, renderer, platformState, audioDeviceID, &gamepad, romFileName, shouldEnableDebugMode, debuggerContext, &debuggerWindow, gbDebug, programState);
    }


exit:

    return 0;
}


//Platform specific
#if defined(WINDOWS) || defined(LINUX)
void renderMainScreen(SDL_Renderer *renderer, PlatformState *platformState, const PaletteColor *screen,
    int w, int h) {
    UNUSED(w);
    UNUSED(h);
    LinuxAndWindowsPlatformState *ps = (LinuxAndWindowsPlatformState*)platformState;
    SDL_Texture *screenTexture = ps->screenTexture;
    u32 *screenPixels;
    int pitch;
    SDL_LockTexture(screenTexture, nullptr, (void**)&screenPixels, &pitch);
    fori (SCREEN_HEIGHT*SCREEN_WIDTH) {
        switch (screen[i]) {
        case PaletteColor::White: screenPixels[i] = 0xFFFFFFFF; break;
        case PaletteColor::LightGray: screenPixels[i] = 0xFFAAAAAA; break;
        case PaletteColor::DarkGray: screenPixels[i] = 0xFF555555; break;
        case PaletteColor::Black: screenPixels[i] = 0xFF000000; break;
        }
    }
    SDL_UpdateTexture(screenTexture, nullptr, screenPixels, pitch);
    SDL_UnlockTexture(screenTexture);
    SDL_RenderCopy(renderer, screenTexture, nullptr, &ps->textureRect);
}

PlatformState *initPlatformState(SDL_Renderer *renderer, int windowW, int windowH) {
    LinuxAndWindowsPlatformState *ret = CO_MALLOC(1, LinuxAndWindowsPlatformState);
    ret->screenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
                                      SCREEN_WIDTH, SCREEN_HEIGHT);
	ret->textureRect = {0, 0, windowW, windowH};
    
    if (!ret->screenTexture) {
        ALERT("Could not create screen. Reason %s", SDL_GetError());
        return nullptr;
    }
    
    return ret;
}
void windowResized(int w, int h, PlatformState *platformState) {
    LinuxAndWindowsPlatformState *ps = (LinuxAndWindowsPlatformState*)platformState;
    int ratio = MIN((w/SCREEN_WIDTH), (h/SCREEN_HEIGHT));
    ps->textureRect.w = ratio * SCREEN_WIDTH;
    ps->textureRect.h = ratio * SCREEN_HEIGHT;
    ps->textureRect.x = (w - ps->textureRect.w) / 2;
    ps->textureRect.y = (h - ps->textureRect.h) / 2;
}
bool setFullScreen(SDL_Window *window, bool isFullScreen) {
    if (SDL_SetWindowFullscreen(window, isFullScreen) == 0) {
        return true;
    }
    else {
        CO_ERR("Could not set full screen to! Reason: %s", SDL_GetError());
        return false;
    }
}

#endif
#ifdef WINDOWS 
bool openFileDialogAtPath(const char *path, char *outPath) {
	OPENFILENAMEW dialog = {};
	wchar_t widePath[MAX_PATH + 1];
	wchar_t wideOutPath[MAX_PATH + 1];
	if (!convertUTF8ToWChar(path, widePath, MAX_PATH)) {
		CO_ERR("Could not convert path to widechar");
		return false;
	}
	dialog.lStructSize = sizeof(dialog);
	dialog.Flags = OFN_FILEMUSTEXIST;
    dialog.lpstrFilter = L"Game Boy ROMs\0*.gb;*.gbc;*.sgb\0All Files\0*.*\0"; 
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrInitialDir = widePath;
	dialog.lpstrFile = wideOutPath;
    dialog.lpstrTitle = L"Open Game Boy ROM File";
	bool result = GetOpenFileNameW(&dialog);

	if (!convertWCharToUTF8(wideOutPath, outPath, MAX_PATH_LEN)) {
		CO_ERR("Could not convert path to widechar");
		return false;
	}

	return result;
}
bool openDirectoryAtPath(const char *path, char *outPath) {
	BROWSEINFOW bi = {};
	wchar_t wcharPath[MAX_PATH + 1];
	bi.lpszTitle = L"Choose GBEmu Home Directory";
	bi.ulFlags = BIF_USENEWUI;

	PIDLIST_ABSOLUTE result = SHBrowseForFolderW(&bi);
	if (!result) {
		return false;
	}
	if (!SHGetPathFromIDListW(result, wcharPath)) {
		ALERT_EXIT("Error with path chosen.");
		return false;
	}

	int toUTF8Result = 
		WideCharToMultiByte(CP_UTF8, 0, wcharPath, MAX_PATH, outPath, MAX_PATH_LEN, nullptr, nullptr);

	if (toUTF8Result == 0) {
		ALERT_EXIT("Error using the non-ASCII path.");
	}
	//TODO free pidlist_absolute

	return toUTF8Result != 0;
}
#elif defined(LINUX)
#include <gtk/gtk.h>
bool openFileDialogAtPath(const char *path, char *outPath) {
    if (!gtk_init_check(nullptr, nullptr)) {
        ALERT_EXIT("Failed to init the ROM file chooser dialog.");
        return false;
    }
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    auto dialog = gtk_file_chooser_dialog_new ("Open Game Boy ROM File",
                                          nullptr,
                                          action,
                                          "_Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "_Open",
                                          GTK_RESPONSE_ACCEPT,
                                          nullptr);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    gtk_file_chooser_set_current_folder(chooser, path);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.gb");
    gtk_file_filter_add_pattern(filter, "*.gbc");
    gtk_file_filter_add_pattern(filter, "*.sgb");
    gtk_file_filter_set_name(filter, "Game Boy ROMs");
    gtk_file_chooser_add_filter(chooser, filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.*");
    gtk_file_filter_set_name(filter, "All Files");

    gtk_file_chooser_add_filter(chooser, filter);
    gint res = gtk_dialog_run (GTK_DIALOG (dialog));

    bool ret = false;
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename (chooser);
        strncpy(outPath, filename, MAX_PATH_LEN);
        g_free (filename);

        ret = true;
    }
    gtk_widget_destroy(dialog);

    while (gtk_events_pending()) {
       gtk_main_iteration();
    }


    return ret;
}

bool openDirectoryAtPath(const char *path, char *outPath) {
    if (!gtk_init_check(nullptr, nullptr)) {
        ALERT_EXIT("Failed to init the ROM file chooser dialog.");
        return false;
    }
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    auto dialog = gtk_file_chooser_dialog_new ("Choose GBEmu Home Directory",
                                          nullptr,
                                          action,
                                          "_Cancel",
                                          GTK_RESPONSE_CANCEL,
                                          "_Open",
                                          GTK_RESPONSE_ACCEPT,
                                          nullptr);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    gtk_file_chooser_set_current_folder(chooser, path);

    gtk_file_chooser_set_action(chooser, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gint res = gtk_dialog_run (GTK_DIALOG (dialog));

    bool ret = false;
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename (chooser);
	copyString(filename, outPath, MAX_PATH_LEN);
        g_free (filename);

        ret = true;
    }
    gtk_widget_destroy(dialog);

    while (gtk_events_pending()) {
       gtk_main_iteration();
    }


    return ret;
}

#endif
