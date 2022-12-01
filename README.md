# Install Dependencies

## Ubuntu

Install these packages:

```
sudo apt install exuberant-ctags <--- Making tags is integral to my build flow
sudo apt install cscope <------------ Like tags but better but only for C
sudo apt install pkgconf <----------- let Make figure out the --cflags and --libs
sudo apt install make <-------------- build stuff
```

Specific to SDL2, this is the essential package::

```
sudo apt install libsdl2-dev <--------- base
```

These are not required but will make some work easier:

```
sudo apt install libsdl2-mixer-dev <--- easier audio
sudo apt install libsdl2-image-dev <--- work with other image formats
sudo apt install libsdl2-ttf-dev <----- easier font rendering
```

# Basic build and run

Build:

```
make
```

Run:

```
. build/main
```

*Expect a simple rectangle in a window.*

Cycle through 23 color schemes:

- `Space` cycle forward
- `Shift-Space` cycle backward

Quit:

- `q`

Toggle fullscreen:

- `F11`

Interact with the spinning rainbow particles:

- `j` - slower
- `k` - faster
- `J` - spin in smaller circles
- `K` - spin in larger circles

Resize the window to see game art resizing behavior. Game art
maintains constant 16:9 aspect ratio, but pixel size increases,
creating a chunky pixel effect. 

As OS window is dragged to larger sizes, area occupied by game
art increases to the largest integer multiple of the game art
that still fits.

Game art clips starting at lower right if OS window is dragged to
a size smaller than the size where game art still fits.

Also play with `GameArt::scale` by editing the code. Here are
some example values:

- `scale=10` (160x90) : very chunky pixels
- `scale=20` (320x180) : balance between chunkiness and resolution
- `scale=50` (800x450) : game art loses all chunky pixel feel, even at fullscreen

The rainbow-colored static animation bases the number of points
on this `scale` so the animation has a consistent density at the
different game scales.

# Makefile

I start with the Makefile. It *can be* very simple if I keep the
project file structure completely flat. By simple I mean I can
just define compiler and linker flags and skip writing a recipe
for the final executable; the only recipes are for tooling stuff
like the tags file. But I know from experience that it is worth a
little complexity to handle a file structure with folders: it is
easier to "look" at the project and to pick what to track or
ignore with Git.

I turn Vim into my IDE. I want to keep the Vimscript in `.vimrc`
as simple as possible, so I follow a convention with the naming
of the files and folders to match the hard-coded file and folder
names in my `.vimrc`.

## File structure convention

These are my project file structure conventions.

Files:

- source code with main entry point is main.c or main.cpp
- final executable is main (Linux) or main.exe (Windows)

Folders:

- source code for the final executable goes in a folder
- source code for making tags and other project tooling is at the
  top folder level, no subfolder
- builds goes in a folder named `build`

Normally I abhor a foolish consistency. But using a consistent naming scheme lets me hardcode names in Vimscript in my `.vimrc`.

Vim lets me make custom shortcuts for any crazy combination of
things Vim can do and anything I can execute from the shell. The
real power of this super-power comes from creating and modifying
my wizard spells on the fly. This works best when the Vimscript
is short and simple.

I don't want to write generically useful Vimscript. To handle a
larger set of use-cases means a lot of lines of code, harder to
understand in the future, so more friction to adapting to
whatever I need now. The point of my Vimscripted shortcuts is that I have almost zero friction to adapting their behavior on the fly.

So no test coverage either because maintaing the tests as I adapt
behavior and even running the tests after making a change is
adding too much overhead to my work flow.

Besides, I'm going to aggressively user-test whatever shortcuts I
create, so unit testing a Vimscript shortcut wins me nothing.

## Example file tree

Example file tree following my conventions:

