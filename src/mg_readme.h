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
 * Video runs at one speed, 60FPS, and that speed is derived from the monitor VSYNC.
 *
 * My physics loop is really multiple physics loops, each running at a different speed.
 *
 * - If I need physics faster than 60FPS, my physics loop runs N times per video loop.
 * - If I need physics slower than 60FPS, my physics loop runs once every N times the video loop runs.
 *
 * I use multiple physics loops, so that I am not stuck with picking one speed for all my
 * physics.
 *
 * My physics loop is "locked" to the VSYNC because my physics and render loops are inside
 * my game loop and my game loop repeats on the VSYNC signal.
 *
 * Since I'm basing the physics loop(s) speed(s) on the video loop VSYNC, they are not
 * completely decoupled, but they are decoupled enough for me.
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
 * My game loop runs at 60fps because it's locked to the VSYNC. Inside that game
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
 *
 * Each section of the program is in ALL CAPS to make it easy to search for.
 *
 * I turn the searches into vim shortcuts: ;1 through ;6.
 * zM folds everything to the max, then the search puts my cursor on the line.
 * zv unfolds as much as necessary to view the line.
 * Then zt scrolls the view so that the line is at the top of the window.
 *
 * ```vim
 * nnoremap <leader>1 zM/GAME GLOBALS<CR>zt
 * nnoremap <leader>2 zM/SETUP<CR>zvzt
 * nnoremap <leader>3 zM/INITIAL GAME STATE<CR>zvzt
 * nnoremap <leader>4 zM/UI - EVENT HANDLER<CR>zvzt
 * nnoremap <leader>5 zM/PHYSICS UPDATE<CR>zvzt
 * nnoremap <leader>6 zM/RENDERING<CR>zvzt
 * ```
 *
 * - Globals
 *      - declare generic application global singletons: Window and Renderer
 *      - set up GAME GLOBALS (game-specific globals), organize them using C++ namespaces
 * - SETUP
 *      - seed the RNG
 *      - set up SDL
 *      - quit if there is something wrong with the computer
 *      - define an INITIAL GAME STATE
 * - GAME LOOP
 *      - Everything happens here!
 *          - I don't (explicitly) use threads
 *          - I redraw everything on screen on every video frame
 * - Shutdown
 *      - clean up nice when the game quits:
 *          - free any manually allocated memory
 *          - depending on how it was allocated, memory freeing is done one of these ways:
 *              - call to `SDL_DestroyBlah` for stuff created with `SDL_CreateBlah`
 *              - use C++ `delete` for stuff created with C++ `new`
 *              - use `free` for stuff created with `malloc`
 *
 * Program structure within the game loop: UI>>>>Physics>>>>Rendering
 *
 * No hard and fast rules on how game loop code divides between those three sections. Still
 * finding the approach with the least friction.
 *
 * - I am consistently finding myself moving calculations to the physics section.
 * - UI section feeds the physics best by setting flags and doing little else.
 * - The rendering section does best by visualizing the latest physics calculations
 * - I often start a new "thing" in the rendering section and calculate its stuff there,
 *   then once I want to decouple it from VSYNC, I move it out to the physics section.
 *   So in the end, the rendering loop ends up not doing calculations.
 *
 * - UI - EVENT HANDLER
 *     - handle all events
 * - PHYSICS UPDATE
 *     - consume flags set by in the UI code
 *     - update animations
 *     - dCB curves:
 *          - update dCB curves by modifying control points
 *          - do not find points on curve unless I need that for physics stuff like
 *            collisions
 * - RENDERING
 *     - GAME ART
 *         - draw current animation state to the game texture
 *         - most of the rendering code goes here
 *         - dCB curves:
 *              - find points on curve using control points
 *              - use a single buffer to store all curves:
 *                  - buffer is size of one curve
 *                  - loop:
 *                      - go to next curve
 *                      - calc points, write to buffer overwriting previous curve
 *                      - render what's in the curve buffer
 *     - OS WINDOW
 *         - just a little bit of rendering code
 *           to get that game texture into the OS window
 * *******************************/
