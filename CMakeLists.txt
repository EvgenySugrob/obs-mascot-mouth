cmake_minimum_required(VERSION 3.24)

project(obs-mascot-mouth VERSION 0.1.0 LANGUAGES CXX)

option(MASCOT_BUILD_PLUGIN "Build the OBS plugin" ON)
option(MASCOT_BUILD_TESTS "Build the dependency-free state-machine tests" ON)

add_library(mascot-mouth-core STATIC
  src/mouth-state.cpp
  src/mouth-state.hpp
)
target_compile_features(mascot-mouth-core PUBLIC cxx_std_17)
target_include_directories(mascot-mouth-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(mascot-mouth-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MASCOT_BUILD_PLUGIN)
  if(NOT TARGET OBS::libobs)
    find_package(libobs CONFIG REQUIRED)
  endif()

  add_library(obs-mascot-mouth MODULE
    src/mascot-source.cpp
    src/mascot-source.hpp
    src/plugin-main.cpp
  )
  target_compile_features(obs-mascot-mouth PRIVATE cxx_std_17)
  target_link_libraries(obs-mascot-mouth PRIVATE OBS::libobs mascot-mouth-core)
  target_compile_definitions(obs-mascot-mouth PRIVATE
    MASCOT_PLUGIN_VERSION="${PROJECT_VERSION}"
  )
  set_target_properties(obs-mascot-mouth PROPERTIES PREFIX "")

  if(MSVC)
    target_compile_options(obs-mascot-mouth PRIVATE /W4 /permissive- /wd4201)
  else()
    target_compile_options(obs-mascot-mouth PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  if(WIN32)
    install(TARGETS obs-mascot-mouth
      RUNTIME DESTINATION "obs-plugins/64bit"
      LIBRARY DESTINATION "obs-plugins/64bit")
    install(DIRECTORY data/ DESTINATION "data/obs-plugins/obs-mascot-mouth")
  elseif(APPLE)
    install(TARGETS obs-mascot-mouth LIBRARY DESTINATION "obs-plugins")
    install(DIRECTORY data/ DESTINATION "data/obs-plugins/obs-mascot-mouth")
  else()
    include(GNUInstallDirs)
    install(TARGETS obs-mascot-mouth LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATADIR}/obs/obs-plugins/obs-mascot-mouth")
  endif()
endif()

if(MASCOT_BUILD_TESTS)
  enable_testing()
  add_executable(mascot-mouth-core-tests tests/mouth-state-test.cpp)
  target_link_libraries(mascot-mouth-core-tests PRIVATE mascot-mouth-core)
  add_test(NAME mascot-mouth-core-tests COMMAND mascot-mouth-core-tests)
endif()
