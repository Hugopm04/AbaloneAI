cmake_minimum_required(VERSION 3.16)
project(abalone_ia CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

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

add_library(abalone_engine STATIC ${ENGINE_SOURCES} ${AGENT_SOURCES})
target_include_directories(abalone_engine PUBLIC include)
target_link_libraries(abalone_engine PUBLIC Threads::Threads)

add_executable(abalone src/main.cpp)
target_link_libraries(abalone PRIVATE abalone_engine)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(abalone_tests tests/test_rules.cpp)
target_link_libraries(abalone_tests PRIVATE abalone_engine)
add_test(NAME rules COMMAND abalone_tests)
