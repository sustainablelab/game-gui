#include "mg_readme.h"
#include <cstdio>
#include <SDL.h>
#include "mg_colors.h"
#include <chrono>
#include <cassert>

namespace GameDemo
{
    //////////////////////////
    // USER: PICK STUFF TO SEE
    //////////////////////////

    constexpr bool RAINBOW_STATIC = false;              // Just random colors
    constexpr bool RAT_CIRCLE = false;                  // FAVORITE: Rational parametrization of a circle
    constexpr bool BLOB = false;                        // Be an ameoba-plasma-ball-thing (uses RatCircle)
    constexpr bool GEN_CURVE = true;                    // Generate a curve with dCB quadratics
    constexpr bool FIT_CURVE = false;                   // Fit a curve with dCB quadratics
}

namespace GameArt
{
    ////////////////
    // CHUNKY PIXELS
    ////////////////
                                                        //      scale <----  Try scale=10, 20, 40, 60, 80
                                                        //      |  10: max chunky! 20: retro game
                                                        //      v  80: high-res
    constexpr int scale = 80;                           // Ex: 20*(16:9) = 320:180
    constexpr SDL_Rect rect = {.x=0, .y=0, .w=scale*16, .h=scale*9}; // Game art has a 16:9 aspect ratio
    SDL_Texture* tex;                                   // Render game art to this texture

    //////////////////////////////////////////////
    // FUNCTIONS TO STRETCH TEXTURE OVER OS WINDOW
    //////////////////////////////////////////////
    SDL_Rect center_src_in_win(const SDL_Rect& window, const SDL_Rect& texture);
    SDL_Rect  scale_src_to_win(const SDL_Rect& window, const SDL_Rect& texture);

}
SDL_Rect GameArt::center_src_in_win(const SDL_Rect& winrect, const SDL_Rect& srcrect)
{
    return SDL_Rect {
        .x=(winrect.w - srcrect.w)/2,
        .y=(winrect.h - srcrect.h)/2,
        .w=srcrect.w, .h = srcrect.h};
}
SDL_Rect GameArt::scale_src_to_win(const SDL_Rect& winrect, const SDL_Rect& srcrect)
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

    return GameArt::center_src_in_win(winrect, scalerect);

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

///////////////
// GAME GLOBALS
///////////////
namespace RatCircle
{
    ////////////////////////////////////////////
    // CIRCLE ART USING RATIONAL PARAMETRIZATION
    ////////////////////////////////////////////

    /////////////
    // GAME STATE
    /////////////
    // TODO: work out the weird periodic aliasing effects and the rules for "good" numbers
    constexpr uint16_t MAX_NUM_POINTS = (1<<9)-3;       // Max points in circle
    constexpr uint16_t MAX_SPEED = MAX_NUM_POINTS/(1<<4);   // Max counter increments per video frame

    /////////////////
    // PURE FUNCTIONS
    /////////////////
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

    struct Spinner
    { // Dots that spin around the rational parametrized circle
        /////////////
        // GAME STATE
        /////////////
        SDL_FPoint* points;                 // Array of points in circle
        uint16_t counter;                   // counter : cycle through points in circle, phase = counter%COUNT
        uint16_t speed;                     // Counter increments per video frame; controlled by j/k
        int N, COUNT;                       // N points in a quarter circle, COUNT is 4*N
        float center_x; float center_y;     // Circle center
        uint8_t RADIUS;                     // Circle size

        ////////////
        // FUNCTIONS
        ////////////

