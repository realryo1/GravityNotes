//sprite.cpp
#include "sprite2d.h"
#include "texture.h"
#include "define.h"
#include "model.h"
#include "shadermanager.h"
#include "mouse.h"
#include <cmath>
using namespace DirectX;

//グローバル変数
static constexpr int NUM_VERTEX = 6; // 使用できる最大頂点数
static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

// 2D描画最適化用フラグ
static bool g_b2DBegun = false;

static void BeginSprite2DCommon()
{
	const XMMATRIX world = XMMatrixIdentity();
	const XMMATRIX view = XMMatrixIdentity();
	const XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0.0f, DRAW_SCREEN_WIDTH, DRAW_SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

	SetWorldMatrix(world);
	SetViewMatrix(view);
	SetProjectionMatrix(proj);
}

//----------------------------
//スプライト初期化
//----------------------------
void Sprite_Initialize()
{
	g_pDevice = GetDevice();

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;//<<<<<<<格納する最大頂点数
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);
}

//----------------------------
//スプライト終了
//----------------------------
void Sprite_Finalize()
{
	if (g_pVertexBuffer) {
		g_pVertexBuffer->Release();
		g_pVertexBuffer = nullptr;
	}
}

//----------------------------
// 2D描画開始（シェーダーセットアップを一度だけ実行）
//----------------------------
void Sprite_BeginDraw2D()
{
	if (g_b2DBegun) return;

	BeginSprite2DCommon();
	g_b2DBegun = true;
}

//----------------------------
// 2D描画終了（フラグをリセット）
//----------------------------
void Sprite_EndDraw2D()
{
	g_b2DBegun = false;
}

bool ClickSprite2D::IsClick()
{
	Mouse_State ms{};
	Mouse_GetState(&ms);

	const float clientW = Direct3D_GetClientWidth();
	const float clientH = Direct3D_GetClientHeight();
	if (clientW <= 0.0f || clientH <= 0.0f) {
		m_WasLeftDown = ms.leftButton;
		return false;
	}

	const float targetAspect = SCREEN_WIDTH / SCREEN_HEIGHT;
	const float windowAspect = clientW / clientH;

	float vpX, vpY, vpW, vpH;
	if (windowAspect > targetAspect)
	{
		vpH = clientH;
		vpW = clientH * targetAspect;
		vpX = (clientW - vpW) * 0.5f;
		vpY = 0.0f;
	}
	else
	{
		vpW = clientW;
		vpH = clientW / targetAspect;
		vpX = 0.0f;
		vpY = (clientH - vpH) * 0.5f;
	}

	const float logicalX = (ms.x - vpX) / vpW * SCREEN_WIDTH;
	const float logicalY = (ms.y - vpY) / vpH * SCREEN_HEIGHT;

	const bool inArea =
		(logicalX >= m_Position.x - m_Scale.x && logicalX <= m_Position.x + m_Scale.x) &&
		(logicalY >= m_Position.y - m_Scale.y && logicalY <= m_Position.y + m_Scale.y);

	const bool leftDown = ms.leftButton;
	const bool pressedThisFrame = (leftDown && !m_WasLeftDown);
	m_WasLeftDown = leftDown;

	return pressedThisFrame && inArea;
}

