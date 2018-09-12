//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#include "3rdparty/GL/gl3w.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "config.h"

//unity
#define CO_IMPL


#include "gbemu.h"
#include "debugger.h"
#include "3rdparty/gl3w.c"
#include "config.cpp"

#ifdef WINDOWS
#include <io.h>
#include <ShlObj_core.h>
#define F_OK 0
#define access(a1,a2) _access(a1,a2)
#endif

#define RUN_BOOT_SCREEN_FLAG "-b"
#define DEBUG_ON_BOOT_FLAG "-d"

#define DEFAULT_GBEMU_HOME_PATH "gbemu_home"
#define HOME_PATH_ENV_VARIABLE "GBEMU_HOME"
#define HOME_PATH_CONFIG_FILE "gbemu_home_path.txt"

#define GBEMU_CONFIG_FILENAME "config.txt"
#define GBEMU_CONFIG_FILE_PATH (".." FILE_SEPARATOR GBEMU_CONFIG_FILENAME)

#define CART_RAM_FILE_EXTENSION "sav"
#define RTC_FILE_LEN 48


#define SECONDS_PER_FRAME (1.f/60)
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

#ifdef CO_PROFILE
    static ProfileState *profileState;
#endif
    
#define ANALOG_STICK_DEADZONE 8000
    

#define DEFAULT_KEY_UP SDLK_w
#define DEFAULT_CONTROLLER_UP SDL_CONTROLLER_BUTTON_DPAD_UP

#define DEFAULT_KEY_DOWN SDLK_s
#define DEFAULT_CONTROLLER_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN

#define DEFAULT_KEY_LEFT SDLK_a
#define DEFAULT_CONTROLLER_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT

#define DEFAULT_KEY_RIGHT SDLK_d
#define DEFAULT_CONTROLLER_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT

#define DEFAULT_KEY_B SDLK_PERIOD
#define DEFAULT_CONTROLLER_B SDL_CONTROLLER_BUTTON_B

#define DEFAULT_KEY_A SDLK_SLASH
#define DEFAULT_CONTROLLER_A SDL_CONTROLLER_BUTTON_A

#define DEFAULT_KEY_START SDLK_RETURN
#define DEFAULT_CONTROLLER_START SDL_CONTROLLER_BUTTON_START

#define DEFAULT_KEY_SELECT SDLK_BACKSLASH
#define DEFAULT_CONTROLLER_SELECT SDL_CONTROLLER_BUTTON_BACK

#define DEFAULT_KEY_REWIND SDLK_LEFT
#define DEFAULT_CONTROLLER_REWIND SDL_CONTROLLER_BUTTON_LEFTSHOULDER

#define DEFAULT_KEY_MUTE SDLK_m
#define DEFAULT_CONTROLLER_MUTE NO_INPUT_MAPPING

#define DEFAULT_KEY_PAUSE SDLK_p
#define DEFAULT_CONTROLLER_PAUSE NO_INPUT_MAPPING

#define DEFAULT_KEY_STEP SDLK_n
#define DEFAULT_CONTROLLER_STEP NO_INPUT_MAPPING

#define DEFAULT_KEY_CONTINUE SDLK_c
#define DEFAULT_CONTROLLER_CONTINUE NO_INPUT_MAPPING

#ifdef MAC 
#   define CTRL "Command-"
#else
#   define CTRL "Ctrl-"
#endif

static const char defaultConfigFileContents[] = 
        "//Keyboard Mappings" ENDL 
        ENDL
       "Up = Key W" ENDL 
       "Down = Key S" ENDL
       "Left = Key A" ENDL
       "Right = Key D" ENDL
       ENDL 
       "Start = Key Enter" ENDL 
       "Select = Key \\" ENDL
       ENDL 
       "A = Key /" ENDL 
       "B = Key ." ENDL
        ENDL
        "Rewind = Key Left" ENDL
        "Mute = Key M" ENDL
        "Step = Key N" ENDL
        "Continue = Key C" ENDL
        "Pause = Key " CTRL "P" ENDL
        "Reset = Key " CTRL "R" ENDL
        "ShowDebugger = Key " CTRL "B" ENDL
        "ShowHomePath = Key " CTRL "H" ENDL
        "FullScreen = Key " CTRL "F" ENDL
        ENDL
        "//Controller Mappings" ENDL
        ENDL
        "Up = Controller Up" ENDL 
        "Down = Controller Down" ENDL
        "Left = Controller Left" ENDL
        "Right = Controller Right" ENDL
        ENDL 
        "Start = Controller Start" ENDL 
        "Select = Controller Select" ENDL
        ENDL 
        "A = Controller A" ENDL 
        "B = Controller B" ENDL
        ENDL
        "Rewind = Controller LeftBumper" ENDL
        ENDL
        "//Misc" ENDL
        "ScreenScale = 4";

