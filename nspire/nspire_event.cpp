/*
 * nspire_event.cpp - Event handling for TI-Nspire CX CAS
 * Replaces zu4's SDL2 event handling with SDL1 + Ndless key polling
 *
 * This file replaces src/src/event_sdl.cpp entirely.
 * It adapts SDL2 event APIs to SDL1, and maps TI-Nspire keys
 * to Ultima IV commands.
 */

#include <SDL/SDL.h>

/* Ndless redefines sleep as a broken macro - undefine it */
#ifdef sleep
#undef sleep
#endif

#ifdef NSPIRE
#include <os.h>
#include <libndls.h>
#ifdef sleep
#undef sleep
#endif
#endif

#include "event.h"
#include "context.h"
#include "error.h"
#include "video.h"

extern bool verbose, quit;
extern int eventTimerGranularity;

/* Forward declaration */
extern "C" void zu4_ogl_swap(void);

KeyHandler::KeyHandler(Callback func, void *d, bool asyncronous) :
    handler(func),
    async(asyncronous),
    data(d)
{}

int KeyHandler::setKeyRepeat(int delay, int interval) {
    return SDL_EnableKeyRepeat(delay, interval);
}

bool KeyHandler::globalHandler(int key) {
    switch(key) {
    case U4_ALT + 'x': /* Alt+x = quit */
        quit = true;
        EventHandler::end();
        return true;
    default: return false;
    }
}

bool KeyHandler::defaultHandler(int key, void *data) {
    bool valid = true;
    switch (key) {
    case '`':
        if (c && c->location)
            printf("x = %d, y = %d, level = %d\n",
                   c->location->coords.x, c->location->coords.y,
                   c->location->coords.z);
        break;
    default:
        valid = false;
        break;
    }
    return valid;
}

bool KeyHandler::ignoreKeys(int key, void *data) {
    return true;
}

bool KeyHandler::handle(int key) {
    bool processed = false;
    if (!isKeyIgnored(key)) {
        processed = globalHandler(key);
        if (!processed)
            processed = handler(key, data);
    }
    return processed;
}

bool KeyHandler::isKeyIgnored(int key) {
    switch(key) {
    case U4_RIGHT_SHIFT:
    case U4_LEFT_SHIFT:
    case U4_RIGHT_CTRL:
    case U4_LEFT_CTRL:
    case U4_RIGHT_ALT:
    case U4_LEFT_ALT:
    case U4_RIGHT_META:
    case U4_LEFT_META:
    case U4_TAB:
        return true;
    default: return false;
    }
}

bool KeyHandler::operator==(Callback cb) const {
    return (handler == cb) ? true : false;
}

KeyHandlerController::KeyHandlerController(KeyHandler *handler) {
    this->handler = handler;
}

KeyHandlerController::~KeyHandlerController() {
    delete handler;
}

bool KeyHandlerController::keyPressed(int key) {
    zu4_assert(handler != NULL, "key handler must be initialized");
    return handler->handle(key);
}

KeyHandler *KeyHandlerController::getKeyHandler() {
    return handler;
}

/* Timer management using polling (nSDL has no thread support) */
TimedEventMgr::TimedEventMgr(int i) : baseInterval(i), locked(false) {
    instances++;
    lastTick = SDL_GetTicks();
    running = true;
}

TimedEventMgr::~TimedEventMgr() {
    running = false;
    if (instances > 0)
        instances--;
}

/* Called from the event loop to check if the timer has fired */
void TimedEventMgr::poll() {
    if (!running) return;
    Uint32 now = SDL_GetTicks();
    if (now - lastTick >= (Uint32)baseInterval) {
        lastTick = now;
        tick();
    }
}

Uint32 TimedEventMgr::callback(Uint32 interval, void *param) {
    /* Not used in polling mode, but kept for compatibility */
    (void)interval; (void)param;
    return 0;
}

void TimedEventMgr::reset(unsigned int interval) {
    baseInterval = interval;
    lastTick = SDL_GetTicks();
    running = true;
}

void TimedEventMgr::stop() {
    running = false;
}

void TimedEventMgr::start() {
    if (!running) {
        lastTick = SDL_GetTicks();
        running = true;
    }
}

EventHandler::EventHandler() : timer(eventTimerGranularity), updateScreen(NULL) {
}

