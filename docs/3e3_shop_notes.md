# 3e.3 ShopWindow: decoded facts (ported: src/windows/ShopWindow.cpp)

Everything here was read from the decompile/asm of libkingdom.so 5.11. The next step is to write
`src/windows/ShopWindow.cpp`, replacing the stand-ins (IsVisible/Show/Hide/GetInfoPanelX), then wire
`BottomCityWindow::OnBuild` @0x25dbec (`ShopWindow::Show()`, and at tutorial 0x5b
`OnSelectItemID(0x8e); ShowArrowOnItem(0x8e)`). The castle panel's Shop button is ItemShopWindow
(items, milestone 4), not this.

Dump it again with:
`tools/fn.sh '^ShopWindow::' | python3 tools/picsym.py | python3 tools/picvar.py`

Caveats:
- picvar mis-resolves some globals in OnItemInfo's building branch by a constant -0x56c, e.g.
  g_62cf68 means 0x62d4d4. Use the map below.
- Wide (UTF-32) format strings are read with `python3 tools/wstr.py ADDR`.

## Already ported for the shop (committed with these notes)
- **SWPrintf / ToWideString** (global; game/StringTable.h): the exact format rules, including the
  chained-specifier quirk. Unit-tested: "%d/%d", "%$d" → 1,234,567, "%03d", "%+d", "%.2f", "%s".
- **StlSort::Sort** (engine/StlSort.h): STLport introsort. The shop sorts each tab list **3 times**
  (verified in asm at 0x34b134..0x34b2cc), so ties end up in the original's order.
- **GameData::EnumBuildings / EnumDecors** (file order, first occurrence of an id) and
  GetMaxBuildingCount @0x11f1d8.
  - GameData::BuildingImage = BuildingData::GetImage @0x11eea8.
  - New DecorData fields: tab +0x24, subtab +0x28, visid +0x2c, buy +0x30, needLevel +0x54,
    cost1 +0x44 (gold), cost2 +0x48 (crystals).
  - BuildingData: tab +0x24, visid +0x2c, buy +0x30, level +0x78. requiresQuest is +0x68.
  - The +0xb4 "exchange limit" is never set by the loader, so it is 0.
- **Map functions:** GetBuildingCount @0x1b7068, GetBuildingMaxUpgrade @0x1b7118,
  GetPendingWorkerCount @0x1ba618, GetUsedWorkerCount @0x1ba6dc.
- **GameState functions:** GetPlayerWorkersCount @0x1927bc and GetMaxWorkerCount @0x19a7a8 are
  real now. TopCity's people counter adds the pending workers, as the original does.
- **NotEnoughWindow:**
  - Requirements: resources, level, population, items, building counts, building levels and the
    exchange limit.
  - The dialog parts (Show/Hide/SetDescriptionText/SetActionCallback) are still 3e.4 stand-ins.
- **Shared widgets:** Shared::TabHolder (LoadFrom @0x342294, SetMode @0x340974) and
  HouseLiving/Training/Converting/Producing/Decoration/LivingWorkerInfo.
- **GUI::DummyWindow().**

## FunctionalWindow "ShopWindow" (static ctor at 0x346ff4)
- Constructed with: init=Init, deinit=Deinit, click=Click, setZ=SetZ, hide=Hide, move=Move.
- Init then sets: show=Show, update=Update, back=OnBack, zRange=ReturnMediumZRange (0.02).
- `wnd->shown` (+0x15) is set by Show, and cleared by OnTweenOutInfo (which also calls
  WindowHide + MoveWindowDown).
- IsVisible @0x346d14 = root->visibleSelf || infoPanel->visibleSelf.

## Globals (0x62d4cc block, plus 0x60f3a4..)

