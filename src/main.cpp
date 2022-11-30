/* *************Dependencies***************
 * - Use SDL for talking to the OS:
 *      - get a window and listen for window events
 *      - listen for UI events
 *      - write to video
 *
 * Why not talk directly to OS?
 * SDL has already done all the work of being platform independent. If there are
 * particular things I wish to do directly I can still do that and only call SDL
 * for the stuff I don't care about. For example, I can use something else to
 * handle rendering and just use SDL for the window and the UI. I don't have to
 * use the SDL_Render API.
 * *******************************/
/* *************Pixel Size and Game Resolution***************
 * Decouple pixel size from video resolution by drawing to a texture then
 * resizing that texture to the window.
 *
 * - Why do I decouple pixel size from video resolution?
 *     - I want chunky pixels -- actual video pixels are too small
 *     - If not decoupled, then I need to draw fake "big" pixels myself
 *         - Instead of dealing with pixels, I'm drawing rectangles
 *         - This is extra work for me and for the GPU
 *         - The GPU is already really good at stretching artwork!
 *     - Easier to adapt to different display resolutions
 *     - Easier to set pixel size
 *     - Code for whatever display size I want, let GPU make it fit
 *     - Plus, I can still do higher-resolution stuff on top (like debug
 *       text or grid lines), just use a different texture!
 * - How do I decouple pixel size from video resolution?
 *     - Create a texture with "classic game" proportion: 16:9
 *         - Examples:
 *         - ?:          160 x  90 (10*16 x 10*9)
 *         - Celeste:    320 x 180 (20*16 x 20*9)
 *         - ?:          480 x 270 (30*16 x 30*9)
 *         - Dead Cells: 640 x 360 (40*16 x 40*9)
 *         - ?:          800 x 450 (50*16 x 50*9)
 *         - ?:          960 x 540 (60*16 x 60*9)
 *     - Draw all chunky pixel art to this texture
 *     - Copy texture to screen
 *     - Remember to clear screen before copying texture to it
 *         - When switching between fullscreen and windowed, OS will not
 *           automatically repaint the window so any window regions not
 *           being painted over on subsequent frames will still show old
 *           paint
 *         - Use this screen clearing to set the color for the
 *           unused portion of the window (applies to fullscreen as well)
 * *******************************/
/* *************Physics and Graphics***************
 * I run my physics loop N times per video loop. They are not completely
 * decoupled, but they are decoupled enough for me.
 *
 * Why not run physics at the same frame rate as video?
 *
 * Video runs at 60 fps. That is fast for animation, but it isn't fast as a
 * maximum speed for physics.
 *
 * I want to keep my physics simple with stuff like this:
 *
 *      bob.x++;
 *
 * I walk bob's pixels left to right across the full width of the game.
 * The game has 320x180 resolution, and bob is a bullet. Bob should go real
 * fast.
 *
 * But bob is not moving very fast if I update my physics at 60fps:
 *
 *      320 columns * 16.7ms per frame = 5.3 seconds to cross the screen!
 * 
 * I could calculate bob's x-coordinate with fancier physics.
 *
 * Or I could just update my simple physics at some multiple of the video frame
 * rate.
 *
 * Say I update the physics four times per video frame. My code is still this:
 *
 *      bob.x++;
 *
 * But the video loop see this:
 *
 *      bob.x += 4;
 *
 * How?
 *
 * My main loop runs at 60fps because it's locked to the VSYNC. Inside that main
 * loop, I have a physics loop:
 *
 *      for(size_t i=0; i<PHYSICS_TICKS; i++)
 *      {
 *          bob.x++;
 *          // Update other stuff...
 *      }
 *
 * So my physics frame rate is 60fps x PHYSICS_TICKS.
 * More PHYSICS_TICKS -> faster-looking physics.
 * *******************************/
/* *************Program Structure***************
 * - Setup
 *      - set up SDL
 *      - quit if there is something wrong with the computer
 *      - define an initial game state
 * - Loop
 *      - Everything happens here!
 *          - I don't (explicitly) use threads
 *      - I redraw everything on screen on every video frame
 * - Shutdown
 *      - clean up nice when the game quits
 *
 * Program structure within the main loop: UI>>>>Physics>>>>Rendering
 *
 * *******************************/
