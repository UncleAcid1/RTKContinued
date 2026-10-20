# 5t-5 notes: PlayerCompetitionWindow's PvP tab (decoded 2026-10-20, not yet ported)

DECIDED (user 2026-10-20): port only the PvP tab (tab 4: the opponent carousel, valor bar, Refresh,
Fight); the activity / friends / gifts / top players / invite tabs stay dormant (hidden, not loaded
beyond what Init needs). The early refresh is FREE: set `pvp_refresh_cost` to 0 (main.cpp, after
LoadSettings; the data file has 5). Refreshes are free anyway in the PvPServer answer (cost 0).
PlayerWeakWindow is REMOVED (user 2026-10-18): skip OnFight's weak check. PlayerInfoWindow is 5u
(OnInfo: note). LoadingInternetWindow is online (skip; loads are instant offline).

## Done already (in the tree)
- `src/windows/PlayerCompetitionWindow.cpp`: only the selection functions (moved from PvP.cpp).
  `windows/Windows.h` declares the whole API (Init, Show, Hide, OnTab, UpdateContents,
  RefreshCurrentView, SetViewMode, SetCurrentView, GetCurrentView, CombatQueued, ...): define them.
- GlobalMapWindow's PvP arrival (g_dest 0x70): rival hero 0xc/0xd, Tasks::Update(true),
  AggroCombat, ForceBossFight(6, 0x70), Map::Update(0) (asm 0x2c2918..). Untested (needs OnFight).
- `docs/5t5_castletop_onpvp.patch`: CastleTopWindow::OnPvP(help) @0x276d64 and HelpWithPvP
  @0x277048 (+ HUD.h decls, the PvP button's OnPvP(0xffffffff)). Apply with `git apply` once
  PlayerCompetitionWindow::Show / OnTab exist. Then port_plan: move CastleTopWindow::(HelpWithPvP|
  OnPvP) from 5x to 5t.
- Tools: `tools/oda.py LO HI` (Ghidra addresses): objdump of libkingdom.so with PIC globals,
  strings and GOT calls resolved (asm_all.txt misses most of UpdateContents). A Ghidra full
  decompile of UpdateContents was started: `out/big/pcw_uca.c` (DecompRangeAddr, 30a720 10e20; log
  out/big/pcw_uc.log). If missing, rerun:
  `tools/ghidra_headless.sh "$PWD/ghidra" rtk511 -process libkingdom.so -readOnly -noanalysis
  -scriptPath "$PWD/tools/ghidra_scripts" -postScript DecompRangeAddr.java "$PWD/out/big/pcw_uca.c" 30a720 10e20`
  (run in the background; up to 30 min).

## Layout: ../resource/kingdom_ui/1Original/Window_pvp.xml (out/assets/.../Window_pvp.xmlb)
- `x_button` -> OnClose ("ui_close"); `tab_01..tab_06` (TabShop, callback OnTab(i), i = 0..5,
  "ui_tab"); tab icons: 0 activity_list_35, 1 friend_list_35, 2 pvp_won_35, 3 gift_35,
  4 pvp_list_35 (THE PvP TAB), 5 add_person. `header`, `text_player_name` (0x629ddc: the title,
  RefreshCurrentView sets PVP_TITLE_ALT on tab 4), `header_02`, `text_player_name_02`.
- `pvp_part_enemy` = EnemyHolder @0x62739c (LoadFrom @0x303734, read eh2 in the decompile):
  +0 root, +4 pvp_selector_bg_arena, +8 ValorBar (`pvp_valor_bar`, LoadFrom @0x3034e0: root,
  icon_rep, text_rep_level, text_rep_stat, rep_bar_holder_pvp.text_bg / text_rep /
  pvp_valor_bar_fat (+0x18), pvp_info_button (+0x1c), icon_holder_info (gold_info), +0x24
  button_pvp_shop_small, its icon_16_pvp (pvp_valor_16), text_unlock_items, +0x30 clip rect of
  the fat bar; SetProgress(f) @0x301eec: clip.x2 = clip.x + bar.w * f), +0x48 param_back,
  +0x4c icon_timer, +0x50 text_stat_num, +0x54 button_pvp_refresh (.text_gold +0x58, .text_price
  +0x5c, .icon_35_crystal +0x6c), +0x60 pvp_refresh_opps (.text_button +0x64, .icon_35 +0x68),
  +0x70 button_info_round (.icon_placeholder_35 gold_info +0x74, .text +0x78), +0x7c backdrop_01,
  +0x80 text_day_01, +0x84 text_day_02, +0x88 text_error, +0x8c button_navigate_left, +0x90 right,
  +0x94 mask_selector (+0x98..+0xa4 clip x,y,x+w,y+h+0x400 for the figures), +0xa8 text_name_01,
  +0xac text_name_02 (both SetAlignment(1,1)), +0xb0 nine Shared::PlayerHolder (0x1bc each;
  figure i loads pvp_char_{order[i]+1}, order = {0,1,2,3,5,6,7,8,4}: index 8 is the big centre
  pvp_char_05; each root clipped by +0x98; per figure +0x1b0 float = body.w / centre body.w,
  +0x1b4 / +0x1b8 = 1.0; the eight small ones take the centre's body textures (male +8, female
  +0x14); first figure moved by -(x(char_03) - x(char_02)), check asm 0x303810..), +0x138c a
  ContentScroller, +0x140c mask window and +0x1410.. its rect, +0x104c text_enemy_squad, +0x1050
  text_squad_front, +0x1054 three Shared::WarriorSlotInfo `slot_warrior_%02d` (0xdc each),
  +0x12e8 Shared::ButtonRaidStart `button_pvp_start`, +0x132c pvp_vic_def_holder (.icon_vic_def,
  .text_victory, .text_defeat, .text_vic_def_exp), +0x1340 sliced_crem_bg_02, +0x1344
  text_enemy_stats, +0x1348 four StatisticHolder `pvp_stat_%02d` (0x10: root, text_stat,
  text_stat_num, icon_stat; LoadFrom @0x303458), +0x1388 sepparator_01.
