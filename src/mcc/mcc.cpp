#include "mcc.h"

#include <offset_mcc.h>

#include "CGameManager.h"
#include "CGameGlobal.h"

#include "mcc/module/Module.h"
#include "mcc/network/Network.h"
#include "mcc/splitscreen/Splitscreen.h"
#include "global/Global.h"

namespace MCC {
    static bool* bIsInGame;
    static float (__fastcall* deltaTime)(long long qpc);

    float DeltaTime(__int64 a1) {
        return deltaTime(a1);
    }

    bool IsInGame() {
        return *bIsInGame;
    }

    // Halo CE Anniversary builds its projection before the split viewports exist, so every
    // split-screen view renders vertically squished until the engine reloads its settings,
    // which is what closing the F4 menu happens to do. Post that reload ourselves shortly after
    // a level starts rendering. Halo CE + split-screen only; other games do not need it.
    void OnFrame() {
        static bool s_was_in_game = false;
        static int s_frames_in_game = -1; // -1: nothing pending

        if (bIsInGame == nullptr || g_ppGameGlobal == nullptr || g_ppGameEngine == nullptr)
            return; // Present can fire before MCC::Initialize has resolved these

        bool in_game = IsInGame();
        if (in_game && !s_was_in_game) s_frames_in_game = 0;   // level just started
        else if (!in_game) s_frames_in_game = -1;
        s_was_in_game = in_game;

        if (s_frames_in_game < 0) return;
        // ponytail: fixed frame count (~3 s at 60 fps) instead of detecting the renderer's own
        // viewport setup; bump it if the reload lands too early on slow first-time level loads.
        if (++s_frames_in_game < 180) return;
        s_frames_in_game = -1;

        auto p_setting = AlphaRing::Global::MCC::Splitscreen();
        auto gg = GameGlobal();
        auto engine = GameEngine();
        if (!p_setting || !p_setting->b_override || p_setting->player_count < 2) return;
        if (!gg || gg->current_game != CGameGlobal::Halo1 || !engine) return;

        LOG_INFO("Halo1 splitscreen: reloading settings to rebuild the anniversary projection");
        engine->load_setting();
    }

    bool Initialize() {
        bool result;
        CGameEngine** ppGameEngine;
        CGameManager* game_manager;
        CDeviceManager** device_manager;

        AlphaRing::Hook::Offset({
            {0x4000BA0/*0x3FFCAA8*/ , 0x3E4F9F8/*0x3E4B048*/, (void**)&ppGameEngine},
            {0x3F7B190/*0x3F76E50*/ , 0x3DCA200/*0x3DC54D0*/, (void**)&game_manager},
            {0x4001B78/*0x3FFFFF8*/ , 0x3E509C0/*0x3E4E590*/, (void**)&device_manager},
            {OFFSET_MCC_PF_DELTA_TIME, OFFSET_MCC_WS_PF_DELTA_TIME, (void**)&deltaTime},
            {0x4000B9F/*0x3FFCAA7*/ ,0x3E4F9F7/*0x3E4B047*/, (void**)&bIsInGame},
            {0x4000BC8/*0x3FFCAC0*/ , 0x3E4FA18/*0x3E4B060*/, (void**)&g_ppGameGlobal},
        });

        assertm(ppGameEngine != nullptr, "MCC: failed to get ppGameEngine");
        assertm(game_manager != nullptr, "MCC: failed to get pGameManager");
        assertm(device_manager != nullptr, "MCC: failed to get ppDeviceManager");

        result = CGameEngine::Initialize(ppGameEngine);

        assertm(result, "MCC: failed to initialize GameEngine");

        result = CGameManager::Initialize(game_manager);

        assertm(result, "MCC: failed to initialize GameManager");

        assertm(GameManager() != nullptr, "MCC:Splitscreen: GameManager is null"); // static instance

        result = CDeviceManager::Initialize(device_manager);

        assertm(result, "MCC: failed to initialize DeviceManager");

        if (!Module::Initialize())
        {
			MessageBox(nullptr, "MCC: failed to initialize Module", "Error", MB_OK);
            return false;
        }

        if (!Splitscreen::Initialize())
        {
			MessageBox(nullptr, "MCC: failed to initialize Splitscreen", "Error", MB_OK);
            return false;
        }

		////Ask user if they want to enable network
  //      if (MessageBox(nullptr, "Would you like to enable network?", "Network", MB_YESNO) == IDYES)
  //      {
  //          if (!Network::Initialize())
  //          {
  //              MessageBox(nullptr, "MCC: failed to initialize Network", "Error", MB_OK);
  //              return false;
  //          }
  //      }

        return true;
    }
}