| Address | Meaning |
|---|---|
| 62d4cc | root Main_buildings_container.xml |
| 62d4d0 | info panel Building_info_panel.xml |
| 62d4d4 | name_text |
| 62d4d8 | text_desc |
| 62d4dc | CostInfo "building_menu_cost_holder" (see below) |
| 62d544 | HouseLivingInfo "building_purpose_holder.purpose_living_house" |
| 62d554 | Training "…purpose_hire" |
| 62d560 | Converting "…purpose_converting" |
| 62d570 | Producing "…purpose_producing" |
| 62d5b4 | Decoration "…purpose_decoration" |
| 62d5bc | LivingWorker "…purpose_living_house_worker" |
| 62d5d0 | icon_lock |
| 62d5d4 | text_level_req |
| 62d5d8 | text_req_upgrade |
| 62d5dc | ContentScroller (see below) |
| 62d6e8 | info panel x offset (0) |
| 62d6ec | cells: vector of BuildingInfo 0x24 (see below) |
| 62d718 | cell count (5 on small screens, else 6) |
| 62d71c | container x offset (0x38 on small screens, × scale) |
| 62d720 | MovementEffect for root (OnTweenMove / OnTweenIn / OnTweenOut) |
| 62d724 | MovementEffect for info (onHidden = OnTweenOutInfo) |
| 62d728 | TabHolder[9] |
| 62d86c | building lists [10 tabs] |
| 62d8e4 | decoration lists [10 tabs] |
| 62d95c | current tab type |
| 62d960 | button_close_building_mode |
| 62d964 | selected cell |
| 62d968 | arrow callback id |
| 62d96c | container_build.button_left |
| 62d970 | container_build.button_right |
| 62d974 | initing flag |
| 62d975 | small screen (HighDPI ? W < 1300 : W < 1000) |
| 62d978 | container_build.back_light_blue |
| 62d97c | close clickArea |
| 60f3a4 | container y margin (10 × scale) |
| 60f3a8 | info y margin (2 × scale) |
| 60f3ac | arrow item (-1) |
| 60f3b0 | show bars on hide (1) |
| 60f3b4 | scale = root->scale |

- **CostInfo:** +0 root, +4 text_cost, then 6 parts of 0x10 "%s.building_req_icon_%02d", each:
  - root
  - "%s.icon" (takesZ)
  - "%s.text_enough"
  - "%s.text_not_enough"
- **ContentScroller fields:**
  - windows = cells' roots, each with SetClipRect(&viewport)
  - viewport = mask "container_build.building_menu_mask" rect; track = mask
  - horizontal = 1, perLine = 1, unkA0 = cell count
  - itemSize = cell[1].x - cell[0].x
  - padEnd = (int)(scale × 10)
  - onScroll = UpdateContents
  - bounce (+0xe0) is changed by OnCellChange
- **BuildingInfo cell** ("container_build.box_%02d"; root SetOnClick(OnItemSelect(i))):
  - +0 root, +4 .bg_default, +8 .back_building_crystal_item
  - +c .icon_gold_quest_new (takesZ), +10 .icon_lock, +14 .text
  - +18 .text_status (MoveWindow(0, -3))
  - +1c .image_holder_idle (takesZ)
  - +20 data
- Boxes beyond the cell count up to 6 are hidden, and 7..9 are always hidden.

## Init @0x34a358
- **Root:** RegisterUI(..., scale -1 = fit, extraW small ? -0x92 : -0x22, fontScale 1.0).
  - RegisterTopWindow(root); hidden.
  - Positioned at ((W - w)/2, H - h).
- **Arrows:** button_left / button_right: OnCellChange(0/1) with sound "ui_click".
- **Tabs:** tab_%02d (1..9) → TabHolder::LoadFrom(root, name, true), with OnTabSelect(i) and sound
  "ui_tab".
- **Tab icons from res_files/1Original/shoptabs.xml:**
  - Tab index for each Xtab id: table {0,1,2,3,4,10,5,6,7,8,9}; ids ≥ 11 and indices ≥ 9 are
    skipped.
  - Icon names: Production→Icon_b_production, Farm→Icon_b_farm, Master→Icon_b_masters,
    Decor→Icon_b_decor, Expand→Icon_b_expand, War→Icon_b_war, Artist→Icon_b_artists,
    anything else→b_houses.
  - Applied as tabs[idx].icon->SetTexture(icon, keepSize false).
- **Info panel:** RegisterUI(Building_info_panel, scale, fontScale 1.0); RegisterTopWindow; hidden.
  - Fields loaded as in the globals table.
  - The close clickArea calls Hide (sound "ui_swing_out").
  - The close button is moved by MoveWindow(-(int)(scale × 24), 0).
- **Lists:**
  - Every EnumBuildings b with tab ≤ 9 and buy goes into buildingLists[tab]. A visid of 0 becomes
    500, 501, ...
  - The same for decorations (their own counter, also from 500).
  - Each list is sorted 3× by key: buildings by level (+0x78, unsigned), decorations by needLevel
    (signed).
