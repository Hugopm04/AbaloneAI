#pragma once

#include "abalone/game.hpp"

namespace abalone {

// Runs the whole application in a graphical window: mode selection, settings,
// and play. Human turns are entered by clicking marbles and then clicking a
// highlighted destination -- no coordinates or direction names to look up.
//
// `config` supplies the starting settings; the in-window settings screen may
// change them. Returns a process exit code.
//
// Only built when ABALONE_GUI is enabled (see CMakeLists.txt). The terminal UI
// and the headless arena never call this and never link raylib.
int run_gui(GameConfig config);

}  // namespace abalone
