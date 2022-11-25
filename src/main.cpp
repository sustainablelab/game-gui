#include <cstdio>
#include <SDL.h>
#include "mg_colors.h"

// Control whether DEBUG stuff generates code or not
enum { DEBUG = 1 };                                     // USER! Set DEBUG : 0 or 1

struct WindowInfo
{
    int x,y,w,h;
    Uint32 flags;
    WindowInfo(int argc, char* argv[]);
};

WindowInfo::WindowInfo(int argc, char* argv[])
{ // Window size, location, and behavior to act like a Vim window

    // Set defaults
    x = 50;                                             // Default x
    y = 50;                                             // Default y
    w = 2*320;                                          // Default w
    h = 2*180;                                          // Default h

    // Overwrite with values, if provided
    if(argc>1) x = atoi(argv[1]);
    if(argc>2) y = atoi(argv[2]);
    if(argc>3) w = atoi(argv[3]);
    if(argc>4) h = atoi(argv[4]);

    // Only do a borderless, always-on-top window if I pass window args
    if(argc>1)
    {
        flags = SDL_WINDOW_BORDERLESS |                 // Look pretty
                SDL_WINDOW_ALWAYS_ON_TOP |              // Stay on top
                SDL_WINDOW_INPUT_GRABBED;               // Really stay on top
    }
    else
    {
        flags = SDL_WINDOW_RESIZABLE;                   // Click drag to resize
    }
}

SDL_Window* win;
SDL_Renderer* ren;

void shutdown(void)
{
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

int main(int argc, char* argv[])
{
    WindowInfo wI(argc, argv);
    if (DEBUG) printf("Window info: %d x %d at %d,%d\n", wI.w, wI.h, wI.x, wI.y);
    if (DEBUG) printf("Number of colors: %ld\n", sizeof(Colors::list)/sizeof(SDL_Color));
    { // SDL Setup
        SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
        win = SDL_CreateWindow(argv[0], wI.x, wI.y, wI.w, wI.h, wI.flags);
        Uint32 ren_flags = 0;
        ren_flags |= SDL_RENDERER_PRESENTVSYNC;         // 60 fps! No SDL_Delay()
        ren_flags |= SDL_RENDERER_ACCELERATED;          // Hardware acceleration
        ren = SDL_CreateRenderer(win, -1, ren_flags);

        // Set up transparency blending for a transparent heads-up overlay.
        // Just this line is enough to start using the alpha channel on my overlay.
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND); // Draw with alpha
    }

    int bgnd_color=6;                                   // Index into Colors::list
    int fgnd_color=Colors::contrasts(bgnd_color);       // Pick a nice foreground
    bool show_overlay;                                  // Help on/off
    bool quit = false;
    while (!quit)
    {
        /////
        // UI
        /////
        SDL_Keymod kmod = SDL_GetModState();            // Check for modifier keys
        SDL_Event e; while(  SDL_PollEvent(&e)  )       // Handle the event queue
        { // See tag SDL_EventType

            // Quit with default OS stuff
            if (  e.type == SDL_QUIT  ) quit = true;    // Alt-F4 / click X

            // Update window info if user resizes window
            if (  e.type == SDL_WINDOWEVENT  )
            {
                switch(e.window.event)
                { // See tag SDL_WindowEventID

                    case SDL_WINDOWEVENT_RESIZED:
                        SDL_GetWindowSize(win, &(wI.w), &(wI.h));
                        break;

                    default: break;
                }
            }

            // Keyboard controls
            // TODO: add filtered control as movement example
            if (  e.type == SDL_KEYDOWN  )
            {
                switch(e.key.keysym.sym)
                { // See tag SDL_KeyCode

                    case SDLK_q: quit = true; break;    // q : quit

                    case SDLK_SPACE:                    // Space : change colors
                        if(  kmod&KMOD_SHIFT  ) Colors::prev(bgnd_color);
                        else                    Colors::next(bgnd_color);
                        fgnd_color = Colors::contrasts(bgnd_color);
                        break;

                    case SDLK_SLASH:                    // ? : Toggle help
                        if(  kmod&KMOD_SHIFT  ) show_overlay = !show_overlay;
                        break;

                    default: break;
                }
            }
        }

        ////////////
        // RENDERING
        ////////////

        { // Background color
            SDL_Color c = Colors::list[bgnd_color];
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderClear(ren);
        }
        { // Border
            float W = static_cast<float>(wI.w);   // W : window W as a float
            float H = static_cast<float>(wI.h);   // H : window H as a float
            float M = 0.01*W;                     // M : Margin in pixels
            SDL_FRect border = {.x=M, .y=M, .w=W-2*M, .h=H-2*M};
            SDL_Color c = Colors::list[fgnd_color];
            // Render
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderDrawRectF(ren, &border);
        }
        if(  show_overlay  )
        { // Overlay help
            { // Darken light stuff
                SDL_Color c = Colors::coal;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a>>1); // 50% darken
                SDL_Rect rect = {.x=0, .y=0, .w=wI.w, .h=100};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
            { // Lighten dark stuff
                SDL_Color c = Colors::snow;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a>>3); // 12% lighten
                SDL_Rect rect = {.x=0, .y=0, .w=wI.w, .h=100};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
        }
        SDL_RenderPresent(ren);
    }
    shutdown();
    return EXIT_SUCCESS;
}