/* *************UI code***************
 * Check for events:
 *
 * - keyboard event
 * - mouse event
 * - OS event
 *
 * The UI code doesn't actually do things.
 * The events cause stuff to happen by updating the game state.
 * Then other parts of the program do the actual thing based on the game state.
 *
 * Examples of state touched by the UI code:
 *
 * - window state:
 *      - on a quit event
 *          - update the quit state
 *          - main loop exits based on this quit state
 *      - get a window resize event
 *          - update the rectangle that tracks the window size
 *          - anything that depends on window size is checking this rectangle on
 *            every frame to use the latest window size
 * - internal state:
 *      - a keypress event puts me in debug mode where keypresses alter physics
 * - physics state:
 *      - a keypress event (if in debug mode) changes the speed of light
 * - graphics state:
 *      - a keypress event toggles visibility on a debug overlay
 *      - a keypress event (if in debug mode) cycles through a list of colors
 *        I'm testing out
 *
 * I try to silo which code has write access to which state, but no single
 * section owns the right to update a particular portion of the state. Some
 * graphics state updating is done by both the UI and by the Physics code.
 * *******************************/
/* *************Physics code***************
 * - calculate the physics for all the stuff
 * - update game state
 *
 * Physics code does not touch as many categories of state as the UI code.
 * But physics code does a lot more work on the graphics state than the UI code
 * does.
 * *******************************/
/* *************Rendering code***************
 * Draw graphics based on the game state.
 *
 * The rendering code does not modify the game state. It reads the game state
 * and modifies the Renderer accordingly. It modifies the Renderer a lot!
 *
 * Any write-access variables in the rendering section (besides the global
 * single Renderer instance) are limited to the scope of the rendering code.
 * *******************************/
#include <cstdio>
#include <SDL.h>
#include "mg_colors.h"
#include <chrono>
#include <cassert>

namespace GameArt
{
    ////////////////
    // CHUNKY PIXELS
    ////////////////
                                                        //      scale -----  Try scale=10, 20, 30, etc.
                                                        //                |
                                                        //                v
    constexpr int scale = 20;                           // Ex: 320:180 = 20*(16:9)
    constexpr SDL_Rect rect = {.x=0, .y=0, .w=scale*16, .h=scale*9}; // Game art has a 16:9 aspect ratio
    SDL_Texture* tex;                                   // Render game art to this texture
}
namespace GameDemo
{
    //////////////////////////
    // USER: PICK STUFF TO SEE
    //////////////////////////

    constexpr bool RAINBOW_STATIC = false;
    constexpr bool RAT_CIRCLE = true;                   // Rational parametrization of a circle
}

SDL_Rect center_src_in_win(const SDL_Rect& winrect, const SDL_Rect& srcrect)
{
    return SDL_Rect {
        .x=(winrect.w - srcrect.w)/2,
        .y=(winrect.h - srcrect.h)/2,
        .w=srcrect.w, .h = srcrect.h};
}

