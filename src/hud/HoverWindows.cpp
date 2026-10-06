#include "hud/HoverWindows.h"

#include <cmath>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "gui/GUI.h"

// ---- BuildingHoverWindow ----

BuildingHoverWindow::BuildingHoverWindow() {
    name = "BuildingHoverWindow";
    usesZRange = false;
}

BuildingHoverWindow::~BuildingHoverWindow() { Render::SortRenderLayer(Render::kLayerGUI, 1); }

void BuildingHoverWindow::Show() {
    if (shownHover) return;
    int step = GameState::TutorialStep();
    if ((WindowManager::GetShownWindowCount() == 0 || step == 0x45 || step == 0x4b) &&
        !BuildingHovers::IsWorldDialogVisible())
        MoveWindowOnTop(true);
    shownHover = true;
}

void BuildingHoverWindow::RemoveWindow() {
    shownHover = false;
    pendingDestroy = true;
    building = nullptr;
    entity = nullptr;
    decor = nullptr;
}

void BuildingHoverWindow::FixWindowPosition(int& x, int& y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
}

// ---- BubbleHoverWindow ----

namespace {
// The contract mission's icon (a crop or product decoration).
Render::Texture* MissionIcon(const Map::Building* b, int contract) {
    const Contracts::ContractMission& m = b->data->delivery->missions[(size_t)contract - 1];
    return Resources::GetDecoration(m.icon.c_str());
}
}  // namespace

BubbleHoverWindow::BubbleHoverWindow() { name = "BubbleHoverWindow"; }

BubbleHoverWindow::~BubbleHoverWindow() { delete root; }

void BubbleHoverWindow::Init() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Icon_friend_sceen_bubble.xml",
                           "Icon_friend_sceen_bubble.png", 1.f, 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    root->SetScreenSpace(false);
    icon = GUI::GetWindowTyped<GUI::Window>(root, "icon_holder");
    t = 0.f;
    icon->takesZ = true;
    icon->y -= 5;
}

void BubbleHoverWindow::Show() {
    if (shownHover) return;
    shownHover = true;
    root->SetVisibility(true);
}

void BubbleHoverWindow::Hide() {
    if (!shownHover) return;
    root->SetVisibility(false);
    BuildingHoverWindow::Hide();
}

