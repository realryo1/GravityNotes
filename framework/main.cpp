//==================================================
//main.cpp
//制作日：2025/05/09
//==================================================

#define _CRT_SECURE_NO_WARNINGS

#include <SDKDDKVer.h> //利用できる最も上位のWindowsプラットフォームが定義される
#define WIN32_LEAN_AND_MEAN	//32bitアプリには不要な情報を無視
#include <iostream>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include "main.h"
#include "define.h"
#include "scene.h"
#include "renderer.h"
#include "debug_ostream.h"
#include "keyboard.h"
#include "mouse.h"
#include "font.h"
#include "sprite2d.h"
#include "fade.h"
#include "sound.h"
#include "shadermanager.h"


#pragma	comment (lib, "d3d11.lib")
#pragma	comment (lib, "d3dcompiler.lib")
#pragma	comment (lib, "winmm.lib")
#pragma	comment (lib, "dxguid.lib")
#pragma	comment (lib, "dinput8.lib")

using namespace DirectX;

// ハイパフォーマンスGPUを使用するためのヒント
extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

//==================================
//グローバル変数
//==================================

int g_CountFPS;       // 描画FPS（Draw/Present の呼び出し回数/秒）
int g_CountUpdateFPS; // 論理更新FPS（Update の呼び出し回数/秒）
long long g_UpdateTime = 0;
long long g_DrawTime = 0;
wchar_t g_DebugStr[2048];
static int g_TargetFPS = FPS;  // 目標FPS（デフォルトは FPS マクロの値）

#pragma comment(lib, "winmm.lib")

//==================================
//SetFPS関数
//==================================
void SetFPS(int fps)
{
	if (fps > 0)
	{
		g_TargetFPS = fps;
	}
}

