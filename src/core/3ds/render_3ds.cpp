﻿/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2023 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <3ds.h>
#include <citro2d.h>

#include <set>

#include <Logger/logger.h>
#include "sdl_proxy/sdl_stdinc.h"

#include "globals.h"
#include "video.h"
#include "frame_timer.h"
#include "core/render.h"
#include "editor/new_editor.h"

#include "change_res.h"
#include "core/3ds/should_swap_screen.h"

#include "core/minport/render_minport_shared.h"

// #include "core/3ds/n3ds-clock.h"
// #include "second_screen.h"
#include "c2d_draw.h"


namespace XRender
{

uint32_t s_current_frame = 0;
float s_depth_slider = 0.;

bool g_in_frame = false;
bool g_screen_swapped = false;

C3D_RenderTarget* s_top_screen;
C3D_RenderTarget* s_right_screen;
C3D_RenderTarget* s_bottom_screen;

Tex3DS_SubTexture s_layer_subtexs[4];
C3D_Tex s_layer_texs[4];
C2D_Image s_layer_ims[4];
C3D_RenderTarget* s_layer_targets[4];
bool s_single_layer_mode = false;

int s_tex_w = 0;
int s_tex_show_w = 0;
int s_tex_h = 0;

int s_num_textures_loaded = 0;

C3D_RenderTarget* s_cur_target = nullptr;

static void s_destroySceneTargets()
{
    for(int i = 0; i < 4; i++)
    {
        if(s_layer_targets[i])
        {
            C3D_RenderTargetDelete(s_layer_targets[i]);
            C3D_TexDelete(&s_layer_texs[i]);
            s_layer_targets[i] = nullptr;
            s_layer_ims[i].tex = nullptr;
            s_layer_ims[i].subtex = nullptr;
        }
    }
}

static void s_createSceneTargets()
{
    s_destroySceneTargets();

    s_tex_w = ScreenW / 2;
    s_tex_h = ScreenH / 2;

    uint16_t mem_w = (s_tex_w <= 256) ? 256 : 512;
    uint16_t mem_h = (s_tex_h <= 256) ? 256 : 512;

    if(s_tex_w > 512)
        mem_w = 1024;

    if(s_tex_w > mem_w)
        s_tex_w = mem_w;

    if(s_tex_h > mem_h)
        s_tex_h = mem_h;

    if(mem_w >= 512 && mem_h == 512)
        s_single_layer_mode = true;
    else if(should_swap_screen())
        s_single_layer_mode = true;
    else
        s_single_layer_mode = false;

    for(int i = 0; i < 4; i++)
    {
        C3D_TexInitVRAM(&s_layer_texs[i], mem_w, mem_h, GPU_RGBA8);
        s_layer_targets[i] = C3D_RenderTargetCreateFromTex(&s_layer_texs[i], GPU_TEXFACE_2D, 0, GPU_RB_DEPTH24_STENCIL8);
        s_layer_subtexs[i] = {(uint16_t)s_tex_w, (uint16_t)s_tex_h, 0.0, 1.0, (float)((double)s_tex_w / (double)mem_w), 1.0f - (float)((double)s_tex_h / (double)mem_h)};
        s_layer_ims[i].tex = &s_layer_texs[i];
        s_layer_ims[i].subtex = &s_layer_subtexs[i];

        if(s_single_layer_mode)
            break;
    }

    // s_tex_show_w = s_tex_w - MAX_3D_OFFSET;
    s_tex_show_w = s_tex_w;
}

void s_ensureInFrame()
{
    if(!g_in_frame)
    {
        minport_initFrame();

        // if(g_screen_swapped != should_swap_screen())
        //     UpdateInternalRes();

        C3D_FrameBegin(0);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_TargetClear(s_layer_targets[layer], C2D_Color32(0, 0, 0, 0));

            if(s_single_layer_mode)
                break;
        }

        C2D_TargetClear(s_bottom_screen, C2D_Color32(0, 0, 0, 0));

        g_in_frame = true;
    }
}

void s_clearAllTextures()
{
    for(StdPicture* p = g_render_chain_tail; p != nullptr; p = p->l.next_texture)
        deleteTexture(*p);
}

