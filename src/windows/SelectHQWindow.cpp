// SelectHQWindow: "who are you?" (Select_HQ.xml): the hero's name and the choice between the man
// and the woman, each shown as a dressed figure. Mode 0 is the new game (tutorial step 2), mode 1
// changes the name only, mode 2 the name and the gender (for crystals, item 0x2f9, outside the
// tutorial).
// Port of SelectHQWindow (libkingdom.so 5.11), 0x338bc8..0x33b060. Statics at 0x62c13c.
// Not ported (online): the Facebook first name and gender as defaults (SystemFuncs::
// IsConnectedToFB), the connect buttons (hidden by Init on the original too), Billing's log.
#include <cmath>
#include <cstring>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/PlayerEquipmentInfo.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace SelectHQWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // SelectHQWindow::wnd
Window* g_root = nullptr;                // 0x62c13c
void (*g_onContinue)() = nullptr;        // 0x62c140
int g_mode = 0;                          // 0x62c144 SelectHQWindow::ChangeMode
Textfield* g_input = nullptr;            // 0x62c148 text_01 (editable, 64 characters)
Window* g_inputBg = nullptr;             // 0x62c14c text_input (pulses while typing)
Textfield* g_nameLabel = nullptr;        // 0x62c150 text_name_choose
Textfield* g_placeholder = nullptr;      // 0x62c154 text_type_something
Textfield* g_hint = nullptr;             // 0x62c158 hint_text
Window* g_maleBg = nullptr;              // 0x62c15c gender_selection_button_01.gender_selection_male_bg
Window* g_femaleBg = nullptr;            // 0x62c160 gender_selection_button_02.gender_selection_female_bg
Textfield* g_okText = nullptr;           // 0x62c164 button_confirm_small.text_start_raid
PvP::PlayerEquipmentInfo g_maleInfo;     // 0x62c168 the looks tried on the man
PvP::PlayerEquipmentInfo g_femaleInfo;   // 0x62c4b0 and on the woman
Shared::PlayerHolder g_maleHolder;       // 0x62c7f8 gender_selection.dude_on
Window* g_itemHolder = nullptr;          // 0x62c9b4 Character_item_holder.xml (the figures' item cell)
Shared::PlayerHolder g_femaleHolder;     // 0x62c9b8 gender_selection.dudette_on
int g_gender = 0;                        // 0x62cb74 0 the man, 1 the woman
Window* g_close = nullptr;               // 0x62cb78 x_button
Window* g_costFemale = nullptr;          // 0x62cb7c change_cost_holder_01
Textfield* g_costFemaleText = nullptr;   // 0x62cb80
Window* g_costMale = nullptr;            // 0x62cb84 change_cost_holder_02
Textfield* g_costMaleText = nullptr;     // 0x62cb88
Window* g_random = nullptr;              // 0x62cb8c button_randomise_hero
Window* g_randomIcon = nullptr;          // 0x62cb90
Textfield* g_randomText = nullptr;       // 0x62cb94
bool g_edited = false;                   // 0x62cb98 the player typed a name
Window* g_confirm = nullptr;             // 0x62cb9c button_confirm_small
GUI::TweenEffect* g_tween = nullptr;     // 0x62cba0
Shared::SmallLogoWindow g_logo;          // 0x62cba4