//==================================
//メイン関数
//==================================
//==================================  
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// モニターごとのDPI認識を有効化（DWMによる拡大描画を防ぎ、物理ピクセルでウィンドウ管理する）
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	//フレームレート計測用変数（steady_clock ベース）
	using SteadyClock = std::chrono::steady_clock;
	using Seconds = std::chrono::duration<double>;
	constexpr double FIXED_STEP = 1.0 / FPS;
	double accumulator  = 0.0;
	auto prevFrameTime  = SteadyClock::now();
	auto fpsLastTime    = SteadyClock::now();
	auto titleLastTime  = SteadyClock::now();
	int  frameCount     = 0; // 描画回数カウント
	int  updateCount    = 0; // 論理更新回数カウント

	//COMコンポーネントの準備（機能を部品化して外部のプログラムから共有利用する仕組み）
	HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

	//ウィンドウクラスの登録
	WNDCLASS wc;//構造体を定義
	ZeroMemory(&wc, sizeof(WNDCLASS));//構造体初期化
	wc.lpfnWndProc = WndProc;//初期化
	wc.lpszClassName = CLASS_NAME;//仕様書の名前
	wc.hInstance = hInstance;//このアプリのこと
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);//cursorの種類
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);//背景色
	RegisterClass(&wc);//構構体をwindowsにセット

	//ウィンドウサイズの調整
	//クライアント領域（描画領域）のサイズを表す矩形
	RECT window_rect = { 0, 0, (LONG)DRAW_SCREEN_WIDTH, (LONG)DRAW_SCREEN_HEIGHT };
	//ウィンドウスタイルの設定
	DWORD window_style = WS_OVERLAPPEDWINDOW;
	//指定のクライアント領域＋ウィンドウスタイルでの全体のサイズを計算
	AdjustWindowRect(&window_rect, window_style, FALSE);
	//矩形の横と縦のサイズを計算
	int window_width = window_rect.right - window_rect.left;
	int window_height = window_rect.bottom - window_rect.top;

	// 作業領域（タスクバーを除いた画面）に収まるようクランプ（16:9比率を保持）
	RECT workArea;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
	// ウィンドウボーダー分を計算（AdjustWindowRect済みの全体サイズ - クライアントサイズ）
	int border_W = window_width  - (int)DRAW_SCREEN_WIDTH;
	int border_H = window_height - (int)DRAW_SCREEN_HEIGHT;
	// 作業領域の70%に収まる最大クライアントサイズ
	int max_client_W = (int)((workArea.right  - workArea.left) * 0.7f) - border_W;
	int max_client_H = (int)((workArea.bottom - workArea.top)  * 0.7f) - border_H;
	if (max_client_W < 1) max_client_W = 1;
	if (max_client_H < 1) max_client_H = 1;
	// 16:9（DRAW_SCREEN比率）を保ちながら縮小スケールを計算
	float scale_W = (float)max_client_W / DRAW_SCREEN_WIDTH;
	float scale_H = (float)max_client_H / DRAW_SCREEN_HEIGHT;
	float client_scale = (scale_W < scale_H) ? scale_W : scale_H;
	if (client_scale > 1.0f) client_scale = 1.0f;
	window_width  = (int)(DRAW_SCREEN_WIDTH  * client_scale) + border_W;
	window_height = (int)(DRAW_SCREEN_HEIGHT * client_scale) + border_H;

	//ウィンドウの作成
	HWND hWnd = CreateWindow(
		CLASS_NAME,
		WINDOW_CAPTION,
		window_style, // リサイズ可能なウィンドウ
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		window_width,//ウィンドウの幅
		window_height,//ウィンドウの高さ
		NULL,
		NULL,
		hInstance,	//アプリのハンドル
		NULL
	);

	ShowWindow(hWnd, nCmdShow);//引数に従って表示非表示

	//ウィンドウ内部の更新要求
	UpdateWindow(hWnd);
	InitRenderer(hInstance, hWnd, TRUE);

	// 初期ウィンドウクライアントサイズをDirect3Dに通知
	{
		RECT cr;
		GetClientRect(hWnd, &cr);
		Direct3D_ResizeWindow((unsigned int)(cr.right - cr.left), (unsigned int)(cr.bottom - cr.top));
		SetWorldViewProjection2D();
	}

	// 全画面・ウィンドウ切替用のキー入力を受け取るために Direct3D_Initialize の後に行う
	// (スワップチェーンへのアクセスが必要な場合があるため)

	Keyboard_Initialize();
	Mouse_Initialize(hWnd);
	InitShader();
	Font_InitializeGlobalData();
	Sprite_Initialize();
	Fade_Initialize();
	InitSound();
	Init();

	//メッセージループ
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	// prevFrameTime / fpsLastTime / titleLastTime は上記の初期化で設定済み
	// （timeBeginPeriod / timeGetTime は使用しない）

	do
	{
		//終了メッセージが来るまでループ （Windowsからのメッセージはそのまま使えない）
		//while (GetMessage(&msg, NULL, 0, 0))　ゲ－ム向きではないらしい
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))	//余計なことをしないので早い
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg); //WndProcが呼び出される
		}
		//ゲームの処理
		else
		{
			auto now = SteadyClock::now();
			double delta = std::chrono::duration_cast<Seconds>(now - prevFrameTime).count();
			prevFrameTime = now;

			// デバッガ停止などの極端な遅延をクランプ
			if (delta > 0.25) delta = 0.25;

			// FPSカウンタ更新（1秒ごと）
			if (std::chrono::duration_cast<Seconds>(now - fpsLastTime).count() >= 1.0)
			{
				g_CountFPS       = frameCount;
				g_CountUpdateFPS = updateCount;
				fpsLastTime  = now;
				frameCount   = 0;
				updateCount  = 0;
			}

			// 論理更新：固定ステップを消化する（最大5ステップ）
			accumulator += delta;
			int steps = 0;
			while (accumulator >= FIXED_STEP && steps < 5)
			{
				// ウィンドウ操作（論理ステップ内で判定：keycopy後に正しいトリガーを参照できる）
				// Alt+Enterで全画面切り替え
				if (Keyboard_IsKeyDown(KK_LEFTALT) || Keyboard_IsKeyDown(KK_RIGHTALT))
				{
					if (Keyboard_IsKeyDownTrigger(KK_ENTER))
					{
						// 全画面切り替え処理
						static bool isFullScreen = false;
						isFullScreen = !isFullScreen;
						IDXGISwapChain* pSwapChain = nullptr;
					}
				}

				// F11キーでボーダレスウィンドウ切り替え
				if (Keyboard_IsKeyDownTrigger(KK_F11))
				{
					static bool isBorderless = false;
					static RECT prevWindowRect = {};
					isBorderless = !isBorderless;

					if (isBorderless)
					{
						// 現在のウィンドウ位置・サイズを保存
						GetWindowRect(hWnd, &prevWindowRect);

						// モニターのサイズを取得
						HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
						MONITORINFO mi = {};
						mi.cbSize = sizeof(mi);
						GetMonitorInfo(hMonitor, &mi);

						// ボーダレス（フレームなし）スタイルに変更
						SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
						SetWindowPos(hWnd, HWND_TOP,
							mi.rcMonitor.left, mi.rcMonitor.top,
							mi.rcMonitor.right - mi.rcMonitor.left,
							mi.rcMonitor.bottom - mi.rcMonitor.top,
							SWP_FRAMECHANGED);
					}
					else
					{
						// 通常ウィンドウスタイルに戻す
						DWORD restored_style = WS_OVERLAPPEDWINDOW;
						SetWindowLong(hWnd, GWL_STYLE, restored_style | WS_VISIBLE);
						SetWindowPos(hWnd, HWND_TOP,
							prevWindowRect.left, prevWindowRect.top,
							prevWindowRect.right - prevWindowRect.left,
							prevWindowRect.bottom - prevWindowRect.top,
							SWP_FRAMECHANGED);
					}
				}

				// 更新時間の計測
				auto startUpdate = std::chrono::high_resolution_clock::now();
				Fade_Update();
				Update();
				auto endUpdate = std::chrono::high_resolution_clock::now();
				g_UpdateTime = std::chrono::duration_cast<std::chrono::microseconds>(endUpdate - startUpdate).count();

				// キー状態を末尾でコピー：Update() が読んだ後に gStateOld を更新
				// → 次ステップで IsKeyDownTrigger が正しく「前回との差分」を検出できる
				keycopy();

				accumulator -= FIXED_STEP;
				steps++;
			}
			updateCount += steps; // 今回のループで実行した論理ステップ数を加算

			// 描画時間の計測
			auto startDraw = std::chrono::high_resolution_clock::now();
			Clear();//バッファのクリア

			SetWorldViewProjection2D(); // 2D専用シーン向けにデフォルトで2D行列を設定
			Draw();

			SetDepthEnable(false);
			Fade_Draw();


			Present();//バッファの表示
			auto endDraw = std::chrono::high_resolution_clock::now();
			g_DrawTime = std::chrono::duration_cast<std::chrono::microseconds>(endDraw - startDraw).count();

			frameCount++; // Present() 1回 = 描画1フレーム

#if defined(_DEBUG)
			//ウィンドウキャプションへ情報を表示（0.2秒に1回更新）
			if (std::chrono::duration_cast<Seconds>(now - titleLastTime).count() >= 0.2)
			{
				titleLastTime = now;
				swprintf(g_DebugStr, sizeof(g_DebugStr) / sizeof(wchar_t),
					L"Draw: %dfps | Logic: %dfps | Total: %lldus | Upd: %lldus | Drw: %lldus",
					g_CountFPS, g_CountUpdateFPS, g_UpdateTime + g_DrawTime, g_UpdateTime, g_DrawTime);
				SetWindowText(hWnd, g_DebugStr);
			}
#endif
		}

	} while (msg.message != WM_QUIT);//windowsから終了メッセージが来たらループ終了

	Finalize();
	UninitSound();
	Fade_Finalize();
	Font_FinalizeGlobalData();
	Sprite_Finalize();
	FinalizeShader();
	FinalizeRenderer();


	//終了
	return (int)msg.wParam;
}

