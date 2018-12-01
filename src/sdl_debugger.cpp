#include "common.h"
struct DebuggerPlatformContext {
    SDL_Window *window;
    void *glContext;

    GLuint shaderID, vertexShaderID, fragmentShaderID;
    GLint attribLocationProjMtx, attribLocationPosition, attribLocationUV, attribLocationColor;
    GLint attribLocationTex;
    GLuint vboID, vaoID, elementsID;
    GLuint fontTexture;
    GameBoyDebug *gbDebug;
    //render thread data
    Thread *renderThread;
    volatile bool isRunning;
    
    Mutex *debuggerMutex;
    volatile bool shouldRender;
    WaitCondition *renderCondition;
};

static void renderDebugger(GameBoyDebug *gbDebug, DebuggerPlatformContext *platformContext) {

    SDL_GL_MakeCurrent(platformContext->window, platformContext->glContext);
    glClearColor(1.0f, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    foriarr (gbDebug->tiles) {
        auto tile = gbDebug->tiles[i];

        //TODO: might have to do this update out of this thread
        if (tile.needsUpdate) {
            u32 pixels[TILE_HEIGHT * DEFAULT_SCREEN_SCALE * TILE_WIDTH * DEFAULT_SCREEN_SCALE];
            for (i64 y = 0; y < TILE_HEIGHT * DEFAULT_SCREEN_SCALE; y+=DEFAULT_SCREEN_SCALE) {
                for (i64 x = 0; x < TILE_WIDTH * DEFAULT_SCREEN_SCALE; x+=DEFAULT_SCREEN_SCALE) {
                    u32 pixel;
                    switch ((PaletteColor)tile.pixels[(y/DEFAULT_SCREEN_SCALE)*TILE_WIDTH + (x/DEFAULT_SCREEN_SCALE)]) {
                    case PaletteColor::White: pixel = 0xFFFFFFFF; break;
                    case PaletteColor::LightGray: pixel = 0xFFAAAAAA; break;
                    case PaletteColor::DarkGray: pixel = 0xFF555555; break;
                    case PaletteColor::Black: pixel = 0xFF000000; break;
                    }

                    for (i64 y2 = 0; y2 < DEFAULT_SCREEN_SCALE; y2++) {
                        for (i64 x2 = 0; x2 < DEFAULT_SCREEN_SCALE; x2++) {
                            pixels[((y+y2)*TILE_WIDTH*DEFAULT_SCREEN_SCALE) + (x + x2)] = pixel;
                        }
                    }

                }
            }
            glBindTexture(GL_TEXTURE_2D, (GLuint)(u64)tile.textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TILE_WIDTH * DEFAULT_SCREEN_SCALE, TILE_HEIGHT * DEFAULT_SCREEN_SCALE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
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
    CO_ASSERT(glGetError() == GL_NO_ERROR);
}

static void renderDebuggerThread(void *arg) {
    auto platformContext = (DebuggerPlatformContext*)arg;
    auto gbDebug = platformContext->gbDebug;
    while (platformContext->isRunning) {
        lockMutex(platformContext->debuggerMutex);
        while (!platformContext->shouldRender) {
            waitForCondition(platformContext->renderCondition, platformContext->debuggerMutex);
        }
        platformContext->shouldRender = false;
        renderDebugger(gbDebug, platformContext);
        unlockMutex(platformContext->debuggerMutex);
        
    }
}
DebuggerPlatformContext *initDebugger(GameBoyDebug *gbDebug, ProgramState *programState, SDL_Window **outDebuggerWindow,
                                      int mainScreenX, int mainScreenY) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetSwapInterval(1);
    
    auto window = SDL_CreateWindow("Debugger", mainScreenX + 20, mainScreenY + 20,
                                   DEBUG_WINDOW_MIN_WIDTH, DEBUG_WINDOW_MIN_HEIGHT, 
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        ALERT("Could not create debugger window. Reason %s", SDL_GetError());
		return nullptr;
    }
    
    SDL_SetWindowMinimumSize(window, DEBUG_WINDOW_MIN_WIDTH, DEBUG_WINDOW_MIN_HEIGHT);

    auto glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    int initResult = gl3wInit();
    if (initResult != 0) {
		ALERT("Cannot init gl3w.  Error num: %d", initResult);
		SDL_DestroyWindow(window);
		return nullptr;
    } 

    *outDebuggerWindow = window;

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

    DebuggerPlatformContext *ret = CO_MALLOC(1, DebuggerPlatformContext);
    ret->shaderID = glCreateProgram();
    ret->vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    ret->fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(ret->vertexShaderID, 1, &vertex_shader, 0);
    glShaderSource(ret->fragmentShaderID, 1, &fragment_shader, 0);
    glCompileShader(ret->vertexShaderID);
    glCompileShader(ret->fragmentShaderID);
    glAttachShader(ret->shaderID, ret->vertexShaderID);
    glAttachShader(ret->shaderID, ret->fragmentShaderID);
    glLinkProgram(ret->shaderID);

    ret->attribLocationTex = glGetUniformLocation(ret->shaderID, "Texture");
    ret->attribLocationProjMtx = glGetUniformLocation(ret->shaderID, "ProjMtx");
    ret->attribLocationPosition = glGetAttribLocation(ret->shaderID, "Position");
    ret->attribLocationUV = glGetAttribLocation(ret->shaderID, "UV");
    ret->attribLocationColor = glGetAttribLocation(ret->shaderID, "Color");

    glGenBuffers(1, &ret->vboID);
    glGenBuffers(1, &ret->elementsID);

    glGenVertexArrays(1, &ret->vaoID);
    glBindVertexArray(ret->vaoID);
    glBindBuffer(GL_ARRAY_BUFFER, ret->vboID);
    glEnableVertexAttribArray((GLuint)ret->attribLocationPosition);
    glEnableVertexAttribArray((GLuint)ret->attribLocationUV);
    glEnableVertexAttribArray((GLuint)ret->attribLocationColor);

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    glVertexAttribPointer((GLuint)ret->attribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer((GLuint)ret->attribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer((GLuint)ret->attribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
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
        glGenTextures(1, &ret->fontTexture);
        glBindTexture(GL_TEXTURE_2D, ret->fontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        // Store our identifier
        io.Fonts->TexID = (void *)(intptr_t)ret->fontTexture;

    }



    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, (GLuint)last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_array_buffer);
    glBindVertexArray((GLuint)last_vertex_array);


    ret->window = window;
    ret->glContext = glContext;
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
    ret->gbDebug = gbDebug;
    SDL_GL_MakeCurrent(ret->window, nullptr);
    ret->isRunning = true;
	ret->debuggerMutex = createMutex();
	ret->renderCondition = createWaitCondition();
    ret->shouldRender = false;
    
    ret->renderThread = startThread(renderDebuggerThread, ret);

    return ret;
}
void newDebuggerFrame(DebuggerPlatformContext *platformContext) {
    lockMutex(platformContext->debuggerMutex);
    ImGui::NewFrame();
}

void closeDebugger(DebuggerPlatformContext *debuggerContext) {
    if (!debuggerContext->isRunning) {
        return;
    }
    debuggerContext->isRunning = false;
    debuggerContext->shouldRender = true; 
    broadcastCondition(debuggerContext->renderCondition);
    waitForAndFreeThread(debuggerContext->renderThread);
    ImGui::DestroyContext();

    if (debuggerContext->glContext) {
        SDL_GL_DeleteContext(debuggerContext->glContext);
    }
    if (debuggerContext->window) {
        SDL_DestroyWindow(debuggerContext->window);
    }
    if (debuggerContext->gbDebug) {
        debuggerContext->gbDebug->isEnabled = false;
    }
	destroyMutex(debuggerContext->debuggerMutex);
	destroyWaitCondition(debuggerContext->renderCondition);
    zeroMemory(debuggerContext, sizeof(*debuggerContext));

}

void signalRenderDebugger(DebuggerPlatformContext *platformContext) {
    platformContext->shouldRender = true; 
    broadcastCondition(platformContext->renderCondition);
    unlockMutex(platformContext->debuggerMutex);
}

