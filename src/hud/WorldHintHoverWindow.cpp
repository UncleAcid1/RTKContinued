// WorldHintHoverWindow (libkingdom.so 5.11, 0x39b028..0x39c3d0): see HoverWindows.h.
#include <cmath>
#include <cstdio>

#include "engine/Render.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"
#include "windows/Windows.h"

// @0x39c3d0
WorldHintHoverWindow::WorldHintHoverWindow() {
    name = "WorldHintHoverWindow";
    scale = GUI::IsHighDPIVersion() ? 1.25f : 1.f;
}

WorldHintHoverWindow::~WorldHintHoverWindow() { delete root; }

// @0x39c0ac
void WorldHintHoverWindow::Init() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Dialog_hint_hd.xml", "Dialog_hint_hd.png", scale, 0, 0, 0, 0,
                           false, 1.f);
    root->SetVisibility(false);
    root->SetScreenSpace(false);
    frame = GUI::GetWindowTyped<GUI::Window>(root, "dialog_hint_window_9s");
    frameW = frame->w;
    arrowSW = GUI::GetWindowTyped<GUI::Window>(root, "dialog_arrow_sw");
    arrowSE = GUI::GetWindowTyped<GUI::Window>(root, "dialog_arrow_se");
    arrowNW = GUI::GetWindowTyped<GUI::Window>(root, "dialog_arrow_nw");
    arrowNE = GUI::GetWindowTyped<GUI::Window>(root, "dialog_arrow_ne");
    arrowNW->SetVisibility(false);
    arrowNE->SetVisibility(false);
    text = GUI::GetWindowTyped<GUI::Textfield>(root, "dialog_text");
    text->SetWorldOverscale(true);
    next = GUI::GetWindowTyped<GUI::Window>(root, "button_dialog_hint_next");
    next->SetOnClick([this] { OnUserClose(); });
    next->SetActionSound("ui_click", 1);
    GUI::Textfield* label = GUI::GetWindowTyped<GUI::Textfield>(root, "button_dialog_hint_next.text_give_job");
    label->SetText(StringTable::GetString("DIALOG_NEXT"));
    label->SetWorldOverscale(true);
}

// @0x39b028
void WorldHintHoverWindow::SetZ(float z) { root->SetZ(z); }

// @0x39c040
void WorldHintHoverWindow::Show() {
    std::puts("WorldHintHoverWindow::Show");
    if (shownHover) return;
    BuildingHoverWindow::Show();
    root->SetVisibility(true);
    next->SetVisibility(true);
    sideLeft = false;
    arrow = false;
    fixedSide = false;
}

// @0x39bfdc
void WorldHintHoverWindow::Hide() {
    if (!shownHover) return;
    root->SetVisibility(false);
    if (GameState::tutorial == 0x3c) BuildingHovers::HideArrow();
    BuildingHoverWindow::Hide();
    offY = 0;
    offX = 0;
}

void WorldHintHoverWindow::HideNextButton() { next->SetVisibility(false); }

void WorldHintHoverWindow::SetDefaultSideLeft(bool left) {
    sideLeft = left;
    fixedSide = true;
}

void WorldHintHoverWindow::ShowArrow() {
    arrow = true;
    GUI::SetInteractionObjectLock(next, nullptr);
}

// @0x39b920
void WorldHintHoverWindow::SetParameters(const char32_t* str, uint32_t taskId, uint32_t nextTaskId, uint32_t specialId) {
    // Back to the layout's size, then as much taller as the text needs.
    root->SetSize((unsigned)root->w, (unsigned)(root->h - grow));
    frame->SetSize((unsigned)(frameW + 2), (unsigned)(frame->h - grow));
    next->MoveWindow(0, -grow);
    arrowSE->RestorePosition();
    arrowSW->RestorePosition();
    grow = 0;
    text->SetAlignment(0, 0);
    text->SetText(str);
    int more = 0;
    if (text->sprite) more = (int)(std::fabs(text->sprite->h) - (float)text->h);
    more -= (int)(scale * 6.f);
    grow = more;
    root->SetSize((unsigned)root->w, (unsigned)(more + root->h));
    frame->SetSize((unsigned)(frameW + 2), (unsigned)(frame->h + grow));
    next->MoveWindow(0, grow);
    arrowSE->MoveWindow(0, grow - 3);
    arrowSW->MoveWindow(0, grow - 3);
    task = taskId;
    nextTask = nextTaskId;
    special = specialId;
    if (GameState::tutorial == 0x51) {
        GameState::tutorial = 0x52;
        Render::CenterOn(entity->worldX, entity->worldY - 80.f);   // (animated in the original: 0.4 s)
        BuildingHovers::HideArrow();
        Tasks::Update(true);
        fixedSide = true;
    }
}

