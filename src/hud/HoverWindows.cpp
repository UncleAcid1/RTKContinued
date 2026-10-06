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
#include "game/StringTable.h"
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

// ---- ResourceRestoreHoverWindow ----

ResourceRestoreHoverWindow::ResourceRestoreHoverWindow() {
    name = "ResourceRestoreHoverWindow";
    usesZRange = true;
    itemBoosts = (int)(GameState::GetSetting("item_boosts") + 0.5f) == 1;   // Setting::GetInt
}

ResourceRestoreHoverWindow::~ResourceRestoreHoverWindow() { delete root; }

void ResourceRestoreHoverWindow::Init() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Compact_progress_holder.xml",
                           "Compact_progress_holder.png", 1.f, 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    border = GUI::GetWindowTyped<GUI::Window>(root, "golden_border_box");
    borderH = border->h;
    footer = GUI::GetWindowTyped<GUI::Window>(root, "hint_window_footer");
    jobText = GUI::GetWindowTyped<GUI::Textfield>(root, "text_job");
    jobText->SetWorldOverscale(true);
    bar = GUI::GetWindowTyped<GUI::Window>(root, "unit_info_progress_bar");
    barColor = GUI::GetWindowTyped<GUI::Window>(root, "unit_info_progress_bar.progres_bar_color");
    textUnder = GUI::GetWindowTyped<GUI::Textfield>(root, "unit_info_progress_bar.text_under");
    textOver = GUI::GetWindowTyped<GUI::Textfield>(root, "unit_info_progress_bar.text_over");
    clip = {bar->x, 0, bar->w + bar->x, 0x400};
    barColor->SetClipRect(&clip);
    if (!GUI::IsSmallScreenVersion()) textOver->SetClipRect(&clip);
    if (GUI::IsSmallScreenVersion()) {
        textUnder->SetWorldOverscale(true);
        textOver->SetWorldOverscale(true);
    }
    hereWorks = GUI::GetWindowTyped<GUI::Textfield>(root, "sepparator_here_works.text_here_works");
    hereWorks->SetText(StringTable::GetString("TAP_TO_SPEED_UP"));
    hurry = GUI::GetWindowTyped<GUI::Window>(root, "button_hurry_tiny");
    hurry->SetOnClick([this] { OnSpeedUp(false); });
    crystalIcon = GUI::GetWindowTyped<GUI::Window>(root, "button_hurry_tiny.icon_35_crystal");
    crystalIcon->SetTexture(IconManager::GetIcon("35_crystal"), true);
    itemIcon = GUI::GetWindowTyped<GUI::Window>(root, "button_hurry_tiny.speed_up_item");
    itemIcon->takesZ = true;
    priceText = GUI::GetWindowTyped<GUI::Textfield>(root, "button_hurry_tiny.text_price");
    GUI::GetWindowTyped<GUI::Textfield>(root, "button_hurry_tiny.text")->SetText(StringTable::GetString("BUILDING_BOOST"));
    priceText->SetWorldOverscale(true);
    GUI::GetWindowTyped<GUI::Textfield>(root, "button_hurry_tiny.text")->SetWorldOverscale(true);
    border->SetBorders(10, 10, 10, 10);
    root->SetScreenSpace(false);
}

void ResourceRestoreHoverWindow::SetZ(float z) { root->SetZ(z - 0.0001f); }

void ResourceRestoreHoverWindow::Show() {
    if (shownHover) return;
    BuildingHoverWindow::Show();
    root->SetVisibility(true);
    hurry->SetVisibility(true);
    hereWorks->SetVisibility(false);
    barStarted = false;
    barTick = 0.f;
    barValue = 0.f;
}

void ResourceRestoreHoverWindow::Hide() {
    if (!shownHover) return;
    BuildingHoverWindow::Hide();
    root->SetVisibility(false);
    hurry->SetVisibility(false);
    hereWorks->SetVisibility(false);
    speedingUp = false;
}

bool ResourceRestoreHoverWindow::Click(int x, int y, bool pressed) {
    if (speedingUp) return true;
    if (!shownHover) return false;
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    return root->Click(wx, wy, pressed, false);
}

void ResourceRestoreHoverWindow::RemoveWindow() {
    if (building && speedingUp) {   // a speed-up in flight is paid for
        speedingUp = false;
        int cost = GameState::AdjustCrystalCost(building->data->speedupCb);
        GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
        // Billing::LogCBPurchase(0x13, id, cost): online logging, not ported.
    }
    BuildingHoverWindow::RemoveWindow();
}

void ResourceRestoreHoverWindow::SetPosition(int x, int y) {
    int ny = y - footer->h - border->h;
    int nx = x - root->w / 2;
    BuildingHoverWindow::SetPosition(nx, ny);
    root->SetPosition(nx, ny);
}

