//
// Created by hoy on 12/10/25.
//

#ifndef ATLAS_HEXMAPDATA_H
#define ATLAS_HEXMAPDATA_H
#include <vector>

#include "SDL3/SDL_stdinc.h"

struct HexTile
{
    float r, g, b, a;
};

class HexMapData
{
    std::pmr::vector<HexTile> _tiles;

public:
    HexMapData();
    std::pmr::vector<HexTile>& GetTiles() { return _tiles; }
};


#endif //ATLAS_HEXMAPDATA_H
