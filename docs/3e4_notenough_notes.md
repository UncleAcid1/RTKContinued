# 3e.4 decode notes: NotEnoughWindow dialog, ConfirmPurchaseWindow, speed-up

Read from the decompile/asm of libkingdom.so 5.11. Ported in 3e.4 part 1 (see STATUS.md):
ConfirmPurchaseWindow, the NotEnoughWindow dialog and the speed-up hooks. Corrections found
while porting: SpeedupBuilding's +0x184 is BuildingData::produceResource (a tree, lumber 0,
gets level 6), not produceAmount; the Find arrow callbacks use the screen point of
(minX +0x28, minY +0x38) and ArrowAt(baseX, minY +0x38) (port field names); ToWideString(format,
...) @0x22df6c formats into 4 rotating 0x80-character buffers.

Dump the functions again with:
`tools/fn.sh '^NotEnoughWindow::' | python3 tools/picsym.py | python3 tools/picvar.py`

The SWPrintf argument shuffling is noise. Filter it out with the regex used in the session
(the lines `local_/uStack_/iStack_ = ...` and `piVarN[k] = ...`).

## ConfirmPurchaseWindow (fully decoded, 0x299888..0x29a034)

- Layout: `Window_popup_spend_crystals.xml` / `.png`, scale -1, font scale DAT_0029a240 (1.0).
  - RegisterTopWindow; hidden.
  - Tablet flag at 0x60f1d4; scale at 0x60f1d8.
- Effects and windows:
  - Tween: CreateTweenEffect(root, null, OnTweenIn, OnTweenOut).
  - icon: "pvp_winner_holder.icon_60" (takesZ).
  - text_price.
  - sliced_dark_bg (+0x5f = 1).
  - text_header.
- Buttons (Shared::ButtonTripleInfo):
  - button_1 (OK): onClick OnOk.
  - button_2 (Cancel): onClick OnCancel.
  - button_3: loaded, then hidden in UpdateContents.
- Window functions:
  - fn.zRange / update / show / back are set by Init.
  - Click: root->Click(x, y, p, false); returns root->visibleSelf (read before the click).
  - Move: no-op.
- Globals:
  - 0x61c618: callback (owned).
  - 0x61c61c: price.
  - 0x61c620: skip flag.
- SetParameters(cb, price, skip): delete the old cb, then store all three.
- Show:
  - If !skip and secondTutorial == 0x100:
    - shown = 1; MoveWindowOnTop(true).
    - If the root is hidden: WindowShow(false); AnimateIn; GameState::RaiseGamePauseState.
    - Root visible; UpdateContents; PreloadWindow.
  - Otherwise: run cb straight away.
- UpdateContents:
  - icon = GetIcon("crystal_60"), keepSize.
  - text_price = ToWideString(price).
  - button_3 hidden.
  - header = SWPrintf(GetString("v5_crystal_confirm"), ToWideString(price)).
  - button_1.ShowGreen("HUD_OK", "gold_confirm"); button_2.ShowBlue("CANCEL", "gold_cancel").
- OnOk: AnimateOut if visible. The callback runs in OnTweenOut.
- OnCancel: delete cb, cb = null, Hide (AnimateOut if visible).
- OnTweenOut:
  - WindowHide(false); shown = 0; MoveWindowDown(true); DropGamePauseState.
  - If cb: run it. It is not deleted here.
- SetZ: root SetZ(z); root SetPosition(tween->GetCenteredX(), GetCenteredY()).
- OnBack: if shown, Hide and return true.

## Speed-up hooks (wire after the dialogs)

### BuildProgressHoverWindow::OnSpeedUp(confirmed) @0x371430

- Without item boosts (+0xac == 0):
  - ResetRequirements; AddRequirement(9, GetSpeedUpCost()).
  - If CheckRequirements fails: BuildingHovers::Hide; NotEnoughWindow::Show; return.
  - If !confirmed: ConfirmPurchaseWindow::SetParameters(cb = OnSpeedUp(true), cost,
    GetCurrentMapID() != 0); Show; return.