static void handleKeyDownEvent(const SDL_Event &event, Controller *controller, updateScreenCallback updateScreen) {
    int processed;
    int key;

    key = event.key.keysym.sym;

    /* TI-Nspire has no Alt key: map Shift to Alt for Alt+letter combos */
#ifdef NSPIRE
    if (event.key.keysym.mod & KMOD_SHIFT)
        key += U4_ALT;
#else
    if (event.key.keysym.mod & KMOD_ALT)
        key += U4_ALT;
    /* Handle shifted letters for capitals */
    if ((event.key.keysym.mod & KMOD_SHIFT)
        && event.key.keysym.sym >= SDLK_a
        && event.key.keysym.sym <= SDLK_z)
        key -= 0x20;
#endif
    if (event.key.keysym.mod & KMOD_CTRL)
        key += U4_CTRL;
    if (event.key.keysym.mod & KMOD_META)
        key += U4_META;

    switch (event.key.keysym.sym) {
        case SDLK_UP: key = U4_UP; break;
        case SDLK_DOWN: key = U4_DOWN; break;
        case SDLK_LEFT: key = U4_LEFT; break;
        case SDLK_RIGHT: key = U4_RIGHT; break;
        case SDLK_BACKSPACE: case SDLK_DELETE: key = U4_BACKSPACE; break;
#ifdef NSPIRE
        case SDLK_8: key = U4_UP; break;
        case SDLK_2: key = U4_DOWN; break;
        case SDLK_4: key = U4_LEFT; break;
        case SDLK_6: key = U4_RIGHT; break;
#endif
        default: break;
    }

    processed = controller->notifyKeyPressed(key);

    if (processed) {
        if (updateScreen)
            (*updateScreen)();
    }
}

void EventHandler::sleep(unsigned int usec) {
    Uint32 endTime = SDL_GetTicks() + usec;

    while (SDL_GetTicks() < endTime) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            default: break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                break; /* discard */
            case SDL_QUIT:
                ::exit(0);
                break;
            }
        }
        /* Keep polling the timer for animations */
        eventHandler->getTimer()->poll();
        zu4_ogl_swap();
        SDL_Delay(10);
    }
}

static int runLoopCount = 0;
void EventHandler::run() {
    if (updateScreen)
        (*updateScreen)();

    runLoopCount = 0;
    while (!ended && !controllerDone) {
        SDL_Event event;

#ifdef NSPIRE
        /* ON+ESC quit combo (standard Ndless convention) */
        if (on_key_pressed() && isKeyPressed(KEY_NSPIRE_ESC)) {
            quit = true;
            end();
            break;
        }
#endif

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            default: break;
            case SDL_KEYDOWN:
                handleKeyDownEvent(event, getController(), updateScreen);
                break;
            case SDL_QUIT:
                ::exit(0);
                break;
            }
        }

        /* Poll timer (replaces SDL_AddTimer thread-based approach) */
        eventHandler->getTimer()->poll();

        zu4_ogl_swap();

        /* Small delay to avoid consuming too much CPU */
        SDL_Delay(10);
        runLoopCount++;
    }
}

void EventHandler::setScreenUpdate(void (*updateScreen)(void)) {
    this->updateScreen = updateScreen;
}

bool EventHandler::timerQueueEmpty() {
    SDL_Event event;
    /* SDL1 PeepEvents has a slightly different API */
    if (SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_EVENTMASK(SDL_USEREVENT)))
        return false;
    else
        return true;
}

void EventHandler::pushKeyHandler(KeyHandler kh) {
    KeyHandler *new_kh = new KeyHandler(kh);
    KeyHandlerController *khc = new KeyHandlerController(new_kh);
    pushController(khc);
}

void EventHandler::popKeyHandler() {
    if (controllers.empty())
        return;
    popController();
}

KeyHandler *EventHandler::getKeyHandler() const {
    if (controllers.empty())
        return NULL;

    KeyHandlerController *khc = dynamic_cast<KeyHandlerController *>(controllers.back());
    zu4_assert(khc != NULL, "EventHandler::getKeyHandler called when controller wasn't a keyhandler");
    if (khc == NULL)
        return NULL;

    return khc->getKeyHandler();
}

void EventHandler::setKeyHandler(KeyHandler kh) {
    while (popController() != NULL) {}
    pushKeyHandler(kh);
}
