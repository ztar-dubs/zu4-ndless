/*
 * nspire_video.c - Video backend for TI-Nspire CX CAS
 * Replaces zu4's OpenGL/SDL2 video with SDL1 software rendering
 *
 * The TI-Nspire CX has a 320x240 16-bit (RGB565) display.
 * zu4's internal framebuffer is 320x200 RGBA32, which fits perfectly.
 * We blit the RGBA32 framebuffer to an SDL1 16-bit surface.
 */

#include <SDL/SDL.h>
#include <string.h>

#include "image.h"
#include "settings.h"
#include "error.h"

static SDL_Surface *sdl_screen = NULL;

/* Convert RGBA8888 pixel to RGB565 */
static inline unsigned short rgba_to_rgb565(unsigned int rgba) {
    unsigned char r = (rgba >>  0) & 0xFF;
    unsigned char g = (rgba >>  8) & 0xFF;
    unsigned char b = (rgba >> 16) & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

/* Blit the zu4 RGBA32 framebuffer to the SDL1 RGB565 surface */
void zu4_ogl_swap(void) {
    Image *screen = zu4_img_get_screen();
    if (!screen || !screen->pixels || !sdl_screen) return;

    unsigned int *src = (unsigned int *)screen->pixels;

    if (SDL_MUSTLOCK(sdl_screen)) SDL_LockSurface(sdl_screen);

    unsigned short *dst = (unsigned short *)sdl_screen->pixels;
    int dst_pitch = sdl_screen->pitch / 2; /* pitch in pixels (16-bit) */

    /* zu4 screen is 320x200, nspire is 320x240 */
    /* Center vertically: offset = (240 - 200) / 2 = 20 lines */
    int y_offset = (240 - SCREEN_HEIGHT) / 2;

    /* Clear the top and bottom borders */
    memset(dst, 0, y_offset * sdl_screen->pitch);
    memset(dst + (y_offset + SCREEN_HEIGHT) * dst_pitch, 0,
           (240 - y_offset - SCREEN_HEIGHT) * sdl_screen->pitch);

    /* Blit the framebuffer */
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        unsigned short *row = dst + (y + y_offset) * dst_pitch;
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            row[x] = rgba_to_rgb565(src[y * SCREEN_WIDTH + x]);
        }
    }

    if (SDL_MUSTLOCK(sdl_screen)) SDL_UnlockSurface(sdl_screen);

    SDL_Flip(sdl_screen);
}

void zu4_video_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        zu4_error(ZU4_LOG_ERR, "Unable to init SDL: %s", SDL_GetError());
        return;
    }

    sdl_screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
    if (!sdl_screen) {
        zu4_error(ZU4_LOG_ERR, "Unable to set video mode: %s", SDL_GetError());
        return;
    }

    SDL_ShowCursor(SDL_DISABLE);
}

void zu4_video_deinit(void) {
    /* SDL_Quit handles surface cleanup */
    SDL_Quit();
}