static const char *resultCodeToString[(int)FileSystemResultCode::SizeOfEnumMinus1 + 1] = {
    [(int)FileSystemResultCode::OK] =  "",
    [(int)FileSystemResultCode::PermissionDenied] = "Permission denied",
    [(int)FileSystemResultCode::OutOfSpace] = "Out of disk space",
    [(int)FileSystemResultCode::OutOfMemory] = "Out of memory",
    [(int)FileSystemResultCode::AlreadyExists] = "File already exists",
    [(int)FileSystemResultCode::IOError] = "Error reading or writing to disk",
    [(int)FileSystemResultCode::NotFound] = "File not found",
    [(int)FileSystemResultCode::Unknown] = "Unknown error"
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

static const int controllerMappingToSDLButton[] = {
  [(int)ControllerMappingValue::A] = SDL_CONTROLLER_BUTTON_A,  
  [(int)ControllerMappingValue::B] = SDL_CONTROLLER_BUTTON_B,  
  [(int)ControllerMappingValue::X] = SDL_CONTROLLER_BUTTON_X,  
  [(int)ControllerMappingValue::Y] = SDL_CONTROLLER_BUTTON_Y,  
    
  [(int)ControllerMappingValue::Up] = SDL_CONTROLLER_BUTTON_DPAD_UP,  
  [(int)ControllerMappingValue::Down] = SDL_CONTROLLER_BUTTON_DPAD_DOWN,  
  [(int)ControllerMappingValue::Left] = SDL_CONTROLLER_BUTTON_DPAD_LEFT,  
  [(int)ControllerMappingValue::Right] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,  
  
  [(int)ControllerMappingValue::LeftBumper] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  
  [(int)ControllerMappingValue::RightBumper] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,  
    
  [(int)ControllerMappingValue::Start] = SDL_CONTROLLER_BUTTON_START,  
  [(int)ControllerMappingValue::Select] = SDL_CONTROLLER_BUTTON_BACK,  
};

#define ALERT(fmt, ...)  do {\
    char *msg = nullptr;\
    buf_printf(msg, fmt, ##__VA_ARGS__);\
    CO_ERR(fmt, ##__VA_ARGS__);\
    alertDialog(msg);\
    buf_free(msg);\
}while(0)
#define ALERT_EXIT(fmt, ...) ALERT(fmt" Exiting...", ##__VA_ARGS__)
bool openFileDialogAtPath(const char *path, char *outPath);
bool openDirectoryAtPath(const char *path, char *outPath);

#ifdef CO_DEBUG
typedef void FnRunFrame(CPU *, MMU *, GameBoyDebug *, ProgramState *, TimeUS dt);
typedef void FnReset(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug, ProgramState *programState);
typedef void FnSetMemoryContext(MemoryContext *mc);
struct GBEmuCode {
    void *handle;
    FnRunFrame *runFrame;
    FnReset *reset;
    FnSetMemoryContext *setMemoryContext;
    time_t timeLastModified;
};

GBEmuCode loadGBEmuCode(const char *gbemuCodePath) {
    GBEmuCode ret;

    ret.handle = SDL_LoadObject(gbemuCodePath);
    CO_ASSERT_MSG(ret.handle, "Error opening shared obj: %s", SDL_GetError());

    ret.runFrame = (FnRunFrame*) SDL_LoadFunction(ret.handle, "runFrame");
    CO_ASSERT(ret.runFrame);

    ret.reset = (FnReset*) SDL_LoadFunction(ret.handle, "reset");
    CO_ASSERT(ret.runFrame);
    
    ret.setMemoryContext = (FnSetMemoryContext*) SDL_LoadFunction(ret.handle, "setMemoryContext");
    CO_ASSERT(ret.setMemoryContext);

    struct stat fileStats;
    stat(gbemuCodePath, &fileStats);

    ret.timeLastModified = fileStats.st_mtime;
    
    ret.setMemoryContext(getMemoryContext());

    return ret;
}

#else 
#define loadGBEmuCode(fileName) 0;(void)gbEmuCode;
void runFrame(CPU *, MMU *, GameBoyDebug *gbDebug, ProgramState *, TimeUS dt);
void reset(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug, ProgramState *programState);
#endif

struct DebuggerPlatformContext {
    SDL_Window *window;
    void *glContext;

    GLuint shaderID, vertexShaderID, fragmentShaderID;
    GLint attribLocationProjMtx, attribLocationPosition, attribLocationUV, attribLocationColor;
    GLint attribLocationTex;
    GLuint vboID, vaoID, elementsID;
    GLuint fontTexture;

    //render thread data
    Thread *renderThread;
    volatile bool isRunning;
    GameBoyDebug *gbDebug;
    
};


static void alertDialog(const char *message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, nullptr);
}
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
    
    buf_printf(message, "GBEmu has no home directory set. This is a required directory that will be used for storage of ROMs, save states, and other files.\n"
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
    buf_free(message);
    
    return (success && pressedButton != -1) ? (HomeDirectoryOption)pressedButton : HomeDirectoryOption::EXIT;
}

static void processKey(SDL_Keycode key, bool isDown, bool isCtrlDown, Input::State *input, const InputMappingConfig *config) {
    if (isCtrlDown) {
        foriarr (config->controlCodeMappings) {
            if (key == config->controlCodeMappings[i].commandCode) {
                input->actionsHit[i] = isDown;
            }
        }
    }
    else {
        foriarr (config->controlCodeMappings) {
            if (key == config->controlCodeMappings[i].keyCode) {
                input->actionsHit[i] = isDown;
            }
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
            input->saveState = isDown;
        }
        else {
            input->restoreState = isDown;
        }
    } break;
    case SDLK_ESCAPE: {
        input->escapeFullScreen = isDown;
    } break;
    case SDLK_KP_ENTER:
    case SDLK_RETURN: {
       input->enterPressed = isDown; 
    } break;
    default:
        //do nothing
        break;
    }
}


