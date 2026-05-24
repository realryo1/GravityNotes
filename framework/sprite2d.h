#pragma once

#include <d3d11.h>
#include "renderer.h"
#include "texture.h"
#include "component.h"
#include "shadermanager.h"
#include <DirectXMath.h>
using namespace DirectX;

// 2D フリップ タイプ
enum class FLIPTYPE2D : unsigned char
{
	FLIPTYPE2D_NONE = 0x00,        // フリップなし
	FLIPTYPE2D_HORIZONTAL = 0x01,  // 左右反転
	FLIPTYPE2D_VERTICAL = 0x02,    // 上下反転
	FLIPTYPE2D_BOTH = 0x03         // 上下左右反転
};

// 頂点構造体
struct Vertex
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT3 normal;
	XMFLOAT4 color;    // 頂点カラー（R,G,B,A）
	XMFLOAT2 texCoord; // テクスチャ座標
};

// プロトタイプ宣言
void Sprite_Initialize(void);
void Sprite_Finalize(void);
void Sprite_BeginDraw2D(void);
void Sprite_EndDraw2D(void);
void Sprite_Single_Draw(XMFLOAT2 pos, XMFLOAT2 size, float rot, XMFLOAT4 color, BLENDSTATE bstate, ID3D11ShaderResourceView* texture, FLIPTYPE2D flipType = FLIPTYPE2D::FLIPTYPE2D_NONE,SHADERTYPE shadertype = S_UNLIT);
void Sprite_Split_Draw(XMFLOAT2 pos, XMFLOAT2 size, float rot, XMFLOAT4 color, BLENDSTATE bstate, ID3D11ShaderResourceView* texture, int divideX, int divideY, int textureNumber , SHADERTYPE shadertype = S_UNLIT);

//void Sprite_Draw(XMFLOAT2 pos, XMFLOAT2 size, float rot, XMFLOAT4 color, BLENDSTATE bstate, ID3D11ShaderResourceView* texture, SHADERTYPE shadertype = S_UNLIT, int divideX = 0, int divideY = 0, int textureNumber = 0);

// Sprite クラス 2D 用 Transform2D に組む
class Sprite2D : public Transform2D
{
protected:
	XMFLOAT4 m_Color;    // スプライトの色
	BLENDSTATE m_BlendState; // ブレンドステート
	ID3D11ShaderResourceView* m_Texture; // テクスチャ
	FLIPTYPE2D m_FlipType; // フリップ設定
public:
	// pos: 中心位置, size: 幅と高さ, rotation: 角度(度)
	// texturePath: テクスチャファイルパス(LoadTexture関数で読み込み)
	Sprite2D(const XMFLOAT2& pos, const XMFLOAT2& size, float rotation, const XMFLOAT4& color, BLENDSTATE bstate, const wchar_t* texturePath)
		: Transform2D(pos, rotation, size), m_Color(color), m_BlendState(bstate), m_Texture(nullptr), m_FlipType(FLIPTYPE2D::FLIPTYPE2D_NONE)
	{
		if (texturePath) {
			m_Texture = LoadTexture(texturePath);
		}
	}

	~Sprite2D()
	{
		if (m_Texture) {
			m_Texture->Release();
			m_Texture = nullptr;
		}
	}

	XMFLOAT4 GetColor(void) const { return m_Color; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	BLENDSTATE GetBlendState(void) const { return m_BlendState; }
	ID3D11ShaderResourceView* GetTexture(void) const { return m_Texture; }
	FLIPTYPE2D GetFlipType(void) const { return m_FlipType; }
	void SetFlipType(FLIPTYPE2D flipType) { m_FlipType = flipType; }

	// インスタンスごとに描画する
	void Draw()
	{
		Sprite_Single_Draw(m_Position, m_Scale, m_Rotation, m_Color, m_BlendState, m_Texture, m_FlipType);
	}
};

// SplitSprite クラス 分割テクスチャ用
class SplitSprite : public Transform2D
{
protected:
	XMFLOAT4 m_Color;    // スプライトの色
	BLENDSTATE m_BlendState; // ブレンドステート
	ID3D11ShaderResourceView* m_Texture; // テクスチャ
	int m_DivideX;       // 横分割数
	int m_DivideY;       // 縦分割数
	int m_TextureNumber; // 描画するテクスチャ番号
public:
	// pos: 中心位置, size: 幅と高さ, rotation: 角度(度)
	// divideX: 横分割数, divideY: 縦分割数, textureNumber: 描画するテクスチャ番号
	// texturePath: テクスチャファイルパス(LoadTexture関数で読み込み)
	SplitSprite(const XMFLOAT2& pos, const XMFLOAT2& size, float rotation, const XMFLOAT4& color, BLENDSTATE bstate, const wchar_t* texturePath, int divideX, int divideY)
		: Transform2D(pos, rotation, size), m_Color(color), m_BlendState(bstate), m_Texture(nullptr), m_DivideX(divideX), m_DivideY(divideY), m_TextureNumber(0)
	{
		if (texturePath) {
			m_Texture = LoadTexture(texturePath);
		}
	}

	~SplitSprite()
	{
		if (m_Texture) {
			m_Texture->Release();
			m_Texture = nullptr;
		}
	}

	BLENDSTATE GetBlendState(void) const { return m_BlendState; }
	int GetDivideX(void) const { return m_DivideX; }
	int GetDivideY(void) const { return m_DivideY; }

	void SetTextureNumber(int textureNumber) { m_TextureNumber = textureNumber; }
	int GetTextureNumber(void) const { return m_TextureNumber; }

	// インスタンスごとに描画する
	//　描画するテクスチャ番号を引数で指定（デフォルト0）
	virtual void Draw()
	{
		Sprite_Split_Draw(m_Position, m_Scale, m_Rotation, m_Color, m_BlendState, m_Texture, m_DivideX, m_DivideY, m_TextureNumber);
	}
};
