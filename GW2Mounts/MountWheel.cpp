#define _USE_MATH_DEFINES
#include <math.h>
#include <functional>
#include "resource.h"
#include "inputs.h"
#include "MountWheel.h"

#define REF_SCREEN_WIDTH		(1920.0f)
#define REF_SCREEN_HEIGHT		(1080.0f)

#define TEX_BACKGROUND_WIDTH	(1024.0f/REF_SCREEN_WIDTH)
#define TEX_BACKGROUND_HEIGHT	(1024.0f/REF_SCREEN_HEIGHT)

#define WHEEL_SIZE				(870.0f)
#define WHEEL_RADIUS			(0.5f * WHEEL_SIZE/REF_SCREEN_WIDTH)
#define MIDDLE_CIRCLE_SIZE		(250.0f)
#define MIDDLE_CIRCLE_RADIUS	(0.5f * MIDDLE_CIRCLE_SIZE/REF_SCREEN_WIDTH)	

#define TEX_MOUNT_WIDTH			(512.0f/REF_SCREEN_WIDTH)
#define TEX_MOUNT_HEIGHT		(512.0f/REF_SCREEN_HEIGHT)

#define TEX_LOGO_WIDTH			((MIDDLE_CIRCLE_SIZE - 80.0f)/REF_SCREEN_WIDTH)
#define TEX_LOGO_HEIGHT			((MIDDLE_CIRCLE_SIZE - 80.0f)/REF_SCREEN_HEIGHT)

#define TEX_CURSOR_WIDTH		(32.0f/REF_SCREEN_WIDTH)
#define TEX_CURSOR_HEIGHT		(32.0f/REF_SCREEN_HEIGHT)

#define SQUARE(x) ((x) * (x))

MountWheel::MountWheel(Mounts *mount_list)
{
	if (!mount_list)
	{
		throw std::invalid_argument("Null pointer");
	}
	MountList = mount_list;
	WheelFadeInEffect.SetEffectDuration(300);
	MountHoverEffect.SetEffectDuration(500);
}

MountWheel::~MountWheel()
{
	ReleaseResources();
	MountList = nullptr;
}

void MountWheel::Show()
{
	if (State == RESOURCES_LOADED)
	{
		State = WINDOW_VISIBLE;
		WheelFadeInEffect.Start();

		if (ActionModeEnabled)
		{
			WheelPosition.x = WheelPosition.y = 0.5f;

			RECT rect = { 0 };
			if (GetWindowRect(GameWindow, &rect))
			{
				if (SetCursorPos((rect.right - rect.left) / 2 + rect.left, (rect.bottom - rect.top) / 2 + rect.top))
				{
					MousePos.x = (LONG)(ScreenSize.cx * 0.5f);
					MousePos.y = (LONG)(ScreenSize.cy * 0.5f);
				}
			}
		}
		else
		{
			(void)GetCursorPos(&MousePos);
			WheelPosition.x = MousePos.x / (float)ScreenSize.cx;
			WheelPosition.y = MousePos.y / (float)ScreenSize.cy;
		}

		DetermineHoveredMount();
	}
}

bool MountWheel::IsVisible()
{
	return (State == WINDOW_VISIBLE);
}

void MountWheel::SetKeyBind(const KeySequence& keybind)
{
	KeyBind = keybind;
}

KeySequence& MountWheel::GetKeyBind()
{
	return KeyBind;
}

void MountWheel::SetWheelScale(float scale)
{
	WheelScale = scale;
}

float MountWheel::GetWheelScale()
{
	return WheelScale;
}

void MountWheel::EnableActionMode(bool enable)
{
	ActionModeEnabled = enable;
}

bool MountWheel::isActionModeEnabled()
{
	return ActionModeEnabled;
}

void MountWheel::SetScreenSize(uint width, uint height)
{
	ScreenSize.cx = width;
	ScreenSize.cy = height;
}