static void processButton(SDL_GameControllerButton button, bool isDown, Input::State *input, const InputMappingConfig *config) {
    foriarr (config->controlCodeMappings) {
        if (button == config->controlCodeMappings[i].buttonCode) {
            input->actionsHit[i] = isDown;
        }
    }
}

static void
closeDebugger(DebuggerPlatformContext *debuggerContext, GameBoyDebug *gbDebug) {
    if (!debuggerContext->isRunning) {
        return;
    }
    debuggerContext->isRunning = false;
    if (gbDebug) {
        gbDebug->shouldRender = true; 
        broadcastCondition(gbDebug->renderCondition);
    }
    waitForAndFreeThread(debuggerContext->renderThread);
    ImGui::DestroyContext();

    if (debuggerContext->glContext) {
        SDL_GL_DeleteContext(debuggerContext->glContext);
    }
    if (debuggerContext->window) {
        SDL_DestroyWindow(debuggerContext->window);
    }
    if (gbDebug) {
        gbDebug->isEnabled = false;
    }

    zeroMemory(debuggerContext, sizeof(*debuggerContext));

}

static void renderDebuggerThread(void *arg) {
    auto platformContext = (DebuggerPlatformContext*)arg;
    auto gbDebug = platformContext->gbDebug;
    while (platformContext->isRunning) {
        lockMutex(gbDebug->debuggerMutex);
        while (!gbDebug->shouldRender) {
            waitForCondition(gbDebug->renderCondition, gbDebug->debuggerMutex);
        }
        gbDebug->shouldRender = false;
        SDL_GL_MakeCurrent(platformContext->window, platformContext->glContext);
        glClearColor(1.0f, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        foriarr (gbDebug->tiles) {
            auto tile = gbDebug->tiles[i];

            //TODO: might have to do this update out of this thread
            if (tile.needsUpdate) {
                u32 pixels[TILE_HEIGHT * SCREEN_SCALE * TILE_WIDTH * SCREEN_SCALE];
                for (i64 y = 0; y < TILE_HEIGHT * SCREEN_SCALE; y+=SCREEN_SCALE) {
                    for (i64 x = 0; x < TILE_WIDTH * SCREEN_SCALE; x+=SCREEN_SCALE) {
                        u32 pixel;
                        switch ((PaletteColor)tile.pixels[(y/SCREEN_SCALE)*TILE_WIDTH + (x/SCREEN_SCALE)]) {
                        case PaletteColor::White: pixel = 0xFFFFFFFF; break;
                        case PaletteColor::LightGray: pixel = 0xFFAAAAAA; break;
                        case PaletteColor::DarkGray: pixel = 0xFF555555; break;
                        case PaletteColor::Black: pixel = 0xFF000000; break;
                        }

                        for (i64 y2 = 0; y2 < SCREEN_SCALE; y2++) {
                            for (i64 x2 = 0; x2 < SCREEN_SCALE; x2++) {
                                pixels[((y+y2)*TILE_WIDTH*SCREEN_SCALE) + (x + x2)] = pixel;
                            }
                        }

                    }
                }
                glBindTexture(GL_TEXTURE_2D, (GLuint)(u64)tile.textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TILE_WIDTH * SCREEN_SCALE, TILE_HEIGHT * SCREEN_SCALE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                tile.needsUpdate = false;
            }
        }

        auto drawData = ImGui::GetDrawData();
        // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
        ImGuiIO& io = ImGui::GetIO();
        int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        CO_ASSERT(fb_width != 0 && fb_height != 0);
        drawData->ScaleClipRects(io.DisplayFramebufferScale);

        // Backup GL state
        GLint last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
        glActiveTexture(GL_TEXTURE0);
        GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
        GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
        GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
        GLint last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, &last_blend_src_rgb);
        GLint last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, &last_blend_dst_rgb);
        GLint last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
        GLint last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);
        GLint last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
        GLint last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
        GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
        GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
        GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

        // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);

        // Setup viewport, orthographic projection matrix
        glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
        const float ortho_projection[4][4] =
        {
            { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
            { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
            { 0.0f,                  0.0f,                  -1.0f, 0.0f },
            {-1.0f,                  1.0f,                   0.0f, 1.0f },
        };
        glUseProgram(platformContext->shaderID);
        glUniform1i(platformContext->attribLocationTex, 0);
        glUniformMatrix4fv(platformContext->attribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
        glBindVertexArray(platformContext->vaoID);

        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = drawData->CmdLists[n];
            const ImDrawIdx* idx_buffer_offset = 0;

            glBindBuffer(GL_ARRAY_BUFFER, platformContext->vboID);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cmd_list->VtxBuffer.Size * (int)sizeof(ImDrawVert)), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformContext->elementsID);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(cmd_list->IdxBuffer.Size * (int)sizeof(ImDrawIdx)), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback)
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                    glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                    glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
                }
                idx_buffer_offset += pcmd->ElemCount;
            }
        }

        // Restore modified GL state
        glUseProgram((GLuint)last_program);
        glBindTexture(GL_TEXTURE_2D, (GLuint)last_texture);
        glActiveTexture((GLuint)last_active_texture);
        glBindVertexArray((GLuint)last_vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_array_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_element_array_buffer);
        glBlendEquationSeparate((GLuint)last_blend_equation_rgb, (GLuint)last_blend_equation_alpha);
        glBlendFuncSeparate((GLuint)last_blend_src_rgb, (GLuint)last_blend_dst_rgb, (GLuint)last_blend_src_alpha, (GLuint)last_blend_dst_alpha);
        if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
        glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);

        
        SDL_GL_SwapWindow(platformContext->window);
        unlockMutex(gbDebug->debuggerMutex);
        
    }
}