/* *************UI code***************
 * Check for events:
 *
 * - keyboard event
 * - mouse event
 * - OS event
 *
 * The UI code doesn't actually do things. I mostly just set flags.
 * So the UI events cause stuff to happen by updating the game state.
 * Then other parts of the program do the actual thing based on the game state.
 *
 * There are two ways to handle keyboard events in SDL.
 *
 * 1. SDL_PollEvent(), then check for a SDL_KEYDOWN event, then check which key
 * 2. SDL_PumpEvents(), then SDL_GetKeyboardState(), then check which key
 *
 * The first method puts a delay between the first keypress and a repeated press.
 * This is the kind of behavior for a tile-based game or navigating text in a text editor.
 *
 * The second method has no delay between the first keypress and a repeated press.
 * This is the kind of behavior for a platformer.
 * It also feels like the repeated presses come faster (rapid fire) in the second method.
 *
 * In GameDemo::BLOB, I assign HJKL for movement using the first method (tile-based),
 * and WASD for movement using the second method (platformer).
 *
 * *******************************/
/* *************Physics code***************
 * - calculate the physics for all the stuff
 * - update game state
 *
 * Physics code does not touch as many categories of state as the UI code.
 * But physics code does a lot more work on the graphics state than the UI code
 * does.
 *
 * The word physics is doing a lot of work here. I really mean animation.
 * Control the speed of animation by putting the state changes in the
 * physics loop, then either running that loop faster at multiples of VSYNC
 * (with a for loop) or at fractions of the VSYNC (using a counter to count
 * video frames before iterating the physics).
 * *******************************/
/* *************Rendering code***************
 * Draw graphics based on the game state.
 *
 * The rendering code does not modify the game state. It reads the game state
 * and modifies the Renderer accordingly. It modifies the Renderer a lot!
 *
 * Any write-access variables in the rendering section (besides the global
 * single Renderer instance) are limited to the scope of the rendering code.
 *
 * A common pattern I find myself doing is to declare a new color within a code block that
 * then sets the renderer to draw with that color.
 * *******************************/

////////////////////
// COMPUTER GRAPHICS
////////////////////

// None of these maths are my original ideas. I learned all of this from Dr. Norman
// Wildberger's videos. These are my attempts at applying his ideas to computer graphics.