```
.
├── build <------------- IGNORE
│   ├── ctags-dlist
│   ├── main
│   └── main.d
├── ctags-dlist.cpp <--- TRACK
├── game-libs <--------- TRACK
│   └── mg_colors.h
├── headers.txt <------- IGNORE
├── Makefile <---------- TRACK
├── README.md <--------- TRACK
├── src <--------------- TRACK
│   └── main.cpp
└── tags <-------------- IGNORE
```

## Minimum viable Makefile

```make
SRC := src/main.cpp
EXE := build/$(basename $(notdir $(SRC)))
HEADER_LIST := build/$(basename $(notdir $(SRC))).d
INC := game-libs

CXXFLAGS_BASE := -std=c++20 -Wall -Wextra -Wpedantic
CXXFLAGS_INC := -I$(INC)
CXXFLAGS_SDL := `pkg-config --cflags sdl2`
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_INC) $(CXXFLAGS_SDL)
LDLIBS := `pkg-config --libs sdl2`

default-target: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

.PHONY: $(HEADER_LIST)
$(HEADER_LIST): $(SRC)
	$(CXX) $(CXXFLAGS) -M $^ -MF $@

build/ctags-dlist: ctags-dlist.cpp
	$(CXX) $(CXXFLAGS_BASE) $^ -o $@

.PHONY: tags
tags: $(HEADER_LIST) build/ctags-dlist
	build/ctags-dlist $(HEADER_LIST)
	ctags --c-kinds=+p+x -L headers.txt
	ctags --c-kinds=+p+x+l -a $(SRC)

.PHONY: what
what:
	@echo
	@echo --- My make variables ---
	@echo
	@echo "CXX          : "$(CXX)
	@echo "CXXFLAGS     : "$(CXXFLAGS)
	@echo "SRC          : "$(SRC)
	@echo "EXE          : "$(EXE)
	@echo "HEADER_LIST  : "$(HEADER_LIST)
	@echo "INC          : "$(INC)

.PHONY: how
how:
	@echo
	@echo --- Build and Run ---
	@echo
	@echo "             Vim shortcut    Vim command line (approximate)"
	@echo "             ------------    ------------------------------"
	@echo "Build        ;<Space>        :make -B  <if(error)> :copen"
	@echo "Run          ;r<Space>       :!./build/main"
	@echo "Run in Vim   ;w<Space>       :!./build/main <args> &"
	@echo "Make tags    ;t<Space>       :make tags"
```

## Vimscript

`make how` lists Vim shortcuts. Below is the Vimscript for those
shortcuts.

### Make tags

- `;t<Space>`

```vim
nnoremap <leader>t<Space> :make tags<CR>
```

### Build

- `;<Space>`
    - build the default target: run `make` with no target name
    - use flag `-B` to force building everything the target needs
        - *overrides `make` dependency checking to build only what's necessary*
        - *avoids doing a `make clean`*

> [!NOTE] `make -B` is only for fast builds
>
> This shortcut only works out well if the build is fast. When
> using a big lib like DearIMGUI, I do something else because I
> cannot wait for all of DearIMGUI to rebuild everytime.

```vim
nnoremap <leader><Space> :call BuildCandCpp()<CR><CR><CR>
function BuildCandCpp()
    echomsg s:happy_kitty
    if (&filetype == 'c') || (&filetype == 'cpp')
        " Make the default target, force rebuild all
        exec "make -B "
        let l:build_failed = CheckIfBuildFailed()
        if l:build_failed | copen | endif
    else
        echomsg s:sad_kitty "This is not a C or C++ file"
    endif
endfunction
```

### Run like normal

- `;r<Space>`
    - run the final executable
    - do not pass any arguments to the executable
    - let Vim switch view back to the shell (where stdout goes)
    - view will stay there until the executable exits

```vim
if CheckOs() == 'linux'
    " echomsg s:happy_kitty "Linux"
    nnoremap <leader>r<Space> :!./build/main<CR>
endif
if CheckOs() == 'windows'
    " echomsg s:trippy_kitty "Windows"
    nnoremap <leader>r<Space> :!./build/main.exe<CR>
endif
```

