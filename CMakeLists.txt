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

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(abalone_tests tests/test_rules.cpp $<TARGET_OBJECTS:abalone_agents>)
target_link_libraries(abalone_tests PRIVATE abalone_engine)
add_test(NAME rules COMMAND abalone_tests)