//----------------------------
//単一スプライト描画（汎用的になるように外に出す）
//----------------------------
void Sprite_Single_Draw(XMFLOAT2 pos, XMFLOAT2 size, float rot, XMFLOAT4 color, BLENDSTATE bstate, ID3D11ShaderResourceView* texture, FLIPTYPE2D flipType, SHADERTYPE shadertype)
{
	// 頂点レイアウトとシェーダーのセット
	GetDeviceContext()->IASetInputLayout(GetShader(shadertype)->GetVertexLayout());
	GetDeviceContext()->VSSetShader(GetShader(shadertype)->GetVertexShader(), NULL, 0);
	GetDeviceContext()->PSSetShader(GetShader(shadertype)->GetPixelShader(), NULL, 0);

	// Sprite_BeginDraw2D()が呼ばれていない場合は個別にセットアップ（互換性維持）
	if (!g_b2DBegun) {
		BeginSprite2DCommon();
	}

	// テクスチャ設定
	ID3D11ShaderResourceView* tex = texture;
	GetDeviceContext()->PSSetShaderResources(0, 1, &tex);
	SetBlendState(bstate);

	// 頂点データ
	D3D11_MAPPED_SUBRESOURCE msr;
	GetDeviceContext()->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = (Vertex*)msr.pData;

	// HD座標→描画解像度へスケーリング
	float drawPosX = pos.x * DRAW_SCALE_X;
	float drawPosY = pos.y * DRAW_SCALE_Y;
	float halfX = size.x * DRAW_SCALE_X * 0.5f;
	float halfY = size.y * DRAW_SCALE_X * 0.5f;

	// 回転（度->ラジアン）
	float rotDeg = rot;
	float rad = XMConvertToRadians(rotDeg);
	float co = cosf(rad);
	float si = sinf(rad);

	// ローカル頂点（中心原点）
	float lx[4] = { -halfX, halfX, -halfX, halfX };
	float ly[4] = { -halfY, -halfY, halfY, halfY };

	// 回転と並進移動の頂点座標を計算
	for (int i = 0; i < 4; ++i) {
		float rx = lx[i] * co - ly[i] * si;
		float ry = lx[i] * si + ly[i] * co;
		v[i].position = { rx + drawPosX, ry + drawPosY, 0.0f };
		v[i].normal = { 0.0f, 0.0f, -1.0f }; // 法線を設定（NaN回避とライティング用）
		v[i].color = color;
	}

	// テクスチャ座標（フリップに対応）
	// flipTypeに応じてテクスチャ座標を反転
	float texCoordU[2] = { 0.0f, 1.0f };
	float texCoordV[2] = { 0.0f, 1.0f };

	// 左右反転（FLIPTYPE2D_HORIZONTAL）
	if (static_cast<unsigned char>(flipType) & static_cast<unsigned char>(FLIPTYPE2D::FLIPTYPE2D_HORIZONTAL))
	{
		texCoordU[0] = 1.0f;
		texCoordU[1] = 0.0f;
	}

	// 上下反転（FLIPTYPE2D_VERTICAL）
	if (static_cast<unsigned char>(flipType) & static_cast<unsigned char>(FLIPTYPE2D::FLIPTYPE2D_VERTICAL))
	{
		texCoordV[0] = 1.0f;
		texCoordV[1] = 0.0f;
	}

	// テクスチャ座標を設定
	v[0].texCoord = { texCoordU[0], texCoordV[0] };
	v[1].texCoord = { texCoordU[1], texCoordV[0] };
	v[2].texCoord = { texCoordU[0], texCoordV[1] };
	v[3].texCoord = { texCoordU[1], texCoordV[1] };

	GetDeviceContext()->Unmap(g_pVertexBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	GetDeviceContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	GetDeviceContext()->Draw(4, 0);
}

//----------------------------
//分割テクスチャ描画（テクスチャを分割して指定したパターンのみ描画）
//----------------------------
void Sprite_Split_Draw(XMFLOAT2 pos, XMFLOAT2 size, float rot, XMFLOAT4 color, BLENDSTATE bstate, ID3D11ShaderResourceView* texture, int divideX, int divideY, int textureNumber, SHADERTYPE shadertype)
{
	g_pDevice = GetDevice();
	g_pContext = GetDeviceContext();

	// 頂点レイアウトとシェーダーのセット
	GetDeviceContext()->IASetInputLayout(GetShader(shadertype)->GetVertexLayout());
	GetDeviceContext()->VSSetShader(GetShader(shadertype)->GetVertexShader(), NULL, 0);
	GetDeviceContext()->PSSetShader(GetShader(shadertype)->GetPixelShader(), NULL, 0);


	// Sprite_BeginDraw2D()が呼ばれていない場合は個別にセットアップ（互換性維持）
	if (!g_b2DBegun) {
		BeginSprite2DCommon();
	}

	// テクスチャ設定
	ID3D11ShaderResourceView* tex = texture;
	g_pContext->PSSetShaderResources(0, 1, &tex);
	SetBlendState(bstate);

	// 頂点データ
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = (Vertex*)msr.pData;

	// HD座標→描画解像度へスケーリング
	float drawPosX = pos.x * DRAW_SCALE_X;
	float drawPosY = pos.y * DRAW_SCALE_Y;
	float halfX = size.x * DRAW_SCALE_X * 0.5f;
	float halfY = size.y * DRAW_SCALE_X * 0.5f;

	float rad = XMConvertToRadians(rot);
	float co = cosf(rad);
	float si = sinf(rad);

	float lx[4] = { -halfX, halfX, -halfX, halfX };
	float ly[4] = { -halfY, -halfY, halfY, halfY };

	for (int i = 0; i < 4; ++i) {
		float rx = lx[i] * co - ly[i] * si;
		float ry = lx[i] * si + ly[i] * co;
		v[i].position = { rx + drawPosX, ry + drawPosY, 0.0f };
		v[i].normal = { 0.0f, 0.0f, -1.0f }; // 法線を設定（NaN回避とライティング用）
		v[i].color = color;
	}

	// 分割されたテクスチャの対応する部分のUV座標を計算
	float texWidth = 1.0f / divideX;        // 1つのテクスチャの横幅
	float texHeight = 1.0f / divideY;       // 1つのテクスチャの縦幅

	// textureNumberから行・列を計算
	int col = textureNumber % divideX;
	int row = textureNumber / divideX;

	// テクスチャ座標の最小・最大値を計算
	float texMinU = col * texWidth;
	float texMaxU = (col + 1) * texWidth;
	float texMinV = row * texHeight;
	float texMaxV = (row + 1) * texHeight;

	// テクスチャ座標を設定
	v[0].texCoord = { texMinU, texMinV };
	v[1].texCoord = { texMaxU, texMinV };
	v[2].texCoord = { texMinU, texMaxV };
	v[3].texCoord = { texMaxU, texMaxV };

	g_pContext->Unmap(g_pVertexBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_pContext->Draw(4, 0);
}


////------------------------------------------------------------------------------------
////スタンダードテクスチャ描画関数（デフォルト引数で非分割テクスチャにも対応）
////------------------------------------------------------------------------------------
//void Sprite_Draw(XMFLOAT2 pos, XMFLOAT2 size, float rot, XMFLOAT4 color, BLENDSTATE bstate, ID3D11ShaderResourceView* texture, SHADERTYPE shadertype = S_UNLIT, int divideX, int divideY, int textureNumber)
//{
//	g_pDevice = GetDevice();
//	g_pContext = GetDeviceContext();
//
//	// 頂点レイアウトとシェーダーのセット
//	GetDeviceContext()->IASetInputLayout(GetShader(shadertype)->GetVertexLayout());
//	GetDeviceContext()->VSSetShader(GetShader(shadertype)->GetVertexShader(), NULL, 0);
//	GetDeviceContext()->PSSetShader(GetShader(shadertype)->GetPixelShader(), NULL, 0);
//
//
//	// Sprite_BeginDraw2D()が呼ばれていない場合は個別にセットアップ（互換性維持）
//	if (!g_b2DBegun) {
//		BeginSprite2DCommon();
//	}
//
//	// テクスチャ設定
//	ID3D11ShaderResourceView* tex = texture;
//	g_pContext->PSSetShaderResources(0, 1, &tex);
//	SetBlendState(bstate);
//
//	// 頂点データ
//	D3D11_MAPPED_SUBRESOURCE msr;
//	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
//	Vertex* v = (Vertex*)msr.pData;
//
//	// HD座標→描画解像度へスケーリング
//	float drawPosX = pos.x * DRAW_SCALE_X;
//	float drawPosY = pos.y * DRAW_SCALE_Y;
//	float halfX = size.x * DRAW_SCALE_X * 0.5f;
//	float halfY = size.y * DRAW_SCALE_X * 0.5f;
//
//	float rad = XMConvertToRadians(rot);
//	float co = cosf(rad);
//	float si = sinf(rad);
//
//	float lx[4] = { -halfX, halfX, -halfX, halfX };
//	float ly[4] = { -halfY, -halfY, halfY, halfY };
//
//	for (int i = 0; i < 4; ++i) {
//		float rx = lx[i] * co - ly[i] * si;
//		float ry = lx[i] * si + ly[i] * co;
//		v[i].position = { rx + drawPosX, ry + drawPosY, 0.0f };
//		v[i].normal = { 0.0f, 0.0f, -1.0f }; // 法線を設定（NaN回避とライティング用）
//		v[i].color = color;
//	}
//
//	// 分割されたテクスチャの対応する部分のUV座標を計算
//	float texWidth = 1.0f / divideX;        // 1つのテクスチャの横幅
//	float texHeight = 1.0f / divideY;       // 1つのテクスチャの縦幅
//
//	// textureNumberから行・列を計算
//	int col = textureNumber % divideX;
//	int row = textureNumber / divideX;
//
//	// テクスチャ座標の最小・最大値を計算
//	float texMinU = col * texWidth;
//	float texMaxU = (col + 1) * texWidth;
//	float texMinV = row * texHeight;
//	float texMaxV = (row + 1) * texHeight;
//
//	// テクスチャ座標を設定
//	v[0].texCoord = { texMinU, texMinV };
//	v[1].texCoord = { texMaxU, texMinV };
//	v[2].texCoord = { texMinU, texMaxV };
//	v[3].texCoord = { texMaxU, texMaxV };
//
//	g_pContext->Unmap(g_pVertexBuffer, 0);
//
//	UINT stride = sizeof(Vertex);
//	UINT offset = 0;
//	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
//	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//	g_pContext->Draw(4, 0);
//}
