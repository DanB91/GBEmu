//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include "common.h"
#include "gbemu.h"
#include "debugger.h"

#pragma clang diagnostic push 
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "3rdparty/imgui_impl_metal.mm"
#pragma clang diagnostic pop

#import <SDL2/SDL.h>
#import <SDL2/SDL_syswm.h>
#import <simd/simd.h>
#import "metal_shader_defs.h"


struct MetalPlatformState : PlatformState {
    id<MTLRenderPipelineState> renderPipelineState;
    id<MTLTexture> screenTexture;
    id<MTLLibrary> library;
    id<MTLBuffer> vertexBuffer;
    id<MTLBuffer> viewportSizeBuffer;

    u32 screenPixels[SCREEN_HEIGHT*SCREEN_WIDTH];
};

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
struct DebuggerPlatformContext {
    SDL_Window *window;
    SDL_Renderer *renderer;
    id<MTLCommandQueue> commandQueue;
    MTLRenderPassDescriptor *renderPassDescriptor;
    id<MTLDrawable> currentDrawable;
    GameBoyDebug *gbDebug;
};

globalvar DebuggerPlatformContext g_DebuggerPlatformContext;

DebuggerPlatformContext *initDebugger(GameBoyDebug *gbDebug,ProgramState *programState,
                                      SDL_Window **outDebuggerWindow, int mainScreenX, int mainScreenY) {
    DebuggerPlatformContext *ret = &g_DebuggerPlatformContext;
    if (ret->window || ret->renderer || ret->commandQueue || ret->gbDebug) {
        closeDebugger(ret);
    }

    auto window = SDL_CreateWindow("Debugger", mainScreenX + 20, mainScreenY + 20,
                                   DEBUG_WINDOW_MIN_WIDTH, DEBUG_WINDOW_MIN_HEIGHT, 
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        ALERT("Could not create debugger window. Reason %s", SDL_GetError());
        return nullptr;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
    if (!renderer) {
        ALERT("Could not create debugger window. Reason %s", SDL_GetError());
        return nullptr;
    }
    *outDebuggerWindow = window;
    ret->window = window;
    ret->renderer = renderer;

    CAMetalLayer *layer = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(renderer);

    ret->commandQueue = [layer.device newCommandQueue];
    programState->guiContext = ImGui::CreateContext();

    ImGui_ImplMetal_Init(layer.device); 
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
    foriarr (gbDebug->tiles) {
        MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
        textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
        textureDescriptor.width = SCALED_TILE_WIDTH;
        textureDescriptor.height = SCALED_TILE_HEIGHT;
        textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;

        // Create the texture from the device by using the descriptor
        id<MTLTexture> texture = [layer.device newTextureWithDescriptor:textureDescriptor];

        auto tile = &gbDebug->tiles[i];
        tile->textureID = (void*)CFBridgingRetain(texture);
        tile->needsUpdate = true;
    }
    ret->gbDebug = gbDebug;
    return ret;
}

//Returns true if succeeded, else false
bool setFullScreen(SDL_Window *window, bool isFullScreen) {
    NSApplicationPresentationOptions opts = [NSApplication sharedApplication].presentationOptions;
    bool isActuallyFullScreen = (opts & NSApplicationPresentationFullScreen);
    if (isActuallyFullScreen != isFullScreen) {
        SDL_SysWMinfo wmInfo;
        SDL_GetVersion(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo)) {
            [wmInfo.info.cocoa.window toggleFullScreen:nil];
        }
    }
    
    return true;
}

void newDebuggerFrame(DebuggerPlatformContext *platformContext) {
    // Start the Dear ImGui frame
    CAMetalLayer *layer = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(platformContext->renderer);
    id<CAMetalDrawable> drawable = [layer nextDrawable];

    MTLRenderPassDescriptor *descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    descriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0, 0, 1.0);
    descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    descriptor.colorAttachments[0].texture = drawable.texture;

    ImGui_ImplMetal_NewFrame(descriptor);

    platformContext->renderPassDescriptor = descriptor;
    platformContext->currentDrawable = drawable;

    ImGui::NewFrame();
}