//==================================
//ウィンドウプロシージャ
//メッセージループ内で呼び出し
//==================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//HGDIOBJ hbrWhite, hbrGray;

	//HDC hdc;			//デバイスコンテキスト
	//PAINTSTRUCT ps;		//ウィンドウ画面の大きさなど描画関連の情報

	Mouse_ProcessMessage(uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_ACTIVATEAPP:

		break;
	case WM_SIZE:
		// ウィンドウサイズが変更された（全画面化など）場合にバックバッファを再構成する
		if (wParam != SIZE_MINIMIZED && GetDeviceContext() != NULL)
		{
			unsigned int newW = LOWORD(lParam);
			unsigned int newH = HIWORD(lParam);
			Direct3D_ResizeWindow(newW, newH);
			Direct3D_Resize(newW, newH); // バックバッファをウィンドウサイズ（ネイティブ解像度）に合わせる
		}
		break;
	case WM_SYSKEYDOWN:
		// Alt+Enterキーの全画面切り替え無効化（手動制御に変更）
		if (wParam == VK_RETURN && (lParam & 0x20000000))
		{
			// Alt+Enterの全画面切り替えを無視（イベントを処理してreturnする）
			return 0;
		}
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;
	case WM_KEYUP:
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;
	case WM_SYSKEYUP:
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;
	case WM_KEYDOWN:	//キーが押された
		Keyboard_ProcessMessage(uMsg, wParam, lParam);

		break;
	case WM_CLOSE:
		if (true)
		{
			//OKが押された
			DestroyWindow(hWnd);
		}
		else
		{
			//終わらない
			return 0;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	//必要のないメッセージは適当に処理するらしい
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