SDL_Rect scale_src_to_win(const SDL_Rect& winrect, const SDL_Rect& srcrect)
{
    /* *************DOC***************
     * Return srcrect centered and scaled up to best fit in the winrect.
     *
     * - scale up srcrect without getting too big to fit
     * - maintain an integer scaling factor (avoids visual artifacts)
     *
     * If winrect is smaller than srcrect, do not scale down, just clip
     * srcrect to fit in winrect.
     *
     * Parameters
     * ------------
     * winrect : SDL_Rect window created by the OS
     * srcrect : SDL_Rect texture with game artwork
     * *******************************/

    // Find ratios for width and height of OS window to game art
    int ratio_w = winrect.w/srcrect.w;
    int ratio_h = winrect.h/srcrect.h;

    // Return srcrect if either ratio is < 1 (because integer part of ratio == 0)
    if (  (ratio_w==0) || (ratio_h==0)  ) return srcrect;

    // Use the smaller of the two ratios as the scaling factor.
    int K = (ratio_w > ratio_h) ? ratio_h : ratio_w;

    SDL_Rect scalerect = {.x=0,.y=0,.w = K*srcrect.w,.h = K*srcrect.h};
    assert( winrect.w >= scalerect.w ); assert( winrect.h >= scalerect.h );

    return center_src_in_win(winrect, scalerect);

}

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
    w = 2*GameArt::rect.w;                              // Default w
    h = 2*GameArt::rect.h;                              // Default h

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
    SDL_DestroyTexture(GameArt::tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

//////////////////////////////////////////////////////////
// FUNCTIONS FOR CIRCLE ART USING RATIONAL PARAMETRIZATION
//////////////////////////////////////////////////////////

namespace RatCircle
{
    ////////////////////
    // ARTWORK FUNCTIONS
    ////////////////////
    float x(int n, int d)
    { // Parameter t = n/d, return x(t) for a circle
        /* *************DOC***************
         * Return x(t) for a unit circle parametrized with t
         *
         *      ------------------
         *      |         1-t*t  |
         *      | x(t) = ------- |
         *      |         1+t*t  |
         *      ------------------
         *
         * Parameters
         * ----------
         * n : int
         *      t = n/d (n is the numerator of t)
         * d : int
         *      t = n/d (d is the denominator of t)
         * *******************************/
        float t = static_cast<float>(n)/static_cast<float>(d);
        return (1-(t*t))/(1+(t*t));
    }
    float y(int n, int d)
    { // Parameter t = n/d, return y(t) for a circle
        /* *************DOC***************
         * Return y(t) for a unit circle parametrized with t
         *
         *      ------------------
         *      |           2*t  |
         *      | y(t) = ------- |
         *      |         1+t*t  |
         *      ------------------
         *
         * Parameters
         * ----------
         * n : int
         *      t = n/d (n is the numerator of t)
         * d : int
         *      t = n/d (d is the denominator of t)
         * *******************************/
        float t = static_cast<float>(n)/static_cast<float>(d);
        return (2*t)/(1+(t*t));
    }

    /////////////
    // GAME STATE
    /////////////
    SDL_FPoint* points;                                 // Array of points in circle
    int counter = 0;                                    // counter : cycle through points in circle
    constexpr int MAX_NUM_POINTS = 256;                 // Max points in circle
    int N = MAX_NUM_POINTS >> 2;                        // N points in a quarter circle
    int COUNT = N << 2;                                 // COUNT is always 4*N
}

int main(int argc, char* argv[])
{
    ////////
    // SETUP
    ////////

    std::srand(std::time(0));                           // Seed RNG with current time
    WindowInfo wI(argc, argv);
    if (DEBUG) printf("Window info: %d x %d at %d,%d\n", wI.w, wI.h, wI.x, wI.y);
    if (DEBUG) printf("Number of colors: %lld\n", sizeof(Colors::list)/sizeof(SDL_Color));
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

        // Create a texture for game art
        GameArt::tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,               // Render to this texture
                GameArt::rect.w, GameArt::rect.h);
        // Set up game art texture for blending to draw on transparent background.
        if(SDL_SetTextureBlendMode(GameArt::tex, SDL_BLENDMODE_BLEND) < 0)
        { // Texture blending is not supported
            puts("Cannot set texture to blendmode blend.");
            shutdown();
            return EXIT_FAILURE;
        }
    }

    /////////////////////
    // INITIAL GAME STATE
    /////////////////////

    int bgnd_color=Colors::DARKGRAVEL;                  // Index into Colors::list
    int fgnd_color=Colors::contrasts(bgnd_color);       // Pick a nice foreground

    bool quit = false;                                  // quit : true ends game
    bool is_fullscreen = false;                         // Fullscreen vs windowed
    // Must initialize bool as true or false to avoid garbage!
    // I use {} (the default initializer) to imply any valid initial value is OK.
    bool show_overlay{};                                // Help on/off

    if (GameDemo::RAT_CIRCLE)
    { // Allocate a memory pool for circle points
        RatCircle::points = (SDL_FPoint *)malloc(sizeof(SDL_FPoint)*RatCircle::MAX_NUM_POINTS);
    }
    ///////
    // LOOP
    ///////
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
                    case SDLK_F11:                      // F11 : toggle fullscreen
                        is_fullscreen = !is_fullscreen;
                        if (is_fullscreen)
                        { // Go fullscreen
                            SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN);
                        }
                        else
                        { // Go back to windowed
                            SDL_SetWindowFullscreen(win, 0);
                        }
                        // Update state with latest window info
                        SDL_GetWindowSize(win, &(wI.w), &(wI.h));
                        break;

                    case SDLK_SPACE:                    // Space : change colors
                        if(  kmod&KMOD_SHIFT  ) Colors::prev(bgnd_color);
                        else                    Colors::next(bgnd_color);
                        fgnd_color = Colors::contrasts(bgnd_color);
                        break;

                    case SDLK_SLASH:                    // ? : Toggle help
                        // TODO: draw text in this overlay
                        if(  kmod&KMOD_SHIFT  ) show_overlay = !show_overlay;
                        break;

                    case SDLK_k:
                        if (GameDemo::RAT_CIRCLE)
                        {
                            using namespace RatCircle;
                            N++;                    // Num points in quarter circle
                            // Clamp N to max memory pool size
                            if (4*N > MAX_NUM_POINTS) N = MAX_NUM_POINTS>>2;
                            COUNT = 4*N;            // Num points in full circle
                        }
                        break;
                    case SDLK_j:
                        if (GameDemo::RAT_CIRCLE)
                        {
                            using namespace RatCircle;
                            N--;                    // Num points in quarter circle
                            // Clamp N to minimum size
                            if (N < 1) N = 1;
                            COUNT = 4*N;            // Num points in full circle
                        }
                        break;
                    default: break;
                }
            }
        }

        ////////////
        // RENDERING
        ////////////

        ///////////
        // GAME ART
        ///////////

        // Default is to render to the OS window.
        // Render game art stuff to the GameArt texture instead.
        SDL_SetRenderTarget(ren, GameArt::tex);

        { // Background color
            SDL_Color c = Colors::list[bgnd_color];
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderClear(ren);
        }
        SDL_FRect border;                               // Use this in later blocks
        { // Border
            float W = static_cast<float>(GameArt::rect.w);
            float H = static_cast<float>(GameArt::rect.h);
            float M = 0.01*W;                           // M : Margin in pixels
            border = {.x=M, .y=M, .w=W-2*M, .h=H-2*M};
            SDL_Color c = Colors::list[fgnd_color];
            // Render
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderDrawRectF(ren, &border);
        }
        if(  GameDemo::RAT_CIRCLE  )
        {
            using namespace RatCircle;

            // Make a quarter circle
            for(int i=0; i<N; i++)
            {
                // Express parameter t as an integer ratio
                int n=i; int d=N;                       // t = n/d
                // Calculate point [x(t), y(t)]
                points[i] = SDL_FPoint{.x=x(n,d), .y=y(n,d)};
            }
            // Make the other three-quarters of the circle
            for(int i=N; i<COUNT; i++)
            {
                // Next point is the point N indices back, rotated a quarter-circle
                points[i] = SDL_FPoint{.x=-1*points[i-N].y, .y=points[i-N].x};
            }
            // Offset and scale the circle of points
            float offset_x; float offset_y;             // Use these for the line too
            for(int i=0; i<COUNT; i++)
            {
                offset_x = GameArt::rect.w/2;           // Offset x to game art center
                offset_y = GameArt::rect.h/2;           // Offset y to game art center
                constexpr int SCALE = 32;              // Scale up by factor SCALE
                points[i] = SDL_FPoint{
                    .x = (SCALE*points[i].x) + offset_x,
                    .y = (SCALE*points[i].y) + offset_y };
            }
            { // Draw tardis-colored points
                SDL_Color c = Colors::tardis;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                SDL_RenderDrawPointsF(ren, points, COUNT);
            }
            { // Draw an orange line from center to a point
                SDL_Color c = Colors::orange;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                SDL_RenderDrawLineF(ren,
                        offset_x, offset_y,
                        points[counter%COUNT].x,
                        points[counter%COUNT].y);
            }
            counter++;
        }
        if(  GameDemo::RAINBOW_STATIC  )
        { // Rainbow static : placeholder to show pixel size changing
            for(int i=0; i<Colors::count; i++)
            {
                SDL_Color c = Colors::list[i];

                constexpr int COUNT = (1<<6) * GameArt::scale*GameArt::scale/100;
                SDL_FPoint points[COUNT];
                for(int i=0; i<COUNT; i++)
                {
                    float x = (static_cast<float>(std::rand()) * (border.w-3)) / RAND_MAX + (border.x+1);
                    float y = (static_cast<float>(std::rand()) * (border.h-3)) / RAND_MAX + (border.y+1);
                    points[i] = SDL_FPoint{x,y};
                }
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                SDL_RenderDrawPointsF(ren, points, COUNT);
            }
        }
        if(  show_overlay  )
        { // Overlay help
            { // Darken light stuff
                SDL_Color c = Colors::coal;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a>>1); // 50% darken
                SDL_Rect rect = {.x=0, .y=0, .w=GameArt::rect.w, .h=100};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
            { // Lighten dark stuff
                SDL_Color c = Colors::snow;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a>>3); // 12% lighten
                SDL_Rect rect = {.x=0, .y=0, .w=GameArt::rect.w, .h=100};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
        }

        ////////////
        // OS WINDOW
        ////////////

        SDL_SetRenderTarget(ren, NULL);                 // Render to OS window
        { // Clear the window to a black background
            SDL_SetRenderDrawColor(ren, 0,0,0,0);
            SDL_RenderClear(ren);
        }
        // Copy the game art to the OS window
        SDL_Rect dstrect;                               // Game location & size in OS window
        SDL_Rect winrect = {.x=0,.y=0,.w=wI.w,.h=wI.h}; // OS window size
        // - Center game art in OS window
        // - Leave scaling 1:1 or scale-up to fit (but maintain aspect ratio)
        constexpr bool SCALE_GAME_ART = true;
        dstrect = (SCALE_GAME_ART) ?
            scale_src_to_win(winrect, GameArt::rect) :
            center_src_in_win(winrect, GameArt::rect);
        SDL_RenderCopy(ren, GameArt::tex, &GameArt::rect, &dstrect);
        SDL_RenderPresent(ren);
    }
    if (GameDemo::RAT_CIRCLE)
    { // Free pool of memory for points in the circle
        free(RatCircle::points);
    }

    shutdown();
    return EXIT_SUCCESS;
}
