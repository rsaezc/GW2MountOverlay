#include "main.h"
#include <d3d9.h>
#include "minhook/include/MinHook.h"
#include "ConfigurationWindow.h"
#include "MountWheel.h"
#include "Mounts.h"
#include "InputKeys.h"

Mounts MountList;
MountWheel WheelWindow(&MountList);
ConfigurationWindow ConfigWindow(&WheelWindow, &MountList);

HWND GameWindow = nullptr;
WNDPROC BaseWndProc = nullptr;
HMODULE DllModule = nullptr;

bool WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
#ifdef _DEBUG
		while (!IsDebuggerPresent());
#endif
		DllModule = hModule;

		/* Add an extra reference count to the library so it persists through GW2's load-unload routine
		   without which problems start arising with ReShade */
		TCHAR selfpath[MAX_PATH];
		GetModuleFileName(DllModule, selfpath, MAX_PATH);
		LoadLibrary(selfpath);

		MH_Initialize();
		ConfigWindow.InitResources();
		InputKeys::InitInputQueue();
		break;
	}
	case DLL_PROCESS_DETACH:
		WheelWindow.ReleaseResources();
		ConfigWindow.DeInitResources();
		/* We'll just leak a bunch of things and let the driver/OS take care of it, since we have no clean exit point
		   and calling FreeLibrary in DllMain causes deadlocks */
		break;
	}

	return true;
}

extern IMGUI_IMPL_API LRESULT  ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	bool msg_exhausted = false;
	InputKeys::InputKey input_key = { msg, wParam, lParam };

	if (msg == WM_KILLFOCUS)
	{
		InputKeys::ClearInput();
		ConfigWindow.Hide();
		WheelWindow.Hide();
	}
	else
	{
		if (!InputKeys::ProcessInputKeyFromInputMessage(input_key, msg, wParam, lParam))
		{
			if (msg != WM_NULL) //Not input key. So, keybind or other window message
			{
				if (InputKeys::IsKeybindSent() && WheelWindow.IsWaitingEvent())
				{
					WheelWindow.DismountEndEvent();
				}
				return CallWindowProc(BaseWndProc, hWnd, input_key.msg, input_key.wParam, input_key.lParam);
			}
			else
			{
				//Input key delayed.
				return true;
			}
		}

		if (ConfigWindow.IsVisible())
		{
			msg_exhausted = ConfigWindow.ProcessInputEvents(hWnd, input_key.msg, input_key.wParam, input_key.lParam);
		}
		else
		{
			if (WheelWindow.IsVisible())
			{
				msg_exhausted = WheelWindow.ProcessInputEvents(input_key.msg, input_key.wParam, input_key.lParam);
			}
			else
			{
				if (InputKeys::GetInputEvent() == InputKeys::INPUT_PRESS_EVENT)
				{
					if (InputKeys::GetPressedKeys() == WheelWindow.GetKeyBind()) //Determine overlay key binds
					{
						WheelWindow.Show();
						msg_exhausted = true;
					}
				}
			}

			if (!msg_exhausted)
			{
				if (InputKeys::GetInputEvent() == InputKeys::INPUT_PRESS_EVENT)
				{
					if (InputKeys::GetPressedKeys() == ConfigWindow.GetKeyBind()) //Determine config window key binds
					{

						ConfigWindow.Show();
						msg_exhausted = true;
					}
				}
			}
		}

		InputKeys::ClearInputEvents();
		if (msg_exhausted)
		{
			return true;
		}
	}
	//Message not exhausted (i.e. needs more processing) must be sent to game
	return CallWindowProc(BaseWndProc, hWnd, input_key.msg, input_key.wParam, input_key.lParam);
}

void PreCreateDevice(HWND hFocusWindow)
{
	GameWindow = hFocusWindow;

	// Hook WndProc
	if (!BaseWndProc)
	{
		BaseWndProc = (WNDPROC)GetWindowLongPtr(hFocusWindow, GWLP_WNDPROC);
		SetWindowLongPtr(hFocusWindow, GWLP_WNDPROC, (LONG_PTR)&WndProc);
	}
}

void PostCreateDevice(IDirect3DDevice9* temp_device, D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	ConfigWindow.ConfigureResources(temp_device, GameWindow);

	WheelWindow.SetScreenSize(pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight);
	WheelWindow.LoadResources(temp_device, DllModule, GameWindow);
}

void PreReset()
{
	ConfigWindow.ReleaseResources();
	WheelWindow.ReleaseResources();
}

void PostReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS *pPresentationParameters)
{

	ConfigWindow.LoadResources();
	WheelWindow.SetScreenSize(pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight);
	WheelWindow.LoadResources(dev, DllModule, GameWindow);

}

void Draw(IDirect3DDevice9* dev, bool FrameDrawn, bool SceneEnded)
{
	// This is the closest we have to a reliable "update" function, so use it as one
	InputKeys::SendQueuedInputs(GameWindow);

	if (FrameDrawn)
	{
		return;
	}

	if (ConfigWindow.IsVisible() || WheelWindow.IsVisible())
	{
		// We have to use Present rather than hooking EndScene because the game seems to do final UI compositing after EndScene
		// This unfortunately means that we have to call Begin/EndScene before Present so we can render things, but thankfully for modern GPUs that doesn't cause bugs
		if (SceneEnded)
		{
			dev->BeginScene();
		}

		ConfigWindow.Draw();
		WheelWindow.Draw();

		if (SceneEnded)
		{
			dev->EndScene();
		}
	}
}