static void renderDebugger(DebuggerPlatformContext *platformContext) {
    auto gbDebug = platformContext->gbDebug;
    constexpr MTLRegion region = {
        { 0, 0, 0 },                   // MTLOrigin
        {SCALED_TILE_WIDTH, SCALED_TILE_HEIGHT, 1} // MTLSize
    };
    foriarr (gbDebug->tiles) {
        auto tile = gbDebug->tiles[i];

        //TODO: might have to do this update out of this thread
        if (tile.needsUpdate) {
            u32 pixels[SCALED_TILE_HEIGHT * SCALED_TILE_WIDTH];
            for (i64 y = 0; y < SCALED_TILE_HEIGHT; y+=DEFAULT_SCREEN_SCALE) {
                for (i64 x = 0; x < SCALED_TILE_WIDTH; x+=DEFAULT_SCREEN_SCALE) {
                    u32 pixel;
                    switch ((PaletteColor)tile.pixels[(y/DEFAULT_SCREEN_SCALE)*TILE_WIDTH + (x/DEFAULT_SCREEN_SCALE)]) {
                    case PaletteColor::White: pixel = 0xFFFFFFFF; break;
                    case PaletteColor::LightGray: pixel = 0xFFAAAAAA; break;
                    case PaletteColor::DarkGray: pixel = 0xFF555555; break;
                    case PaletteColor::Black: pixel = 0xFF000000; break;
                    }

                    for (i64 y2 = 0; y2 < DEFAULT_SCREEN_SCALE; y2++) {
                        for (i64 x2 = 0; x2 < DEFAULT_SCREEN_SCALE; x2++) {
                            pixels[((y+y2)*SCALED_TILE_WIDTH) + (x + x2)] = pixel;
                        }
                    }

                }
            }
            id<MTLTexture> texture = (__bridge id<MTLTexture>)tile.textureID;
            [texture replaceRegion:region
                                   mipmapLevel:0
                                   withBytes:pixels
                                   bytesPerRow:sizeof(pixels)/(TILE_HEIGHT * DEFAULT_SCREEN_SCALE)];
            tile.needsUpdate = false;
        }
    }
    id<MTLCommandBuffer> commandBuffer = [platformContext->commandQueue commandBuffer];

    id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor: 
            platformContext->renderPassDescriptor];
    ImDrawData *drawData = ImGui::GetDrawData();
    ImGui_ImplMetal_RenderDrawData(drawData, commandBuffer, commandEncoder);
    [commandEncoder endEncoding];

    [commandBuffer presentDrawable: platformContext->currentDrawable];
    [commandBuffer commit];
}

void closeDebugger(DebuggerPlatformContext *debuggerContext) {
    auto gbDebug = debuggerContext->gbDebug;
    if (!gbDebug || !gbDebug->isEnabled) {
        return;
    }
    ImGui::DestroyContext();
    if (debuggerContext->renderer) {
        SDL_DestroyRenderer(debuggerContext->renderer);
    }

    if (debuggerContext->window) {
        SDL_DestroyWindow(debuggerContext->window);
    }
    if (gbDebug) {
        gbDebug->isEnabled = false;
    }

    zeroMemory(debuggerContext, sizeof(*debuggerContext));
}

void signalRenderDebugger(DebuggerPlatformContext *platformContext) {
    renderDebugger(platformContext);
}


PlatformState *initPlatformState(SDL_Renderer *renderer, int windowW, int windowH) {
    NSError *error;
    CAMetalLayer *layer = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(renderer);
    id<MTLDevice> device = layer.device;
    id<MTLLibrary> library = [device newDefaultLibrary];
    if (!library) {
        ALERT_EXIT("Could not init Metal renderer.  Reason: Could not find shaders.");
        return nullptr;
    }

    vector_float2 viewportSize = {(float)windowW, (float)windowH};
    vector_float2 edge = {viewportSize.x/1, viewportSize.y/1};
    VertexData initialVertexData[6] = {{{-edge.x, -edge.y},{0., 1.}}, 
                                       {{-edge.x, edge.y},{0., 0.}}, 
                                       {{edge.x, edge.y},{1., 0.}}, 
                                       {{edge.x, -edge.y},{1., 1.}}, 
                                       {{-edge.x, -edge.y},{0., 1.}}, 
                                       {{edge.x, edge.y},{1., 0.}}};
    id<MTLBuffer> vertexBuffer = [device newBufferWithBytes:initialVertexData
            length:sizeof(initialVertexData)
            options:MTLResourceStorageModeShared];
    id<MTLBuffer> viewportBuffer = [device newBufferWithBytes:&viewportSize
            length:sizeof(viewportSize)
            options:MTLResourceStorageModeShared];

    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
    textureDescriptor.width = SCREEN_WIDTH;
    textureDescriptor.height = SCREEN_HEIGHT;
    textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;

    // Create the texture from the device by using the descriptor
    id<MTLTexture> texture = [device newTextureWithDescriptor:textureDescriptor];

    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"GBEmu Pipeline";
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = layer.pixelFormat;
    pipelineStateDescriptor.vertexFunction = [library newFunctionWithName:@"vertexShader"];
    pipelineStateDescriptor.fragmentFunction = [library newFunctionWithName:@"fragmentShader"];


    id<MTLRenderPipelineState> renderPipelineState = [device 
            newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
            error:&error];

    if (!renderPipelineState) {
        ALERT_EXIT("Could not init Metal renderer.  Reason: %s", 
                   [error.localizedDescription UTF8String]);
        return nullptr;
    }

    MetalPlatformState *ret = CO_CALLOC(1, MetalPlatformState);
    ret->screenTexture = texture;
    ret->renderPipelineState = renderPipelineState;
    ret->library = library;
    ret->vertexBuffer = vertexBuffer;
    ret->viewportSizeBuffer = viewportBuffer;
    return ret;
}