void ResourceRestoreHoverWindow::FixWindowPosition(int& x, int& y) {
    BuildingHoverWindow::FixWindowPosition(x, y);
    if (Render::ScreenWidth() < root->w + x) x = Render::ScreenWidth() - root->w;
}

void ResourceRestoreHoverWindow::SetBuilding(Map::Building* b) {
    itemBoosts = (int)(GameState::GetSetting("item_boosts") + 0.5f) == 1;
    BuildingHoverWindow::SetBuilding(b);
    if (!building) return;
    jobText->SetText(StringTable::GetString(building->data->name.c_str()));
    crystalIcon->SetVisibility(!itemBoosts);
    itemIcon->SetVisibility(itemBoosts);
    itemIcon->SetTexture(IconManager::GetIcon("hourglass_speed_35"), true);
    boostItem = 0x269;
}

// The hurry button: the crystal price (or a speed-up item) is confirmed in ConfirmPurchaseWindow,
// then the bar runs to the end in two seconds (OnSpeedUpFinished).
void ResourceRestoreHoverWindow::OnSpeedUp(bool confirmed) {
    if (!building) return;
    if (!itemBoosts) {
        // UNVERIFIED (milestone 3e): NotEnoughWindow checks the crystals (AdjustCrystalCost of
        // speedupcb) and ConfirmPurchaseWindow asks first (OnSpeedUp(true) on yes); neither is
        // ported, so the button does nothing yet.
        (void)confirmed;
        return;
    }
    // UNVERIFIED (Items): the speed-up item path (NotEnoughWindow item check, ItemShopWindow).
}

void ResourceRestoreHoverWindow::OnSpeedUpFinished() {
    speedingUp = false;
    if (!building) return;
    if (!itemBoosts) {
        int cost = GameState::AdjustCrystalCost(building->data->speedupCb);
        GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
    }
    // UNVERIFIED (Items): with item boosts the item is used up (GameState::RemoveItem).
    Hide();
    BuildingHovers::Hide();
    BuildingHovers::Update(0.0, true);
}

void ResourceRestoreHoverWindow::Update(float dt) {
    if (speedingUp) {
        speedT += dt * 0.5f;
        if (speedT > 1.f) OnSpeedUpFinished();
    }
    if (!building) return;
    // UNVERIFIED (Items): boostAmount = Items::GetAmountToBoostTime(boostItem, buildLeft).
    int price = itemBoosts ? boostAmount : GameState::AdjustCrystalCost(building->data->speedupCb);
    std::string ps = std::to_string(price);
    priceText->SetText(std::u32string(ps.begin(), ps.end()).c_str());
    const auto& resp = building->data->respawn;
    uint32_t elapsed = Timer::GetGlobalTime() - building->growStart;
    int total = 0;
    if (resp.size() != 1)
        for (size_t i = 0; i < resp.size() - 1; ++i) total += resp[i].time;
    int left = total - (int)elapsed;
    if (left < 0) left = 0;
    float progress = (float)elapsed / (float)(unsigned)total;
    if (progress < 0.f) progress = 0.f;
    else if (progress > 1.f) progress = 1.f;
    if (speedingUp) {
        progress = speedT;
        left = (int)((float)left * (1.f - speedT));
    }
    std::u32string text;
    if (const char32_t* fmt = StringTable::GetString("BUILDING_RES_CLEAN_PROGRESS")) {
        // SWPrintf("%s" -> the time), then cut at the first '%' left (the format's trailing one).
        std::u32string f = fmt, time = StringTable::GetTimeString(left, false);
        size_t p = f.find(U"%s");
        text = p == std::u32string::npos ? f : f.substr(0, p) + time + f.substr(p + 2);
        size_t cut = text.find(U'%');
        if (cut != std::u32string::npos) text.resize(cut);
    }
    textUnder->SetText(text.c_str());
    textOver->SetText(text.c_str());
    float t = dt + barTick;
    if (!barStarted) barValue = progress;
    barStarted = true;
    barTick = t;
    if (t > 1.f / 30.f) {
        barTick = 0.f;
        barValue = (float)((double)barValue + (double)-(progress - barValue) * -0.1339745962155614);
    }
    float v = speedingUp ? speedT : barValue;
    clip.right = (int)((float)clip.left + (float)bar->w * (0.125f + v * 0.75f));
    bar->UpdatePosition();
    if (speedingUp) {
        int all = 0;
        for (const auto& r : resp) all += r.time;
        float g = (float)Timer::GetGlobalTime() - (float)(unsigned)all * speedT;
        building->growStart = g > 0.f ? (uint32_t)(int)g : 0;
        SetPosition((int)((building->minX + building->maxX) * 0.5f), (int)building->minY);
    }
}