void MountWheel::LoadResources(IDirect3DDevice9 * dev, HMODULE dll, HWND game_window)
{
	if ((State == RESOURCES_NO_INIT) && dev && dll && game_window)
	{
		Device = dev;
		DllModule = dll;
		GameWindow = game_window;
		try
		{
			Quad = std::make_unique<UnitQuad>(Device);
		}
		catch (...)
		{
			Quad = nullptr;
			return;
		}
		ID3DXBuffer* errorBuffer = nullptr;
		D3DXCreateEffectFromResource(Device, DllModule, MAKEINTRESOURCE(IDR_SHADER), nullptr, nullptr, 0, nullptr, &MainEffect, &errorBuffer);
		if (errorBuffer)
		{
			errorBuffer->Release();
			errorBuffer = nullptr;
		}
		if (!MainEffect)
		{
			Quad.reset();
			return;
		}
		D3DXCreateTextureFromResource(Device, DllModule, MAKEINTRESOURCE(IDR_BACKGROUND), &BackgroundTexture);
		if (!BackgroundTexture)
		{
			Quad.reset();
			MainEffect->Release();
			MainEffect = nullptr;
			return;
		}
		MountList->LoadTextures(Device, DllModule);
		State = RESOURCES_LOADED;
	}
}

void MountWheel::ReleaseResources()
{
	if (State != RESOURCES_NO_INIT)
	{
		Quad.reset();
		MountList->UnloadTextures();
		BackgroundTexture->Release();
		BackgroundTexture = nullptr;
		if (ActionCursorTexture)
		{
			ActionCursorTexture->Release();
			ActionCursorTexture = nullptr;
		}
		MainEffect->Release();
		MainEffect = nullptr;
		Device = nullptr;
		DllModule = nullptr;
		GameWindow = nullptr;
		State = RESOURCES_NO_INIT;
	}
}

bool MountWheel::ProcessInputEvents(UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (State != WINDOW_VISIBLE)
	{
		return false;
	}

	switch (msg)
	{
	case WM_KILLFOCUS:
		Hide();
		break;
	case WM_MOUSEMOVE:
		MousePos.x = (signed short)(lParam);
		MousePos.y = (signed short)(lParam >> 16);
		if (!CameraEnabled)
		{
			DetermineHoveredMount();
			if (DragEnabled)
			{
				WheelPosition.x += (MousePos.x - DragStartPos.x) / (float)ScreenSize.cx;
				WheelPosition.y += (MousePos.y - DragStartPos.y) / (float)ScreenSize.cy;
				DragStartPos = MousePos;
				return true;
			}
			if (MouseOverWheel || ActionModeEnabled)
			{
				return true;
			}
		}
		break;
	case WM_INPUT:
		if (ActionModeEnabled)
		{
			UINT dwSize = 40;
			static BYTE lpb[40];

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT,
				lpb, &dwSize, sizeof(RAWINPUTHEADER));

			RAWINPUT* raw = (RAWINPUT*)lpb;

			if (raw->header.dwType == RIM_TYPEMOUSE)
				return true;
		}
		break;
	case WM_LBUTTONDOWN:
		LeftMousePressed = true;
		if (ActionModeEnabled)
		{
			return true;
		}
		if (MouseOverWheel)
		{
			return true;
		}
		CameraEnabled = true;
		break;
	case WM_LBUTTONUP:
		if (LeftMousePressed)
		{
			if (CurrentMountHovered == Mounts::NONE)
			{
				CurrentMountHovered = MountList->GetFavoriteMount();
			}
			KeySequence mount_keybind;
			if (MountList->GetMountKeyBind(CurrentMountHovered, mount_keybind))
			{
				SendKeybind(mount_keybind);
			}
			bool left_mouse_bypass = CameraEnabled;
			Hide();
			if (!left_mouse_bypass)
			{
				return true;
			}
		}
		break;
	case WM_RBUTTONDOWN:
		if (ActionModeEnabled)
		{
			return true;
		}
		if (MouseOverWheel)
		{
			DragEnabled = true;
			DragStartPos = MousePos;
			return true;
		}
		CameraEnabled = true;
		break;
	case WM_RBUTTONUP:
		if (ActionModeEnabled)
		{
			return true;
		}
		if (DragEnabled)
		{
			DragEnabled = false;
			return true;
		}
		if (CameraEnabled)
		{
			MousePos.x = (signed short)(lParam);
			MousePos.y = (signed short)(lParam >> 16);
			CameraEnabled = false;
		}
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDBLCLK:
		Hide();
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			EscPressed = true;
			return true;
		}
		Hide();
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if ((wParam == VK_ESCAPE) && EscPressed)
		{
			Hide();
			return true;
		}
		break;
	}
	return false;
}