- With item boosts:
  - ItemShopWindow path (milestone 4).
  - The error is printed when the boost item (+0xb0) is 0.
- Then:
  - Sound "ui_on_speedup", except at tutorial steps 0x3b and 0x1f.
  - speedingUp = 1, fake = 0, speedT = 1.0 - progress (+0x80).
  - Tutorial steps 0x1f, 0x3b, 0x54 and 0x5f: SetInteractionLock(true) + HideArrow.
  - Steps 0x1f and 0x5f also call HideWorldDialog.

### ResourceRestoreHoverWindow::OnSpeedUp(confirmed) @0x39462c

- Return if there is no building.
- Cost = AdjustCrystalCost(data->speedupCb (+0xb0)).
- Same NotEnough and ConfirmPurchase flow as above, with skip = false.
- Then:
  - Sound "ui_on_speedup".
  - +0x80 = +0x88; +0x7c = 1.
  - Hide +0x6c and +0x68.
  - The layout moves: +0x40 MoveWindow(0, +0x3c - (+0x38)->h); then (+0x38)->SetSize(w,
    +0x3c - (+0x6c)->h); then +0x40 MoveWindow(0, (+0x38)->h - +0x3c).

### BuildProgressHoverWindow::OnSpeedUpFinished @0x37116c

The port is partial. Missing pieces:

- Tutorial steps:
  - 0x1f → 0x20.
  - 0x3b → 0x3c.
  - 0x54 → CenterOn the building, BuildingHovers::Hide, 0x55.
  - 0x5f → SetInteractionLock(true).
- Building path: when it is not a class-2 contract, Map::Building::SpeedupBuilding.
- Decor path (+0x20): charge the crystals again, then Map::Decor::SpeedupDecoration, which sets
  collectStart (+0x48) = now + ~f50, i.e. now - f50 - 1.

### Map::Building::SpeedupBuilding @0x124308

- **Class 4** (trees and rocks):
  - n = min(resourceLeft, data->speedupAmount (+0x180)).
  - n times: GameState::PlaceOrder(this, Map::GetNearestStorage(this, false), 1,
    data->speedupResource (+0x17c), 0).
  - resourceLeft -= n; resources[speedupResource] += n.
  - If resourceLeft == 0:
    - level = (data->produceAmount (+0x184) == 0 ? 6 : 0).
    - gatherAcc = 0; resourceState = 2.
    - UpdateImage(); RemoveWorker(workers[0]).
  - UpdateResources(); gatherAcc = 0.
- **Class 2** with an active contract:
  - stateTime = f54 = GetGlobalTime() - GetContractTime(-1).
- **Otherwise**: buildLeft = 0.0.

## NotEnoughWindow dialog (0x2f6c5c..0x2fd9a4)

### Globals (0x62591c block)