### Run as if in a Vim window

This is for opening a little window version of the executable
without leaving my Vim view. This way I can "see" my executable
and my code at the same time.

Out-of-the-box, Vim lets me divide the Vim screen into as many
windows as I like with whatever crazy config of windows and
splits. The sizes of these windows are easily changed and Vim
wraps all the text accordingly. It's great.

But Vim is not graphical, it's just text.

"Run as if in a Vim window" means, whatever Vim window my cursor
is in, run the executable and lay it on top of that Vim window.
It is *as if* the executable was running inside of Vim. This is
just an illusion, of course, but it serves my purpose.

- `;w<Space>`
    - run the executable with location and size args
        - *executable opens directly on top of the active Vim window*
    - run the executable as its own process
        - *in Linux, this is appending `&`*
        - *in Windows, this is using PowerShell `Start-Process`*
        - *this keeps my view in Vim -- I don't get bumped out to the shell*
        - *Note that opening a terminal within a Vim window, and
          then running the executable from the shell in that
          terminal, is like running the executable in its own
          process -- this is the quick and dirty way to lay the
          executable window on top of the Vim view.*

```vim
nnoremap <leader>w<Space> :call RunInVimWindow()<CR>:redraw!<CR>:echo GetWindowXYWHinPixels()<CR>
function RunInVimWindow()
    "{{{ Ubuntu : Auto-hide the Dock
    " Appearance Settings -> turn on "Auto-hide the Dock"
    "
    " The "dock" on the left-side of the screen messes up where Ubuntu thinks
    " screen coordinate 0,0 is!
    " Fix this by Auto-hiding the Dock.
    " Now windows are allowed to use this real estate.
    "}}}
    let l:bin = "./build/main"
    if CheckOs() == 'windows'
        let l:bin = l:bin..".exe"
    endif
    let xywh = GetWindowXYWHinPixels()
    let l:x = xywh[0]
    let l:y = xywh[1]
    let l:w = xywh[2] " Make sure Scrollbar=none in minttyrc
    "{{{ Scrollbar=none :
    " No vertical scrollbar on right-hand side of Vim window!
    " Otherwise the reported Vim column width is for real estate
    " less than actual screen width.}}}
    let l:h = xywh[3]
    echomsg s:trippy_kitty "Open at "..l:x..","..l:y.." size: "..l:w..","..l:h
    if CheckOs() == 'linux'
        let l:cmd_str = l:bin.." "..l:x.." "..l:y.." "..l:w.." "..l:h.." &"
    endif
    execute "!"..l:cmd_str
endfunction
```

`GetWindowXYWHinPixels()` is a function I wrote to find out the
size and position of the Vim window in units of screen pixels:

```vim
function GetWindowXYWHinPixels()
    " Get the ID of this window.
    let wid = win_getid()
    " Get its x,y position. Subtract 1 because (0,0) is (1,1).
    let l:x = CursorWidthsToPixels(win_screenpos(wid)[1] - 1)
    let l:y = CursorHeightsToPixels(win_screenpos(wid)[0] - 1)
    let l:w = CursorWidthsToPixels(winwidth(wid))
    let l:h = CursorHeightsToPixels(winheight(wid))
    return [l:x, l:y, l:w, l:h]
endfunction

function CursorWidthsToPixels(c_w) "{{{
    " Maximum window size (size of window if it is the only window in the tab)
    let max_c_w = &columns
    " Assume Vim is fullscreen. Then window W x H in pixels is 1360x768
    let p_w = a:c_w * 1360/max_c_w
    return p_w
endfunction
"}}}
function CursorHeightsToPixels(c_h) "{{{
    " Maximum window size (size of window if it is the only window in the tab)
    let max_c_h = &lines
    let p_h = a:c_h * 768/max_c_h
    return p_h
endfunction
"}}}
```

