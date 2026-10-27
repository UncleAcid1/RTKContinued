<div align="center">


<br>

<img src="docs/images/readme/logo.png" alt="Rule the Kingdom" width="320">

### A native, open-source C++ port of *Rule the Kingdom*, rebuilt function by function from the original Android game library, so the kingdom can be ruled again on a modern computer.

![status](https://img.shields.io/badge/status-complete%20%C2%B7%201.0-6ee7a0)
![ported](https://img.shields.io/badge/game%20functions%20ported-100%25-6ee7a0)
![fidelity](https://img.shields.io/badge/port-100%25%20faithful-brightgreen)
![platforms](https://img.shields.io/badge/macOS-SDL3%20%2B%20OpenGL-lightgrey)
![offline](https://img.shields.io/badge/play-100%25%20offline-8ab4ff)
![money](https://img.shields.io/badge/real--money%20purchases-none-ff8a8a)
![license](https://img.shields.io/badge/code-MIT-blue)

<br>

<img src="docs/images/readme/port_chapter1.jpg" alt="The port running: Chapter 1, The Story Begins" width="760">
<br><sub><b>This is the port itself</b>, running natively: Chapter 1, "The Story Begins". Not a mock-up, not the original app.</sub>

<br><br>

<img src="docs/images/readme/trailer_intro.gif" alt="Official trailer, opening" width="380">
&nbsp;
<img src="docs/images/readme/port_footage.gif" alt="The port running on macOS" width="380">

<sub>Left: the official 2012 trailer (Game Insight / innoWate). Right: the port, running natively on a Mac.</sub>

<sub>README last updated <b>2026-10-27</b>.</sub>

</div>

---

## Contents

1. [Where things stand](#where-things-stand)
2. [What is ported, and what is not](#what-is-ported-and-what-is-not)
3. [Play it](#play-it) · [Controls](#controls)
4. [Found a bug?](#found-a-bug)
5. [The game that was](#the-game-that-was) · [History](#full-history-and-timeline) · [How it played](#how-the-game-played) · [The people](#the-people-and-companies-behind-it) · [Gallery](#gallery)
6. [Why a port](#why-a-port) · [Scope and rules](#scope-and-rules-of-the-port)
7. [Progress](#progress)
8. [Building](#building-macos) · [Layout](#layout) · [Sources](#sources-and-accuracy-notes) · [Licence and credits](#licence-and-credits)

---

## Where things stand

> ### 🏰 Ready for the realm
> **The port is complete (version 1.0, 27 October 2026).** Every game function of the original
> 5.11 release is ported: the opening tutorial, the city, workers and farms, quests, campaign maps,
> boss fights, spells, crafting, chests, the shops, the daily bonus, the arena and offline PvP,
> saves, sound, music and particle effects, from a new game to the end of the campaign.
>
> The project is finished. From here on it only gets **minor bug fixes**, as they turn up in play.

| Phase | State |
|---|---|
| 1. **Port the code** | ✅ every game function of 5.11; what is left out is online-only, dead in 5.11, or platform code SDL3 replaces |
| 2. **Make it playable** | ✅ new game to campaign finish, with every system working |
| 3. **Bug hunt** | ✅ play-tested; bugs are fixed from the original as they turn up |
| 4. **Polish and release** | ✅ a Mac app with Retina, fullscreen, a frame-rate cap, desktop notifications, and a DMG |

---

## What is ported, and what is not

All of the game is ported, **function by function**, each tagged with its original address (`// @0x...`).

| Area | State |
|---|---|
| Map rendering, GUI, input, dialogs | ✅ done |
| City economy: buildings, workers, farms, taxes, shop | ✅ done |
| Hero, army, quests, items, the opening tutorial | ✅ done |
| Campaign maps, world map, campaign combat, boss fights | ✅ done |
| Spells, rage and mana, buffs | ✅ done |
| Hiring and healing troops, repairs, raids | ✅ done |
| Character info, sets, collections, professions | ✅ done |
| Crafting, chests and keys, the item shop | ✅ done |
| Daily bonus, tavern, calendar events | ✅ done |
| The arena, and PvP against generated rivals | ✅ done (offline stand-in for the server) |
| Sound, music, particles, screen effects | ✅ done |
| Screenshot mode | ✅ done (saves a PNG to `~/Pictures/Rule the Kingdom`) |
| Desktop: Retina, fullscreen, frame-rate cap, notifications, saves | ✅ done |

**The exceptions** (on purpose):

| Exception | Why |
|---|---|
| **Real-money purchases** | Removed. Crystals are earned in play. |
| **Facebook, friends, gifts, the social tabs** | Need the publisher's servers, which are gone. Kept dormant in the code. |
| **Server saves, live events and live PvP** | Replaced by local saves, the calendar and generated rivals. |
| **Admin / editor / debug console** | Not reachable in the shipping game, so deliberately not ported. |

---

## Play it

**You need your own copy of the original game files.** No game data is included in this
repository or in the download. The game asks for the folder that holds them the first time it
starts (the one with `Android files/rule-the-kingdom-5-11-multi-android`, `files` and `_extract`),
and remembers it.

**On a Mac:** download `Rule-the-Kingdom-1.0-macOS-AppleSilicon.dmg` from the
[releases](https://github.com/UncleAcid1/RTKContinued/releases), open it and drag the game to
Applications. The app is signed ad hoc rather than with an Apple developer certificate: the first
time, open it with right-click → Open (or System Settings → Privacy & Security → Open Anyway).

Or build it yourself (see [Building](#building-macos)). Saves go in
`~/Library/Application Support/Rule the Kingdom/`; `--storage <folder>` keeps them somewhere else.

### Controls

The mouse (or trackpad) does what a finger did on the phone: click to tap, drag to scroll, the
wheel or a pinch to zoom.

| Key | Does |
|---|---|
| **Esc** | Back (closes the top window, as Android's Back button did) |
| **F10** | Frame-rate cap: 30 / 60 / the display's rate |
| **F11** (or Cmd+Ctrl+F, Alt+Enter) | Fullscreen on / off |

---

## Found a bug?

Please open an [issue](https://github.com/UncleAcid1/RTKContinued/issues) with:

1. **What you did** (the quest, the map, the window).
2. **What you expected**, and **what happened instead**.
3. A **screenshot**, and the end of the log if it crashed.

---

## The game that was

**Rule the Kingdom** (Russian: *Империя героев*, "Empire of Heroes", as the Russian gameplay video in this archive is titled) is a free-to-play hybrid of
**role-playing, city-building and farming**, published by **Game Insight** and developed by its studio
**innoWate**. Its official site described it as *"an unprecedented blend of RPG action, city-building, farming and storytelling!"*

You play a ruler who has come home to a half-ruined realm. You build houses and workshops for your
subjects, farm and gather resources, then lead a hero and a squad of hired warriors across a
campaign of isometric maps: forests, deserts, snowy mountains and an imperial capital. You fight
monsters and bosses, cast spells, collect and enchant gear, and send your hero into a PvP arena.

<div align="center">
<img src="docs/images/readme/store_city.jpg" alt="A grown kingdom with castle walls" width="31%">
<img src="docs/images/readme/store_snow.jpg" alt="The snowy frontier" width="31%">
<img src="docs/images/readme/store_oilfarm.jpg" alt="The Oil Farm" width="31%">
<br><sub>Official store screenshots (Windows Store listing): a walled kingdom, the frozen frontier, and the Oil Farm.</sub>
</div>

### At a glance

| | |
|---|---|
| **Publisher** | Game Insight (listed as "Game Insight Global" on the Windows Phone Store) |
| **Developer** | innoWate, a studio within Game Insight |
| **Genre** | RPG / city-builder / farming hybrid, isometric, tap-to-move |
| **Original release** | Android (Google Play): April 2012. iOS: June 2012 |
| **Other platforms** | Amazon Appstore (July 2012), Facebook (May 2013), Windows Phone 8 and Windows 8 (2013) |
| **Launch claims** | More than 1,000 quests across 12 campaigns, boss monsters, three schools of magic, magical gems for crafting rare weapon and armor sets |
| **Model** | Free-to-play with two currencies: gold (earned) and crystals (earned slowly or bought), timers you could skip |
| **Final Android version** | 5.11 (October 2014), the code base this port is built from |

---

## Full history and timeline

> Every dated claim below carries its source in [Sources and accuracy notes](#sources-and-accuracy-notes).
> Where a date could not be verified it says so instead of guessing.

### Before the game: Game Insight and innoWate (2009 to early 2012)

- **2009.** Game Insight is founded in **Moscow**. (Wikipedia gives 2009; a 2012 interview dates it to
  December 2009, when it started as a holding company.) It builds a business of publishing
  free-to-play games for Facebook and then mobile, with a network of partner studios.
- **2009.** innoWate, a European social-game studio (LinkedIn lists Riga, Latvia, and a 2009
  founding), is working on social games. Its **My Country**, a nation-building Facebook game,
  later reached over 10 million players according to Game Insight's own 2012 press release, and
  became its first Game Insight title.
- **October 2010.** Game Insight releases *Mystery Manor*, billed as the first hidden-object game
  on Facebook.
- **2010 to 2011.** *Paradise Island* (first called *Resort World* on Facebook) arrives on Android
  in January 2011 and tops Google Play's grossing charts. *Airport City* follows in May 2011.
- **2011.** Game Insight's catalogue reaches about 50 million monthly active users and about
  **$50 million** in revenue for the year (VentureBeat's 2012 profile).
- **March 2012.** *The Tribez* launches.

### The life of Rule the Kingdom

| Date | Event |
|---|---|
| **27 April 2012** | Game Insight announces **Rule the Kingdom** "now available on Android platforms" (Google Play) in a San Francisco-dated press release. It describes a crossover of RPG and city-builder, with over 1,000 quests, 12 campaigns and three schools of magic. Chief Producer **Leonid Sirotin** calls it "the first, of what we plan to be many, crossover mobile titles appealing to casual and core gamers." |
| **June 2012** | iOS launch. The iPad edition (*Rule the Kingdom HD*) was free from day one and the iPhone version was scheduled for 28 June (Joystiq/AOL, 27 June 2012). |
| **July 2012** | Amazon Appstore release (per Wikipedia's Game Insight article). |
| **2012 (Android builds)** | Version 2.07 (the early Android build included in this archive) uses the original engine with data in `.jet` containers. |
| **May 2013** | Facebook canvas version (per Wikipedia). The archive holds many screenshots from it. |
| **2013** | Windows Phone 7.5/8 and Windows 8 versions. The Windows Phone `.xap` in this archive is v1.4.0.5. Ewan Spence reviews version 1.4.0.14 for All About Windows Phone on **17 October 2013** and scores it **76%**. |
| **December 2013** | The newest Windows 8 build in the archive, 5.0.0.39, with the latest content. |
| **19 February 2014** | The official site `rulekingdom.com` is captured (the archived copy is in this folder). It advertises Android, iPhone, iPad, Facebook, Windows 8 and Windows Phone. |
| **October 2014** | Last Android release in the archive: **5.11**, the code base of this port. |
| **Later** | The publisher's servers behind saves, events, friends, PvP and purchases (the game's own domains were `rtk-*.susiworld.com`) stopped working, and the game could no longer be played as designed. **The exact shutdown dates could not be verified**, so none are claimed here. |
| **2026** | This project begins on 2026-10-04. |

### The publisher's trajectory (for context)

- **2012.** Game Insight opens a San Francisco office. Its 2012 plan: 26 games in the year, and revenue
  of over $150 million, triple 2011's.
- **April 2014.** The San Francisco office (eight people) is closed.
- **29 May 2014.** Headquarters moves to **Vilnius, Lithuania**.
- **May 2017.** *Guns of Boom*, a multiplayer shooter, launches.
- **14 June 2022.** Game Insight announces the closure of a Russian legal entity and suspends
  employees' access at those offices.
- Wikipedia's description today: a developer and publisher of more than 45 free-to-play mobile and
  social games, localized in 10 languages and played in 218 countries.

<div align="center">
<img src="docs/images/readme/trailer_end.jpg" alt="End card of the official trailer: innoWate and Game Insight present Rule the Kingdom" width="560">
<br><sub>End card of the official trailer: "innoWate" and "Game Insight" present Rule the Kingdom.</sub>
</div>

---

## How the game played

### The world

A linear, episodic **campaign** of maps joined by a world map: your battered home kingdom, the
emperor's capital, an arena, and villages in trouble. As you level up, the fog lifts on new
regions (the reviews and archived screenshots show green forests, deserts and the snowy north).
Quests arrive as speech bubbles from villagers, refugees and envoys; the opening has you meet
old **Stoutbeard** and restore your kingdom. Heroes can be male or female, and the game's art
shows both a male and a female lead.

<div align="center">
<img src="docs/images/readme/campaign_map.jpg" alt="The campaign world map" width="48%">
<img src="docs/images/readme/select_path.jpg" alt="Select a path: Kingdom, Arena or PvP" width="48%">
<br><sub>The world map (left, a screen recording) and the "Select a path" screen: Kingdom, Arena and PvP (right).</sub>
</div>

### The kingdom

Place houses, farms, workshops and decorations; gather wood and stone; keep your helpers busy;
collect taxes; grow crops (the **Oil Farm** shown above grows sunflowers); craft food, weapons
and armor. Everything runs on **real-time timers**, which is where the crystal currency came in.

<div align="center">
<img src="docs/images/readme/farm_plot.jpg" alt="A farm plot being harvested" width="31%">
<img src="docs/images/readme/buy_farm.jpg" alt="Buying a farm plot" width="31%">
<img src="docs/images/readme/fix_houses.jpg" alt="The fix-the-houses quest" width="31%">
</div>

### The hero, the army and the fight

Your hero and the soldiers you hire (pikemen, archers, mages and more) march into hostile
maps. Standard fights resolve as automated turns; **boss fights** let you choose a stance each
turn. A fury meter charges as you trade blows, and spells and elixirs spend it. Equipment, orbs
and enchanting with gems shape your power; the inventory and shop show it.

<div align="center">
<img src="docs/images/readme/boss_skeleton.jpg" alt="Boss battle: Skeleton Knight" width="31%">
<img src="docs/images/readme/victory.jpg" alt="Victory over the Skeleton Knight" width="31%">
<img src="docs/images/readme/death.jpg" alt="Defeat screen: heal your hero" width="31%">
<br><sub>The Skeleton Knight boss, the victory screen, and the defeat screen, where the hero is healed with time or crystals.</sub>
</div>

<div align="center">
<img src="docs/images/readme/inventory.jpg" alt="The inventory" width="31%">
<img src="docs/images/readme/fb_shop.jpg" alt="The shop" width="31%">
<img src="docs/images/readme/pvp_ambush.jpg" alt="Battle of the Kings, a PvP ambush" width="31%">
<br><sub>Inventory, shop, and a "Battle of the Kings" ambush against another player's hero.</sub>
</div>

### Quests, campaigns and the emperor

<div align="center">
<img src="docs/images/readme/quest.jpg" alt="A quest menu" width="31%">
<img src="docs/images/readme/campaign_select.jpg" alt="Campaign briefing with progress and rewards" width="31%">
<img src="docs/images/readme/campaign_complete.jpg" alt="Campaign complete" width="31%">
<br>
<img src="docs/images/readme/emperor.jpg" alt="Speaking with the emperor" width="31%">
<img src="docs/images/readme/empire.jpg" alt="The empire" width="31%">
<img src="docs/images/readme/downloading.jpg" alt="Downloading an update: new campaigns and locations" width="31%">
</div>

### What the reviewers said

> **All About Windows Phone, 17 October 2013 (Ewan Spence, 76%).** The game offers very long-term
> play, but timers and freemium design are what stretch it out; without them the reviewer
> estimates the quests could be finished in about nine days. Praised as one of the better RPGs on
> Windows Phone; criticised for a story that is too on-rails.

---

## The people and companies behind it

> Only facts that sources support are listed. Rule the Kingdom's own game files and store pages
> credit **companies, not individual developers**, so no designer, programmer or artist is named
> here. Guessing names would not be accurate.

### Game Insight (publisher)

- Founded **2009 in Moscow**; headquarters moved to **Vilnius, Lithuania** on 29 May 2014.
- In 2012, VentureBeat described a leadership team of women, led by founder and CEO
  **Alisa Chumachenko**, who had worked in the games business since 2000 and previously headed
  marketing and advertising at Astrum Online Entertainment until its 2009 merger into Mail.Ru
  Group. Others named: **Darya Trushkina** (VP business development, leading the San Francisco
  expansion), **Olga Skvortsova** (COO), **Alexandra Pestretsova** (VP marketing) and
  **Leonid Sirotin** (VP production; quoted in Rule the Kingdom's launch release as Chief Producer).
- It worked by owning 51% to 75% of about 15 partner studios (300+ developers) in Russia, Belarus and
  Ukraine, publishing their games on Facebook, Android and iOS.
- Well-known titles: *Mystery Manor* (2010), *Paradise Island* (2010 on Facebook; top-grossing on
  Google Play for more than 26 consecutive weeks on Android), *Airport City* (2011), *The Tribez*
  (2012), *Rule the Kingdom* (2012), *Guns of Boom* (2017).

<div align="center">
<img src="docs/images/readme/gi_logo.png" alt="Game Insight logo" height="48">
&nbsp;&nbsp;&nbsp;
<img src="docs/images/readme/innowate_logo.png" alt="innoWate logo" height="40">
</div>

### innoWate (developer)

- A studio within Game Insight, called a "veteran studio" in the launch release. Its first Game
  Insight title was **My Country** (over 10 million players according to the publisher), a
  nation-building Facebook game Adweek described as the work of a European social-game developer.
- LinkedIn lists the company's headquarters in **Riga, Latvia** (founded 2009, 11 to 50 employees).
  That is a single, thinly sourced listing, so treat the location as likely rather than certain.
- Rule the Kingdom's own text credits "Game Insight, innoWate" and says the trademarks belong to their owners.

### Not verified

- Individual designers, programmers and artists of Rule the Kingdom: none credited in any source found.
- The exact date the servers went offline and the exact dates the game left each store.
- An old secondhand summary in this archive claims 2016 to 2018 shutdowns and "abandonware" status;
  it is labelled by its own author as AI-written and unreliable, so it is **not** used as a source here.

---

## Gallery

<div align="center">

**Official trailer, feature reel** (Game Insight / innoWate, 2012)

<img src="docs/images/readme/trailer_features.gif" alt="Official trailer feature reel" width="560">

<br>

| | | |
|:-:|:-:|:-:|
| <img src="docs/images/readme/trailer_worldmap.jpg" width="280"><br><sub>Reign over a prospering kingdom</sub> | <img src="docs/images/readme/trailer_snow.jpg" width="280"><br><sub>Drive your foes from the frozen frontier</sub> | <img src="docs/images/readme/trailer_farm.jpg" width="280"><br><sub>Farm the land to provide for your subjects</sub> |
| <img src="docs/images/readme/trailer_spells.jpg" width="280"><br><sub>Unleash the fury of deadly spells</sub> | <img src="docs/images/readme/trailer_city.jpg" width="280"><br><sub>Make friends in high places</sub> | <img src="docs/images/readme/pc_overview.jpg" width="280"><br><sub>Fog of war, desktop/web version</sub> |

**From the official website** (`rulekingdom.com`, captured 19 February 2014)

<img src="docs/images/readme/site_header.jpg" width="23%">
<img src="docs/images/readme/site_kingdom_building.jpg" width="23%">
<img src="docs/images/readme/site_battle_magic.jpg" width="23%">
<img src="docs/images/readme/site_legendary_treasures.jpg" width="23%">

<sub>Kingdom building, battle and magic, legendary treasures, eccentric characters: the site's own feature sections.</sub>

</div>

---

## Why a port

Like many free-to-play games of its era, Rule the Kingdom depended on its publisher's servers:
saves, events, friends, PvP and purchases all went online. When those services stopped, the game
could no longer be played as it was. No server data or content beyond version 5.11 survives, so this
project brings back what the game files themselves contain, and makes it run on current hardware
(including Apple Silicon Macs, which cannot execute the original 32-bit ARM Android build).

<div align="center">
<img src="docs/images/city.jpg" alt="The port: the city" width="31%">
<img src="docs/images/shop.jpg" alt="The port: the shop" width="31%">
<img src="docs/images/requirements.jpg" alt="The port: building requirements" width="31%">
<br><sub>Screenshots of the port itself: the city, the shop and a requirements dialog.</sub>
</div>

## A note from the maintainer

<div align="center">
<sub><i>Rule the Kingdom, kept alive by one person who never wanted to say goodbye to it.</i></sub>
</div>

RTKContinued is maintained solely by **[UncleAcid1](https://github.com/UncleAcid1)**. It began as
nostalgia: afternoons spent playing this game with their mother, and the wish to give those hours
back to anyone who remembers them. It has been a labour of love, and a personal investment, to
bring it back to life.

Part of the pull is the look. The game's rendered, storybook artwork and gentle whimsy feel less
like a phone game and more like the casual PC classics of the late 1990s and early 2000s: warm,
handmade and unhurried. It is a time capsule of how casual gaming once felt, and it remains more
intuitive than most of its kind, then or now.

The port was made possible with Claude Opus and Claude Sonnet 5.5, under constant human
supervision, creativity, research and ingenuity. Every function is checked against the original,
and nothing in the game's art or content is generated; all of it is the original work.

## Scope and rules of the port

- **A faithful port, not a remake.** Every function is translated from the original's code and
  tagged with its address (`// @0x...`); its formats, formulas and even its quirks are kept.
  Every guess was checked against the original's code before the project closed.
- **Offline first.** Everything that needed the server is rebuilt to run locally: seasonal events
  follow the calendar, crystals are earned in play (daily rewards, chests, quests) instead of bought,
  the arena and PvP run offline against generated rivals, and friends stay dormant. Online code is kept dormant so a community
  server could be added later. **There are no real-money purchases.**
- **Platforms:** macOS (released), with SDL3 and OpenGL; the code keeps Linux and Windows within reach.
- **Saves** stay in the original's format.
- **No game data is included.** You need your own copy of the original files (the 5.11 Android
  APK assets and the `.kbf` expansion packs). This repository holds only the port's source and the
  tools that read the original formats.

## Progress

| # | Milestone | Status |
|---|---|---|
| 1 | Map rendering | ✅ done |
| 2 | GUI, input, dialogs | ✅ done |
| 3 | City economy (buildings, workers, shop, farms, saves) | ✅ done |
| 4 | Hero, army, quests, items, decoration jobs, campaign maps, campaign combat, new-game tutorial | ✅ done |
| 5 | The rest of the game: spells, bosses, world map, hiring, crafting, chests, shops, tavern, events, arena, PvP | ✅ done |
| 6 | Sound and effects | ✅ done |
| 7 | Desktop release and polish | ✅ done |
| 🏰 | **Version 1.0** (27 October 2026) | ✅ **complete**; minor bug fixes only from here on |

The full log of every step lives in [STATUS.md](STATUS.md), the changes per version in
[CHANGELOG.md](CHANGELOG.md), and the per-function map of the original in
[docs/port_inventory.md](docs/port_inventory.md).

**At the close, 2026-10-27:** every game function of the original ported, in 23 days (the first
commit was on 4 October 2026); about 100,000 lines of C++, about 5,000 functions tagged with their
original address. What stays unported is online-only, dead in 5.11 (nothing reaches it), or platform
code that SDL3 replaces (`python3 tools/inventory.py --steps`).
Language and stack: C++17, SDL3, OpenGL, pugixml, libpng, libjpeg, zlib, libvorbis, FreeType.
Code base ported: Android 5.11 `libkingdom.so` (ARMv7, October 2014).

<div align="center">
<img src="docs/images/readme/map0_render.jpg" alt="The port's renderer drawing the starting kingdom map from the original map file" width="48%">
<img src="docs/images/readme/map0_schematic.jpg" alt="Schematic of map_0 produced by the map-format tools" width="48%">
<br><sub>The starting kingdom drawn by this project's own renderer (left), and a schematic from its map-format decoder (right).</sub>
</div>

## Building (macOS)

Requirements: CMake 3.20+, Apple's command line tools (`xcode-select --install`), and SDL3,
pugixml, libpng, libjpeg, libvorbis and libogg
(`brew install cmake sdl3 pugixml libpng jpeg-turbo libvorbis libogg`). FreeType 2.4.5 and
SDL_ttf 2.0.11 are bundled in `third_party/`.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target app
```

That leaves `build/Rule the Kingdom.app` (the Homebrew libraries copied in, signed ad hoc). The
bare program runs too: `./build/rtk --root <folder with the game files> --storage <save folder>`.

### The Mac disk image (DMG)

To share the game, make a disk image: a window with the game's icon, an arrow and the
Applications folder over the original world map, ready to drag and install.

```
bash tools/make_dmg.sh
```

It installs what it needs with Homebrew the first time (and `create-dmg`), builds HEAD in a clean
worktree (local, uncommitted changes are left out), and leaves
`dist/Rule-the-Kingdom-<version>-macOS-<chip>.dmg` (the version comes from `CHANGELOG.md`; a build
after the release's commit adds the commit, `1.0+3f2a1b9`). The first time, macOS asks whether
Terminal may control Finder: allow it, that's how the window is laid out. The background is
`packaging/macos/dmg/background.png` (+ `@2x`), drawn by `tools/make_dmg_background.py`; the icon is
`packaging/macos/AppIcon.icns`, made from the original Android icon by `tools/make_app_icon.py`.

<a id="layout"></a>
## Layout and reverse-engineering workspace

- `src/` the port (engine, game logic, GUI, HUD, windows)
- `tools/` Python/Ghidra tools for the original's formats and for reading the decompile
- `docs/` working notes
- `third_party/` bundled libraries

Code base: Android 5.11 `libkingdom.so` (ARMv7, Oct 2014, about 10.5k exported C++ symbols).
Primary data: the 5.11 APK `assets/data`. Secondary: the Win8 5.0.0.39 Appx and the `.kbf` expansion packs.

- `tools/rtk_extract.py`: extracts the resource containers (Android `.jet`, Win8 `res_data.bin`, `.kbf`).
- `tools/ghidra_headless.sh`: runs Ghidra headless with the Homebrew JDK 21.
- `tools/ghidra_scripts/ExportDecomp.java`: dumps decompiled C for every function.
- `ghidra/`: the Ghidra project (`rtk511`), which can be opened in the GUI with `ghidraRun`.
- `out/`: generated output (assets, `decomp.c`, `functions.tsv`). It can be regenerated at any time.

### Regenerate
```
python3 tools/rtk_extract.py android "../Android files/rule-the-kingdom-5-11-multi-android/assets/data" out/assets/android511
python3 tools/rtk_extract.py win8    ../_extract/win5/Assets                            out/assets/win8
python3 tools/rtk_extract.py kbf     ../files/main.27.kbf                               out/assets/kbf_main27
tools/ghidra_headless.sh "$PWD/ghidra" rtk511 -import "../Android files/rule-the-kingdom-5-11-multi-android/lib/armeabi-v7a/libkingdom.so" \
    -scriptPath "$PWD/tools/ghidra_scripts" -postScript ExportDecomp.java "$PWD/out"
```

### Container format (verified on every entry)
Index entry: `u32 name_len` (includes the NUL), then the name, then 8 little-endian `u32` fields:
`hash, width, height, is_data, bpp, offset, size, chunk`.
- Android: payload is in `data/<chunk>.jet`. `offset` is global, so subtract the smallest offset of any entry in that chunk.
- Win8: payload is at `res_data.bin[offset:offset+size]`.
- KBF: header `u32 magic=0x75B4A351, u32 index_size, 3×u32`, then the index, then data at `20 + index_size + offset`.
- XML, xmlb, map `.bin` and TTF payloads are gzipped on Android (and some on Win8).

## Sources and accuracy notes

Facts about the game and companies were checked against these sources (accessed 2026-10-18):

- Game Insight press release, 27 April 2012: [Rule the Kingdom now available on Android](https://news.cision.com/game-insight/r/game-insight-announces-rule-the-kingdom-now-available-on-android-platforms,c9253189)
- Wikipedia: [Game Insight](https://en.wikipedia.org/wiki/Game_Insight) (also saved as a PDF in the archive)
- VentureBeat: [With 4 women leading 300 men, Game Insight will triple revenues in 2012](https://gamesbeat.com/game-insight-aims-to-triple-growth-in-mobile-games-this-year-exclusive/)
- Joystiq/AOL, 27 June 2012: Rule the Kingdom comes to iOS (summarised from search results; the page itself no longer loads)
- All About Windows Phone, 17 October 2013: [Review: Rule The Kingdom](https://allaboutwindowsphone.com/reviews/item/18573_Rule_The_Kingdom.php)
- Adweek, on innoWate and My Country; LinkedIn and GamePressure for innoWate's location and publisher link
- Primary material in this archive: the 2014 capture of rulekingdom.com, the official trailer, the Android 2.07 and 5.11 packages, the Windows 8 and Windows Phone builds, and the game's own localized strings

Images and video frames on this page come from the archived official site, the official trailer,
store listings and recordings of the original game, and screenshots of this port. All Rule the
Kingdom artwork and trademarks belong to their respective owners (Game Insight and innoWate); they
are shown here for documentation and preservation. This project is not affiliated with them.

## Licence and credits

**The port's code** (everything in `src/`, `tools/` and `packaging/`) is released under the
[MIT licence](LICENSE), © 2026 UncleAcid1. The bundled libraries in `third_party/` keep their own
licences.

**The game itself is not covered by it.** *Rule the Kingdom*, its art, music, text, data and
trademarks belong to **Game Insight** (publisher) and **innoWate** (developer). None of it is
included here: players need their own copy of the original game files. This project is a
non-commercial preservation effort and is not affiliated with either company.

**Credits**
- *Rule the Kingdom* (2012-2014): developed by innoWate, published by Game Insight.
- The port: [UncleAcid1](https://github.com/UncleAcid1), with Claude (Anthropic) under constant
  human supervision.
- Built on SDL3, OpenGL, pugixml, libpng, libjpeg-turbo, zlib, libvorbis / libogg, FreeType and
  SDL_ttf.
