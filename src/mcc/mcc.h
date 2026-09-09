#pragma once

namespace MCC {
    bool Initialize();
    float DeltaTime(__int64 a1);
    bool IsInGame();
    // Called once per presented frame (from the D3D11 Present hook).
    void OnFrame();
}