void BubbleHoverWindow::SetZ(float z) {
    if (root->z == z) return;
    root->SetZ(z);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// The bubble bobs 4 px (cos(8t)) under the building's top; it follows the building if that moved.
void BubbleHoverWindow::Update(float dt) {
    t += dt;
    if (!root->sprite) return;
    if (building) {
        building->FindBaseCoordinates();
        int iconY = building->data->iconY;
        if ((float)baseY != (building->minY - (float)root->h) + (float)iconY)
            SetPosition((int)((building->minX + building->maxX) * 0.5f), (int)(building->minY + (float)iconY));
    }
    root->SetPosition(root->x, (int)((float)baseY + std::cos(t * 8.f) * 4.f));
    // UNVERIFIED (tutorial): step 0x55 points the tutorial arrow at the bubble.
}

void BubbleHoverWindow::SetEntity(Entity* e) {
    BuildingHoverWindow::SetEntity(e);
    // UNVERIFIED (farms): a farmer (AI state 0x19) shows the crop of the patch it works.
}

void BubbleHoverWindow::SetBuilding(Map::Building* b) {
    BuildingHoverWindow::SetBuilding(b);
    Map::Building* bb = building;
    if (!bb) return;
    const char* iconName = nullptr;
    if (bb->needsBuilder == 0 && bb->upgrading == 0) {
        unsigned cls = bb->data->buildingClass;
        if (cls == 2 && bb->HasActiveContract() && bb->contractDone) {
            icon->SetTexture(MissionIcon(bb, bb->contract));
            return;
        }
        if (cls == 4) {
            if (bb->resourceState == 2) iconName = "gold_trash";
        } else if (cls == 0xc && bb->HasActiveContract() && !bb->contractDone) {
            icon->SetTexture(IconManager::GetIcon("gold_school"));
            return;
        }
        if (!iconName) {
            if (b->GetFarmState(0) == 1) {
                iconName = "gold_water";
            } else {
                // UNVERIFIED (farms): a farm with a ready patch shows that patch's crop
                // (GetFirstReadySoilPath).
                if (bb->data->buildingClass != 0xc) return;
                iconName = "gold_school";
            }
        }
    } else {
        iconName = "gold_build";
    }
    icon->SetTexture(IconManager::GetIcon(iconName));
}

void BubbleHoverWindow::SetPosition(int x, int y) {
    BuildingHoverWindow::SetPosition(x, y);
    root->SetPosition(x - root->w / 2, y - root->h);
    baseY = y - root->h;
    // UNVERIFIED (farms): a farm with a ready patch refreshes the crop icon here.
    Update(0.f);
}

bool BubbleHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    if (!root->Click(wx, wy, pressed, false)) return false;
    if (!GUI::CanInteractWith(root)) return true;
    if (pressed) return true;
    Map::Building* b = building;
    if (b) {
        if (b->data->buildingClass == 4 && b->resourceState == 2) {
            b->ResetResource();
            BuildingHovers::Update(0.0, true);
            return true;
        }
        if (b->needsBuilder != 0 || b->upgrading != 0) {
            if (!b->BuilderAssigned()) {
                BuildingHovers::OnBuildingAssignBuilder(building);
                return true;
            }
            b = building;
        }
        if (b) {
            unsigned cls = b->data->buildingClass;
            if (cls == 2) {
                BuildingHovers::OnBuildingFinishedClick(b);
                return true;
            }
            if (cls == 0xd) {
                // UNVERIFIED (farms): FarmCollectAndReplant, else the farm map (ShowFarm).
                return true;
            }
            if (cls == 0xc) {
                // UNVERIFIED (milestone 4): HireTroopsWindow::ShowForBuilding(b->id, false).
                return true;
            }
        }
    }
    // UNVERIFIED (farms): a farmer's bubble (AI state 0x19) opens the patch's farm hover.
    return true;
}

// ---- TaxesHoverWindow ----

TaxesHoverWindow::TaxesHoverWindow() {
    name = "TaxesHoverWindow";
    bonusTime = (unsigned)(int)(GameState::GetSetting("tax_bonus_time") + 0.5f);   // Setting::GetInt
    bonusReady = false;
    pulse = 0.f;
    bonusPulse = false;
}

TaxesHoverWindow::~TaxesHoverWindow() {
    delete root;
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

void TaxesHoverWindow::Init() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_small_round_blue.xml",
                           "Button_small_round_blue.png", 1.f, 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    root->SetScreenSpace(false);
    blue = GUI::GetWindowTyped<GUI::Window>(root, "rage_bar_button_blue");
    green = GUI::GetWindowTyped<GUI::Window>(root, "rage_bar_button_green");
    bronze = GUI::GetWindowTyped<GUI::Window>(root, "rage_bar_button_bronze");
    GUI::GetWindowTyped<GUI::Window>(root, "rage_bar_button_red")->SetVisibility(false);
    green->SetVisibility(false);
    icon = GUI::GetWindowTyped<GUI::Window>(root, "button_round_icon_holder");
    icon->takesZ = true;
    clickArea = GUI::GetWindowTyped<GUI::Button>(root, "clickArea");
}

void TaxesHoverWindow::Show() {
    if (shownHover) return;
    shownHover = true;
    root->SetVisibility(true);
}

void TaxesHoverWindow::Hide() {
    if (!shownHover) return;
    root->SetVisibility(false);
    BuildingHoverWindow::Hide();
}

void TaxesHoverWindow::SetZ(float z) { root->SetZ(z); }

void TaxesHoverWindow::SetBuilding(Map::Building* b) {
    BuildingHoverWindow::SetBuilding(b);
    if (!building) return;
    icon->SetTexture(IconManager::GetIcon("gold_pile"));
    clickArea->SetOnClick([this] { OnCollect(); });
}

void TaxesHoverWindow::SetDecoration(Map::Decor* d) {
    BuildingHoverWindow::SetDecoration(d);
    if (!d) return;
    icon->SetTexture(IconManager::GetIcon("gold_pile"));
    clickArea->SetOnClick([this] { OnCollect(); });
}