- DEAD: WindowPvp::LoadFrom (no caller) and what only it loads (PvpPartEnemy, PvpValorBar,
  PlayerCompetitionWindow::ButtonRaidStart): port_plan `dead`.

## Init @0x306120 (PvP parts; all else social/online)
RegisterUI Window_pvp.xml (scale -1.5), RegisterTopWindow, hidden, tween (OnTweenIn/OnTweenOut),
g_60f32c = root scale; fn: show Show, back OnBack, update Update, zRange ZRange (0.01 shown?),
hideOnOuterClick true; callbacks: valor bar root +8 and +0x24.. -> OnValor, shop button ->
OnValorShop, refresh -> OnRefresh(false), refresh_opps -> OnRefreshFree, info -> OnInfo, left ->
OnLeft, right -> OnRight, the 8 side figures -> OnPerson(i) (not index 4 = g_626378 = 4), the 3
warrior slots -> OnWarrior(i) ("ui_click", slot hidden), start -> OnFight (hidden at first);
g_60f334 = 10.0 (step), an ItemHoverWindow at 0x626388 (new ItemHoverWindow), the displayed list
vector 0x629da4 cleared, selected gid 0, tab 0x626364 = 0, 0x629df0 = 0.

## Small functions (decompile in the scratch dump; all checked)
- Show @0x31daf8: (FB profile check: online) mode 0x60f320 not in 1..3 -> 1; 0x629dfc = 0;
  shown, MoveWindowOnTop; if hidden: WindowShow, AnimateIn, RaiseGamePauseState; visible;
  0x62637c (fight queued) = 0; shadow gid 1 not beaten and nothing selected -> select 1;
  ScrollToTop the scrollers; RefreshCurrentView; UpdateContents; PreloadWindow.