// @0x39bb94: right of the world point when the box fits on the screen there, else left of it; a
// box that would leave the screen is held at its edge.
void WorldHintHoverWindow::SetPosition(int x, int y) {
    int sx = x, sy = y;
    Map::WorldCoordinatesToScreen(sx, sy);
    int leftEdge = 0, rightEdge = GUI::ScreenWidth(), unused = 0;
    Map::MouseCoordinatesToWorld(leftEdge, unused);
    Map::MouseCoordinatesToWorld(rightEdge, unused);
    int width = (int)((float)root->w * Render::GetBaseZoomFactor());
    bool offLeft;    // on the left the box would leave the screen
    bool toLeft;
    if (!sideLeft && !fixedSide) {
        toLeft = GUI::ScreenWidth() <= width + sx;
        offLeft = toLeft && sx - width < 0;
    } else {
        bool roomLeft = sideLeft ? fixedSide : !(sx - width < 0);
        offLeft = width + sx < GUI::ScreenWidth() ? false : sx - width < 0;
        toLeft = roomLeft;
    }
    if (toLeft) {
        arrowSW->SetVisibility(false);
        arrowSE->SetVisibility(true);
        if (!offLeft || sideLeft) root->SetPosition((-0x14 - root->w) + x, y - root->h);
        else root->SetPosition(leftEdge - 0x14, y - root->h);
    } else {
        arrowSW->SetVisibility(true);
        arrowSE->SetVisibility(false);
        if (!offLeft || sideLeft) root->SetPosition(x + 10, y - root->h);
        else root->SetPosition(rightEdge - root->w, y - root->h);
    }
    root->SetZ(root->z);
    if (!arrow || !next->visibleSelf) return;
    if (arrowCallback != BuildingHovers::GetArrowClickCallbackID())
        arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnUserClose(); }));
    BuildingHovers::ArrowAt((float)(next->x + root->x + next->w / 2), (float)(next->y + root->y + next->h), false, false,
                            false, true, false, false, false);
}

// @0x39b74c: the box follows its speaker (hidden while the speaker walks its patrol).
void WorldHintHoverWindow::Update(float) {
    if (!shownHover || !entity) return;
    const bool was = root->visibleSelf;
    if (!entity->f96) {
        root->SetVisibility(!entity->IsOnPatrol());
        if (!was && root->visibleSelf && GameState::GetCurrentMapID() == 0 && GameState::secondTutorial == 0x100 &&
            !Map::GetCurrentFarm()) {
            if (BuildingHovers::IsHoverVisible()) BuildingHovers::Hide();
            CenterIfOffScreen();
        }
    }
    if (!IsWindowHigherThan("BuildingHovers")) MoveWindowOnTop(true);
    float wx = 0.f, wy = 0.f;
    entity->GetWorldPos(wx, wy);
    float up = 0.f;
    if (entity->GetEntityData()->clas == 6) up = entity->GetIdleHeight();
    else if (entity->GetSprite()) up = entity->GetSprite()->h;
    SetPosition((int)wx + offX, (int)(wy - up) + offY);
}

// @0x39b0d8
void WorldHintHoverWindow::CenterIfOffScreen() {
    if (entity && !entity->f96 && entity->IsOnPatrol()) return;
    int sx = root->x, sy = root->y;
    Map::WorldCoordinatesToScreen(sx, sy);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    if (sx >= 0 && root->w + sx <= W && root->h + sy < H - 0x31) {
        if (sy > 0x5e) return;
        // Too high: the view goes up.
        int mx = sx, my = sy - 0x5f + H / 2;
        Map::MouseCoordinatesToWorld(mx, my);
        // (animated in the original: 0.4 s)
        if (GameState::secondTutorial != 0x8d) Render::CenterOn(-Render::offsetX, (float)my);
        else Render::CenterOn((float)(root->x - 0x32), (float)my);
        return;
    }
    Render::CenterOn((float)root->x + (float)root->w * 0.5f, (float)root->y + (float)root->h * 0.5f);
}