void MountWheel::Draw()
{
	if (State != WINDOW_VISIBLE)
	{
		return;
	}

	Quad->Bind();

	uint passes = 0;

	// Setup viewport
	D3DVIEWPORT9 vp;
	vp.X = vp.Y = 0;
	vp.Width = (DWORD)ScreenSize.cx;
	vp.Height = (DWORD)ScreenSize.cy;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	Device->SetViewport(&vp);

	D3DXVECTOR4 screenSize((float)ScreenSize.cx, (float)ScreenSize.cy, 1.f / ScreenSize.cx, 1.f / ScreenSize.cy);
	D3DXVECTOR4 baseSpriteDimensions;
	baseSpriteDimensions.x = WheelPosition.x;
	baseSpriteDimensions.y = WheelPosition.y;
	baseSpriteDimensions.z = WheelScale;
	baseSpriteDimensions.w = WheelScale;

	MainEffect->SetTechnique("Background");
	MainEffect->SetTexture("texBackground", BackgroundTexture);
	D3DXVECTOR4 background_dimensions = baseSpriteDimensions;
	background_dimensions.z *= TEX_BACKGROUND_WIDTH;
	background_dimensions.w *= TEX_BACKGROUND_HEIGHT;
	MainEffect->SetVector("g_vSpriteDimensions", &background_dimensions);
	MainEffect->SetFloat("g_fFadeInProgress", WheelFadeInEffect.GetProgress());
	MainEffect->SetFloat("g_fHoverProgress", MountHoverEffect.GetProgress());
	MainEffect->SetFloat("g_fTimer", fmod(timeInMS() / 1010.f, 55000.f));

	MainEffect->Begin(&passes, 0);
	MainEffect->BeginPass(0);
	Quad->Draw();
	MainEffect->EndPass();
	MainEffect->End();

	MainEffect->SetTechnique("MountImage");
	MainEffect->SetInt("g_iMountHovered", CurrentMountHovered);
	MainEffect->Begin(&passes, 0);
	MainEffect->BeginPass(0);

	POINT mount_sectors[Mounts::NUMBER_MOUNTS] = { {0,-1},{1,-1},{1,1},{0,1},{-1,1},{-1,-1} };
	for (int it = Mounts::RAPTOR; it < Mounts::NUMBER_MOUNTS; it++)
	{
		Mounts::Mount mount = static_cast<Mounts::Mount>(it);
		D3DXVECTOR4 mount_dimensions = baseSpriteDimensions;
		mount_dimensions.z *= TEX_MOUNT_WIDTH;
		mount_dimensions.w *= TEX_MOUNT_HEIGHT;

		mount_dimensions.x += mount_sectors[it].x * mount_dimensions.z/2.0f;
		mount_dimensions.y += mount_sectors[it].y * mount_dimensions.w/2.0f;

		MainEffect->SetBool("g_bMountHovered", (mount == CurrentMountHovered));
		MainEffect->SetTexture("texMountImage", MountList->GetMountTexture(mount));
		MainEffect->SetVector("g_vSpriteDimensions", &mount_dimensions);
		MainEffect->CommitChanges();

		Quad->Draw();
	}

	MainEffect->EndPass();
	MainEffect->End();

	Mounts::Mount central_mount = CurrentMountHovered;
	if (central_mount == Mounts::NONE)
	{
		central_mount = MountList->GetFavoriteMount();
	}
	if (central_mount != Mounts::NONE)
	{
		MainEffect->SetTechnique("MountLogo");
		MainEffect->SetTexture("texMountLogo", MountList->GetMountLogoTexture(central_mount));
		D3DXVECTOR4 logo_dimensions = baseSpriteDimensions;
		logo_dimensions.z *= TEX_LOGO_WIDTH;
		logo_dimensions.w *= TEX_LOGO_HEIGHT;
		MainEffect->SetVector("g_vSpriteDimensions", &logo_dimensions);
		std::array<float, 4> mount_color = { 255, 255, 255, 255 };
		(void)MountList->GetMountColor(central_mount, mount_color);
		MainEffect->SetValue("g_vColor", mount_color.data(), sizeof(D3DXVECTOR4));

		MainEffect->Begin(&passes, 0);
		MainEffect->BeginPass(0);
		Quad->Draw();
		MainEffect->EndPass();
		MainEffect->End();
	}

	if (ActionModeEnabled)
	{
		if (!ActionCursorTexture)
		{
			D3DXCreateTextureFromResource(Device, DllModule, MAKEINTRESOURCE(IDR_ACTION_CURSOR), &ActionCursorTexture);
		}
		MainEffect->SetTechnique("Cursor");
		MainEffect->SetTexture("texCursor", ActionCursorTexture);
		D3DXVECTOR4 cursor_dimensions;
		cursor_dimensions.x = MousePos.x * screenSize.z + TEX_CURSOR_WIDTH / 2.0f;
		cursor_dimensions.y = MousePos.y * screenSize.w + TEX_CURSOR_HEIGHT / 2.0f;
		cursor_dimensions.z = TEX_CURSOR_WIDTH;
		cursor_dimensions.w = TEX_CURSOR_HEIGHT;
		MainEffect->SetVector("g_vSpriteDimensions", &cursor_dimensions);
		MainEffect->SetBool("g_bMountHovered", (Mounts::NONE != CurrentMountHovered));

		MainEffect->Begin(&passes, 0);
		MainEffect->BeginPass(0);
		Quad->Draw();
		MainEffect->EndPass();
		MainEffect->End();
	}
	else
	{
		if (ActionCursorTexture)
		{
			ActionCursorTexture->Release();
			ActionCursorTexture = nullptr;
		}
	}
}

