// ImGui - standalone example application for DirectX 9
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.

#include <imgui.h>
#include "imgui_impl_dx9.h"
#include "imgui_impl_dx9.cpp"
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

/////////////////////////////////////////////////////////////////////////////////
#include <boost/filesystem.hpp>
#include <Windows.h>
typedef int(*TModUnInit)(ImGuiContext* context);
typedef int(*TModRender)(ImGuiContext* context);
typedef int(*TModInit)(ImGuiContext* context);
typedef void(*TModTextureData)(int index, unsigned char** out_pixels, int* out_width, int* out_height, int* out_bytes_per_pixel);
typedef bool(*TModGetTextureDirtyRect)(int index, int dindex, RECT* rect);
typedef void(*TModSetTexture)(int index, void* texture);
typedef int(*TModTextureBegin)();
typedef void(*TModTextureEnd)();
typedef bool(*TModUpdateFont)(ImGuiContext* context);
typedef bool(*TModMenu)(bool* show);

TModUnInit modUnInit = nullptr;
TModRender modRender = nullptr;
TModInit modInit = nullptr;
TModTextureData modTextureData = nullptr;
TModGetTextureDirtyRect modGetTextureDirtyRect = nullptr;
TModSetTexture modSetTexture = nullptr;
TModTextureBegin modTextureBegin = nullptr;
TModTextureEnd modTextureEnd = nullptr;
TModUpdateFont modUpdateFont = nullptr;
TModMenu modMenu = nullptr;
HMODULE mod = nullptr;
/////////////////////////////////////////////////////////////////////////////////

// Data
//static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp;
static std::vector<LPDIRECT3DTEXTURE9>       g_ModTextures;

static bool ImGui_ImplDX9_CreateModTexture(int texindex)
{
	if (!modTextureData)
		return false;

	g_ModTextures.resize(std::max<size_t>(g_ModTextures.size(), texindex + 1));
	auto& g_ModTexture = g_ModTextures.at(texindex);

	if (LPDIRECT3DTEXTURE9 tex = g_ModTexture)
	{
		tex->Release();
		if (modSetTexture)
			modSetTexture(texindex, nullptr);
		g_ModTexture = nullptr;
	}
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height, bytes_per_pixel;
	modTextureData(texindex, &pixels, &width, &height, &bytes_per_pixel);

	// Upload texture to graphics system
	if (g_pd3dDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_ModTexture, NULL) < 0)
		return false;
	D3DLOCKED_RECT tex_locked_rect;
	if (g_ModTexture->LockRect(0, &tex_locked_rect, NULL, 0) != D3D_OK)
		return false;
	for (int y = 0; y < height; y++)
		memcpy((unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + (width * bytes_per_pixel) * y, (width * bytes_per_pixel));
	g_ModTexture->UnlockRect(0);

	if (modSetTexture)
		modSetTexture(texindex, g_ModTexture);
	return true;
}

bool ImGui_ImplDX9_CreateDeviceObjects_()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	if (!g_pd3dDevice)
		return false;
	if (!ImGui_ImplDX9_CreateFontsTexture())
		return false;
	return true;
}