/* *************The case for parametric graphics***************
 *
 * -------------------------------------------------------------------
 * | For graphics, use parameter λ and define points as [x(λ), y(λ)] |
 * -------------------------------------------------------------------
 *
 * In school I was taught to think of curves like this: y=f(x).
 *
 * This got me off on the wrong foot with writing graphics calculations because screen
 * coordinates are x and y, so I naively started trying to work out graphics like this:
 * "I want to draw blah curve or simulate blah physics behavior. Given the x-coordinate of
 * this pixel, what should it's y-coordinate be?"
 *
 * That is the wrong way of thinking: I have to do a lot of extra work and the computer has
 * to do a lot of extra work, all with zero benefit.
 *
 * This wrong way of thinking results from an unfortunate name overlap between two very
 * different math scenarios:
 *
 * 1. The notation y=f(x) makes y dependent on x: pick x, get y.
 * 2. The notation x,y is a 2D-coordinate: the y-value has no dependence on the x-value.
 *
 * Clearly these are two different ideas! Why are they both using the letters x and y?
 *
 * I can do whatever I want. Say I keep x as the name of a dependent variable. To avoid
 * this confusion, I stop using it as the name of a dimension.
 *
 * Then let's call the dimensions a and b. A 2D number is a coordinate a,b.
 *
 * That change in symbol choice clearly says "hey, now our calculation space lives on a 2D
 * plane, not on a 1D number line." Now it is clear that I am not making one coordinate
 * dimension dependent on the other coordinate dimension.
 *
 * Well, if they are independent of each other, then each dimension should gets its own
 * function: a=f1(x) and b=f2(x). So a point is [a=f1(x), b=f2(x)], or perhaps I just write
 * [a(x), b(x)].
 *
 * Well, I don't want to completely break with convention. Everyone calls screen pixel
 * coordinates x,y. So let's leave those names intact: x,y.
 *
 * Instead of renaming the coordinate dimensions, let's rename the independent variable
 * from x to λ:
 *
 * -------------------------------------------------------------------
 * | For graphics, use parameter λ and define points as [x(λ), y(λ)] |
 * -------------------------------------------------------------------
 *
 * This raises a lot of interesting questions to come back to. I have four fundamental
 * arithmetic operations for values made of a single number: addition, subtraction,
 * multiplication, division. How do I want to define those operations for 2D numbers? And
 * now that I'm in 2D, are there maybe new operations that are fundamental, like rotation?
 *
 * I haven't pondered those too deeply yet.
 *
 * The question I'm going to address now is why my coordinates are functions of the same
 * independent variable.
 *
 * Making x and y functions of the same variable is called "parameterizing".
 *
 * So why parameterize? Why didn't I go even more general and define points as [x(a), y(b)]
 * where a and b are both independent variables?
 *
 * --------------------------------------------------------
 * | I want a 1D number input to drive a 2D number output |
 * --------------------------------------------------------
 *
 * My screen is 2D -> I want to calculate pixel locations -> I want to output a 2D number.
 *
 * How can a calculation result be expressed as two numbers? Values made of multiple
 * numbers comes up pretty early on in maths. Fractions are lists of two numbers, and yet
 * everyone is comfortable thinking of fractions as representing a single thing (because
 * they can write it in decimal notation, or because they can visualize it as a pizza pie
 * with slices missing).
 *
 * Same idea here. A point is a list of two numbers, x and y. But really, these two numbers
 * represent a single output value: a pixel location. So I am still reducing my calculation
 * result to a "single" thing I can visualize!
 *
 * My input can be 1D or 2D.
 *
 * In drawing and animating, it is very useful to have a 1D number drive my 2D output.
 *
 * If my parametric curves are for an animation (physics!), this 1D number represents time.
 * But even if I'm going to draw all the points at the same time (art!), there are lots of
 * benefits to having a 1D number input that generates points on the screen.
 * *******************************/
 /* *************Parameterize all graphics***************
 * I parameterize all graphics. Everything is some sort of "curve" and I want to express
 * that curve in parametric form, meaning that my `x` and `y` are both functions of
 * parameter `λ` and I generate each next point on the curve by incrementing `λ`.
 *
 * So how do I parameterize a curve?
 * *******************************/
 /* *************Polynomial (linear) curves***************
 * The main work-horse for parameterization is polynomial curves. Parameterize polynomial
 * curves using dCB curves.
 *
 * Note that a straight line is still a polynomial curve, it's just the simplest case of a
 * polynomial curve where the curve's rate of change is constant (so it's a "straight
 * curve").
 *
 * Since straight lines are polynomial curves, I parameterize straight lines too!
 *
 * In fact, dCB curves are generated by parameterizing straight lines!
 *
 * If a curve is not some polynomial, I call it non-linear. I don't have much to say about
 * non-linear curves, so I'm going to hit that before I dive into dCB curves.
 * *******************************/
 /* *************What about non-linear curves***************
 *
 * The big one is the circle. I was surprised when I realized SDL didn't provide a
 * `SDL_RenderDrawCircle`. Then I tried to write my own and I appreciated why the SDL authors
 * decided against providing this in the SDL2 lib. I don't know if this is the reason they
 * left out `SDL_RenderDrawCircle`, but I'm glad they did because I learned a lot trying to
 * make my own.
 *
 * What's so special about circles? Circles are special because the screen points are
 * transcendental when parameterized by angle.
 *
 * Transcendental is potentially bad for computations: need trig functions sinθ and cosθ.
 *
 * If I know ahead of time all the angles I'd ever want to use, I can calculate
 * approximations of these to the desired precision, then turn each of those into a lookup.
 *
 * Or I can blindly toss in `math.h` and call trig functions on any angle and get
 * approximations to any desired precision. I'm guessing `math.h` does an approximation
 * using finite number of terms from an infinite series. I don't know. I suspect this is
 * computationally expensive, but I haven't played around with it. I think the question of
 * computational efficiency ends up depending a lot on the specific hardware. Trig
 * functions must be so ubiquitous in graphics libraries by now, I'd imagine processors
 * have specific modules dedicated to them.
 *
 * But I'm not doing this professionally, so while I am curious to know, I also don't care.
 * I'm excited to explore "maths without transcendentals" as a constraint on the kinds of
 * calculations I tell the computer to do.
 *
 * My RatCircle and Blob demos use rational parameterizations of circles. An interesting
 * consequence of parameterizing the circles this way is that a uniformly spaced sampling
 * of λ [0:1] results in points on the circle that are NOT uniformly spaced! The
 * parameterization maps to a quarter-circle. As λ goes from 0 to 1, the angle goes from 0°
 * to 90°, but the angular spacing between each point gets smaller. For animation purposes,
 * this has the pleasant effect of the particle "slowing down" as it nears the end of a
 * quarter turn. In my RatCircle demo, the particles with trails have a very organic feel
 * to their animation, as if they are squirming the circle in quarter turns. This was not
 * intentional, just a side-effect of parameterizing this way. Much to explore here!
 *
 * So circles, yeah. I can't think of any other non-linear curves that I'd find much use
 * for in a game. If it ever comes up, I'll look into how to parameterize those.
 * *******************************/