void MountWheel::Hide()
{
	if (State == WINDOW_VISIBLE)
	{
		State = RESOURCES_LOADED;
		DragEnabled = false;
		MouseOverWheel = false;
		CameraEnabled = false;
		LeftMousePressed = false;
		EscPressed = false;
		CurrentMountHovered = Mounts::NONE;
	}
}

void MountWheel::DetermineHoveredMount()
{
	D3DXVECTOR2 mouse_pos;
	mouse_pos.x = MousePos.x / (float)ScreenSize.cx;
	mouse_pos.y = MousePos.y / (float)ScreenSize.cy;
	mouse_pos -= WheelPosition;

	mouse_pos.y *= (float)ScreenSize.cy / (float)ScreenSize.cx;

	Mounts::Mount LastMountHovered = CurrentMountHovered;


	FLOAT d3dx_mouse_pos = D3DXVec2LengthSq(&mouse_pos);

	if (d3dx_mouse_pos > SQUARE(WheelScale * WHEEL_RADIUS)) //Out of wheel
	{
		MouseOverWheel = false;
		CurrentMountHovered = Mounts::NONE;
	}
	else
	{
		MouseOverWheel = true;
		// Middle circle does not count as a hover event
		if (d3dx_mouse_pos > SQUARE(WheelScale * MIDDLE_CIRCLE_RADIUS))
		{
			/*atan2 returns values from (-PI, PI], graphically:
						|-PI/2
						|
			   PI -------------- 0
						|
						|PI/2
			 So, if the first mount (the raptor) is in positive Y-axis, 
			 adding PI/2 returns 0 radians for the raptor. Then, other mounts 
			 have an angle relative to it:
						|Raptor = 0
						|
			-PI/2 -------------- PI/2  
						|
						|PI
			*/
			float MouseAngle = atan2(mouse_pos.y, mouse_pos.x) + 0.5f * (float)M_PI;
			/* Convert negative values to positive adding 2*PI, so:
			   0, PI  -> mounts in right middle circle
			   -PI, 0 -> mounts in left middle circle -> PI, 2*PI */
			if (MouseAngle < 0)
			{
				MouseAngle += 2.0f * (float)M_PI;
			}

			float MountAngle = 2.0f * (float)M_PI / Mounts::NUMBER_MOUNTS;
			int MountId = int((MouseAngle - MountAngle / 2) / MountAngle + 1) % Mounts::NUMBER_MOUNTS;

			CurrentMountHovered = static_cast<Mounts::Mount>(MountId);
			if (!MountList->IsMountEnabled(CurrentMountHovered))
			{
				CurrentMountHovered = Mounts::NONE;
			}
		}
		else
		{
			CurrentMountHovered = Mounts::NONE;
		}
	}

	if ((CurrentMountHovered != Mounts::NONE) && (LastMountHovered != CurrentMountHovered))
	{
		MountHoverEffect.Start();
	}
		
}