void windowResized(int w, int h, PlatformState *platformState) {
    MetalPlatformState *metalPlatformState = (MetalPlatformState *)platformState;
    int wRatio = w/SCREEN_WIDTH;
    int hRatio = h/SCREEN_HEIGHT;
    int ratio = MIN(wRatio, hRatio);
    
    vector_float2 edge = {(ratio * SCREEN_WIDTH)/1.f, (ratio * SCREEN_HEIGHT)/1.f};
    VertexData vertexData[6] = {{{-edge.x, -edge.y},{0., 1.}}, 
                                       {{-edge.x, edge.y},{0., 0.}}, 
                                       {{edge.x, edge.y},{1., 0.}}, 
                                       {{edge.x, -edge.y},{1., 1.}}, 
                                       {{-edge.x, -edge.y},{0., 1.}}, 
                                       {{edge.x, edge.y},{1., 0.}}};
    copyMemory(vertexData, metalPlatformState->vertexBuffer.contents, sizeof(vertexData));
    [metalPlatformState->vertexBuffer didModifyRange:(NSRange){0,sizeof(vertexData)}];
    vector_float2 viewport = {(float)w, (float)h};
    copyMemory(&viewport, metalPlatformState->viewportSizeBuffer.contents, sizeof(viewport));
    [metalPlatformState->viewportSizeBuffer didModifyRange:(NSRange){0,sizeof(viewport)}];
}

void renderMainScreen(SDL_Renderer *renderer, PlatformState *platformState, const PaletteColor *screen,
                      int windowW, int windowH) {
    MetalPlatformState *metalPlatformState = (MetalPlatformState *)platformState;
    constexpr MTLRegion region = {
        { 0, 0, 0 },                   // MTLOrigin
        {SCREEN_WIDTH, SCREEN_HEIGHT, 1} // MTLSize
    };

    fori (SCREEN_HEIGHT*SCREEN_WIDTH) {
        switch ((PaletteColor)screen[i]) {
        case PaletteColor::White: metalPlatformState->screenPixels[i] = 0xFFFFFFFF; break;
        case PaletteColor::LightGray: metalPlatformState->screenPixels[i] = 0xFFAAAAAA; break;
        case PaletteColor::DarkGray: metalPlatformState->screenPixels[i] = 0xFF555555; break;
        case PaletteColor::Black: metalPlatformState->screenPixels[i] = 0xFF000000; break;
        }
    }
    [metalPlatformState->screenTexture replaceRegion:region
                                                     mipmapLevel:0
                                                     withBytes:metalPlatformState->screenPixels
                                                     bytesPerRow:sizeof(metalPlatformState->screenPixels)/SCREEN_HEIGHT];

    id<MTLRenderCommandEncoder> renderEncoder = (__bridge id<MTLRenderCommandEncoder>)SDL_RenderGetMetalCommandEncoder(renderer);

    [renderEncoder setViewport:(MTLViewport){0.0, 0.0, (double)windowW, (double)windowH, -1.0, 1.0 }];
    [renderEncoder setVertexBuffer:metalPlatformState->vertexBuffer offset:0 atIndex:(int)VertexBufferIndex::ScreenVertices];
    [renderEncoder setVertexBuffer:metalPlatformState->viewportSizeBuffer offset:0 atIndex:(int)VertexBufferIndex::ViewportSize];
    [renderEncoder setFragmentTexture:metalPlatformState->screenTexture atIndex:0];
    [renderEncoder setRenderPipelineState:metalPlatformState->renderPipelineState];
    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
}