void s_loadTexture(StdPicture& target, C2D_SpriteSheet& sheet)
{
    C2D_Image im = C2D_SpriteSheetGetImage(sheet, 0);

    target.d.texture = sheet;
    target.d.image = im;

    if(!target.w)
    {
        target.w = im.subtex->width * 2;
        target.h = im.subtex->height * 2;
    }
}

void s_loadTexture2(StdPicture& target, C2D_SpriteSheet& sheet)
{
    C2D_Image im = C2D_SpriteSheetGetImage(sheet, 0);

    target.d.texture2 = sheet;
    target.d.image2 = im;
}

void s_loadTexture3(StdPicture& target, C2D_SpriteSheet& sheet)
{
    C2D_Image im = C2D_SpriteSheetGetImage(sheet, 0);

    target.d.texture3 = sheet;
    target.d.image3 = im;
}

bool init()
{
    // 3ds libs
    gfxInitDefault();

    gfxSet3D(true); // Enable stereoscopic 3D

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C2D_SetTintMode(C2D_TintMult);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

    // consoleInit(GFX_BOTTOM, NULL);

    s_top_screen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    s_right_screen = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    s_bottom_screen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    // s_bottom_screen = s_right_screen;

    updateViewport();

    return true;
}

void quit()
{
    s_clearAllTextures();
    s_destroySceneTargets();

    // 3ds libs
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    aptExit();
}

void setTargetTexture()
{
    s_ensureInFrame();

    if(!s_single_layer_mode)
    {
        // screen plane layer
        C2D_SceneBegin(s_layer_targets[2]);
        s_cur_target = s_layer_targets[2];
    }
    else
    {
        C2D_SceneBegin(s_layer_targets[0]);
        s_cur_target = s_layer_targets[0];
    }

    C2D_ViewReset();
}

void setTargetScreen()
{
}

void setTargetMainScreen()
{
    s_ensureInFrame();

    C2D_TargetClear(s_top_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));

    C2D_SceneBegin(s_top_screen);
    s_cur_target = s_top_screen;

    C2D_ViewReset();
}

void setTargetSubScreen()
{
    s_ensureInFrame();

    C2D_TargetClear(s_bottom_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));

    C2D_SceneBegin(s_bottom_screen);
    s_cur_target = s_bottom_screen;

    C2D_ViewReset();
}

void setTargetLayer(int layer)
{
    s_ensureInFrame();

    if(!s_single_layer_mode)
    {
        C2D_SceneBegin(s_layer_targets[layer]);
        s_cur_target = s_layer_targets[layer];
    }
    else
    {
        C2D_SceneBegin(s_layer_targets[0]);
        s_cur_target = s_layer_targets[0];
    }

    minport_ApplyViewport();
}

void clearBuffer()
{
    if(!g_in_frame)
    {
        C3D_FrameBegin(0);
        C2D_ViewReset();

        C2D_TargetClear(s_top_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_top_screen);
        renderRect(0, 0, 400, 240, 0.0f, 0.0f, 0.0f, 1.0f, true);

        C2D_TargetClear(s_right_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_right_screen);
        renderRect(0, 0, 400, 240, 0.0f, 0.0f, 0.0f, 1.0f, true);

        C2D_TargetClear(s_bottom_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_bottom_screen);
        renderRect(0, 0, 320, 240, 0.0f, 0.0f, 0.0f, 1.0f, true);

        C3D_FrameEnd(0);
    }
}