- **State:** selected = 0, tab = 0, initing = 0.
- **Size and offsets:**
  - back_light_blue->SetSize(w - (int)(itemSize × (small ? 4.275 : 3.275)), h).
  - On small screens: button_right->MoveWindow(-itemSize, 0) and viewport.right -= itemSize.
  - Then the offsets are scaled: 62d71c ×= scale, 60f3a4 = 10 × scale, 60f3a8 = 2 × scale.
- **Tables:** TabIndexToType = {0,1,2,3,4,6,7,8,9}. Type 5 (land patches) never shows:
  GetTabItemCount(5) = 0, so FillPatches is not needed.

## GetTabItemCount(type)
- Type 5 → 0.
- Types 4 and 7 → decoration list.
- Types 8 and 9 → decoration list if Setting "roads_enabled" == 1, else the building list.
- Everything else → building list.

## Show @0x34d73c
- HUDWindow::ExitFarm if on a farm (3f).
- If !wnd->shown: WindowShow(false).
- BattleBarWindow::HideAllTasks (M4).
- SetHoverVisiblity(false, false); wnd->shown = 1; MoveWindowOnTop(true).
- If root is hidden:
  - Main slide: Animate(x, H, x, H + 60f3a4 - root->h, true), where x = 62d71c + (W - w)/2.
  - If info is hidden: Animate(ix, H, ix, H + 60f3a8 - info->h, true), where
    ix = 62d6e8 + W - info->w.
  - HUDWindow::ShowMainUI(false) (a no-op); hide BattleBar, BeltBar and BottomCity.
- Show root, info and the close button.
- For each tab i: SetMode(type(i) == current, false); tab root visible iff GetTabItemCount(type) != 0.
- FillBuildings(); Update(0); OnItemInfo(selected, false); PreloadWindow(root); sound "ui_swing_in".

## Hide @0x3479b8
- **Root already hidden:** if info is visible, slide info down to y = H, starting from
  H + 60f3a8 - h; then return.
- **Root visible:**
  - Slide root down to H.
  - If !60f3b0: close->SetVisibility(false). Otherwise slide info down and show
    BattleBar/BeltBar/BottomCity.
  - Then: 60f3b0 = 1; Update(0); BuildingPlacement::Decline if active.
  - If arrowItem != -1: set it to -1, HideArrow, SetArrowVisibleWindowLimit(0).
  - Finally: arrowCb = 0; SetArrowVisibleWindowLimit(0).

## Smaller functions
- **SetZ:** returns unless wnd->shown.
  - root->SetZ(z).
  - Root position: (62d71c + (W - w)/2, (slide active ? H : H - h) + 60f3a4).
  - info->SetZ(z - 0.001).
  - Info position: (62d6e8 + W - w, (slide active ? H : H - h) + 60f3a8).
  - UpdateClip().
- **UpdateClip / OnTweenMove:** cell roots → UpdatePosition.
- **OnTweenIn:** FillBuildings + Sort(14).
- **OnTweenOut:** root hidden.
- **OnTweenOutInfo:** info hidden, close shown; WindowHide(false); shown = 0; MoveWindowDown(true);
  ShowAllTasks (M4); SetHoverVisiblity(true, true).
- **OnCellChange(dir):** scroller.bounce += itemSize (dir 0) or -= itemSize (dir 1).
- **Update(dt):**
  - Tutorial 0x5b and arrowItem paths: the arrow on cell 0 / arrowItem, with screen-space ArrowAt.
    UNVERIFIED (tutorial).
  - If root is visible: scroller.Update(dt); button_left enabled = CanMoveLeft;
    button_right enabled = CanMoveRight.
- **Move:** scroller.Move.
- **Click:**
  - BuildingHovers::ClickOnArrow is not ported, so treat it as false.
  - If arrowItem != -1 && !pressed: set arrowItem to -1 and HideArrow.
  - Pass to the scroller first, if root is visible and tutorial != 0x5b. If it returns false, try
    info->Click(x, y, p, false), then root->Click(x, y, p, false).
  - If neither takes it, root is visible, secondTutorial == 0x100 and placement is not active:
    Hide() and return true. Otherwise return false.
  - Any earlier success returns true.
- **OnBack:** only outside the city tutorial while shown. Placement inactive → Hide, return true.
  Otherwise, if the placement controls are visible → BuildingPlacement::Decline, return true.
  Otherwise false.
- **OnTabSelect(i):**
  - If the type changes: firstIndex = 0, HandleMove(-offset), selected = 0.
  - Set the tab; SetMode on every tab; clear every cell image (SetTexture(null)).
  - OnItemInfo(0, false); FillBuildings().
