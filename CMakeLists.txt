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
  src/engine/Timer.cpp
  src/engine/TextInput.cpp
  src/engine/Image.cpp
  src/engine/Render.cpp
  src/engine/Resources.cpp
  src/game/Rand48.cpp
  src/game/SaveManager.cpp
  src/game/GameData.cpp
  src/game/Map.cpp
  src/game/BossIntroEffect.cpp
  src/game/Fog.cpp
    src/game/Locations.cpp
    src/game/Notifications.cpp
    src/game/Spells.cpp
    src/game/SpellMovement.cpp
    src/game/ParticlePresets.cpp
    src/game/MapCamera.cpp
    src/game/MapDecor.cpp
    src/game/MapSpawn.cpp
  src/game/Game.cpp
  src/game/MapMovement.cpp
  src/game/Background.cpp
  src/game/StringTable.cpp
  src/game/Building.cpp
  src/game/BuildingHovers.cpp
  src/game/BuildingMovement.cpp
  src/game/BuildingPlacement.cpp
  src/game/HiddenObjects.cpp
  src/game/Contracts.cpp
  src/game/Sets.cpp
  src/game/Chests.cpp
  src/game/Collections.cpp
  src/game/Professions.cpp
  src/game/EntityData.cpp
  src/game/Animation.cpp
  src/game/AI.cpp
  src/game/AIState.cpp
    src/game/AICombat.cpp
    src/game/Combat.cpp
    src/game/ArenaCombat.cpp
    src/game/BossCombat.cpp
    src/game/EntityCombat.cpp
    src/game/Projectile.cpp
    src/game/AIEnemy.cpp
    src/game/AIFarm.cpp
  src/game/AIPlayer.cpp
  src/game/Entity.cpp
  src/game/EntityManager.cpp
  src/game/SoldierSlots.cpp
  src/game/Squad.cpp
  src/game/GameState.cpp
  src/game/MetaData.cpp
  src/game/MetaExpression.cpp
  src/game/Tags.cpp
  src/game/Items.cpp
  src/game/PlayerEquipmentInfo.cpp
  src/game/PvP.cpp
  src/game/PvPServer.cpp
  src/game/Tasks.cpp
  src/game/Tavern.cpp
  src/game/TasksHelp.cpp
  src/game/EntityMeta.cpp
  src/game/Override.cpp
  src/game/Setting.cpp
  src/engine/Text.cpp
  src/engine/IconManager.cpp
  src/gui/GUI.cpp
  src/gui/Textfield.cpp
  src/gui/TextStyleManager.cpp
  src/gui/WindowManager.cpp
  src/hud/HUDWindow.cpp
  src/hud/Shared.cpp
  src/hud/ContentScroller.cpp
  src/hud/TopCityWindow.cpp
  src/hud/TutorialWindow.cpp
  src/hud/BottomBattleWindow.cpp
  src/hud/PlayerHolder.cpp
  src/windows/ArenaDifficultyWindow.cpp
  src/windows/ArenaEventWindow.cpp
  src/windows/CityRenameWindow.cpp
  src/windows/ConfirmPurchaseWindow.cpp
  src/windows/CraftItemWindow.cpp
  src/windows/CraftSuccessWindow.cpp
  src/windows/CraftingWindow.cpp
  src/windows/ExchangeWindow.cpp
  src/windows/FoundItemsWindow.cpp
  src/windows/ChapterWindow.cpp
  src/windows/CampaignCompleteWindow.cpp
  src/windows/CampaignInfoWindow.cpp
  src/windows/CampaignPerfectedWindow.cpp
  src/windows/ChestOpenedWindow.cpp
  src/windows/ChestsWindow.cpp
  src/windows/ChoosePathWindow.cpp
  src/windows/ItemShopWindow.cpp
  src/windows/LandExpandedWindow.cpp
  src/windows/LandWindow.cpp
  src/windows/LevelInfoWindow.cpp
  src/windows/LevelUpWindow.cpp
  src/windows/LoadingScreenWindow.cpp
  src/windows/PopupSelectionWindow.cpp
  src/windows/PopupWindow.cpp
  src/windows/RaidCompleteWindow.cpp
  src/windows/RaidInfoWindow.cpp
  src/windows/StartRaidingWindow.cpp
  src/windows/SultanArrivingWindow.cpp
  src/windows/RepairWindow.cpp
  src/windows/SettingsWindow.cpp
  src/windows/ShopWindow.cpp
  src/windows/SelectHQWindow.cpp
  src/windows/DailyBonusWindow.cpp
  src/windows/HealSquadWindow.cpp
  src/windows/HireTroopsWindow.cpp
  src/windows/NeedItemWindow.cpp
  src/windows/TaskCompleteWindow.cpp
  src/windows/TaskInfoWindow.cpp
  src/windows/TaskListWindow.cpp
  src/windows/NotEnoughWindow.cpp
  src/windows/CharacterInfoWindow.cpp
  src/windows/GlobalMapWindow.cpp
  src/windows/BossIntroWindow.cpp
  src/windows/BossOutroWindow.cpp
  src/windows/ArenaTurnWindow.cpp
  src/windows/PlayerBattleInOutWindow.cpp
  src/windows/SetInfoWindow.cpp
  src/hud/PlayerTopWindow.cpp
  src/hud/CastleTopWindow.cpp
  src/hud/BeltBarWindow.cpp
  src/hud/BattleBarWindow.cpp
  src/hud/BottomCityWindow.cpp
  src/hud/BottomFarmWindow.cpp
  src/hud/TaskHolderWindow.cpp
  src/hud/HoverWindows.cpp
  src/hud/HealHoverWindow.cpp
  src/hud/WorldHintHoverWindow.cpp
  src/hud/InfoHoverWindows.cpp
  src/hud/ItemHoverWindow.cpp
)
target_include_directories(rtk PRIVATE src)
target_compile_definitions(rtk PRIVATE GL_SILENCE_DEPRECATION)
# -ffp-contract=off: ARMv7 VMLA/VMLS round the product and the sum separately; no fused multiply-add.
target_compile_options(rtk PRIVATE -Wall -Wextra -Wno-unused-parameter -ffp-contract=off)
target_link_libraries(rtk PRIVATE SDL3::SDL3 pugixml::pugixml PNG::PNG JPEG::JPEG ZLIB::ZLIB OpenGL::GL sdl_ttf2011)
