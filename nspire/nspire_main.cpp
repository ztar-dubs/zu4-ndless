/*
 * nspire_main.cpp - Entry point for Ultima IV on TI-Nspire CX CAS
 * Replaces src/src/u4.cpp with Ndless-specific initialization
 */

#include <cstring>
#include <cstdio>
#include <unistd.h>

#ifdef NSPIRE
#include <os.h>
#include <libndls.h>
/* Ndless redefines sleep as a broken macro - undefine it */
#ifdef sleep
#undef sleep
#endif
#endif

#include "u4.h"
#include "config.h"
#include "creature.h"
#include "error.h"
#include "game.h"
#include "image.h"
#include "intro.h"
#include "music.h"
#include "person.h"
#include "progress_bar.h"
#include "random.h"
#include "screen.h"
#include "settings.h"
#include "sound.h"
#include "u4file.h"

bool verbose = false;
bool quit = false;
bool useProfile = false;
std::string profileName = "";

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#ifdef NSPIRE
    if (!has_colors) {
        printf("Ultima IV requires TI-Nspire CX (color screen)\n");
        return 1;
    }
#endif

    /* Change working directory to the folder containing the .tns executable */
    if (argc > 0 && argv[0]) {
        char progdir[256];
        strncpy(progdir, argv[0], sizeof(progdir) - 1);
        progdir[sizeof(progdir) - 1] = '\0';
        char *last_slash = strrchr(progdir, '/');
        if (last_slash) {
            *last_slash = '\0';
            chdir(progdir);
        }
    }

    /* Check for game data */
    {
        U4FILE *check = u4fopen("AVATAR.EXE");
        if (!check) {
            zu4_error(ZU4_LOG_ERR,
                "Ultima IV requires the PC version game data.\n"
                "Place ultima4.zip in the same directory as this program.\n");
        }
        u4fclose(check);
    }

    /* Initialize settings with defaults */
    zu4_settings_init(false, "");

    /* Override settings for TI-Nspire */
    settings.scale = 1;
    settings.fullscreen = 1;
    settings.musicVol = 0;
    settings.soundVol = 0;
    settings.screenShakes = 0;
    settings.validateXml = 0;

    zu4_srandom();

    screenInit();

    ProgressBar pb((320/2) - (200/2), (200/2), 200, 10, 0, 7);
    pb.setBorderColor(240, 240, 240);
    pb.setColor(0, 0, 128);
    pb.setBorderWidth(1);

    screenTextAt(15, 11, "Loading...");
    ++pb;

    zu4_music_init();
    zu4_snd_init();
    ++pb;

    Tileset::loadAll();
    ++pb;

    creatureMgr->getInstance();
    ++pb;

    intro = new IntroController();
    ++pb;
    intro->init();
    intro->preloadMap();
    ++pb;

    /* Clear progress bar before intro event loop */
    zu4_img_fill(zu4_img_get_screen(), 0, 0, 320, 200, 0, 0, 0, 255);
    intro->updateScreen();

    eventHandler->pushController(intro);
    eventHandler->run();
    eventHandler->popController();
    intro->deleteIntro();

    eventHandler->setControllerDone(false);

    if (quit) {
        delete intro;
        intro = NULL;
        Tileset::unloadAll();
        CreatureMgr::destroy();
        zu4_snd_deinit();
        zu4_music_deinit();
        screenDelete();
        Config::destroy();
        return 0;
    }

    /* Play the game! */
    game = new GameController();
    game->init();

    eventHandler->pushController(game);
    eventHandler->run();
    eventHandler->popController();

    game->deinit();
    delete game;
    game = NULL;

    delete intro;
    intro = NULL;

    Tileset::unloadAll();
    CreatureMgr::destroy();

    zu4_snd_deinit();
    zu4_music_deinit();
    screenDelete();
    Config::destroy();

    return 0;
}