void TaxesHoverWindow::SetPosition(int x, int y) {
    if (building) y += building->data->iconY;
    BuildingHoverWindow::SetPosition(x, y);
    root->SetPosition(x - root->w / 2, y - root->h / 2);
    // UNVERIFIED (tutorial): step 0x21 locks the interaction on this button and points the arrow at it.
}

bool TaxesHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    if (root->Click(wx, wy, pressed, false)) return !pressed;
    return false;
}

void TaxesHoverWindow::Activate(bool on) {
    if (!on || !shownHover) return;
    OnCollect();
}

// The gold drops at the building's base; collected within the bonus time (the house was full for
// less than "tax_bonus_time" seconds) it comes at once plus a "tax_bonus_percent" bonus drop.
void TaxesHoverWindow::OnCollect() {
    if (building) {
        int ready = building->GetReadyGoldAmount();
        if (ready != 0) {
            if (building->GetReadyTime() == 0 || bonusTime <= (unsigned)building->GetReadyTime() ||
                GameState::TutorialStep() < 0x22) {
                BuildingHovers::DropResource(building->baseX, building->baseY, GameState::kGold, (unsigned)ready, false, false);
            } else {
                BuildingHovers::DropResource(building->baseX, building->minY, GameState::kGold, (unsigned)ready, true, false);
                float pct = GameState::GetSetting("tax_bonus_percent");
                BuildingHovers::DropResource(building->baseX, building->baseY, GameState::kGold,
                                             (unsigned)(int)((float)ready * pct), false, true);
            }
        }
        int amount = 0;
        bool full = false;
        building->CollectResources(full, amount);
    }
    if (decor) {
        decor->collectStart = Timer::GetGlobalTime();
        BuildingHovers::CreateDecorationDrop(decor);
    }
    Hide();
    // BuildingHovers::HideWorldDialog(nullptr): the world dialog is not ported yet.
}

void TaxesHoverWindow::Update(float dt) {
    if (GameState::TutorialStep() < 0x22) return;
    if (building) {
        int ready = building->GetReadyGoldAmount();
        int full = building->GetFullGoldAmount();
        if (ready != gold) {
            pulse = 1.f;
            bonusPulse = false;
            gold = ready;
            bonusReady = ready == full && (unsigned)building->GetReadyTime() < bonusTime;
            blue->SetVisibility(true);
            green->SetVisibility(bonusReady);
            bronze->SetVisibility(false);
            IconManager::GetIcon("bless_count_60");   // Texture::ForceLoad
            icon->SetTexture(IconManager::GetIcon(ready == full ? "gold_pile" : "gold_pile_small"));
        } else if (ready == full && !(pulse > 0.f)) {
            if ((unsigned)building->GetReadyTime() < bonusTime) {
                pulse = 1.f;
                bonusPulse = true;
                bonusReady = false;
                blue->SetVisibility(false);
                green->SetVisibility(true);
                bronze->SetVisibility(false);
                icon->SetTexture(IconManager::GetIcon("bless_count_60"));
            } else {
                icon->SetTexture(IconManager::GetIcon("gold_pile"));
                blue->SetVisibility(true);
                green->SetVisibility(false);
                bronze->SetVisibility(false);
            }
        }
    }
    if (!(pulse > 0.f)) return;
    // A pulse: shader 0xe brightens the button around its centre, |cos(pi t)| shaped.
    float amp = bonusPulse ? 0.05f : 0.1f;
    float k = amp - amp * std::fabs(std::cos(pulse * 3.14159f));
    float a = k + k + 1.f;
    root->SetAlpha(a, false);
    blue->SetAlpha(a, false);
    green->SetAlpha(bonusReady ? 1.f - pulse : 1.f, false);
    root->SetCustomShader(0xe, true);
    root->SetColor((float)root->x + (float)root->w * 0.5f, (float)root->y + (float)root->h * 0.5f, k + 1.f, true);
    pulse -= dt;
    if (!(pulse > 0.f)) {
        root->SetCustomShader(0, true);
        root->SetColor(1.f, 1.f, 1.f, true);
    }
}