// @0x39b2e8 WorldHintHoverWindow::OnClose: its task is completed (unless the next task already
// replaced it), the tutorials move on.
void WorldHintHoverWindow::OnClose() {
    // (tutorial 0x17: TutorialSender::AddTutorialStep(0x658), online)
    if (task == 0x5a8) {
        // UNVERIFIED (milestone 5): with profession 3 learnt, CraftSuccessWindow for it.
    }
    if (task == 0x76d) {
        next->SetVisibility(false);
        Map::AddCameraPoint(7, 0x10, 1, 1, 0, 0, 0, 0, 1.f, true, 0.f, 0);
    }
    if (task == 0x76f) {
        GameState::CompleteTask(0x76f, false);
        next->SetVisibility(false);
        Entity* soldier = EntityManager::GetPlayer()->GetSquad()->GetSoldier(1);
        BuildingHovers::DropItem(soldier->worldX, soldier->worldY, Items::GetItemInfo(0x254), 0, 1);
        // UNVERIFIED (milestone 4g): BottomBattleWindow::EnableNet(0).
    }
    if (task != 0) {
        uint32_t then = nextTask;
        if (then == 0) {
            uint32_t done = task;
            task = 0;
            Tasks::CompleteTask(done, true, true, false);
            then = task != 0 ? nextTask : 0;   // (a dialog shown by the completion)
        }
        if (then != 0) {
            Tasks::TaskInfo* t = Tasks::GetTask(then);
            if (t && (t->disableTask == 0 || !GameState::TaskCompleted(t->disableTask))) GameState::ResetTask(nextTask);
        }
    }
    special = 0;
    task = 0;
    nextTask = 0;
    Hide();
    if (GameState::tutorial == 7) {
        GameState::tutorial = 8;
    } else if (GameState::tutorial == 0xf) {
        GameState::tutorial = 0x10;
    } else if (GameState::tutorial == 0x36) {
        if (GameState::GetTutorialType() == 1 || GameState::GetTutorialType() == 2) {
            // The short tutorials skip the city's first part.
            if (Tasks::TaskInfo* t = Tasks::GetTask(0xe6)) t->dnShow = true;
            if (Tasks::TaskInfo* t = Tasks::GetTask(0x185)) t->dnShow = true;
            BuildingHovers::HideArrow();
            GUI::SetInteractionLock(true);
            Tasks::CompleteTask(0xd5, true, true, false);
            GameState::AddItem(0x38, 1, false, false, false);
            GameState::tutorial = 0x58;
        } else {
            GameState::tutorial = 0x37;
        }
    } else if (GameState::tutorial == 0x11) {
        GameState::tutorial = 0x12;
    }
    if (GameState::secondTutorial == 0x83) GameState::secondTutorial = 0x84;
    else if (GameState::secondTutorial == 0x202) GameState::secondTutorial = 0x203;
    else if (GameState::secondTutorial == 0x20c) GameState::secondTutorial = 0x20d;
    if (arrow) {
        BuildingHovers::HideArrow();
        GUI::SetInteractionObjectLock(nullptr, nullptr);
    }
    arrowCallback = 0;
}

// @0x39b64c
void WorldHintHoverWindow::OnUserClose() {
    if (special == 0x578) {
        special = 0;
        TaskInfoWindow::SetTask(0x5b7, false);
        TaskInfoWindow::Show();
    } else if (special == 0x4b6 || special == 0x4b4) {
        special = 0;
        // UNVERIFIED (milestone 5): ArenaEventWindow::SetParameters(0 / 1) and Show.
    } else if (special == 0x3b1 && task == 1099) {
        // UNVERIFIED (milestone 5): CastleTopWindow::OnBuyItem(0).
    }
    OnClose();
}

// @0x39b700
bool WorldHintHoverWindow::Back() {
    std::puts("WorldHintHoverWindow::Back");
    if (!shownHover || !next->visibleSelf) return false;
    OnUserClose();
    return true;
}

// @0x39bf04: without its "next" button a tap anywhere closes it in tutorial steps 0xb and 0x17.
bool WorldHintHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (!next->visibleSelf && (GameState::tutorial == 0xb || GameState::tutorial == 0x17)) {
        OnClose();
        return true;
    }
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    if (root->Click(wx, wy, pressed, false)) return true;
    return arrow;
}