static bool
initDebugger(DebuggerPlatformContext *out, GameBoyDebug *gbDebug, ProgramState *programState, int mainScreenX, int mainScreenY) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetSwapInterval(1);
    auto window = SDL_CreateWindow("Debugger", mainScreenX + 20, mainScreenY + 20,
                                   DEBUG_WINDOW_MIN_WIDTH, DEBUG_WINDOW_MIN_HEIGHT, 
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        ALERT("Could not create debugger window. Reason %s", SDL_GetError());
		return false;
    }
    
    SDL_SetWindowMinimumSize(window, DEBUG_WINDOW_MIN_WIDTH, DEBUG_WINDOW_MIN_HEIGHT);

    auto glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    int initResult = gl3wInit();
    if (initResult != 0) {
		ALERT("Cannot init gl3w.  Error num: %d", initResult);
		SDL_DestroyWindow(window);
		return false;
    } 


    GLint last_texture, last_array_buffer, last_vertex_array;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    const GLchar *vertex_shader =
            "#version 330\n"
            "uniform mat4 ProjMtx;\n"
            "in vec2 Position;\n"
            "in vec2 UV;\n"
            "in vec4 Color;\n"
            "out vec2 Frag_UV;\n"
            "out vec4 Frag_Color;\n"
            "void main()\n"
            "{\n"
            "	Frag_UV = UV;\n"
            "	Frag_Color = Color;\n"
            "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
            "}\n";

    const GLchar* fragment_shader =
            "#version 330\n"
            "uniform sampler2D Texture;\n"
            "in vec2 Frag_UV;\n"
            "in vec4 Frag_Color;\n"
            "out vec4 Out_Color;\n"
            "void main()\n"
            "{\n"
            "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
            "}\n";

    out->shaderID = glCreateProgram();
    out->vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    out->fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(out->vertexShaderID, 1, &vertex_shader, 0);
    glShaderSource(out->fragmentShaderID, 1, &fragment_shader, 0);
    glCompileShader(out->vertexShaderID);
    glCompileShader(out->fragmentShaderID);
    glAttachShader(out->shaderID, out->vertexShaderID);
    glAttachShader(out->shaderID, out->fragmentShaderID);
    glLinkProgram(out->shaderID);

    out->attribLocationTex = glGetUniformLocation(out->shaderID, "Texture");
    out->attribLocationProjMtx = glGetUniformLocation(out->shaderID, "ProjMtx");
    out->attribLocationPosition = glGetAttribLocation(out->shaderID, "Position");
    out->attribLocationUV = glGetAttribLocation(out->shaderID, "UV");
    out->attribLocationColor = glGetAttribLocation(out->shaderID, "Color");

    glGenBuffers(1, &out->vboID);
    glGenBuffers(1, &out->elementsID);

    glGenVertexArrays(1, &out->vaoID);
    glBindVertexArray(out->vaoID);
    glBindBuffer(GL_ARRAY_BUFFER, out->vboID);
    glEnableVertexAttribArray((GLuint)out->attribLocationPosition);
    glEnableVertexAttribArray((GLuint)out->attribLocationUV);
    glEnableVertexAttribArray((GLuint)out->attribLocationColor);

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    glVertexAttribPointer((GLuint)out->attribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer((GLuint)out->attribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer((GLuint)out->attribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF

    programState->guiContext = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize.x = DEBUG_WINDOW_MIN_WIDTH;
    io.DisplaySize.y = DEBUG_WINDOW_MIN_HEIGHT;
    io.IniFilename = "layout.ini";
    io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
    io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SDLK_a;
    io.KeyMap[ImGuiKey_C] = SDLK_c;
    io.KeyMap[ImGuiKey_V] = SDLK_v;
    io.KeyMap[ImGuiKey_X] = SDLK_x;
    io.KeyMap[ImGuiKey_Y] = SDLK_y;
    io.KeyMap[ImGuiKey_Z] = SDLK_z;
    
    gbDebug->nextTextPos = gbDebug->inputText;
    *gbDebug->nextTextPos = '\0';

    // Upload texture to graphics system
    {
        // Build texture atlas
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.
        glGenTextures(1, &out->fontTexture);
        glBindTexture(GL_TEXTURE_2D, out->fontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        // Store our identifier
        io.Fonts->TexID = (void *)(intptr_t)out->fontTexture;

    }



    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, (GLuint)last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_array_buffer);
    glBindVertexArray((GLuint)last_vertex_array);


    out->window = window;
    out->glContext = glContext;
    foriarr (gbDebug->tiles) {
        auto tile = &gbDebug->tiles[i];
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        tile->textureID = (void*)textureID;
        tile->needsUpdate = true;
    }
    out->isRunning = true;
    if (!gbDebug->debuggerMutex) {
        gbDebug->debuggerMutex = createMutex();
    }
    if (!gbDebug->renderCondition) {
        gbDebug->renderCondition = createWaitCondition();
    }
    gbDebug->shouldRender = false;
    out->gbDebug = gbDebug;
    SDL_GL_MakeCurrent(out->window, nullptr);
    out->renderThread = startThread(renderDebuggerThread, out);
    return true;
}

static void
cleanUp(CartRAMPlatformState *crps, DebuggerPlatformContext *debuggerContext) {
    closeMemoryMappedFile(crps->cartRAMFileHandle);
    closeDebugger(debuggerContext, debuggerContext->gbDebug);
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
    buf_printf(srcFileName, "%s.%s",fileNameToBackup, extension);
    ALERT("Invalid cart RAM file: %s%s! This file will be copied to %s%s and a new file will be created.", homeDirectory, srcFileName, homeDirectory, newPath);
    auto copyResult = copyFile(srcFileName, newPath, fileMemory);
    buf_free(srcFileName);
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

static bool handleInputMappingFromConfig(InputMapping *mapping, LHS *lhs, RHS *rhs) {
    switch (rhs->rhsType) {
    case RHSType::ControllerMapping: {
        mapping->buttonCode = controllerMappingToSDLButton[(int)rhs->controllerMapping.value];
    } break;
    case RHSType::KeyMapping: {
        if (rhs->keyMapping.isCtrlHeld) {
            switch (rhs->keyMapping.type) {
            case KeyMappingType::Character: {
               mapping->commandCode = rhs->keyMapping.characterValue; 
            } break;
            case KeyMappingType::MovementKey: {
                SDL_Keycode sdlCode = movementKeyMappingToSDLKeyCode[(int)rhs->keyMapping.movementKeyValue]; 
                mapping->commandCode = sdlCode;
            } break;
            }
        }
        else {
            switch (rhs->keyMapping.type) {
            case KeyMappingType::Character: {
                mapping->keyCode = rhs->keyMapping.characterValue; 
            } break;
            case KeyMappingType::MovementKey: {
                SDL_Keycode sdlCode = movementKeyMappingToSDLKeyCode[(int)rhs->keyMapping.movementKeyValue]; 
                mapping->keyCode = sdlCode; 
            } break;
            }
        }
    } break;
    default: {
        ALERT_EXIT("Option at line: %d, column %d in %s must be bound to a controller or key mapping.", lhs->line, lhs->posInLine, GBEMU_CONFIG_FILENAME);
        return false;
    } break;
    }
    return true;

}

static bool doConfigFileParsing(const char *configFilePath, ProgramState *programState) {
    auto result = parseConfigFile(configFilePath); 
    switch (result.fsResultCode) {
    case FileSystemResultCode::OK:  {
        //do nothing and continue 
    } break;
    case FileSystemResultCode::NotFound: {
        freeParserResult(&result);
        
        profileStart("Save new file", profileState);
        auto writeResult = writeDataToFile(defaultConfigFileContents, sizeof(defaultConfigFileContents) - 1, configFilePath);
        switch (writeResult) {
        case FileSystemResultCode::OK: {
            //continue 
        } break;
        default: {
            buf_free(configFilePath);
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

    } break;
    default: {
       ALERT_EXIT("Failed to read " GBEMU_CONFIG_FILENAME ". Reason: %s.",  resultCodeToString[(int)result.fsResultCode]);
       return false;
    } break;
    }
    
    switch (result.status) {
    case ParserStatus::OK: {
        //do nothing and continue 
    } break;

        //TODO
        
    }
    
    fori (result.numConfigPairs) {
#define CASE_MAPPING(mapping)  case LHSValue::mapping: {\
            if (!handleInputMappingFromConfig(&programState->input.inputMappingConfig.mapping##Mapping, &cp->lhs, &cp->rhs)) {\
               return false;\
            }\
        } break
        
        ConfigPair *cp = result.configPairs + i;  
        switch (cp->lhs.value) {
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
        CASE_MAPPING(Step);
        CASE_MAPPING(Continue);
        CASE_MAPPING(Reset);
        CASE_MAPPING(FullScreen);
        case LHSValue::ScreenScale: {
           switch (cp->rhs.rhsType) {
           case RHSType::Integer: {
              programState->screenScale = cp->rhs.intValue; 
              //TODO: validate screen scale
           } break;
           default:  {
               ALERT_EXIT("Screen Scale at line: %d, column %d in %s must be bound to a controller or key mapping.", 
                          cp->lhs.line, cp->lhs.posInLine, GBEMU_CONFIG_FILENAME);
               return false;
           } break;
           }
        } break;
        }
    }
    
    //set defaults
    if (programState->screenScale <= 0) {
        programState->screenScale = SCREEN_SCALE;
    }

    freeParserResult(&result);
    return true;
}

static void 
mainLoop(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *screenTexture,
         SDL_AudioDeviceID audioDeviceID, SDL_GameController **controller, const char *romFileName,
         bool shouldEnableDebugMode, DebuggerPlatformContext *debuggerContext, GameBoyDebug *gbDebug, ProgramState *programState) {
    char filePath[MAX_PATH_LEN];
    
#define GAME_LIB_PATH "gbemu.so"
    char gbemuCodePath[MAX_PATH_LEN];
    //used for hot loading
    getCurrentWorkingDirectory(gbemuCodePath);
    snprintf(gbemuCodePath, MAX_PATH_LEN, "%s/%s", gbemuCodePath, GAME_LIB_PATH);
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
		{
			u8 computedChecksum = 0;
			for (int i = 0x134; i <= 0x14C; i++) {
				computedChecksum -= mmu->romData[i] + 1;
			}
			if (computedChecksum != mmu->romData[0x14D]) {
				ALERT("This file is not a valid Game Boy ROM file. Checksum does not match.");
				return;
			}
		}
		{
			u16 computedChecksum = 0;
            u16 globalChecksum = (u16)((mmu->romData[0x14E] << 8) | mmu->romData[0x14F]);
			for (int i = 0; i < mmu->romSize ; i++) {
                if (i == 0x14E || i == 0x14F) {
                    continue;
                }
				computedChecksum += mmu->romData[i];
			}
			if (computedChecksum != globalChecksum) {
				ALERT("This file is not a valid Game Boy ROM file. Global checksum does not match.");
				return;
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

    auto gbEmuCode = loadGBEmuCode(gbemuCodePath);

    
    //Notification init
    NotificationState *notifications = &programState->notifications;
    char currentNotification[MAX_NOTIFICATION_LEN + 1] = {};
    TimeUS timeNotificationIsShown = 0;
    TimeUS startNotificationTime = nowInMicroseconds();
    
    //controller init
    SDL_JoystickID controllerID = 0;
    {
        fori (SDL_NumJoysticks()) {
            if (SDL_IsGameController((int)i)) {
                *controller = SDL_GameControllerOpen((int)i);
                if (*controller) {
                    controllerID = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*controller));
                    CO_LOG("%s attached", SDL_GameControllerNameForIndex((int)i));
                    NOTIFY(notifications, "%s attached", SDL_GameControllerNameForIndex((int)i));
                }
                else {
                    ALERT("Could not use controller!");
                }
                break;
            }
        }

    }
    
    //Sound init
    SoundState *platformSoundState = &programState->soundState;
    platformSoundState->buffer.len = (AUDIO_SAMPLE_RATE / 10); //100 ms
    platformSoundState->volume = 50;
    platformSoundState->buffer.data = PUSHM(platformSoundState->buffer.len, SoundFrame);

    fillMemory((i16*)platformSoundState->buffer.data, SHRT_MIN, platformSoundState->buffer.len * (i64)(sizeof(i16) * 2));

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
#ifdef CO_DEBUG
    gbEmuCode.reset(cpu, mmu, gbDebug, programState);
#else
    reset(cpu, mmu, gbDebug, programState);
#endif

    TimeUS startTime = nowInMicroseconds(), dt;
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

            time_t fileTimeModified = fileStats.st_mtime;

            //if the .so is new and the .so isn't locked, reload the code
            if (fileTimeModified > gbEmuCode.timeLastModified &&
                    access("./soLock", F_OK) != 0) {
                SDL_UnloadObject(gbEmuCode.handle);
                gbEmuCode = loadGBEmuCode(gbemuCodePath);
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
                        else if (e.window.windowID == SDL_GetWindowID(debuggerContext->window)) {
                            closeDebugger(debuggerContext, gbDebug);
                        }
                    } break;
                    case SDL_WINDOWEVENT_RESIZED: { 
                        if (e.window.windowID == SDL_GetWindowID(debuggerContext->window)) {
                            auto &io = ImGui::GetIO(); 
                            io.DisplaySize.x = e.window.data1;
                            io.DisplaySize.y = e.window.data2;
                        }

                    } break;
                    }

                } break;
                case SDL_QUIT:
                    cleanUp(&mmu->cartRAMPlatformState, debuggerContext);
                    return;

                case SDL_KEYDOWN: {
                    processKey(e.key.keysym.sym, true, (keyMod & CTRL_KEY) != 0, &input->newState, &input->inputMappingConfig);
                    
                } break;

                case SDL_KEYUP: {
                    processKey(e.key.keysym.sym, false, (keyMod & CTRL_KEY) != 0, &input->newState, &input->inputMappingConfig);
                } break;

                case SDL_CONTROLLERBUTTONUP:  
                case SDL_CONTROLLERBUTTONDOWN:  {
                    if (*controller && e.cbutton.which == controllerID) {
                        processButton((SDL_GameControllerButton)e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN, &input->newState, 
                                      &input->inputMappingConfig);
                    }
                    
                } break;
                case SDL_CONTROLLERAXISMOTION: {
                    if (*controller && e.caxis.which == controllerID) {
                        switch (e.caxis.axis) {
                        case SDL_CONTROLLER_AXIS_LEFTX: {
                            input->newState.xAxis = e.caxis.value;
                            if (e.caxis.value < ANALOG_STICK_DEADZONE && e.caxis.value > -ANALOG_STICK_DEADZONE) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, false, &input->newState, &input->inputMappingConfig);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false, &input->newState, &input->inputMappingConfig);
                            }
                            else if (e.caxis.value >= ANALOG_STICK_DEADZONE && 
                                     e.caxis.value > abs(input->newState.yAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, false, &input->newState, &input->inputMappingConfig);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, true, &input->newState, &input->inputMappingConfig);
                            }
                            else if (e.caxis.value <= -ANALOG_STICK_DEADZONE && 
                                     abs(e.caxis.value) > abs(input->newState.yAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, true, &input->newState, &input->inputMappingConfig);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false, &input->newState, &input->inputMappingConfig);
                            }
                        } break;
                        case SDL_CONTROLLER_AXIS_LEFTY: {
                            input->newState.yAxis = e.caxis.value;
                            if (e.caxis.value < ANALOG_STICK_DEADZONE && e.caxis.value > -ANALOG_STICK_DEADZONE) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_UP, false, &input->newState, &input->inputMappingConfig);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false, &input->newState, &input->inputMappingConfig);
                            }
                            else if (e.caxis.value >= ANALOG_STICK_DEADZONE && 
                                     e.caxis.value > abs(input->newState.xAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_UP, false, &input->newState, &input->inputMappingConfig);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, true, &input->newState, &input->inputMappingConfig);
                            }
                            else if (e.caxis.value <= -ANALOG_STICK_DEADZONE && 
                                     abs(e.caxis.value) > abs(input->newState.xAxis)) {
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_UP, true, &input->newState, &input->inputMappingConfig);
                                processButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false, &input->newState, &input->inputMappingConfig);
                            }
                        } break;
                        }
                    }
                    
                } break;

                case SDL_CONTROLLERDEVICEADDED: {
                    if (*controller) {
                        break;
                    }
                    int i = e.cdevice.which;
                    *controller = SDL_GameControllerOpen(i);
                    controllerID = i;
                    if (controller) {
                        NOTIFY(notifications, "%s attached", SDL_GameControllerNameForIndex(i));
                        CO_LOG("%s attached at index %d", SDL_GameControllerNameForIndex(i), i);
                    }
                    else {
                        CO_ERR("Could not open controller");
                    }
                    break;
                } break;

                case SDL_CONTROLLERDEVICEREMOVED: {
                    auto controllerToClose = SDL_GameControllerFromInstanceID(e.cdevice.which);

                    if (*controller == controllerToClose) {
                        NOTIFY(notifications, "Controller detached");
                        CO_LOG("Controller detached");
                        SDL_GameControllerClose(*controller);
                        *controller = nullptr;
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

            if (input->newState.ShowDebugger && !input->oldState.ShowDebugger && !gbDebug->isEnabled && !programState->isFullScreen) {
                int windowX, windowY;
                SDL_GetWindowPosition(window, &windowX, &windowY);
                if (initDebugger(debuggerContext, gbDebug, programState, windowX, windowY)) {
                    gbDebug->isEnabled = true;
                }
            }
            if (input->newState.FullScreen && !input->oldState.FullScreen) {
                if (programState->isFullScreen) {
                    if (SDL_SetWindowFullscreen(window, 0) == 0) {
                        SDL_ShowCursor(true);
                        programState->isFullScreen = false;
                    }
                    else {
                        CO_ERR("Could not go to windowed mode! Reason: %s", SDL_GetError());
                    }
                }
                else {
                    if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) == 0) {
                        programState->isFullScreen = true;
                        SDL_ShowCursor(false);
                    }
                    else {
                        CO_ERR("Could not go full screen! Reason: %s", SDL_GetError());
                    }
                }
            }
            if (input->newState.escapeFullScreen && !input->oldState.escapeFullScreen && programState->isFullScreen) {
                if (SDL_SetWindowFullscreen(window, 0) == 0) {
                    programState->isFullScreen = false;
                    SDL_ShowCursor(true);
                }
                else {
                    CO_ERR("Could not go to windowed mode! Reason: %s", SDL_GetError());
                }
            }
        }
        TimeUS now = nowInMicroseconds();
        dt = now - startTime;
        startTime = now;
        if (gbDebug->isEnabled) {
            if (!debuggerContext->window) {
                int windowX, windowY;
                SDL_GetWindowPosition(window, &windowX, &windowY);
                initDebugger(debuggerContext, gbDebug, programState, windowX, windowY);
            }

            SDL_GetMouseState(&gbDebug->mouseX, &gbDebug->mouseY);

            // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.

            gbDebug->isWindowInFocus =
                    SDL_GetWindowFlags(debuggerContext->window) & SDL_WINDOW_MOUSE_FOCUS;


            input->newState.mouseX = gbDebug->mouseX;
            input->newState.mouseY = gbDebug->mouseY;


        }

        profileStart("run frame", profileState);
        if(!isPaused) {
#ifdef CO_DEBUG
            gbEmuCode.runFrame(cpu, mmu, gbDebug, programState, dt);
#else
            runFrame(cpu, mmu, gbDebug, programState, dt);
#endif
        }
        profileEnd(profileState);

        /***************
         * Play Audio
         **************/
        {
#define MAX_FRAMES_TO_PLAY 1000
#define VOLUME_AMPLIFIER 4
            SoundFrame *framesToPlay = PUSHM(platformSoundState->buffer.numItemsQueued, SoundFrame);
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
                u32 *screenPixels;
                int pitch;
                SDL_LockTexture(screenTexture, nullptr, (void**)&screenPixels, &pitch);
                fori (SCREEN_HEIGHT*SCREEN_WIDTH) {
                    switch ((PaletteColor)lcd->screen[i]) {
                    case PaletteColor::White: screenPixels[i] = 0xFFFFFFFF; break;
                    case PaletteColor::LightGray: screenPixels[i] = 0xFFAAAAAA; break;
                    case PaletteColor::DarkGray: screenPixels[i] = 0xFF555555; break;
                    case PaletteColor::Black: screenPixels[i] = 0xFF000000; break;
                    }
                }
                SDL_UpdateTexture(screenTexture, nullptr, screenPixels, pitch);
                SDL_UnlockTexture(screenTexture);
                SDL_RenderCopy(renderer, screenTexture, nullptr, nullptr);
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
                if (strcmp("-d", argv[i]) == 0) {
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
       PRINT("Usage :gbemu [-d] path_to_ROM");
       PRINT("\t" DEBUG_ON_BOOT_FLAG " -- Start with the debugger screen up and emulator paused");
       return 1; 
    }
    
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *screenTexture = nullptr;
    SDL_AudioSpec as;
    SDL_AudioDeviceID audioDeviceID = 0;
    SDL_GameController *controller = nullptr;
	ProgramState *programState;
    char *romsDir = nullptr;
    const char *openDialogPath = nullptr;
    const char *gbEmuHomePath; 
    char homePathConfigFilePath[MAX_PATH_LEN] = {};
    bool isHomePathValid = false;
    char *configFilePath = nullptr;
    {
        if (!initMemory(GENERAL_MEMORY_SIZE + FILE_MEMORY_SIZE, GENERAL_MEMORY_SIZE)) {
            //Do not use ALERT since it uses general memory which failed to init
            alertDialog("Not enough memory to run the emulator!");
            goto exit;
        }
        programState = PUSHM(1, ProgramState);
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
    /**********************
     * Parse Config file
     *********************/
    profileStart("Parse Config file", profileState);
    configFilePath = nullptr;
    buf_printf(configFilePath, "%s" FILE_SEPARATOR GBEMU_CONFIG_FILENAME, programState->homeDirectoryPath);
    if (!doConfigFileParsing(configFilePath, programState)) {
        return 1;
    }
    buf_free(configFilePath);
    profileEnd(profileState);
    
    //ROMs dir
    buf_printf(romsDir, "%s" FILE_SEPARATOR "ROMs" FILE_SEPARATOR,programState->homeDirectoryPath);
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
    profileStart("init sdl", &programState->profileState);
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        ALERT("Error initing the emulator!  Reason: %s", SDL_GetError());
        goto exit;
    }
    profileEnd(&programState->profileState);
	if (isEmptyString(romFileName)){
		//NOTE: interestingly, SDL_Init must be called before opening an open dialog.  else an app instance won't be created in macOS
        if (!openFileDialogAtPath(openDialogPath, romFileName)) {
            return 1;
        }
	}
    buf_free(romsDir);

    window = SDL_CreateWindow("GB Emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
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
    
    screenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
                                      SCREEN_WIDTH, SCREEN_HEIGHT);
    
    if (!screenTexture) {
        ALERT("Could not create screen. Reason %s", SDL_GetError());
        goto exit;
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

    //open controller if present


    {
        auto debuggerContext = PUSHMCLR(1, DebuggerPlatformContext);
        auto gbDebug = PUSHMCLR(1, GameBoyDebug);
        AutoMemory _am1(debuggerContext), _am2(gbDebug);
        if (shouldEnableDebugMode) {
            int windowX, windowY;
            SDL_GetWindowPosition(window, &windowX, &windowY);
            if (!initDebugger(debuggerContext, gbDebug, programState, windowX, windowY)) {
                CO_ERR("Could not init debugger!");
                goto exit;
            }
        }

        mainLoop(window, renderer, screenTexture, audioDeviceID, &controller, romFileName, shouldEnableDebugMode, debuggerContext, gbDebug, programState);
        
    }


exit:

    if (controller) {
        SDL_GameControllerClose(controller);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);


    SDL_Quit();

    return 0;
}


//Platform specific
#ifdef WINDOWS 
bool openFileDialogAtPath(const char *path, char *outPath) {
	OPENFILENAMEW dialog = {};
	wchar_t widePath[MAX_PATH + 1];
	wchar_t wideOutPath[MAX_PATH + 1];
	if (!convertUTF8ToWChar(path, widePath, MAX_PATH)) {
		CO_ERR("Could not convert path to widechar");
		return false;
	}
	//TODO utf8
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
