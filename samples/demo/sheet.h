#pragma once

// Where each tile sits in content/sheet.png. The sheet is a fixed grid, every tile
// outlined in magenta with its name under it, so it can be edited by hand. To add a tile:
// draw it in the next cell and add its name here, in the same order.

#include <coldx/coldx.h>

#include <string_view>

namespace sheet {

constexpr int CellWidth = 48;
constexpr int CellHeight = 64;
constexpr int Columns = 3;
constexpr int Tile = 32;

constexpr std::string_view Names[] = {"ship", "star", "gem", "meteor", "spark", "heart"};

// The tile's rectangle in the sheet. An unknown name is a bug: it returns the first tile.
constexpr coldx::Rectangle Get(std::string_view name) {
    int index = 0;
    for (int i = 0; i < static_cast<int>(std::size(Names)); ++i)
        if (Names[i] == name) index = i;
    return {(index % Columns) * CellWidth + (CellWidth - Tile) / 2, (index / Columns) * CellHeight + 4, Tile, Tile};
}

} // namespace sheet
