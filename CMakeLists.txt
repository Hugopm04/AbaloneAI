cmake_minimum_required(VERSION 3.16)
project(abalone_ia CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

# The graphical UI is optional. Turn it off (-DABALONE_GUI=OFF) for headless
# machines and CI: the terminal UI and the batch arena never touch raylib.
option(ABALONE_GUI "Build the graphical UI (requires raylib)" ON)

if(ABALONE_GUI)
  # Locate raylib by hand rather than via find_package(). The raylib-config.cmake
  # that MSYS2 ships resolves its paths through pkg-config, which is not part of
  # a minimal MinGW install -- when it is missing the config reports success but
  # leaves the include dir unset, and the GUI silently drops out of the build.
  get_filename_component(_abalone_cc_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_abalone_prefix "${_abalone_cc_dir}" DIRECTORY)

  find_path(RAYLIB_INCLUDE_DIR NAMES raylib.h HINTS "${_abalone_prefix}/include")
  find_library(RAYLIB_LIBRARY NAMES raylib HINTS "${_abalone_prefix}/lib")

  if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
    add_library(abalone_raylib INTERFACE)
    target_include_directories(abalone_raylib INTERFACE "${RAYLIB_INCLUDE_DIR}")
    target_link_libraries(abalone_raylib INTERFACE "${RAYLIB_LIBRARY}")
    if(WIN32)
      target_link_libraries(abalone_raylib INTERFACE opengl32 gdi32 winmm)
    endif()
    message(STATUS "Graphical UI enabled (raylib: ${RAYLIB_LIBRARY})")
  else()
    message(WARNING
      "raylib not found -- building without the graphical UI. "
      "Install it (pacman -S mingw-w64-ucrt-x86_64-raylib) or pass -DABALONE_GUI=OFF "
      "to silence this.")
    set(ABALONE_GUI OFF)
  endif()
endif()

# ---------------------------------------------------------------------------
# Add your own AIs here. One line per agent source file.
# ---------------------------------------------------------------------------
set(AGENT_SOURCES
  agents/random_agent.cpp
)

set(ENGINE_SOURCES
  src/board.cpp
  src/move.cpp
  src/agent.cpp
  src/game.cpp
  src/ui.cpp
  src/arena.cpp
)

add_library(abalone_engine STATIC ${ENGINE_SOURCES})
target_include_directories(abalone_engine PUBLIC include)
target_link_libraries(abalone_engine PUBLIC Threads::Threads)

# Agents register themselves from a static initialiser (REGISTER_AGENT). In a STATIC
# library nothing references those objects, so the linker drops them and the registry
# comes up empty. An OBJECT library forces every agent's object file into the final
# binary, which is what makes self-registration work.
add_library(abalone_agents OBJECT ${AGENT_SOURCES})
target_link_libraries(abalone_agents PRIVATE abalone_engine)

add_executable(abalone src/main.cpp $<TARGET_OBJECTS:abalone_agents>)
target_link_libraries(abalone PRIVATE abalone_engine)

if(ABALONE_GUI)
  target_sources(abalone PRIVATE src/gui.cpp)
  target_link_libraries(abalone PRIVATE abalone_raylib)
  target_compile_definitions(abalone PRIVATE ABALONE_GUI)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(abalone_tests tests/test_rules.cpp $<TARGET_OBJECTS:abalone_agents>)
target_link_libraries(abalone_tests PRIVATE abalone_engine)
add_test(NAME rules COMMAND abalone_tests)
