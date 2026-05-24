#pragma once

#include <d3d11.h>
#include <string>
#include "sprite2d.h"
#include "sound.h"

struct IMFDXGIDeviceManager;
struct IMFMediaEngine;

class Movie : public Transform2D
{
public:
    Movie(const XMFLOAT2& pos, float width, float rotation, const XMFLOAT4& color, BLENDSTATE bstate, const wchar_t* filePath);
    ~Movie();

    void Update();
    void Draw();

    ID3D11ShaderResourceView* GetShaderResourceView() const;

private:
    bool Initialize(const wchar_t* filePath);
    bool CreateVideoTexture(UINT width, UINT height);
    void Finalize();

    class MediaEngineNotify;

    XMFLOAT4 m_Color;
    BLENDSTATE m_BlendState;
    SoundData* m_pAudio;
    bool m_AudioStarted;

    IMFDXGIDeviceManager* m_pDXGIDeviceManager;
    struct IMFMediaEngine* m_pMediaEngine;
    MediaEngineNotify* m_pMediaEngineNotify;
    ID3D11Texture2D* m_pVideoTexture;
    ID3D11ShaderResourceView* m_pVideoSRV;
    UINT m_DxgiResetToken;
};