        Spinner(float, float, uint8_t, uint16_t, uint16_t);   // Setup initial values and allocate memory
        void calc_circle_points(void);      // Initial circle points calc
        void increase_resolution(void);     // Increment number of points in circle
        void decrease_resolution(void);     // Decrement number of points in circle
    };
    Spinner::Spinner(float x, float y, uint8_t r, uint16_t s, uint16_t p)
    { // Initial spinner values and memory for points in circle
        //////////////////
        // SPIN PARAMETERS
        //////////////////
        center_x = x;                       // GameArt::rect.w/2 : offset x to game art center
        center_y = y;                       // GameArt::rect.h/2 : offset y to game art center
        RADIUS = r;                         // 32 : Scale up points calc by factor RADIUS
        speed = s;                          // 1-MAX_SPEED : number of physics frames per video frame
        counter = p;                        // 0 : start spinning from first point in circle
        /////////////////
        // SPIN INTERNALS
        /////////////////
        N = MAX_NUM_POINTS/(1<<2);          // N points in a quarter circle
        COUNT = N << 2;                     // COUNT is always 4*N

        // Allocate a memory pool for cicle points. Remember to free(points).
        points = (SDL_FPoint *)malloc(sizeof(SDL_FPoint)*MAX_NUM_POINTS);
        // Calculate circle points (recalc later if change: N, RADIUS, center)
        calc_circle_points();               // Initial circle points calc
    }
    void Spinner::calc_circle_points(void)
    { // Write to array of rational points: 4*N in full circle
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
        for(int i=0; i<COUNT; i++)
        {
            points[i] = SDL_FPoint{
                .x = (RADIUS*points[i].x) + center_x,
                .y = (RADIUS*points[i].y) + center_y };
        }
    }
    ///////////////////////////////////////
    // HOW MANY POINTS : RESOLUTION CONTROL
    ///////////////////////////////////////
    // NOT USED
    void Spinner::increase_resolution(void)
    { // Add one more point to the quarter circle (adds four points)
        N++;                        // Num points in quarter circle
        // Clamp N to max memory pool size
        if (4*N > MAX_NUM_POINTS) N = MAX_NUM_POINTS/(1<<2);
        COUNT = 4*N;                // Num points in full circle
        calc_circle_points();       // Update circle
    }
    // NOT USED
    void Spinner::decrease_resolution(void)
    { // Take away one point from the quarter circle (removes four points)
        N--;                        // Num points in quarter circle
        // Clamp N to minimum size
        if (N < 1) N = 1;
        COUNT = 4*N;                // Num points in full circle
        calc_circle_points();       // Update circle
    }

}

namespace Blob
{ // A RatCircle with jiggly points

    // Specify the circle the Blob is based on
    SDL_FPoint center;
    float radius;

    // A Blob is a RatCircle with jiggly points
    constexpr float JIGAMT = 0.1;                       // Jiggle amount: [0:1]
                                                        //
    // Set the number of points in the RatCircle here
    //         ----------------
    //                   |
    //                   |----(explore powers of 2, between 2^2 and 2^3 looks good)
    //                   v
    constexpr int N =  6;                               // Num points in quarter-circle
    constexpr int FULL = N*4;                           // Num points in full-circle
    SDL_FPoint* points;                                 // The jiggly circle points
    SDL_FPoint* points_debug;                           // Circle points without jiggle
}

///////
// MAIN
///////

