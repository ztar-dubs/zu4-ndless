/* Redirect <SDL.h> to nSDL's <SDL/SDL.h> for TI-Nspire */
#include <SDL/SDL.h>

/* Ndless defines 'sleep' as a broken macro (via os.h -> libndls.h).
 * This conflicts with EventHandler::sleep() and other C++ code.
 * Undefine it after SDL includes everything. */
#ifdef sleep
#undef sleep
#endif

/* Also fix other problematic Ndless macros if needed */
#ifdef puts
#undef puts
#endif
