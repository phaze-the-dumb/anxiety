#include "main.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector3.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "questui/shared/BeatSaberUI.hpp"
#include "HMUI/Touchable.hpp"
#include "questui/shared/QuestUI.hpp"
#include "config-utils/shared/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "ModConfig.hpp"
#include "System/Action.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <ctime>

using namespace QuestUI;
using namespace UnityEngine;
using namespace GlobalNamespace;

bool firstNote = false;

static ModInfo modInfo; // Stores the ID and version of our mod, and is sent to the modloader upon startup
DEFINE_CONFIG(ModConfig);

// Loads the config from disk using our modInfo, then returns it for use
Configuration& getConfig() {
    static Configuration config(modInfo);
    config.Load();
    return config;
}

// Returns a logger, useful for printing debug messages
Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

MAKE_HOOK_MATCH(AudioTimeSyncController_StartSong, &AudioTimeSyncController::StartSong, void,
    AudioTimeSyncController* self,
    float startTimeOffset
) {
    AudioTimeSyncController_StartSong(self, startTimeOffset);

    firstNote = true;
}

MAKE_HOOK_MATCH(ScoreController_HandleNoteWasCut, &ScoreController::HandleNoteWasCut, void,
    ScoreController* self,
    NoteController* note,
    ByRef<GlobalNamespace::NoteCutInfo> info
) {
    ScoreController_HandleNoteWasCut(self, note, info);

    if(info.heldRef.get_allIsOK()){
        firstNote = false;
    } else{
        if(getModConfig().Active.GetValue() && firstNote){
            firstNote = false;
            CRASH_UNLESS(false);
        }

        if(getModConfig().Active.GetValue() && getModConfig().RanNote.GetValue()){
            srand((unsigned) time(0));
            if(1 + (rand() % 6) == 1){
                CRASH_UNLESS(false);
            }
        }
    };
}

MAKE_HOOK_MATCH(ScoreController_HandleNoteWasMissed, &ScoreController::HandleNoteWasMissed, void,
    ScoreController* self,
    NoteController* note
) {
    ScoreController_HandleNoteWasMissed(self, note);

    if(getModConfig().Active.GetValue() && firstNote){
        firstNote = false;
        CRASH_UNLESS(false);
    }

    if(getModConfig().Active.GetValue() && getModConfig().RanNote.GetValue()){
        srand((unsigned) time(0));
        if(1 + (rand() % 6) == 1){
            CRASH_UNLESS(false);
        }
    }
}

// Called at the early stages of game loading
extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
	
    getConfig().Load(); // Load the config file
    getLogger().info("Completed setup!");
}

void DidActivate(HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling){
    if(firstActivation){
        GameObject* container = BeatSaberUI::CreateScrollableSettingsContainer(self->get_transform());

        BeatSaberUI::CreateToggle(container->get_transform(), "Active", getModConfig().Active.GetValue(),
            [](bool value) { 
                getModConfig().Active.SetValue(value);
            });

        BeatSaberUI::CreateToggle(container->get_transform(), "Crash If you miss a random note?", getModConfig().RanNote.GetValue(),
            [](bool value) { 
                getModConfig().RanNote.SetValue(value);
            });
    }
}

// Called later on in the game loading - a good time to install function hooks
extern "C" void load() {
    il2cpp_functions::Init();
    getModConfig().Init(modInfo);

    LoggerContextObject logger = getLogger().WithContext("load");

    QuestUI::Init();
    QuestUI::Register::RegisterModSettingsViewController(modInfo, DidActivate);
    QuestUI::Register::RegisterMainMenuModSettingsViewController(modInfo, DidActivate);
    getLogger().info("Successfully installed Settings UI!");

    getLogger().info("Installing hooks...");
    INSTALL_HOOK(logger, AudioTimeSyncController_StartSong);
    INSTALL_HOOK(logger, ScoreController_HandleNoteWasCut);
    INSTALL_HOOK(logger, ScoreController_HandleNoteWasMissed);
    getLogger().info("Installed all hooks!");
}