int main(int argc, char* argv[])
{
    ////////
    // SETUP
    ////////

    std::srand(std::time(0));                           // Seed RNG with current time
    WindowInfo wI(argc, argv);
    if (DEBUG) printf("Window info: %d x %d at %d,%d\n", wI.w, wI.h, wI.x, wI.y);
    if (DEBUG) printf("Number of colors in palette: %ld\n", sizeof(Colors::list)/sizeof(SDL_Color));
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
    bool flag_smaller{};                                // Pressed key for smaller
    bool flag_bigger{};                                 // Pressed key for bigger
    bool flag_down{};                                   // Pressed key for down
    bool flag_up{};                                     // Pressed key for up
    bool flag_left{};                                   // Pressed key for left
    bool flag_right{};                                  // Pressed key for right

    // RAT_CIRCLE demo -- globals
    RatCircle::Spinner *ali, *bob;                      // Example code for individual spinners
    constexpr int NSPIN = 1<<12;                        // Number of spinners on screen: BIG number
    constexpr int NTRAIL = 25;                          // Number of pixels in spinner trail : 1 - 25
    RatCircle::Spinner *spinners[NSPIN];                // Just a giant array of pointers

    if (GameDemo::RAT_CIRCLE)
    { // Allocate memory for spinners only if RAT_CIRCLE==true
        SDL_FRect border;
        { // Spawn spinners within this border
            float W = static_cast<float>(GameArt::rect.w);
            float H = static_cast<float>(GameArt::rect.h);
            float M = 0.01*W;                           // M : Margin in pixels
            border = {.x=M, .y=M, .w=W-2*M, .h=H-2*M};
        }
        for(int i=0; i<NSPIN; i++)
        { // Randowm spawn a bunch of spinners
            // Spawn within the border
            float x = (static_cast<float>(std::rand()) * (border.w-3)) / RAND_MAX + (border.x+1);
            float y = (static_cast<float>(std::rand()) * (border.h-3)) / RAND_MAX + (border.y+1);
            // Start off with a radius between 2 and 64
            uint8_t r = (std::rand() % 62)+ 2;
            // Start off with a random speed between 1 and 11
            uint16_t s = (std::rand() % 10)+1; // Initial speed
            uint16_t p = std::rand() % RatCircle::MAX_NUM_POINTS; // Initial phase
            spinners[i] = new RatCircle::Spinner(x,y,r,s,p);
        }
        if (0)
        { // Example spawning individuals deliberately
            ali = new RatCircle::Spinner(
                                    GameArt::rect.w/2+20,   // x
                                    GameArt::rect.h/2+5,    // y
                                    32,                     // radius
                                    4,                      // speed
                                    0                       // phase
                                    );
            bob = new RatCircle::Spinner(
                                    GameArt::rect.w/2,      // x
                                    GameArt::rect.h/2,      // y
                                    32,                     // radius
                                    11,                      // speed
                                    0                       // phase
                                    );
        }
    }

    // BLOB demo -- globals

    if (GameDemo::BLOB)
    {
        // Blob initial center: center of game window
        Blob::center = SDL_FPoint{
            .x=static_cast<float>(GameArt::rect.w/2),
            .y=static_cast<float>(GameArt::rect.h/2)
        };
        // Blob initial radius: tiny fraction of the game window width
        Blob::radius = static_cast<float>(GameArt::rect.w/12);
        // Allocate memory for the points in the blob shape
        Blob::points = (SDL_FPoint*)malloc(sizeof(SDL_FPoint) * Blob::FULL);
        // Allocate memory for debug overlay: debug blob points do NOT jiggle
        Blob::points_debug = (SDL_FPoint*)malloc(sizeof(SDL_FPoint) * Blob::FULL);
    }
    if (0)
    { // Debugging my Spinner constructor
        if (DEBUG) printf(  "Bob info:\n"
                            "\t- counter: %d\n"
                            "\t- speed: %d\n"
                            "\t- N: %d\n"
                            "\t- COUNT: %d\n",
                            bob->counter, bob->speed, bob->N, bob->COUNT);
        if (DEBUG) printf(  "RatCircle info:\n"
                            "\t- MAX_NUM_POINTS: %d\n",
                            RatCircle::MAX_NUM_POINTS);
    }

    ////////////
    // GAME LOOP
    ////////////
    while (!quit)
    {
        /////////////////////
        // UI - EVENT HANDLER
        /////////////////////
        SDL_Keymod kmod = SDL_GetModState();            // Check for modifier keys
        { // Polled : for tile-game WASD movement style

            // See tag SDL_EventType
            SDL_Event e; while(  SDL_PollEvent(&e)  )   // Handle the event queue
            {
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

                        case SDLK_SPACE:                // Space : change colors
                            if(  kmod&KMOD_SHIFT  ) Colors::prev(bgnd_color);
                            else                    Colors::next(bgnd_color);
                            fgnd_color = Colors::contrasts(bgnd_color);
                            break;

                        case SDLK_SLASH:                // ? : Toggle help
                                                        // TODO: draw text in this overlay
                            if(  kmod&KMOD_SHIFT  ) show_overlay = !show_overlay;
                            break;

                        case SDLK_k:                    // k : up, faster, K : bigger
                            if (GameDemo::RAT_CIRCLE)
                            {
                                using namespace RatCircle;

                                if(  kmod&KMOD_SHIFT  )
                                { // Increment RADIUS, clamp at window h
                                    int MAX = GameArt::rect.h/2;
                                    for(int i=0; i<NSPIN; i++)
                                    {
                                        spinners[i]->RADIUS++;
                                        if (spinners[i]->RADIUS > MAX) spinners[i]->RADIUS = MAX;
                                        // Update points
                                        spinners[i]->calc_circle_points();
                                        if(0)
                                        { // Compensate for increased radius with decreased speed
                                            spinners[i]->speed--;
                                            if (spinners[i]->speed==0) spinners[i]->speed=1;
                                        }

                                    }
                                    if(0)
                                    {
                                        ali->RADIUS++;
                                        bob->RADIUS++;
                                        if (ali->RADIUS > MAX) ali->RADIUS = MAX;
                                        if (bob->RADIUS > MAX) bob->RADIUS = MAX;
                                        // Update points
                                        ali->calc_circle_points();
                                        bob->calc_circle_points();
                                    }
                                }
                                else
                                { // Increment speed, clamp at MAX_SPEED
                                    for(int i=0; i<NSPIN; i++)
                                    {
                                        spinners[i]->speed++;
                                        if (spinners[i]->speed > MAX_SPEED) spinners[i]->speed = MAX_SPEED;
                                    }
                                    if(0)
                                    {
                                        ali->speed++;
                                        bob->speed++;
                                        if (ali->speed > MAX_SPEED) ali->speed = MAX_SPEED;
                                        if (bob->speed > MAX_SPEED) bob->speed = MAX_SPEED;
                                    }
                                }
                            }
                            if (GameDemo::BLOB)         // Demo tile-based-game "up"
                            {
                                if(  kmod&KMOD_SHIFT  ) flag_bigger = true;
                                else                    flag_up = true;
                            }
                            break;

                        case SDLK_j:                    // j : down, slower, J : smaller
                            if (GameDemo::RAT_CIRCLE)
                            {
                                using namespace RatCircle;

                                if(  kmod&KMOD_SHIFT  )
                                { // Decrement RADIUS, clamp at 4

                                    for(int i=0; i<NSPIN; i++)
                                    {
                                        spinners[i]->RADIUS--;
                                        int MIN = 2;
                                        if (spinners[i]->RADIUS<MIN) spinners[i]->RADIUS=MIN;
                                        // Update points
                                        spinners[i]->calc_circle_points();
                                        if (0)
                                        { // Compensate for decreased radius with increased speed
                                            spinners[i]->speed++;
                                            if (spinners[i]->speed > MAX_SPEED) spinners[i]->speed = MAX_SPEED;
                                        }
                                    }
                                    if(0)
                                    {
                                        ali->RADIUS--;
                                        bob->RADIUS--;
                                        int MIN = 2;
                                        if (ali->RADIUS<MIN) ali->RADIUS=MIN;
                                        if (bob->RADIUS<MIN) bob->RADIUS=MIN;
                                        // Update points
                                        ali->calc_circle_points();
                                        bob->calc_circle_points();
                                    }
                                }
                                else
                                { // Decrement speed, clamp at 1
                                    for(int i=0; i<NSPIN; i++)
                                    {
                                        spinners[i]->speed--;
                                        if (spinners[i]->speed==0) spinners[i]->speed=1;
                                    }
                                    if(0)
                                    {
                                        ali->speed--;
                                        bob->speed--;
                                        if (ali->speed==0) ali->speed=1;
                                        if (bob->speed==0) bob->speed=1;
                                    }
                                }
                            }
                            if (GameDemo::BLOB)         // Demo tile-based-game "down"
                            {
                                if(  kmod&KMOD_SHIFT  ) flag_smaller = true;
                                else                    flag_down = true;
                            }
                            break;

                        case SDLK_h:                    // h : left
                             if (GameDemo::BLOB) flag_left = true; // Demo tile-based-game "left"
                             break;

                        case SDLK_l:                    // l : right
                             if (GameDemo::BLOB) flag_right = true; // Demo tile-based-game "right"
                             break;

                        default: break;
                    }
                }
            }
        }
        { // Filtered : for platformer-style WASD
            SDL_PumpEvents();
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            if (GameDemo::BLOB)
            { // WASD
                if(  k[SDL_SCANCODE_W]  ) flag_up = true;
                if(  k[SDL_SCANCODE_A]  ) flag_left = true;
                if(  k[SDL_SCANCODE_S]  ) flag_down = true;
                if(  k[SDL_SCANCODE_D]  ) flag_right = true;
            }
        }
        /////////////////
        // PHYSICS UPDATE
        /////////////////

        if(  GameDemo::RAT_CIRCLE  )
        {
            for(int i=0; i<NSPIN; i++)
            {
                for( int j=0; j<spinners[i]->speed; j++)
                {
                    spinners[i]->counter++;             // Track location on circle
                }
            }
            if(0)
            {
                for( int i=0; i<ali->speed; i++)
                {
                    ali->counter++;                          // Track location on circle
                }
                for( int i=0; i<bob->speed; i++)
                {
                    bob->counter++;                          // Track location on circle
                }
            }
        }

        if(  GameDemo::BLOB  )
        {
            { // Handle UI flags
                if(flag_smaller)
                { // Decrease blob radius
                    flag_smaller = false;
                    Blob::radius--;
                    if (Blob::radius <=2) Blob::radius = 2;
                }
                if(flag_bigger)
                { // Increase blob radius
                    flag_bigger = false;
                    Blob::radius++;
                    float MAX = GameArt::rect.w/4;
                    if (Blob::radius >=MAX) Blob::radius = MAX;
                }
                // Note: speed of moving up/down/left/right depends on radius
                const float move_amount = Blob::radius/4;
                if(flag_down)
                { // Move blob down
                    flag_down = false;
                    Blob::center.y += move_amount;
                }
                if(flag_up)
                { // Move blob up
                    flag_up = false;
                    Blob::center.y -= move_amount;
                }
                if(flag_left)
                { // Move blob left
                    flag_left = false;
                    Blob::center.x -= move_amount;
                }
                if(flag_right)
                { // Move blob right
                    flag_right = false;
                    Blob::center.x += move_amount;
                }
            }
            { // Make the circle
                for(int i=0; i<Blob::N; i++)
                { // Make a quarter circle

                    /////////////////////////////////////
                    // FIND RATIONAL POINTS ON THE CIRCLE
                    /////////////////////////////////////
                    Blob::points[i] = SDL_FPoint{
                        .x=RatCircle::x(i,Blob::N),
                        .y=RatCircle::y(i,Blob::N)
                    };
                    // Same for debug circle
                    Blob::points_debug[i] = SDL_FPoint{
                        .x=RatCircle::x(i,Blob::N),
                        .y=RatCircle::y(i,Blob::N)
                    };

                    //////////////////////
                    // JIGGLE THOSE POINTS
                    //////////////////////
                    // At this point in the circle-making, each point's x&y are still in range [0,1].

                    // Get a random float from -0.5 to 0.5
                    float jiggle = (static_cast<float>(std::rand()) / RAND_MAX) - 0.5;
                    // And scale it by JIGAMT
                    Blob::points[i].x += Blob::JIGAMT*jiggle;
                    Blob::points[i].y += Blob::JIGAMT*jiggle;
                }
                for(int i=Blob::N; i<Blob::FULL; i++)
                { // Make the other three-quarters of the circle
                  // Next point is N indices back, rotated a quarter-circle
                    Blob::points[i]       = SDL_FPoint{
                        .x=-1*Blob::points[i-Blob::N].y,
                        .y=   Blob::points[i-Blob::N].x
                    };
                    // Same for debug circle
                    Blob::points_debug[i] = SDL_FPoint{
                        .x=-1*Blob::points_debug[i-Blob::N].y,
                        .y=   Blob::points_debug[i-Blob::N].x
                    };
                }
                for(int i=0; i<Blob::FULL; i++)
                { // Scale circle by radius and offset by center
                    Blob::points[i] = SDL_FPoint{
                        .x = (Blob::radius * Blob::points[i].x) + Blob::center.x,
                        .y = (Blob::radius * Blob::points[i].y) + Blob::center.y
                    };
                    // Same for debug circle
                    Blob::points_debug[i] = SDL_FPoint{
                        .x = (Blob::radius * Blob::points_debug[i].x) + Blob::center.x,
                        .y = (Blob::radius * Blob::points_debug[i].y) + Blob::center.y
                    };
                }
                // Set final point = initial point to close the shape (for DrawLines)
                Blob::points[Blob::FULL-1] = Blob::points[0];
                // Same for debug circle
                Blob::points_debug[Blob::FULL-1] = Blob::points_debug[0];
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
        if(  GameDemo::BLOB  )
        { // Draw the circle specified by Blob::center and Blob::radius
            if (  show_overlay  )
            { // Debug overlay -- expect circle in center of jiggle
                { // Pick an obvious debug color, but make it a little transparent
                    SDL_SetRenderDrawColor(ren, 100, 255, 100, 255/(1<<1));
                }
                { // Draw blob points without jiggle, connect with lines
                    SDL_RenderDrawLinesF(ren, Blob::points_debug, Blob::FULL);
                }
            }
                if (1)
                { // Connects points with lines
                    { // Use tardis blue with transparency
                        SDL_Color c = Colors::tardis;
                        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a/(1<<1));
                    }
                    SDL_RenderDrawLinesF(ren, Blob::points, Blob::FULL);    // Render the circle
                }
                if (1)
                { // Draw points
                    { // Use foreground color
                        SDL_Color c = Colors::list[fgnd_color];
                        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    }
                    SDL_RenderDrawPointsF(ren, Blob::points, Blob::FULL);   // Render the circle
                }
        }
        if(  GameDemo::RAT_CIRCLE  )
        {
            if (0)
            { // Draw tardis-colored points
                SDL_Color c = Colors::tardis;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                SDL_RenderDrawPointsF(ren, bob->points, bob->COUNT);
            }
            if (0)
            { // Draw an orange line from center to a point
                SDL_Color c = Colors::orange;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                SDL_RenderDrawLineF(ren,
                        bob->center_x, bob->center_y,
                        bob->points[bob->counter%bob->COUNT].x,
                        bob->points[bob->counter%bob->COUNT].y);
            }
            if (1)
            { // Draw each spinner at its active point
                for(int i=0; i<NSPIN; i++)
                {
                    int index = i%Colors::count;
                    if (index == bgnd_color) index++;   // Don't make spinners same color as bgnd
                    SDL_Color c = Colors::list[i%Colors::count];
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    // TODO: just redo this as phase and be done with it
                    uint16_t counter = spinners[i]->counter; int COUNT = spinners[i]->COUNT;
                    int phase = counter%COUNT; // a point from 0 to COUNT-1
                    // Draw the point AND a trail after it for one color spinners
                    int ntrail = (index == fgnd_color) ? NTRAIL : 1;
                    for(int j=0; j<ntrail; j++)
                    {
                        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a-(j*10));
                        SDL_FPoint active_point = spinners[i]->points[phase-j];
                        SDL_RenderDrawPointF(ren, active_point.x, active_point.y);
                    }
                }
                if(0)
                { // ali is lime
                    SDL_Color c = Colors::lime;
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    SDL_FPoint active_point = ali->points[ali->counter%ali->COUNT];
                    SDL_RenderDrawPointF(ren, active_point.x, active_point.y);
                }
                if(0)
                { // bob is orange
                    SDL_Color c = Colors::orange;
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    SDL_FPoint active_point = bob->points[bob->counter%bob->COUNT];
                    SDL_RenderDrawPointF(ren, active_point.x, active_point.y);
                }
            }
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
        if(  GameDemo::GEN_CURVE  )
        {
            float x = GameArt::rect.w/2;
            float y = GameArt::rect.h/2;
            constexpr int ORDER = 2;                    // 2nd-order dCB curve
            constexpr int NC = ORDER+1;                 // 2nd-order has 3 control points
            SDL_FPoint P0 = {.x=x-50, .y=y+20};
            SDL_FPoint P1 = {.x=x,    .y=y-20};
            SDL_FPoint P2 = {.x=x+50, .y=y+10};
            SDL_FPoint control_points[NC] = {P0, P1, P2};
            constexpr int K = 128;                      // Sample curve at K points
            SDL_FPoint points[K];
            { // Draw the dCB curve as lime points and foreground lines
                // Generate the curve using convex affine combinations
                for(int i=0; i<K; i++)
                {
                    float t = static_cast<float>(i)/static_cast<float>(K);
                    // Q0 traces the JOIN (P0,P1)
                    SDL_FPoint Q0 = {
                        .x=(1-t)*P0.x + t*P1.x,
                        .y=(1-t)*P0.y + t*P1.y
                    };
                    // Q1 traces the JOIN (P1,P2)
                    SDL_FPoint Q1 = {
                        .x=(1-t)*P1.x + t*P2.x,
                        .y=(1-t)*P1.y + t*P2.y
                    };
                    // point traces the JOIN (Q0,Q1)
                    points[i] = {
                        .x=(1-t)*Q0.x + t*Q1.x,
                        .y=(1-t)*Q0.y + t*Q1.y
                    };
                }
                { // Render as lines in foreground color
                    { // Use foreground color
                        SDL_Color c = Colors::list[fgnd_color];
                        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    }
                    SDL_RenderDrawPointsF(ren, points, K);
                }
                { // Render as lime points
                    { // Use lime color
                        SDL_Color c = Colors::lime;
                        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    }
                    SDL_RenderDrawLinesF(ren,points,K);
                }
            }
            { // Draw the JOIN segment of each pair of control points in tardis blue
                { // Use tardis color
                    SDL_Color c = Colors::tardis;
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                }
                SDL_RenderDrawLinesF(ren,control_points,NC);
            }
            { // Draw the dCB curve control points in red/pink
                { // Use dress color
                    SDL_Color c = Colors::dress;
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                }
                SDL_RenderDrawPointsF(ren,control_points,NC);
            }
            if (0) // Draw a quick throwaway rectangle to get going
            { // Draw a filled rect
                { // Use foreground color
                    SDL_Color c = Colors::list[fgnd_color];
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                }
                SDL_Rect rect = {
                    .x=GameArt::rect.w/2,
                    .y=GameArt::rect.h/2,
                    .w=10, .h=10
                };
                SDL_RenderFillRect(ren, &rect);
            }
        }
        if(  show_overlay  )
        { // Overlay help
            { // Darken light stuff
                SDL_Color c = Colors::coal;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a/(1<<1)); // 50% darken
                SDL_Rect rect = {.x=0, .y=0, .w=GameArt::rect.w, .h=100};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
            { // Lighten dark stuff
                SDL_Color c = Colors::snow;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a/(1<<3)); // 12% lighten
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
            GameArt::scale_src_to_win(winrect, GameArt::rect) :
            GameArt::center_src_in_win(winrect, GameArt::rect);
        SDL_RenderCopy(ren, GameArt::tex, &GameArt::rect, &dstrect);
        SDL_RenderPresent(ren);
    }

    if (GameDemo::RAT_CIRCLE)
    { // Free pool of memory for points in the circle
        for(int i=0; i<NSPIN; i++)
        {
            free(spinners[i]->points);
            delete spinners[i];
        }
        if(0)
        {
            free(ali->points);
            delete ali;
            free(bob->points);
            delete bob;
        }
    }
    if (GameDemo::BLOB)
    { // Free the array of blob points
        free(Blob::points);
        free(Blob::points_debug);
    }

    shutdown();
    return EXIT_SUCCESS;
}