- OnTab @0x31d514: ignore 1/4 while loading; same tab -> nothing; tab 1 -> mode 2, tab 4 -> mode 1,
  0 / 2 / 3 / 5 -> mode 5; tab < 3 -> last tab 0x60f324 = tab; (social bookkeeping); tab =
  0x626364; RefreshCurrentView; UpdateContents.
- Hide @0x302d00 (AnimateOut; NET_GetUpdate online); OnTweenIn @0x31b738 (tutorial: all 3 shadows
  beaten -> SetCompletedPvPTutorial, refresh time 1 = now+10, last loaded 1 = now; UpdateContents);
  OnTweenOut @0x3025ac (WindowHide, not shown, MoveWindowDown, DropGamePauseState; if a fight was
  queued and the selected rival exists and the hero lives: GlobalMapWindow::Show unless shown or
  on map 0x74, then ShowPvPAnimation -> the arrival above starts the fight).
- OnBack @0x302e94 (selected = 0; hover shown ? hover Hide : Hide), OnClose @0x31d7b4, SetZ
  @0x31de7c, IsVisible, OnLeft (+1 to 0x6287cc), OnRight (-1), OnPerson(i) (0x626374 += 4 - i),
  SetViewMode @0x301ea0 (2 -> tab 1, 1 -> tab 4), SetCurrentView, GetCurrentView, CombatQueued
  (0x62637c), OnValor (LevelInfoWindow::Show + OnTab(profession 7's tab, true)), OnValorShop
  (ItemShopWindow::Show + ShowShopTab(7)), OnWarrior(i) (hover: SetEntity(null, data of the
  slot's soldier +0x168?, 0, 0), SetTargetRect slot root, RecomputePosition, SetZ 0.001,
  SortRenderLayer(0xe,1)), OnInfo (PlayerInfoWindow 5u if the shown rival was fought, else popup
  PVP_BATTLE_TO_SHOW_INFO), DataDownloadCallback @0x309310, OnRefreshFree @0x31d300 (RemoveCached
  Data(mode, per-mode bool), GameState::SetPvPRefreshTime(now) (online: skip), RefreshPlayers,
  RefreshCurrentView, UpdateContents), OnRefresh(bool) @0x31d3b0 (NotEnough check crystals
  pvp_refresh_cost; false -> ConfirmPurchaseWindow with OnRefresh(true); true -> as OnRefreshFree
  plus 0x629df0 = cost), OnFight @0x309148 (shown rival = list[(0x626378 + 0x6287cc) % n] or
  gid 0x629df4; valor = GetValorForVictory once the tutorial is done; selected = his gid; dead hero
  -> HealSquadWindow with post action; else queued = 1, Hide).
- Click @0x302f08 / Move @0x31df1c / Update @0x31ba54: the carousel (decoded; see below).
  Globals: V 0x629c24 velocity, P 0x629c28 sub step, steps 0x626374, off 0x6287cc, W 0x60f334
  (10), S 0x60f338, drag 0x6287e4 (pressed) / 0x6287e5 (dragging) / 0x6287e8 x.
  Click: root hidden -> root Click, false. ClickOnArrow -> true; release: HideArrow; hover Click ->
  true; (social scrollers: hidden, skip); press inside enemy root's rect (0x6287ac..b8 + root pos)
  -> drag x, pressed. Release: pressed = 0; if dragging: dragging = 0, V = GetMouseSpeed(true) /
  scale; V < -2000 -> -2000, > 2000 -> 2000 (steps 0, true); |V| > 400 -> steps 0, true; else V = 0,
  P > 0 -> steps++, P < 0 -> steps--, true. Then root Click, return root visible.
  Move: pressed and |x - dragX| >= 11 -> dragging, V = 0, steps = 0; not dragging -> return;
  P += (x - dragX) * 0.05; P > W -> P -= W, off--; P < -W -> P += W, off++; dragX = x; UpdateContents.
  Update(dt): t 0x629e08 += dt; loading or hidden -> return; (scroller updates: social, skip);
  if |V| > 400 or (V != 0 and steps == 0): P += V*0.05*dt with the W wrap (off -/+); V decays
  by 2000*dt towards 0 (clamped at 0); when |V| <= 400: P > 0 -> steps++, P < 0 -> steps--;
  UpdateContents. If steps != 0: target = |steps*40|; S moves towards target by 40*dt;
  speed = V != 0 ? |V*0.05| : S; steps > 0: P += speed*dt, P > W -> P -= W, steps--, off--, V = 0,
  steps 0 -> P = 0; steps < 0 mirrored (off++); UpdateContents. (Social search textfields' cursor
  blink: dormant.) Once per wall-clock second: UpdateContents. (Exact asm: oda.py 31ba54 31c1cc.)
- RefreshCurrentView @0x305874: title PVP_TITLE_ALT (tab 4); hide all 9 figures; squad text
  PVP_SQUAD (PVP_SQUAD_FRIEND on tab 1); start button hidden, icon gold_attack, PVP_FIGHT; stats:
  PVP_STATS, PVP_REFRESH on refresh_opps text, the 4 stat icons pvp_won_35 / pvp_lost_35 /
  pvp_valor_35 / xp, texts PVP_VICTORIES / PVP_DEFEATS / PVP_RANK / PVP_LEVEL, hide the 4 numbers,
  refresh buttons, info, timer...; the 3 warrior slots RestorePosition + hidden, soldier 0; the
  displayed list 0x629da4 = EnumPlayers of type == mode; off = index of the selected gid - 4.
- UpdateContents @0x30a720 (69 KB, all tabs): the tab loop (SetTabMode @0x302658: icon restore,
  raised 4*scale when active, active/inactive look, notification count), then per tab. PvP strings
  to look for: PVP_REFRESH_IN, PVP_REFRESH, pvp_refresh_cost, gold_refresh, friend_fight_timeout,
  gold_speedup, rep_pvp_valor_60, HINT_VALOR, PVP_VALOR_NAME, PVP_INFO, PVP_NO_WARRIORS,
  PVP_KILLED / PVP_KILLED_DESC, pvp_lost_60, PlayerHolder::PlacePlayerEquipment (asm 0x312ab8).

## Then (5t-5 rest) and closing 5t
- port_plan: replace `PlayerCompetitionWindow` in the online rule by a 5t rule for the PvP-tab
  functions above (Init, Show, Hide, OnTab, Click, Move, Update, UpdateContents, RefreshCurrentView,
  SetZ, ZRange, OnBack, OnClose, OnTweenIn/Out, IsVisible, the On* above, SetViewMode,
  Set/GetCurrentView, CombatQueued, EnemyHolder / ValorBar / StatisticHolder / TabShop /
  GiNotificationHolder / IconGoldQuestNew ::LoadFrom, ValorBar::SetProgress, SetTabMode(TabShop)),
  `dead` for WindowPvp / PvpPartEnemy / PvpValorBar / PCW::ButtonRaidStart; the rest stays online.
- Register Queue() in main.cpp after PlayerBattleInOutWindow.
- Resolve the "5t" notes: FoundItemsWindow.cpp RunPostAction (Show + OnTab(4)), ChestsWindow.cpp
  1472/1496 (hide it), NotEnoughWindow.cpp 293/346 and TasksHelp.cpp 177/687-688
  (CastleTopWindow::HelpWithPvP), LevelInfoWindow.cpp 473 (g_pvpOpen ? OnPvP(-1) : HelpWithPvP()),
  TaskInfoWindow.cpp 759 (OnPvP(0x317): help page, online), STATUS lines with "(5t)".
- Verify in a run: city save with task 0x74d done -> PvP button -> list of 5 generated rivals,
  arrows / drag, Refresh, Fight -> world map travel -> arena fight -> valor.
- Close 5t: `inventory.py --functions 5t` and `--unplanned` empty; STATUS; commit; push.