| Address | Meaning |
|---|---|
| 62591c | root `Window_upgrade.xml` (RegisterUI scale -1, extraH 0x32) |
| 625920 | tween |
| 625924 | required[11] (gold = 625944, crystals = 625948) |
| 625950 | level |
| 625954 / 625958 | profession, profession level (M4) |
| 62595c | items vector (id, count) |
| 625968 | population |
| 62596c | level fail message (SetLevelFailMessage) |
| 625970 | gold fail message (SetGoldFailMesage) |
| 625974 | building counts |
| 625980 | building levels |
| 62598c | exchange limit |
| 625990 | upgradable building (SetUpgradableBuilding) |
| 625994 | action flag (SetActionCallback's bool) |
| 625998 / 6259a0 / 6259a4 / 6259a8 | item to produce: id, amount, x, y (Items, M4) |
| 62599c | button_upgrade_window_upgrade; its onClick is the action callback |
| 6259ac | its text_upgrade |
| 6259b0 | the text_upgrade of button_upgrade_window_upgrade_02 |
| 6259b4 | BuildingPurposeInfo "upgrade_unlocked_holder.building_purpose_holder" (see below) |
| 625a44 | lines[8], 100 bytes each (see below) |
| 625d64 / 625d68 | text_header / text_desc |
| 625d6c | missing count (set by CheckRequirements) |
| 625d70 | golden_border_box (orig size saved at 60f2fc/60f300) |
| 625d74 | scaler_transparent (orig size saved at 60f304/60f308) |
| 625d78 / 625d7c / 625d80 | layout offsets |
| 625d84 | crystal total for Buy all |
| 625d88 / 625d8c | button_upgrade_window_buy_all / _02 |
| 625d90 / 625d94 | buy_all_02 text / text_price |
| 625d98 / 625d9c / 625da0 | upgrade_02, its need_level_lock, its icon_profession_builder |
| 625da4 / 625da8 | buy_all text / text_price |
| 625dac / 625db0 | upgrade need_level_lock / icon_profession_builder |
| 625db4 / 625db8 | upgrade_unlocked_holder.darker_02 / text_upgrade_bonus |
| 625dbc | scaler_limit_underline_2px |
| 625dc0 | x_button.clickArea (Hide, sound "ui_close") |
| 60f2f0 | pending find line (-1) |
| 60f2f4 / 60f2f8 | root orig size |
| 60f30c | scale |
| 60f310 | tablet |

- **BuildingPurposeInfo** (LoadFrom @0x2f7850):
  - +0 root.
  - +4 Living, +0x14 Training, +0x20 Converting, +0x30 Producing, +0x74 Decoration,
    +0x7c LivingWorker (the Shared::House* structs).
- **Line** ("window_uprgade_1_line_%02d", sic):
  - +0 root.
  - +4 icon_60_on (takesZ).
  - +8 sepparator_vertical_02, +c sepparator_vertical_03.
  - +10 text_resource_name.
  - +14 text_quantity_resource, +18 text_quantity_resource_not_enough.
  - +1c text_level, +20 text_level_not_reached.
  - +24..+34 text_item_class_01..05.
  - +38 text_quantity_item, +3c text_quantity_item_not_enough.
  - +40 button_upgrade_window_find_one (onClick OnFind(i)), +44 its .text_upgrade.
  - +48 icon_complete (takesZ, icon gold_confirm).
  - State filled by UpdateContents:
    - +4c type: 0 resource, 1 gold, 2 crystals, 3 level, 4 profession, 5 building count,
      6 building level, 7 population, 8 item.
    - +50 resource type, +54 building id, +58 item id, +5c profession.
    - +60 missing amount.
- **Button callbacks:** buy_all → OnBuyAll; buy_all_02 → OnBuyItem.
- **Window functions:** Init sets fn.back (+0x3c) and fn.show (+0x2c).

### Small functions

- **SetActionCallback(cb, text, flag):** upgrade->SetOnClick(cb); both text_upgrade fields =
  text; 625994 = flag.
- **SetDescriptionText(desc, title):**
  - header = GetString("REQUIREMENTS"), or SWPrintf(L"%s - %s", REQUIREMENTS, title) when a
    title is given.
  - text_desc = desc.
- **ResetRequirements** also clears the following, and sets the upgrade button's onClick to null:
  - the profession and its level;
  - both fail messages;
  - the exchange limit;
  - the upgradable building;
  - the item to produce;
  - the flag.
- **Hide:** CampaignInfoWindow / RaidInfoWindow UpdateContents (M4); AnimateOut if visible.
- **OnTweenOut:**
  - WindowHide; shown = 0; MoveWindowDown.
  - If pending != -1: pending = -1, then OnFindActual(the old value).
- **Click:**
  - TopCityWindow::Click first. It is in an anonymous namespace in the port; it must be exposed.
  - Then root->Click; returns root->visibleSelf.
- **SetZ:** as ConfirmPurchaseWindow.
- **OnBack:** if shown, Hide and return true.
- **ExecuteFirstHelp** (bot only): skip.
- **OnBuyItem** (Items): M4.

### Show @0x2fc7f8

- BuildingHovers::HideArrow first.
- **One missing requirement** (missing == 1), when the upgrade button has no onClick or the flag
  is set, checked in this order:
  - **Level:** PopupWindow(SWPrintf(levelFailMsg or "SHOP_UNLOCK", ToWideString(level)),
    PopupWindow::Hide).
  - **Gold** (needed > amount):
    - text = SWPrintf(goldFailMsg or "NO_GOLD", ToWideString(needed - amount)).
    - ExchangeWindow::Show(); ExchangeWindow::OnTab(0, text).
  - **Crystals:**
    - text = SWPrintf("NO_CRYSTALS", ToWideString(needed - amount), GetPlayerName()).
    - ExchangeWindow::Show(); OnTab(1, text).
  - **Population:** PopupWindow(SWPrintf("NO_PEOPLE", ToWideString(population)), Hide, Hide).
- **Otherwise:**
  - shown = 1; MoveWindowOnTop(true).
  - If the root is hidden: WindowShow(false); root visible; UpdateContents; AnimateIn.
  - Else: root visible; UpdateContents.
  - Then PreloadWindow.

### UpdateContents @0x2f8ecc

**Reset:**

- root->RescaleWindow(1.0).
- root SetSize(orig 60f2f4/f8); golden SetSize(orig); scaler SetSize(orig).
- d78 = d7c = d80 = 0.
- Every line: hide the root and its parts.

**Lines,** in this order (n = the line count):

1. **Level**, when level != 0 and GetLevel() < level:
   - Root and icon shown; icon = GetIcon(GetResourceMapIconName(10)).
   - +20 shown with SWPrintf("SHOP_UNLOCK", level).
   - +48 shown with icon "impossible".
   - Type 3.
   - "levelOk" (local_c7c) is 0 here and 1 otherwise.
2. **Profession** (M4).
3. **Items** (M4; GetItemInfo is null, so they are skipped). Their missing price × count is added
   to d84.
4. **Population**, when free workers (players - used) < population:
   - Root and icon shown; icon "icon_b_houses".
   - Still short (the same test): +20 = SWPrintf("PEOPLE_REQUIRED", population + used - players);
     find button and text "PERFORM_FIND".
   - Otherwise: +1c = SWPrintf("PEOPLE_REQUIRED", population) with +48 "gold_confirm".
   - Type 7.
5. **Resources** 0..10 with required != 0. Stop at 8 lines with the error
   "ERROR: NotEnoughWindow::UpdateContents() has too many requirements\n".
   - Root, icon (resource icon) and sep02 shown; sep03 hidden.
   - +10 = GetResourceGameName.
   - **If amount < required:**
     - +18 = SWPrintf(L"%d/%d", amount, required) (0x59bf6c).
     - Find button shown; text "PERFORM_FIND".
     - For types < 9: d84 += ceil(Setting(exchange_X) × (required - amount)). The names are
       table 0x601798: exchange_lumber, rocks, food, planks, stones, meat, sausages, oil, gold.
   - **Else:** +14 = ToWideString(required); +48 "gold_confirm".
   - Type: 1 for gold, 2 for crystals, else 0.
   - +50 = the type; +60 = required - amount.
6. **Building counts**, when GetBuildingCount < count:
   - Text: "NEED_BUILDING_NAME" (name), or "NEED_BUILDING_NAME_MULTIPLE" (count, name) when
     count != 1.
   - Root, +20 = text, find button, "PERFORM_FIND".
   - Type 5; +54 = id.
7. **Building levels**, when GetBuildingMaxUpgrade < level:
   - Text: "NEED_BUILDING_LEVEL" (name, level).
   - Root shown; icon "icon_castle_60" for ids 99 and 100.
   - +20 = text; find button and text.
   - Type 6; +54 = id.

**Buy-all total:** if d84 != 0:

- d84 = (uint)(d84 × Setting "buy_all_modifier").
- Clamp to the exchange limit when it is != 0.
- If the result is < 1, d84 = 1.

**Buttons** (the item-to-produce branch is M4; this is the normal branch):

- buy_all_02 and upgrade_02 hidden.
- buy_all visible iff missing != 0; text "BUY_ALL"; price = ToWideString(d84).
- **missing == 0:** upgrade shown and enabled; lock hidden; builder icon shown.
- **!levelOk:** upgrade shown but disabled; lock shown; builder icon hidden.
- **Otherwise:** upgrade hidden.

**Layout:**

- iVar = (n < 2 ? 0 : lines[n-1].root->y - lines[0].root->y) - 10. With n == 8 it uses line 7.
- Purpose block hidden (root, darker, bonus text).
- **Upgradable building** (625990): not needed by the shop. It is used by the building upgrade.
  - It shows "UPGRADE_WILL_UNLOCK" and the purpose by class:
    - Producing (classes 2/0xd with a contract): unlocked mission icons. The confirm and lock
      icons follow each icon's enabled state.
    - Living (class 0): GetTimeString, gold; tax = (givePop + 1 + level) × 5 + collectMoney;
      workers = givePop + 1 + level.
    - Training (class 7): the image fitted, and SWPrintf("STORAGE_UPGRADE", storage_space[level
      + 1] - storage_space[level]).
  - Then purpose, darker and text visible iff one of those applied.
- d7c = d78.
- underline visible = the purpose root's visibleSelf. If shown: d78 += purpose->h + underline->h.
- Rescale:
  - scale = GUI::GetScaleFactor(root->origW, golden->origH + 0x50 + d78, !tablet, 1.0).
  - root->RescaleWindow(scale).
  - tween CenterWith(0, (int)(scale × -60), 0, 0).
  - d78 = (int)(d78 × scale); d7c = (int)(d7c × scale).
- Resize:
  - golden SetSize(w, h + d78); scaler SetSize(w, h + d7c).
  - upgrade, buy_all, upgrade_02 and buy_all_02: MoveWindow(0, d7c).
- d80:
  - If the purpose block is hidden: d80 = -(int)(underline->h + purpose->h + scale × -10).
  - Otherwise d80 = 0.
  - desc, scaler, the 8 line roots and the 4 buttons: MoveWindow(0, d80).
- root SetSize(w, close->h + golden->h).
- **Gotcha:** SetSize also rewrites origW/H (GUI.cpp:257), so the reset at the top must use the
  sizes saved at Init.

### OnBuyAll @0x2fd2ac

1. d84 = sum over types < 9 with a deficit: ceil(exchange_X × deficit). Plus the items (M4).
2. d84 = AdjustCrystalCost(d84). If it is != 0, apply buy_all_modifier, the limit and the minimum
   of 1 as in UpdateContents. picvar shows other globals here; they are all 625d84.
3. **If crystals < d84:** ExchangeWindow::Show(); OnTab(1, SWPrintf("NO_CRYSTALS", d84,
   GetPlayerName())).
4. **Otherwise:**
   - For types < 9 with a deficit: ChangeResourceAmount(type, deficit). Items: AddItem (M4).
   - ChangeResourceAmount(9, -d84).
   - If CheckRequirements passes: run the upgrade button's onClick, then Hide. Otherwise
     UpdateContents.

### OnFind(i) @0x2f76f8

- Hide().
- During a boss combat: popup "CAN_NOT_TRAVEL".
- Otherwise:
  - BuildingHovers::Hide, plus several M4 windows' Hide.
  - If the line's type != 7: ShopWindow::Hide.
  - pending (60f2f0) = i. It runs in OnTweenOut.

### OnFindActual(i) @0x2f7b98

SetArrowVisibleWindowLimit(2) first. Then, by the line's type:

- **0** → FindResourceHelp(the resource, i, true).
- **1** → ExchangeWindow::Show(); OnTab(0, SWPrintf("NO_GOLD", missing)).
- **2** → ExchangeWindow::Show(); OnTab(1, SWPrintf("NO_CRYSTALS", missing, player name)).
- **3** → nothing.
- **4** → professions (M4).
- **5** → on map 0: ShopWindow::Show + OnSelectItemID(id) + ShowArrowOnItem(id). Otherwise
  GlobalMapWindow::SwitchMap(0, false, true) (M4).
- **6** → building level, on map 0:
  - **GetUpgradeableBuildingWithID(id)** found (farm handling is 3f):
    - If it is opened, not class 0xd, and (no active contract or contractDone):
      - CenterOn (y = (minY + maxY) × 0.5, zoom 0.4).
      - BuildingHovers::OnBuildingClick(b, W/2, H/2); ShowArrowAtUpgrade.
    - Otherwise:
      - CenterOn.
      - Arrow at (baseX, maxY), with an arrow-click callback calling OnBuildingClick(b, the
        screen point of (minX, maxY)). FUN_002f79a4 builds that callback.
  - **Else GetUnfinishedBuildingWithID(id, true)** found: the same arrow/CenterOn path.
  - **Else:** ShopWindow::Show + OnSelectItemID + ShowArrowOnItem.
- **7** → population:
  - **GetUnfinishedBuildingedWithPopulation** found:
    - ShopWindow::Hide if it is visible.
    - Arrow-click callback as above; CenterOn(baseX, maxY); limit 2.
    - ArrowAt(maxX - 30, (minY + maxY) × 0.5, left = true).
  - **Otherwise:** ShopWindow::Show; OnTabSelect(0); ShowArrowOnItem(0x20).
- **8** → items (M4).

### FindResourceHelp(type, line, fromLine) @0x2f7a0c

- **Table 0x59c47c:** the building id per resource 0..7, plus a "sold in the shop" byte at +0x20:

  | Resource | Building | Sold |
  |---|---|---|
  | 0 lumber | 17 | 0 |
  | 1 rocks | 20 | 0 |
  | 2 food | 19 | 1 |
  | 3 planks | 102 | 1 |
  | 4 stones | 101 | 1 |
  | 5 meat | 1006 | 1 |
  | 6 sausage | 1008 | 1 |
  | 7 oil | 114 | 1 |

- On a farm: ExitFarm first (3f).
- **On map 0:**
  - **Map::GetBuildingWithID(id)** found:
    - CenterOn(baseX, (minY + maxY) × 0.5, zoom 0.4).
    - Arrow-click callback → OnBuildingClick(b, the screen point of (minX, maxY)).
    - ArrowAt(baseX, maxY).
  - **Else, if the sold byte is set:** ShopWindow::Show + OnSelectItemID + ShowArrowOnItem.
- **Elsewhere:** set the pending line, then GlobalMapWindow::SwitchMap(0, false, true) (M4).

## Not yet decoded

- **LevelUpWindow** (0x2f147c..0x2f3740): dumped, not read.
- **ExchangeWindow** (0x2abcbc..0x2af8bc): dumped, not read.
  - It is the crystal/gold store: OnPayForCB pays with crystals, OnPayForDollars with real money.
  - NotEnough's gold and crystal paths open it.
  - **Decision for the user before porting:** what to do offline with the real-money purchase
    buttons.
- **Dependencies NotEnough needs that are not ported:**
  - BuildingHovers::OnBuildingClick / ShowArrowAtUpgrade (the building info windows).
  - Map::GetUpgradeableBuildingWithID, GetUnfinishedBuildingWithID,
    GetUnfinishedBuildingedWithPopulation.
  - Render::CenterOn's animated path.
  - GameState::IsBossCombatActive.

## Suggested order for 3e.4

1. ConfirmPurchaseWindow, the NotEnough dialog (Show, UpdateContents without the
   upgradable-building block, OnBuyAll, OnFind/OnFindActual with stand-ins where noted) and the
   speed-up hooks, including SpeedupBuilding and SpeedupDecoration.
2. ExchangeWindow (after the user's decision).
3. LevelUpWindow.