- **OnSelectItemID(id):** finds the building list (types 0..8) that contains the id; tab index =
  the count of non-empty tabs before it. Then OnTabSelect(index); ScrollIntoView; FillBuildings.
- **ShowArrowOnItem(id):** SetArrowVisibleWindowLimit(2). For each visible cell whose data id == id:
  arrowItem = cell; OnItemInfo(cell, false).
- **ShowBestOfTab(t):** the building with the highest level (+0x78, strictly greater, so the first wins); Show();
  OnSelectItemID(its id).
- **OnItemSelect(i):** Decline the placement if active; selected = i; OnItemInfo(i, true); OnBuy().
  A tap buys straight away.
- **Load…Requirements:**
  - Decorations: Reset; AddLevel(needLevel); gold cost1; crystals AdjustCrystalCost(cost2).
  - Buildings: Reset; AddLevel(level +0x78); AddPopulaion(costPopulation +0xa8); resources 0..10
    from cost[]; gold; crystals; AddBuildingLevel(requiredId, requiredCount) if requiredId != 0;
    SetExchangeLimit(0).
- **SetInfoLine(i < 6, icon, text, enough):** shows part i; icon set, enabled = enough; both texts
  set; text_enough is shown iff enough, text_not_enough iff not. If i ≥ 6: puts
  "SetInfoLine() Too many requirements".

## FillBuildings @0x34c958
- itemCount = GetTabItemCount(tab).
- Types 4, 7, 8 and 9 → FillDecorations + UpdateClip. Type 5 → FillPatches (unreachable).
- Otherwise hide every cell (data = 0). For k < min(count - firstIndex, cellCount):
  - Show the cell; data = list[k + firstIndex]; text = name; icon_gold_quest_new hidden.
  - bg_default shown iff cb ≤ 1 (sic: `1 - cb` when cb ≤ 1); crystal background shown iff cb != 0.
  - atMax = max != 0 && count ≥ max, where count = GetBuildingCount(id) and max = GetMaxBuildingCount.
  - LoadBuildingRequirements(k + firstIndex).
- **Status per cell:**
  - **atMax:** icon_lock hidden; status shown with "%d/%d" (count, max). Image disabled.
  - **level > player level:** lock shown; status = SWPrintf(SHOP_UNLOCK "Level %s required",
    ToWideString(level)). Image disabled.
  - **requirements fail:** lock hidden; status "NO_RESOURCES". Image disabled.
  - **requiresQuest done or none:** lock hidden. If max < 0x400, status "%d/%d", else status
    hidden. Image enabled.
  - **otherwise (quest locked):** lock shown; if Tasks::GetTask (M4) → status "SHOP_UNLOCK_TASK".
    Image disabled.
  - "%d/%d" is (count, max), checked in the asm at 0x34d130.
- image_holder_idle->SetEnabled(enabled); FitImageIntoWindow(holder, BuildingImage(data, 0), true,
  true).
- Then UpdateClip and Sort(14).

## FitImageIntoWindow @0x176a28 (to port into GUI)
- sx = win->w / tex->w; s = min(sx, win->h / tex->h); cap = root ? root->scale : 1;
  s = min(s, cap).
- SetTexture(tex, true, (int)(tex->w × s/cap), (int)(tex->h × s/cap), true, 0).
- With the two bools false: sx = 1, and s = sx.

## FillDecorations
- Like FillBuildings. bg_default shown if cost2 == 0 || cost1 != 0, else the crystal background.
- Image from DecorData. LoadDecorationRequirements.
- Status:
  - needLevel > level → lock + SHOP_UNLOCK.
  - Requirements fail → "NO_RESOURCES".
  - Otherwise lock and status hidden, image enabled.

## OnItemInfo(i, _) @0x34900c
Common setup: hide the CostInfo root and the 6 part roots; text_cost = "HIRE_TROOPS_COST"; hide
every purpose root and text_req_upgrade.

**Decoration tabs:**
- name = GetString(name); desc = GetString(name + "_DESC_B").
- Decoration purpose shown with text "BUILDING_DECORATION". Cost shown.
- Line 0: gold cost1 (if != 0; icon GetResourceMapIconName(8), ToWideString, enough = cost ≤ amount).
- Next line: crystals AdjustCrystalCost(cost2) with icon 9.
- lock / text_level_req visible iff level < needLevel; text_level_req = ToWideString(needLevel).

