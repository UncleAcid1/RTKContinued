cmake_minimum_required(VERSION 3.20)
project(RuleTheKingdom CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(pugixml REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenGL REQUIRED)

add_subdirectory(third_party)   # FreeType 2.4.5 + SDL_ttf 2.0.11, the versions the game shipped

add_executable(rtk
  src/main.cpp
  src/engine/FileManager.cpp
  src/engine/SystemFuncs.cpp
  src/engine/TextInput.cpp
  src/engine/Image.cpp
  src/engine/Render.cpp
  src/engine/Resources.cpp
  src/game/Rand48.cpp
  src/game/SaveManager.cpp
  src/game/GameData.cpp
  src/game/Map.cpp
  src/game/Game.cpp
  src/game/MapMovement.cpp
  src/game/Background.cpp
  src/game/StringTable.cpp
  src/game/Building.cpp
  src/game/Contracts.cpp
  src/game/GameState.cpp
  src/game/MetaData.cpp
  src/game/Setting.cpp
  src/engine/Text.cpp
  src/engine/IconManager.cpp
  src/gui/GUI.cpp
  src/gui/Textfield.cpp
  src/gui/WindowManager.cpp
  src/hud/HUDWindow.cpp
  src/hud/Shared.cpp
  src/hud/ContentScroller.cpp
  src/hud/TopCityWindow.cpp
  src/windows/CityRenameWindow.cpp
  src/windows/PopupWindow.cpp
  src/windows/SettingsWindow.cpp
  src/hud/PlayerTopWindow.cpp
  src/hud/CastleTopWindow.cpp
  src/hud/BeltBarWindow.cpp
  src/hud/BattleBarWindow.cpp
  src/hud/BottomCityWindow.cpp
  src/hud/TaskHolderWindow.cpp
)
target_include_directories(rtk PRIVATE src)
target_compile_definitions(rtk PRIVATE GL_SILENCE_DEPRECATION)
# -ffp-contract=off: ARMv7 VMLA/VMLS round the product and the sum separately; no fused multiply-add.
target_compile_options(rtk PRIVATE -Wall -Wextra -Wno-unused-parameter -ffp-contract=off)
target_link_libraries(rtk PRIVATE SDL3::SDL3 pugixml::pugixml PNG::PNG JPEG::JPEG ZLIB::ZLIB OpenGL::GL sdl_ttf2011)