extern LRESULT ImGui_ImplDX9_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplDX9_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
			ImGui_ImplDX9_InvalidateDeviceObjects();
			int idx = 0;
			for (auto& tex : g_ModTextures)
			{
				if (tex)
				{
					tex->Release();
					modSetTexture(idx, nullptr);
					++idx;
				}
			}
			g_ModTextures.clear();

			g_d3dpp.BackBufferWidth = LOWORD(lParam);
			g_d3dpp.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
            if (hr == D3DERR_INVALIDCALL)
                IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects_();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int main(int argc, char** argv)
{
	/////////////////////////////////////////////////////////////////////////////////
	wchar_t szPath[1024] = { 0, };
	GetModuleFileNameW(NULL, szPath, 1023);
	boost::filesystem::path p(szPath);
	if (argc > 1)
	{
		if(boost::filesystem::exists(p.parent_path() / argv[1]))
			p = p.parent_path() / argv[1];
		else
			p = argv[1];
	}
	else
	{
		p = p.parent_path() / "ActWebSocketImguiOverlay.dll";
	}
	mod = LoadLibraryW(p.wstring().c_str());
	if (mod)
	{
		modInit = (TModInit)GetProcAddress(mod, "ModInit");
		modRender = (TModRender)GetProcAddress(mod, "ModRender");
		modUnInit = (TModUnInit)GetProcAddress(mod, "ModUnInit");
		modTextureData = (TModTextureData)GetProcAddress(mod, "ModTextureData");
		modSetTexture = (TModSetTexture)GetProcAddress(mod, "ModSetTexture");
		modGetTextureDirtyRect = (TModGetTextureDirtyRect)GetProcAddress(mod, "ModGetTextureDirtyRect");
		modTextureBegin = (TModTextureBegin)GetProcAddress(mod, "ModTextureBegin");
		modTextureEnd = (TModTextureEnd)GetProcAddress(mod, "ModTextureEnd");
		modUpdateFont = (TModUpdateFont)GetProcAddress(mod, "ModUpdateFont");
		modMenu = (TModMenu)GetProcAddress(mod, "ModMenu");
	}
	/////////////////////////////////////////////////////////////////////////////////

    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, _T("ImGui Example"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(_T("ImGui Example"), _T("ImGui DirectX9 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    LPDIRECT3D9 pD3D;
    if ((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
    {
        UnregisterClass(_T("ImGui Example"), wc.hInstance);
        return 0;
    }
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	/////////////////////////////////////////////////////////////////////////////////
	if (modInit)
	{
		modInit(ImGui::GetCurrentContext());
	}
	/////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	if (modUnInit)
	{
		modUnInit(ImGui::GetCurrentContext());
	}
	/////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////
	if (modInit)
	{
		modInit(ImGui::GetCurrentContext());
	}
	/////////////////////////////////////////////////////////////////////////////////
	// Create the D3DDevice
    if (pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
    {
        pD3D->Release();
        UnregisterClass(_T("ImGui Example"), wc.hInstance);
        return 0;
    }

    // Setup ImGui binding
    ImGui_ImplDX9_Init(hwnd, g_pd3dDevice);
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;

    // Load Fonts
    // (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
    //ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    bool show_test_window = true;
    bool show_another_window = false;
    ImVec4 clear_col = ImColor(114, 144, 154);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
	ImGui_ImplDX9_CreateDeviceObjects_();
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
		if (modUpdateFont)
		{
			if (modUpdateFont(ImGui::GetCurrentContext()))
			{
				if (g_FontTexture)
					g_FontTexture->Release();
				g_FontTexture = nullptr;
			}
		}
		if (modTextureBegin && modTextureEnd && modTextureData && modGetTextureDirtyRect && modSetTexture)
		{
			int textures = modTextureBegin();
			for (int texidx = 0; texidx < textures; ++texidx)
			{
				if (texidx >= g_ModTextures.size() || !g_ModTextures[texidx]) {
					ImGui_ImplDX9_CreateModTexture(texidx);
				}
				else
				{
					RECT rect;
					int rectidx = 0;
					int width, height, bits_per_pixel;
					unsigned char *pixels;
					if (modGetTextureDirtyRect(texidx, 0, &rect))
					{
						modTextureData(texidx, &pixels, &width, &height, &bits_per_pixel);
						int rectidx = 0;
						D3DLOCKED_RECT mod_atlas_rect;

						HRESULT hr = S_OK;
						auto tex = g_ModTextures[texidx];
						if (SUCCEEDED(tex->LockRect(0, &mod_atlas_rect, nullptr, 0)))
						{
							if (mod_atlas_rect.pBits)
							{
								while (modGetTextureDirtyRect(texidx, rectidx++, &rect))
								{
									int bx = std::min<int>(rect.left, std::min<int>(rect.right, width - 1));
									int by = std::min<int>(rect.top, std::min<int>(rect.bottom, height - 1));
									int ex = std::max<int>(rect.left, std::min<int>(rect.right, width - 1));
									int ey = std::max<int>(rect.top, std::min<int>(rect.bottom, height - 1));
									for (int y = by; y <= ey; y++)
									{
										BYTE* mapped_data = static_cast<BYTE *>(mod_atlas_rect.pBits) + mod_atlas_rect.Pitch * y + bx *bits_per_pixel;
										BYTE* data = pixels + (width * bits_per_pixel) * y + bx *bits_per_pixel;
										const int size = (ex - bx + 1) * bits_per_pixel;
										memcpy(mapped_data, data, size);
									}
								}
							}
							tex->UnlockRect(0);
						}
					}
				}
			}
			modTextureEnd();
		}
        ImGui_ImplDX9_NewFrame();

        // Rendering
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, false);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_col.x*255.0f), (int)(clear_col.y*255.0f), (int)(clear_col.z*255.0f), (int)(clear_col.w*255.0f));
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
			/////////////////////////////////////////////////////////////////////////////////
			if (modRender)
			{
				modRender(ImGui::GetCurrentContext());
			}
			/////////////////////////////////////////////////////////////////////////////////

            ImGui::Render();
            g_pd3dDevice->EndScene();
        }
        g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    }

    ImGui_ImplDX9_Shutdown();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (pD3D) pD3D->Release();
    UnregisterClass(_T("ImGui Example"), wc.hInstance);

	/////////////////////////////////////////////////////////////////////////////////
	if (modUnInit)
	{
		modUnInit(ImGui::GetCurrentContext());
	}
	/////////////////////////////////////////////////////////////////////////////////

    return 0;
}