**Building tabs** (data = list[i + firstIndex]; return if out of range):
- name / desc as above.
- **Purpose by class (+0x1ac):**
  - Class 0 with givePopulation 0 → HouseLiving: time = GetTimeString(collectTime +0x34, false);
    icon gold; tax = ToWideString(collectMoney +0x38).
  - Class 0xc or 7 → Training:
    - Text by id: 0x12 → string at LAB_0034a2b0, 0x8e → LAB_0034a2c8, 0x69 → LAB_0034a2f8,
      0x93 → "BUILDING_HIRE_WIZARDS".
    - Icon by id: 0x12 → string at LAB_0034a298, 0x8e → LAB_0034a2e0, 0x69 → "army_archer_60",
      0x93 → "army_mage_60".
    - The unresolved strings still need resolving with tools.
  - Class 2 with a delivery contract:
    - **Converting** if missions[0].item == null (always, until Items are ported): text
      "BUILDING_CONVERING"; from = icon of priceResource, to = icon of rewardResource.
    - **Producing** otherwise (and for class 0xd with a contract): text "BUILDING_PRODUCING"; for
      i < 5 missions, the item image or icon(mission.icon) into icons[i]; hide confirms and locks.
  - Class 0 with givePopulation → LivingWorker: time, gold icon, tax =
    givePopulation × 5 + collectMoney, workers = givePopulation.
- **requiredId (+0x6c) != 0:**
  - met = GetBuildingCount(required) != 0 && MaxUpgrade(required) ≥ requiredCount.
  - SWPrintf(W_NEED_STUFF, ToWideString(requiredCount)). Its result looks unused.
  - If !met: text_req_upgrade shown with SWPrintf(NEED_BUILDING_LEVEL, name of required,
    ToWideString(requiredCount)).
- **If text_req_upgrade is hidden:** show the cost lines.
  - Gold +0x44, then crystals AdjustCrystalCost(+0x48), then resources 0..10 where cost != 0.
  - If GetPlayerWorkersCount - GetUsedWorkerCount < costPopulation (+0xa8): line icon "b_houses",
    text ToWideString(costPopulation), enough = true.
- lock and level text from level +0x78.

## OnBuy @0x347c4c
- NotEnoughWindow::Hide.
- **Decoration tabs:** LoadDecorationRequirements.
  - OK: new Decor{id}; 60f3b0 = 0; Hide(); BuildingPlacement::DecorBought(decor,
    cell.image->sprite (+0x4c), false).
  - Not OK: OnShowRequirements.
- **Building tabs:**
  - Return if atMax.
  - requiresQuest not done → popup "HIRE_TROOPS_TASK_LOCK" or "SHOP_UNLOCK_TASK" (Tasks: M4).
  - If GetPlayerWorkersCount + GetPendingWorkerCount + givePopulation > GetMaxWorkerCount →
    PopupWindow::Show(SWPrintf(PEOPLE_LIMIT, ToWideString(max)), PopupWindow::Hide).
  - LoadBuildingRequirements; if it fails → OnShowRequirements.
  - Tutorial 0x5b → SetInteractionLock(false), HideArrow, tutorial = 0x5c. The arrowItem reset as
    in Hide.
  - new Building{baseX = maxY = 0, data, id}; SetOpened() (the preview shows the finished
    building, so BuildingPlacement's test hack `shop.opened = true` was right).
  - 60f3b0 = 0; Hide(); BuildingBought(b, cell image sprite, false); delete b.
- **The cell's sprite is passed:** so BuildingHovers::AddItemMovement @0x26c124 is now needed (the
  icon flies to the preview over 1 s). It is used in BuildingPlacement BuildingBought/DecorBought.

## OnShowRequirements
- Tab 5 → return.
- Decorations: LoadDecorationRequirements; if they fail → NotEnoughWindow SetDescriptionText
  ("REQUIREMENT_BUILD"), SetActionCallback(OnItemSelect(selected), "PERFORM_BUY"), Show.
- Buildings: return if atMax or quest-locked; the rest is the same with the building requirements.

## Still open
- BuildingPlacement::UpdateCost: format "%s$%$d" at 0x5a86d4; its target window is unknown.
- BattleBarWindow::HideAllTasks/ShowAllTasks (quest lines, M4).
- Resolved since: AddItemMovement (ported), the Training strings (BUILDING_HIRE_GOBLINS /
  WARRIORS / ARCHERS, icons goblin_60 / army_warrior_60).
