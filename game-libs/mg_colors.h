#ifndef __MG_COLORS_H__
#define __MG_COLORS_H__

namespace Colors
{ // 
    /* *************Colors from Steve Losh badwolf.vim***************
     *      _               _                 _  __
     *     | |__   __ _  __| | __      _____ | |/ _|
     *     | '_ \ / _` |/ _` | \ \ /\ / / _ \| | |_
     *     | |_) | (_| | (_| |  \ V  V / (_) | |  _|
     *     |_.__/ \__,_|\__,_|   \_/\_/ \___/|_|_|
     * 
     *      I am the Bad Wolf. I create myself.
     *       I take the words. I scatter them in time and space.
     *        A message to lead myself here.
     * 
     * A Vim colorscheme pieced together by Steve Losh.
     * Available at http://stevelosh.com/projects/badwolf/
     * *******************************/

    // Greys
    constexpr SDL_Color plain          = {0xf8,0xf6,0xf2,0xff};
    constexpr SDL_Color snow           = {0xff,0xff,0xff,0xff};
    constexpr SDL_Color coal           = {0x00,0x00,0x00,0xff};
    constexpr SDL_Color brightgravel   = {0xd9,0xce,0xc3,0xff};
    constexpr SDL_Color lightgravel    = {0x99,0x8f,0x84,0xff};
    constexpr SDL_Color gravel         = {0x85,0x7f,0x78,0xff};
    constexpr SDL_Color mediumgravel   = {0x66,0x64,0x62,0xff};
    constexpr SDL_Color deepgravel     = {0x45,0x41,0x3b,0xff};
    constexpr SDL_Color deepergravel   = {0x35,0x32,0x2d,0xff};
    constexpr SDL_Color darkgravel     = {0x24,0x23,0x21,0xff};
    constexpr SDL_Color blackgravel    = {0x1c,0x1b,0x1a,0xff};
    constexpr SDL_Color blackestgravel = {0x14,0x14,0x13,0xff};

    // Colors
    constexpr SDL_Color dalespale      = {0xfa,0xde,0x3e,0xff};
    constexpr SDL_Color dirtyblonde    = {0xf4,0xcf,0x86,0xff};
    constexpr SDL_Color taffy          = {0xff,0x2c,0x4b,0xff};
    constexpr SDL_Color saltwatertaffy = {0x8c,0xff,0xba,0xff};
    constexpr SDL_Color tardis         = {0x0a,0x9d,0xff,0xff};
    constexpr SDL_Color orange         = {0xff,0xa7,0x24,0xff};
    constexpr SDL_Color lime           = {0xae,0xee,0x00,0xff};
    constexpr SDL_Color dress          = {0xff,0x9e,0xb8,0xff};
    constexpr SDL_Color toffee         = {0xb8,0x88,0x53,0xff};
    constexpr SDL_Color coffee         = {0xc7,0x91,0x5b,0xff};
    constexpr SDL_Color darkroast      = {0x88,0x63,0x3f,0xff};

    constexpr SDL_Color list[] =
                            {
                                snow,
                                coal,
                                plain,
                                brightgravel,
                                lightgravel,
                                gravel,
                                mediumgravel,
                                deepgravel,
                                deepergravel,
                                darkgravel,
                                blackgravel,
                                blackestgravel,
                                dalespale,
                                dirtyblonde,
                                taffy,
                                saltwatertaffy,
                                tardis,
                                orange,
                                lime,
                                dress,
                                toffee,
                                coffee,
                                darkroast,
                            };
    enum
    {
        SNOW,
        COAL
    };

    constexpr int count = sizeof(list)/sizeof(SDL_Color);
    void next(int& index)
    {
        index++;
        if (index >= count) index = 0;
    }
    void prev(int& index)
    {
        if (index == 0) index = count;
        index--;
    }

    int contrasts(int index)
    { // Return the index of the contrasting color

        switch(index)
        {
            case  0: return 1;                          // coal on snow
            case  1: return 0;                          // snow on coal
            case  2: return 1;                          // coal on plain
            case  3: return 1;                          // coal on brightgravel
            case  4: return 1;                          // coal on lightgravel
            case  5: return 1;                          // coal on gravel
            case  6: return 0;           /*>^.^<*/      // snow on mediumgravel
            case  7: return 0;                          // snow on deepgravel
            case  8: return 0;                          // snow on deepergravel
            case  9: return 0;                          // snow on darkgravel
            case 10: return 0;                          // snow on blackgravel
            case 11: return 0;                          // snow on blackestgravel
            case 12: return 1;           /*>^.^<*/      // coal on dalespale
            case 13: return 1;                          // coal on dirtyblonde
            case 14: return 0;                          // snow on taffy
            case 15: return 1;                          // coal on saltwatertaffy
            case 16: return 0;           /*>^.^<*/      // snow on tardis
            case 17: return 1;                          // coal on orange
            case 18: return 1;                          // coal on lime
            case 19: return 1;                          // coal on dress
            case 20: return 1;                          // coal on toffee
            case 21: return 0;                          // snow on coffee
            case 22: return 0;                          // snow on darkroast
            default: return 1;
        }
    }
}

#endif // __MG_COLORS_H__