void CenterOnScreen() {
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

bool HasText() {
    const char32_t* t = g_input->GetText();
    return t && *t;
}

// @0x338bc8
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

void OnTweenIn() {}   // @0x339500

// @0x338c80: closing it in the tutorial is the tutorial's step 4.
void OnTweenOut() {
    if (GameState::IsTutorial()) GameState::tutorial = 4;
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x338ccc: the input box pulses while the field is being edited.
void Update(float) {
    if (!g_queue->shown) return;
    if (!g_input->IsInputFocused()) {
        g_inputBg->SetAlpha(1.f, true);
        return;
    }
    float c = std::cos((float)Timer::GetTime() * 8.f);
    g_inputBg->SetAlpha(1.1f + c * 0.1f, true);
}

// @0x338d88
void UpdateContents() {
    const bool named = HasText();
    const bool nameMode = (unsigned)g_mode < 2, genderMode = g_mode == 0 || g_mode == 2;
    g_inputBg->SetVisibility(nameMode);
    g_nameLabel->SetVisibility(nameMode);
    g_placeholder->SetVisibility(nameMode && !named);
    g_input->SetVisibility(nameMode);
    g_hint->SetVisibility(genderMode);
    g_maleBg->SetVisibility(genderMode);
    g_femaleBg->SetVisibility(genderMode);
    g_hint->SetText(StringTable::GetString("C_HERO_GENDER"));
    g_nameLabel->SetText(StringTable::GetString("C_HERO_NAMEP_TXT"));
    g_okText->SetText(StringTable::GetString("HUD_OK"));
    g_placeholder->SetText(StringTable::GetString("C_HERO_NAME_PLACEHOLDER"));
    // The two figures: in the new game each wears the looks tried on it; when the gender is being
    // changed both start from what the hero wears now, and the hero's own side shows just that.
    PvP::PlayerEquipmentInfo own;
    own.GetPlayerEquipment();
    if (g_mode == 2) {
        std::memcpy(g_maleInfo.items, own.items, sizeof own.items);
        std::memcpy(g_femaleInfo.items, own.items, sizeof own.items);
    }
    const bool ownMale = g_mode != 0 && GameState::IsMalePlayer();
    g_maleHolder.PlacePlayerEquipment(ownMale ? own : g_maleInfo, (float)g_maleHolder.root->x,
                                      (float)g_maleHolder.root->y, g_itemHolder, true, 1.f, 0.f, 0.f, 0.f, true);
    const bool ownFemale = g_mode != 0 && !GameState::IsMalePlayer();
    g_femaleHolder.PlacePlayerEquipment(ownFemale ? own : g_femaleInfo, (float)g_femaleHolder.root->x,
                                        (float)g_femaleHolder.root->y, g_itemHolder, false, 1.f, 0.f, 0.f, 0.f, true);
    // The chosen one is bright, the other dim.
    g_maleHolder.root->SetAlpha(g_gender == 0 ? 1.f : 0.3f, true);
    g_maleHolder.root->SetCustomShader(10, true);
    g_femaleHolder.root->SetAlpha(g_gender == 1 ? 1.f : 0.3f, true);
    g_femaleHolder.root->SetCustomShader(10, true);
    g_maleBg->SetAlpha(g_gender == 0 ? 1.f : 0.5f, false);
    g_femaleBg->SetAlpha(g_gender == 1 ? 1.f : 0.5f, false);
    const bool tutorial = GameState::IsTutorial();
    g_close->SetVisibility(!tutorial);
    // The change's price stands under the other gender.
    const Items::ItemInfo* change = Items::GetItemInfo(0x2f9);
    const bool toFemale = !tutorial && GameState::IsMalePlayer() && g_mode == 2;
    const bool toMale = !tutorial && !GameState::IsMalePlayer() && g_mode == 2;
    g_costFemale->SetVisibility(toMale);
    g_costFemaleText->SetVisibility(toMale);
    if (change) g_costFemaleText->SetText(ToWideString((int)change->GetCostCrystals(false)));
    g_costMale->SetVisibility(toFemale);
    g_costMaleText->SetVisibility(toFemale);
    if (change) g_costMaleText->SetText(ToWideString((int)change->GetCostCrystals(false)));
    g_random->SetVisibility(g_mode == 0 && Setting("enable_random_face").GetInt() == 1);
    g_randomIcon->SetTexture(IconManager::GetIcon("gold_refresh", false), true);
    g_randomText->SetText(StringTable::GetString("RANDOMIZE_APPEARANCE"));
}

// @0x339504: a name too long for the field is cut; the placeholder shows while it is empty and OK
// needs a name.
void OnEdit() {
    if (const char32_t* t = g_input->GetText()) {
        unsigned fit = Render::GetSymbolCountInWidth(g_input->font, t, (unsigned)(g_input->w - (int)(g_root->scale * 20.f)));
        std::u32string s = t;
        if (fit < s.size()) {
            s.resize(fit);
            g_input->SetText(s.c_str());
        }
    }
    g_edited = true;
    if (!HasText()) {
        g_placeholder->SetVisibility(true);
        g_confirm->SetEnabled(false);
        return;
    }
    g_placeholder->SetVisibility(false);
    g_confirm->SetEnabled(g_gender != -1);
}

// @0x338c50
void OnContinue() {
    void (*fn)() = g_onContinue;
    g_onContinue = nullptr;
    if (fn) fn();
}

// @0x3396f0
void OnOk() {
    if (!HasText()) {
        PopupWindow::Show(StringTable::GetString("ERROR_HERO_NAME"), PopupWindow::Hide, PopupWindow::Hide, nullptr);
        return;
    }
    if (Entity* player = EntityManager::GetPlayer()) {
        // The hero is entity 0xc (the man) or 0xd (the woman).
        const bool toMale = g_gender == 0 && player->GetEntityData()->id == 0xd;
        const bool toFemale = g_gender == 1 && player->GetEntityData()->id == 0xc;
        const bool changed = toMale || toFemale;
        const Items::ItemInfo* change = Items::GetItemInfo(0x2f9);
        if (changed && !GameState::IsTutorial() && change) {
            NotEnoughWindow::ResetRequirements();
            NotEnoughWindow::AddRequirement(GameState::kCrystal, change->GetCostCrystals(false));
            if (!NotEnoughWindow::CheckRequirements()) {
                NotEnoughWindow::Show();
                return;
            }
        }
        if (toMale) player->SwitchEntityData(EntityFactory::GetEntityByID(0xc));
        if (toFemale) player->SwitchEntityData(EntityFactory::GetEntityByID(0xd));
        GameState::SetPlayerGender(g_gender == 0);
        if (changed) {
            // The old looks do not fit the new body.
            for (unsigned b = 1; b < 10; ++b) GameState::BindCustomizationTo(b, nullptr);
            if (!GameState::IsTutorial() && change)
                GameState::ChangeResourceAmount(GameState::kCrystal, -(int)change->GetCostCrystals(false));
        }
    }
    // The new game's hero gets the looks chosen here.
    if (g_mode == 0 && GameState::IsTutorial()) {
        const PvP::PlayerEquipmentInfo& info = g_gender == 0 ? g_maleInfo : g_femaleInfo;
        for (unsigned b = 0; b < 0x20; ++b) {
            uint32_t id = info.customs[b].id;
            if (id == 0) continue;
            if (GameState::GetItemAmount(id, false) == 0) GameState::AddItem(id, 1, false, false, false);
            GameState::BindCustomizationTo(b, GameState::GetFirstItemByID(id, false));
        }
    }
    GameState::SetPlayerName(g_input->GetText());
    GameState::SetCastleName(StringTable::GetString("DEFAULT_CASTLE_NAMES"));
    if (!GameState::IsTutorial()) {
        // UNVERIFIED (milestone 5, the character window): CharacterInfoWindow::RemovePreviewItems.
        CharacterInfoWindow::UpdateContents();
    }
    Hide();
}

// @0x339ad0
void SetZ(float z) {
    g_root->SetZ(z);
    CenterOnScreen();
}

// @0x3396b8 (in the tutorial Back does nothing, but is taken)
bool OnBack() {
    if (!g_queue->shown) return false;
    if (!GameState::IsTutorial()) Hide();
    return true;
}

// @0x339ba8: a random look. For each customisation binding 2..9 but 7, one of the customisation
// items on sale for it (type 0x11, not the other gender's by its name) or, as one more equal
// chance, none. Binding 5 never takes items 0x2ef and 0x2f0.
void OnRandomizeAppearance() {
    PvP::PlayerEquipmentInfo& info = g_gender == 0 ? g_maleInfo : g_femaleInfo;
    std::vector<const Items::ItemInfo*> all;
    for (unsigned i = 0; const Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
        if (it->type != 0x11 || !it->IsOnSale()) continue;
        const bool male = std::strncmp(it->name, "male_", 5) == 0, female = std::strncmp(it->name, "female_", 7) == 0;
        if ((male && g_gender == 1) || (female && g_gender == 0)) continue;
        all.push_back(it);
    }
    for (unsigned b = 2; b < 10; ++b) {
        if (b == 7 || all.empty()) continue;
        std::vector<const Items::ItemInfo*> fit;
        for (const Items::ItemInfo* it : all)
            if (it->slot == b) fit.push_back(it);
        if (fit.empty()) continue;
        unsigned r = (unsigned)Rand48::lrand48() % ((unsigned)fit.size() + 1);
        if (r == fit.size() || (b == 5 && (fit[r]->id == 0x2ef || fit[r]->id == 0x2f0))) info.customs[b].id = 0;
        else info.customs[b].id = fit[r]->id;
    }
    UpdateContents();
}

// @0x33a0a4: a gender is chosen; while the player has typed no name it follows the gender.
void OnCharacter(int gender) {
    if (g_mode == 1) return;
    g_gender = gender;
    if (!g_edited)
        g_input->SetText(StringTable::GetString(gender != 0 ? "DEFAULT_USER_NAMES_FEMALE" : "DEFAULT_USER_NAMES"));
    UpdateContents();
    g_confirm->SetEnabled(HasText());
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("SelectHQWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root->visibleSelf; }           // @0x338c28
void SetMode(int mode) { g_mode = mode; }                  // @0x338c3c
void SetOnContinue(void (*fn)()) { g_onContinue = fn; }    // @0x338c14 (no caller in 5.11, nor of OnContinue)

// @0x33a3d0
void Init() {
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Select_HQ.xml", "", -1.f, 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    CenterOnScreen();
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(Hide);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_logo.LoadFrom(g_root, "logo_holder_small");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_maleHolder.LoadFrom(g_root, "gender_selection.dude_on");
    g_femaleHolder.LoadFrom(g_root, "gender_selection.dudette_on");
    Window* boy = GUI::GetWindowTyped<Window>(g_root, "gender_selection.click_area_boy");
    boy->SetOnClick([] { OnCharacter(0); });
    boy->SetActionSound("ui_click", 1);
    Window* girl = GUI::GetWindowTyped<Window>(g_root, "gender_selection.click_area_girl");
    girl->SetOnClick([] { OnCharacter(1); });
    girl->SetActionSound("ui_click", 1);
    g_maleBg = GUI::GetWindowTyped<Window>(g_root, "gender_selection_button_01.gender_selection_male_bg");
    g_femaleBg = GUI::GetWindowTyped<Window>(g_root, "gender_selection_button_02.gender_selection_female_bg");
    Button* maleButton = GUI::GetWindowTyped<Button>(g_root, "gender_selection_button_01.clickArea");
    maleButton->SetOnClick([] { OnCharacter(0); });
    maleButton->SetActionSound("ui_click", 1);
    Button* femaleButton = GUI::GetWindowTyped<Button>(g_root, "gender_selection_button_02.clickArea");
    femaleButton->SetOnClick([] { OnCharacter(1); });
    femaleButton->SetActionSound("ui_click", 1);
    g_costFemale = GUI::GetWindowTyped<Window>(g_root, "change_cost_holder_01");
    g_costFemaleText = GUI::GetWindowTyped<Textfield>(g_root, "change_cost_holder_01.text_price");
    g_costMale = GUI::GetWindowTyped<Window>(g_root, "change_cost_holder_02");
    g_costMaleText = GUI::GetWindowTyped<Textfield>(g_root, "change_cost_holder_02.text_price");
    g_hint = GUI::GetWindowTyped<Textfield>(g_root, "hint_text");
    g_nameLabel = GUI::GetWindowTyped<Textfield>(g_root, "text_name_choose");
    g_inputBg = GUI::GetWindowTyped<Window>(g_root, "text_input");
    g_placeholder = GUI::GetWindowTyped<Textfield>(g_root, "text_type_something");
    g_input = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_input->SetEditable(0x40);
    g_input->SetOnEdit(OnEdit);
    g_random = GUI::GetWindowTyped<Window>(g_root, "button_randomise_hero");
    g_random->SetOnClick(OnRandomizeAppearance);
    g_randomIcon = GUI::GetWindowTyped<Window>(g_root, "button_randomise_hero.icon_placeholder_35");
    g_randomIcon->takesZ = true;
    g_randomText = GUI::GetWindowTyped<Textfield>(g_root, "button_randomise_hero.text");
    g_confirm = GUI::GetWindowTyped<Window>(g_root, "button_confirm_small");
    g_confirm->SetOnClick(OnOk);
    g_confirm->SetActionSound("ui_click", 1);
    g_okText = GUI::GetWindowTyped<Textfield>(g_root, "button_confirm_small.text_start_raid");
    GUI::GetWindowTyped<Window>(g_root, "button_confirm_small.button_inactive")->SetVisibility(false);
    GUI::GetWindowTyped<Window>(g_root, "connect_fb_buttons")->SetVisibility(false);
    GUI::GetWindowTyped<Window>(g_root, "facebook_no_wifi")->SetVisibility(false);
    g_itemHolder = GUI::RegisterUI("../resource/kingdom_icons/1Original/Character_item_holder.xml", "", 1.f, 0, 0, 0, 0,
                                   false, 1.f);
    g_queue->hideOnOuterClick = true;
    g_queue->fn.back = OnBack;
    g_queue->fn.zRange = WindowManager::ReturnMediumZRange;
    g_queue->fn.update = Update;
}

// @0x339b40
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    delete g_itemHolder;
}

// @0x33ab58
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_gender = !GameState::IsTutorial() && !GameState::IsMalePlayer() ? 1 : 0;
    g_maleInfo = PvP::PlayerEquipmentInfo();
    g_femaleInfo = PvP::PlayerEquipmentInfo();
    UpdateContents();
    g_edited = false;
    g_input->SetText(StringTable::GetString("DEFAULT_USER_NAMES"));
    g_input->SetEditable(0x40);
    if (!GameState::IsTutorial()) {
        g_edited = true;
        g_input->SetText(GameState::GetPlayerName());
    }
    g_root->SetZ(g_root->z);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x339694
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

}  // namespace SelectHQWindow