void repaint()
{
    if(!g_in_frame)
        return;

    constexpr int shift = 0;
    // constexpr int shift = MAX_3D_OFFSET / 2;
    constexpr double shift_i[] = {shift, shift * 0.4, 0, shift * -0.4};

    s_depth_slider = osGet3DSliderState();

    s_cur_target = nullptr;
    C2D_ViewReset();

    // in this case, the level graphics have already been rescaled to the bottom screen
    if(g_screen_swapped && (LevelEditor || MagicHand) && editorScreen.active)
    {
        C2D_TargetClear(s_top_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_top_screen);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_DrawImage_Custom(s_layer_ims[layer],
                                 g_screen_phys_x + 40, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h,
                                 shift, 0, s_tex_show_w, s_tex_h,
                                 X_FLIP_NONE, 1.0f, 1.0f, 1.0f, 1.0f);

            if(s_single_layer_mode)
                break;
        }
    }
    else if(g_screen_swapped)
    {
        C2D_TargetClear(s_bottom_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_bottom_screen);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_DrawImage_Custom(s_layer_ims[layer],
                                 g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h,
                                 shift, 0, s_tex_show_w, s_tex_h,
                                 X_FLIP_NONE, 1.0f, 1.0f, 1.0f, 1.0f);

            if(s_single_layer_mode)
                break;
        }
    }
    // normally in editor mode, just center the level graphics
    else if(LevelEditor && !editorScreen.active)
    {
        C2D_TargetClear(s_bottom_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_bottom_screen);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_DrawImage_Custom(s_layer_ims[layer],
                                 g_screen_phys_x - 40, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h,
                                 shift, 0, s_tex_show_w, s_tex_h,
                                 X_FLIP_NONE, 1.0f, 1.0f, 1.0f, 1.0f);

            if(s_single_layer_mode)
                break;
        }
    }
    else if(s_depth_slider <= 0.05 || s_single_layer_mode)
    {
        C2D_TargetClear(s_top_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_top_screen);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_DrawImage_Custom(s_layer_ims[layer],
                                 g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h,
                                 shift, 0, s_tex_show_w, s_tex_h,
                                 X_FLIP_NONE, 1.0f, 1.0f, 1.0f, 1.0f);

            if(s_single_layer_mode)
                break;
        }
    }
    else
    {
        C2D_TargetClear(s_top_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_top_screen);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_DrawImage_Custom(s_layer_ims[layer],
                                 g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h,
                                 shift + (int)(shift_i[layer] * s_depth_slider), 0, s_tex_show_w, s_tex_h,
                                 X_FLIP_NONE, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        C2D_TargetClear(s_right_screen, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
        C2D_SceneBegin(s_right_screen);

        for(int layer = 0; layer < 4; layer++)
        {
            C2D_DrawImage_Custom(s_layer_ims[layer],
                                 g_screen_phys_x, g_screen_phys_y, g_screen_phys_w, g_screen_phys_h,
                                 shift - (int)(shift_i[layer] * s_depth_slider), 0, s_tex_show_w, s_tex_h,
                                 X_FLIP_NONE, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    s_current_frame ++;
    g_in_frame = false;

    // leave the draw context and wait for vblank...
    g_microStats.start_sleep();
    // if(g_videoSettings.renderMode == RENDER_ACCELERATED_VSYNC)
    //     C3D_FrameSync();

    g_microStats.start_task(MicroStats::Graphics);
    C3D_FrameEnd(0);
}

void mapToScreen(int x, int y, int* dx, int* dy)
{
    // lower screen to upper screen conversion
    if((LevelEditor || MagicHand) && editorScreen.active)
    {
        *dx = x * 2;
        *dy = y * 2;
        return;
    }

    if(!g_screen_swapped)
        x += 40;

    *dx = (x - g_screen_phys_x) * ScreenW / g_screen_phys_w;
    *dy = (y - g_screen_phys_y) * ScreenH / g_screen_phys_h;
}

void mapFromScreen(int scr_x, int scr_y, int* window_x, int* window_y)
{
    // lower screen to upper screen conversion
    if((LevelEditor || MagicHand) && editorScreen.active)
    {
        *window_x = scr_x / 2;
        *window_y = scr_y / 2;
        return;
    }

    *window_x = (scr_x * g_screen_phys_w / ScreenW) + g_screen_phys_x;
    *window_y = (scr_y * g_screen_phys_h / ScreenH) + g_screen_phys_y;

    if(!g_screen_swapped)
        *window_x -= 40;
}

void minport_TransformPhysCoords() {}

void minport_ApplyPhysCoords()
{
    int tex_w = ScreenW / 2;
    int tex_h = ScreenH / 2;

    if(tex_w != s_tex_w || tex_h != s_tex_h || g_screen_swapped != should_swap_screen())
        s_createSceneTargets();

    g_screen_swapped = should_swap_screen();

    GPU_TEXTURE_FILTER_PARAM filter =  GPU_LINEAR;

    for(int layer = 0; layer < 4; layer++)
    {
        C3D_TexSetFilter(&s_layer_texs[layer], filter, filter);

        if(s_single_layer_mode)
            break;
    }
}

void minport_ApplyViewport()
{
    if(!g_in_frame)
        return;

    C2D_Flush();

    C2D_ViewReset();

    if(!g_viewport_offset_ignore)
        C2D_ViewTranslate(g_viewport_offset_x, g_viewport_offset_y);

    if(s_cur_target)
    {
        int hw_viewport_w, hw_viewport_h;
        int hw_viewport_x, hw_viewport_y;

        // rotated 90deg clockwise
        if(s_cur_target->linked)
        {
            hw_viewport_x = SDL_min(SDL_max(g_viewport_y, 0), s_cur_target->frameBuf.width - 1);
            hw_viewport_y = SDL_min(SDL_max(g_viewport_x, 0), s_cur_target->frameBuf.height - 1);
            hw_viewport_w = SDL_min(s_cur_target->frameBuf.width - hw_viewport_x, g_viewport_h);
            hw_viewport_h = SDL_min(s_cur_target->frameBuf.height - hw_viewport_y, g_viewport_w);
        }
        else
        {
            hw_viewport_x = SDL_min(SDL_max(g_viewport_x, 0), s_cur_target->frameBuf.width - 1);
            hw_viewport_y = SDL_min(SDL_max(g_viewport_y, 0), s_cur_target->frameBuf.height - 1);
            hw_viewport_w = SDL_min(s_cur_target->frameBuf.width - hw_viewport_x, g_viewport_w);
            hw_viewport_h = SDL_min(s_cur_target->frameBuf.height - hw_viewport_y, g_viewport_h);
        }

        if(s_cur_target->linked)
        {
            C3D_SetViewport(s_cur_target->frameBuf.width - hw_viewport_x - hw_viewport_w, hw_viewport_y, hw_viewport_w, hw_viewport_h);
            C2D_SceneSize(g_viewport_h, g_viewport_w, s_cur_target->linked);
        }
        else
        {
            C3D_SetViewport(hw_viewport_x, s_cur_target->frameBuf.height - hw_viewport_y - hw_viewport_h, hw_viewport_w, hw_viewport_h);
            C2D_SceneSize(g_viewport_w, g_viewport_h, s_cur_target->linked);
        }
    }
}

StdPicture LoadPicture(const std::string& path, const std::string& maskPath, const std::string& maskFallbackPath)
{
    (void)maskPath;
    (void)maskFallbackPath;

    StdPicture target;
    C2D_SpriteSheet sourceImage;

    if(!GameIsActive)
        return target; // do nothing when game is closed

    target.inited = false;
    target.l.path = path;

    if(target.l.path.empty())
        return target;

    target.inited = true;
    target.l.lazyLoaded = false;

    sourceImage = C2D_SpriteSheetLoad(target.l.path.c_str());

    if(sourceImage)
    {
        // Consider using https://github.com/xerpi/sf2dlib/blob/master/libsf2d/source/sf2d_texture.c to get some more advanced tex loads
        s_loadTexture(target, sourceImage);

        s_num_textures_loaded ++;
    }

    if(!target.d.texture)
    {
        pLogWarning("FAILED TO LOAD!!! %s\n", path.c_str());
        target.inited = false;
    }

    return target;
}

StdPicture LoadPicture_1x(const std::string& path, const std::string& maskPath, const std::string& maskFallbackPath)
{
    // The asset converter should have known not to downscale this image. Let's hope it was right.
    return LoadPicture(path, maskPath, maskFallbackPath);
}

StdPicture lazyLoadPictureFromList(FILE* f, const std::string& dir)
{
    StdPicture target;

    if(!GameIsActive)
        return target; // do nothing when game is closed

    int length;

    char filename[256];

    if(fscanf(f, "%255[^\n]%n%*[^\n]\n", filename, &length) != 1)
    {
        pLogWarning("Could not load image path from load list");
        return target;
    }

    if(length == 255)
    {
        pLogWarning("Image path %s was truncated in load list", filename);
        return target;
    }

    target.inited = true;
    target.l.path = dir;
    target.l.path += filename;
    target.l.lazyLoaded = true;

    int w, h;

    if(fscanf(f, "%d\n%d\n", &w, &h) != 2 || w < 0 || w > 8192 || h < 0 || h > 8192)
    {
        pLogWarning("Could not load image %s dimensions from load list", filename);
        target.inited = false;
        return target;
    }

    // pLogDebug("Successfully loaded %s (%d %d)", target.l.path.c_str(), w, h);

    target.w = w;
    target.h = h;

    return target;
}


StdPicture lazyLoadPicture(const std::string& path, const std::string& maskPath, const std::string& maskFallbackPath)
{
    (void)maskPath;
    (void)maskFallbackPath;

    StdPicture target;

    if(!GameIsActive)
        return target; // do nothing when game is closed

    target.inited = false;
    target.l.path = path;

    if(target.l.path.empty())
        return target;

    target.inited = true;

    target.l.lazyLoaded = true;

    // We need to figure out the height and width!
    std::string sizePath = path + ".size";
    FILE* fs = fopen(sizePath.c_str(), "r");

    // NOT null-terminated: wwww\nhhhh\n
    char contents[10];

    if(fs != nullptr)
    {
        fread(&contents[0], 1, 10, fs);
        contents[4] = '\0';
        contents[9] = '\0';
        target.w = atoi(&contents[0]);
        target.h = atoi(&contents[5]);

        if(fclose(fs))
            pLogWarning("lazyLoadPicture: Couldn't close file.");
    }
    // lazy load and unload to read dimensions if it doesn't exist.
    // unload is essential because lazy load would save the address incorrectly.
    else
    {
        pLogWarning("lazyLoadPicture: Couldn't open size file.");
        lazyLoad(target);
        lazyUnLoad(target);
    }

    return target;
}

static C2D_SpriteSheet s_tryHardToLoadC2D_SpriteSheet(const char* path)
{
    C2D_SpriteSheet sourceImage = C2D_SpriteSheetLoad(path);

    if(!sourceImage)
    {
        if(linearSpaceFree() < 4000000)
            minport_freeTextureMemory();

        sourceImage = C2D_SpriteSheetLoad(path);
    }

    return sourceImage;
}

void lazyLoad(StdPicture& target)
{
    if(!target.inited || !target.l.lazyLoaded || target.d.texture)
        return;

    C2D_SpriteSheet sourceImage;
    std::string suppPath;

    sourceImage = s_tryHardToLoadC2D_SpriteSheet(target.l.path.c_str()); // some other source image

    if(!sourceImage)
    {
        pLogWarning("Permanently failed to load %s, %lu free", target.l.path.c_str(), linearSpaceFree());
        pLogWarning("Error: %d (%s)", errno, strerror(errno));
        target.inited = false;
        return;
    }

    s_loadTexture(target, sourceImage);

    if(target.h > 2048)
    {
        suppPath = target.l.path + '1';
        sourceImage = s_tryHardToLoadC2D_SpriteSheet(suppPath.c_str());

        if(!sourceImage)
        {
            pLogWarning("Permanently failed to load %s, %lu free", suppPath.c_str(), linearSpaceFree());
            pLogWarning("Error: %d (%s)", errno, strerror(errno));
        }
        else
            s_loadTexture2(target, sourceImage);
    }

    if(target.h > 4096)
    {
        suppPath = target.l.path + '2';
        sourceImage = C2D_SpriteSheetLoad(suppPath.c_str());

        if(!sourceImage)
        {
            pLogWarning("Permanently failed to load %s, %lu free", suppPath.c_str(), linearSpaceFree());
            pLogWarning("Error: %d (%s)", errno, strerror(errno));
        }
        else
            s_loadTexture3(target, sourceImage);
    }

    s_num_textures_loaded++;

    if(linearSpaceFree() < 4194304)
    {
        pLogDebug("Triggering free texture memory due to low memory (%u bytes)", (unsigned)linearSpaceFree());
        minport_freeTextureMemory();
    }
}

void lazyPreLoad(StdPicture& target)
{
    lazyLoad(target);
}

void lazyUnLoad(StdPicture& target)
{
    if(!target.inited || !target.l.lazyLoaded || !target.d.texture)
        return;

    deleteTexture(target, true);
}

void deleteTexture(StdPicture& tx, bool lazyUnload)
{
    if(!tx.inited)
        return;

    minport_unlinkTexture(&tx);

    if(tx.d.texture)
    {
        s_num_textures_loaded --;
        C2D_SpriteSheetFree(tx.d.texture);
        tx.d.texture = nullptr;
    }

    if(tx.d.texture2)
    {
        C2D_SpriteSheetFree(tx.d.texture2);
        tx.d.texture2 = nullptr;
    }

    if(tx.d.texture3)
    {
        C2D_SpriteSheetFree(tx.d.texture3);
        tx.d.texture3 = nullptr;
    }

    if(!lazyUnload)
    {
        tx.inited = false;
        tx.l.lazyLoaded = false;
        tx.w = 0;
        tx.h = 0;
        tx.frame_w = 0;
        tx.frame_h = 0;
    }
}

void minport_RenderBoxFilled(int x1, int y1, int x2, int y2, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    uint32_t clr = C2D_Color32(red, green, blue, alpha);

    C2D_DrawRectSolid(x1, y1, 0, x2 - x1, y2 - y1, clr);
}

void minport_RenderTexturePrivate(int16_t xDst, int16_t yDst, int16_t wDst, int16_t hDst,
                                  StdPicture& tx,
                                  int16_t xSrc, int16_t ySrc, int16_t wSrc, int16_t hSrc,
                                  float rotateAngle, FPoint_t* center, unsigned int flip,
                                  float red, float green, float blue, float alpha)
{
    if(!tx.inited)
        return;

    if(!tx.d.texture && tx.l.lazyLoaded)
        lazyLoad(tx);

    if(!tx.d.texture)
        return;

    // automatic flipping based on SMBX style!
    unsigned int mode = 0;

    while(ySrc >= tx.h / 2 && mode < 3)
    {
        ySrc -= tx.h / 2;
        mode += 1;
    }

    flip ^= mode;

    C3D_Mtx prev_view;

    // handle rotation NOW
    if(rotateAngle)
    {
        int16_t cx, cy;

        if(center)
        {
            cx = center->x / 2.0f + 0.5f;
            cy = center->y / 2.0f + 0.5f;
        }
        else
        {
            cx = wDst / 2;
            cy = hDst / 2;
        }

        C2D_ViewSave(&prev_view);

        C2D_ViewTranslate(xDst + cx, yDst + cy);
        C2D_ViewRotateDegrees(rotateAngle);

        xDst = -cx;
        yDst = -cy;
    }

    // texture boundaries
    // this never happens unless there was an invalid input
    // if((xSrc < 0.0f) || (ySrc < 0.0f)) return;

    // TODO: graphics tests for how offscreen draws interact with flips
    //       handling rotations properly is probably impossible

    C2D_Image* to_draw = nullptr;
    C2D_Image* to_draw_2 = nullptr;

    // Don't go more than size of texture
    // Failure conditions should only happen if texture is smaller than expected

    if(ySrc + hSrc > 1024)
    {
        if(ySrc + hSrc > 2048)
        {
            if(tx.d.texture3)
                to_draw = &tx.d.image3;

            if(ySrc < 2048 && tx.d.texture2)
                to_draw_2 = &tx.d.image2;

            ySrc -= 1024;
        }
        else
        {
            if(tx.d.texture2)
                to_draw = &tx.d.image2;

            if(ySrc < 1024)
                to_draw_2 = &tx.d.image;
        }

        // draw the top pic
        if(to_draw_2 != nullptr)
        {
            C2D_DrawImage_Custom(*to_draw_2, xDst, yDst, wDst, (1024 - ySrc) * hDst / hSrc,
                                 xSrc, ySrc, wSrc, 1024 - ySrc, flip, red, green, blue, alpha);
            yDst += (1024 - ySrc) * hDst / hSrc;
            hDst -= (1024 - ySrc) * hDst / hSrc;
            hSrc -= (1024 - ySrc);
            ySrc = 0.0f;
        }
        else
            ySrc -= 1024.0f;
    }
    else to_draw = &tx.d.image;

    if(to_draw != nullptr)
    {
        C2D_DrawImage_Custom(*to_draw, xDst, yDst, wDst, hDst,
                             xSrc, ySrc, wSrc, hSrc, flip, red, green, blue, alpha);
    }

    // Finalize rotation HERE
    if(rotateAngle)
        C2D_ViewRestore(&prev_view);
}

} // namespace XRender