/* *************De Casteljau - Bezier (dCB) curves***************
 * - dCB curves produce polynomial curves in 2D
 *      - to go higher dimension I think it's the same math, just a bigger list of numbers,
 *        like, points become a list of three numbers instead of a list of two numbers
 * - dCB curves have a specific start and end point
 * - let parameter t (sometimes called λ) go from 0 to 1
 *      - if parameter goes outside this range, the math still works!
 *      - t < 0 gets points before the curve's start point
 *      - t > 1 gets points after the curve's stop point
 * - dCB curve is DEFINED by its control points
 *      - dCB curve for polynomial order N has N+1 control points
 * - dCB curve is generated by defining the JOIN of adjacent points as a convex affine combination
 * - This is what that process looks like in words:
 *      - connect the control points with straight line segments
 *      - express each of these line segments as a convex affine combination of its end points
 *      - if there are N+1 control points, there are now N line segments
 *      - each line segment is traced from end to end as t varies from 0 to 1
 *      - "trace point" : the point that moves from end to end as t varies
 *          - each line segment has a trace point
 *          - all trace points move at the same time as t varies
 *      - for each pair of adjacent lines, connect their "trace points" to define a new line
 *          - if there are N trace points, there are now N-1 new lines
 *      - these lines are ALSO traced from end to end as t varies from 0 to 1
 *      - and for each pair of adjacent lines, again connect trace points, define new lines
 *      - repeat this until there is only ONE trace point
 *      - this trace point IS the dCB curve
 *
 * My workhorse is 2nd-order curves. I don't think I need to get fancier. Higher order
 * curves mean more control points to store and more computations to do.
 *
 * *******************************/
 /* *************Generate 2nd-order dCB curves***************
 * For a 2nd order curve (a.k.a., a quadratic curve), there are 3 control points: P0, P1,
 * P2. Draw a line segment from P0 to P1 and from P1 to P2. Note that these two line
 * segments are tangents to the curve.
 *
 * Create the convex affine combinations for JOIN(P0,P1) and JOIN(P1,P2). The convex affine
 * combination is a new point on the JOIN of the end points:
 *
 * Q0 = (1-t)*P0 + (t*P1)
 * Q1 = (1-t)*P1 + (t*P2)
 *
 * Now convex affine combination for JOIN(Q0,Q1):
 *
 * R0 = (1-t)*Q0 + (t*Q1)
 *
 * R0(t) is the 2nd-order curve defined by control points P0, P1, P2.
 *
 * At this point, I can calculate points on the curve.
 *
 * But I can take this further, and I think this how it is usually done in practice.
 * Expand the above linear equations into the second order equation:
 *
 * ((1-λ)*(1-λ) * P0) + ((2*λ)*(1-λ) * P1) + ((λ*λ) * P2)
 *
 * This is:
 *
 * (1 - 2λ +  λ^2) * P0 + (2λ - 2λ^2) * P1 + (λ^2)*P2
 *
 * Remember λ is a value from [0:1], so for a specific value of λ,
 * all of the coefficients evaluate to scalars:
 *
 * a*P0 + b*P1 + c*P2
 *
 * Define multiplication of a point by a scalar as K*[x,y] = [K*x,K*y]
 *
 * Again, at this point, I can calculate points on the curve if for some reason this form
 * were more useful. But lets keep going!
 *
 * The coefficients are Bernstein polynomials of degree two. There are three such
 * polynomials, B02, B12, and B22. The first number indicates with control point they are
 * the coefficient for. The second number indicates degree two.
 *
 * Expand these polynomials and write down all powers of λ, even if its coefficient is zero.
 *
 * - Coefficient of P0: (1-λ)*(1-λ) = 1*(λ^0) - 2*(λ^1) + 1*(λ^2)
 * - Coefficient of P1: (2*λ)*(1-λ) = 0*(λ^0) + 2*(λ^1) - 2*(λ^2)
 * - Coefficient of P2:       (λ*λ) = 0*(λ^0) + 0*(λ^1) + 1*(λ^2)
 *
 * Rewriting that as a matrix multiplication:
 *
 *                  (3 x 3)      (3 x 1)           (3 x 1)
 * coeff of P0:  |  1 -2  1  |   | λ^0 |   1*(λ^0) - 2*(λ^1) + 1*(λ^2)
 * coeff of P1:  |  0  2 -2  | x | λ^1 | = 0*(λ^0) + 2*(λ^1) - 2*(λ^2)
 * coeff of P2:  |  0  0  1  |   | λ^2 |   0*(λ^0) + 0*(λ^1) + 1*(λ^2)
 *
 * I wrote powers of λ as a 3x1 matrix. But remember λ is K values uniformally spaced over
 * the range [0:1]. So actually, my λ matrix is 3xK. So I end up with a resulting matrix
 * that has K columns.
 *
 * Say I sample λ at five values: [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]:
 *
 *      |P|      x               |T|                      =              |B|
 *
 *    (3 x 3)                  (3 x 5)                                 (3 x 5)
 *                  λ=0.0 λ=0.2 λ=0.4 λ=0.6 λ=0.8 λ=1.0
 *                  ----- ----- ----- ----- ----- -----
 * |  1 -2  1  |   | 1.0   1.0   1.0   1.0   1.0    1.0 |   | B0(λ1) B0(λ2) B0(λ3) B0(λ4) B0(λ5) |
 * |  0  2 -2  | x | 0.0   0.2   0.4   0.6   0.8    1.0 | = | B1(λ1) B1(λ2) B1(λ3) B1(λ4) B1(λ5) |
 * |  0  0  1  |   | 0.0   0.04  0.16  0.36  0.64   1.0 |   | B2(λ1) B2(λ2) B2(λ3) B2(λ4) B2(λ5) |
 *
 * I named the matrices:
 *
 * - |P| (for polynomial) -- matrix of Bernstein polynomial coefficients for each control point P0, P1, P2
 * - |T| (for λ matrix)   -- matrix of five λ values for each of the three powers of λ
 * - |B| (for Bernstein)  -- matrix of Bernstein polynomials evaluated at each value of λ
 *
 * My (3x5) matrix multiplication result is:
 *
 * |  B0(λ=0)  B0(λ=0.2)  B0(λ=0.4)  B0(λ=0.6)  B0(λ=0.8)  B0(λ=1.0)  |
 * |  B1(λ=0)  B1(λ=0.2)  B1(λ=0.4)  B1(λ=0.6)  B1(λ=0.8)  B1(λ=1.0)  |
 * |  B2(λ=0)  B2(λ=0.2)  B2(λ=0.4)  B2(λ=0.6)  B2(λ=0.8)  B2(λ=1.0)  |
 *
 * Where B0() B1() and B2() are functions that take a value of λ and output the
 * coefficients of P0, P1, and P2 respectively.
 *
 * Note the coefficients of P0, P1, and P2 are ALWAYS these Bernstein polynomials -- this
 * is independent of the control points P0, P1, P2.
 *
 * If I can settle on predetermined curve resolution of K points (sample λ at K values),
 * then I can precompute the matrix. I don't need to think of the B0() B1() B2() as
 * functions, I can just compute the values and store that matrix. I think that ends up
 * being a computational efficiency gain, but I haven't tested this.
 *
 * So if I precompute B = P x T (assuming I want to settle on a predetermined value for K,
 * my curve resolution), then ANY 2nd-order dCB curve I ever need is calculated by picking
 * control points.
 *
 * With B precomputed, and with list of points P0, P1, P2, I think it is computationally
 * cheap enough to do the matrix multiplication {P0,P1,P2} x B every time I want the curve.
 *
 * {P0,P1,P2} is 1x3 and B is 3xK, so I end up with a 1xK.
 *
 * For example, instead of the physics loop calculating all curves and storing that result
 * for the rendering loop, the physics loop can just operate on the control points P0, P1,
 * P2 for each curve, then the renderer can calculate the curve points R0..RK just-in-time
 * as it draws each curve.
 *
 * How big is the memory win here?
 *
 * P0 P1 P2 is a (1 x 3) and my B matrix is a (3 x K). Say I draw all curves with a
 * resolution of 128 points. And say I have 100 curves to render every frame.
 *
 * If I calculate all the curves ahead of time in physics loop and store them for the
 * renderer, then I need 128*100 points. If each point is two floats and each float is 4
 * bytes, that is 128*100*4=51200 bytes.
 *
 * If instead I only update the control points in the physics loop and wait to generate my
 * curves until the rendering loop, then I need 3*100 points. Again, each point is two
 * floats, so that is 3*100*4=1200 bytes.
 *
 * The savings is K/3 bytes per curve. For K=128, that is about 42 bytes per curve. For 100
 * curves total, that's about 4KB.
 *
 * Then I do not have to store memory to hold all curves, I just need memory to hold lists
 * of control points.
 *
 * An interesting aspect of the control points is that scaling the control point scales the
 * curve by the same amount. Similarly, offsetting all the control points offsets the curve
 * by the same amount. This is what is giving me the flexibility to update control points
 * in my physics loop.
 *
 * *******************************/
 /* *************Fit 2nd-order dCB curves to existing data***************
 * Above I start with control points and generate a curve.
 * What about going in the opposite direction: start with a datapoints on some curve and
 * try to fit a dCB curve to it.
 *
 * Like, say I'm starting with locations I want a particle to pass through. Now I need the
 * dCB curves to generate the path that passes through those points.
 *
 * If the curve is really crazy and if I insist on only using 2nd-order dCB curves, I'll
 * need to split the curve up into segments.
 *
 * Identify the end points of the curve segment (these are P0 and P2), now draw tangent
 * lines there (draw the JOIN of two points: the end point with its neighbor). The MEET of
 * these tangent lines is P1.
 *
 * Side note on rotation:
 *
 * The kinds of parabolas I drew in school (y=x^2 stuff) are all in the NOT-rotated case:
 *
 *      - P0.x < P1.x < P2.x
 *      - P1.y is either the max y (concave down) or the min y (concave up) of the
 *        y-coordinates of the three points
 *
 * In this NOT-rotated case, P0 and P2 define locations on the curve with opposite-sign
 * slopes (slope = Δy/Δx for a pair of points very close together on the curve).
 * I think that relationship of the slopes is useful, so I want to generalize it to the
 * arbitrary rotation case.
 *
 * Try sketching some ARBITRARY segment of a quadratic (for example, draw the classic
 * y=x^2, but put the start and end points on the same "side" of the parabola). Draw
 * tangents at the segment end points to find the P1. Note P1 breaks the above
 * "observations" for the NOT-rotated case.
 *
 * But if I just imagine my x-y plane rotated so that the line x=0 bisects the angle formed
 * by the two tangent lines, then the "observations" hold true.
 *
 *
 *
  * *******************************/
