/*
	D2GL: Diablo 2 LoD Glide/DDraw to OpenGL Wrapper.
	Copyright (C) 2023  Bayaraa

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "pch.h"
#include "win32.h"
#include "d2/common.h"
#include "helpers.h"
#include "modules/hd_cursor.h"
#include "option/menu.h"

#include <detours/detours.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ShowCursor_t ShowCursor_Og = ShowCursor;
SetCursorPos_t SetCursorPos_Og = SetCursorPos;
SetWindowPos_t SetWindowPos_Og = SetWindowPos;
SendMessageA_t SendMessageA_Og = SendMessageA;
DrawTextA_t DrawTextA_Og = DrawTextA;
GetPixel_t GetPixel_Og = GetPixel;

namespace d2gl::win32 {

void initHooks()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)ShowCursor_Og, ShowCursor);
	DetourAttach(&(PVOID&)SetCursorPos_Og, SetCursorPos);
	DetourAttach(&(PVOID&)SetWindowPos_Og, SetWindowPos);
	DetourAttach(&(PVOID&)SendMessageA_Og, SendMessageA);
	DetourAttach(&(PVOID&)DrawTextA_Og, DrawTextA);
	DetourAttach(&(PVOID&)GetPixel_Og, GetPixel);
	DetourTransactionCommit();
}

void destroyHooks()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)ShowCursor_Og, ShowCursor);
	DetourDetach(&(PVOID&)SetCursorPos_Og, SetCursorPos);
	DetourDetach(&(PVOID&)SetWindowPos_Og, SetWindowPos);
	DetourDetach(&(PVOID&)SendMessageA_Og, SendMessageA);
	DetourDetach(&(PVOID&)DrawTextA_Og, DrawTextA);
	DetourDetach(&(PVOID&)GetPixel_Og, GetPixel);
	DetourTransactionCommit();
}

int WINAPI ShowCursor(BOOL bShow)
{
	return bShow ? 1 : -1;
}

BOOL WINAPI SetCursorPos(int X, int Y)
{
	trace("locked: %d", App.cursor.locked);
	if (App.hwnd && App.cursor.locked) {
		POINT pt = { (LONG)((float)X * App.cursor.scale.x), (LONG)((float)Y * App.cursor.scale.y) };
		pt.x += App.viewport.offset.x;
		pt.y += App.viewport.offset.y;

		return BOOL(ClientToScreen(App.hwnd, &pt) && SetCursorPos_Og(pt.x, pt.y));
	}
	return true;
}

BOOL WINAPI SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	UINT req_flags = SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER;
	if ((X == 0 && Y == 0 && cx == 0 && cy == 0) || (uFlags & req_flags) == req_flags)
		SetWindowPos_Og(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);

	return false;
}

LRESULT WINAPI SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (App.hwnd == hWnd && Msg == WM_MOUSEMOVE) {
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);

		x = glm::max((int)glm::round((x + App.viewport.offset.x * App.cursor.unscale.x) * App.cursor.scale.x), 0);
		y = glm::max((int)glm::round((y + App.viewport.offset.y * App.cursor.unscale.y) * App.cursor.scale.y), 0);
		x = glm::min(x, (int)App.window.size.x);
		y = glm::min(y, (int)App.window.size.y);
		lParam = MAKELPARAM(x, y);
	}
	return SendMessageA_Og(hWnd, Msg, wParam, lParam);
}

int WINAPI DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format)
{
	return 0;
}

COLORREF WINAPI GetPixel(HDC hdc, int x, int y)
{
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (App.ready && Menu::instance().isVisible())
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	switch (uMsg) {
		case WM_GETMINMAXINFO:
		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONUP:
		case WM_NCPAINT:
		case WM_CANCELMODE:
		case WM_DISPLAYCHANGE: {
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		case WM_MOVING: {
			auto rect = reinterpret_cast<RECT*>(lParam);

			App.window.position.x = rect->left;
			App.window.position.y = rect->top;
			App.window.centered = false;

			if (Menu::instance().isVisible()) {
				auto option = Menu::instance().getOption();
				option->window.position.x = rect->left;
				option->window.position.y = rect->top;
				option->window.centered = false;
				option->pos_changed = true;
			}
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		case WM_NCHITTEST: {
			LRESULT result = DefWindowProc(hWnd, uMsg, wParam, lParam);
			switch (result) {
				case HTBOTTOM:
				case HTBOTTOMLEFT:
				case HTBOTTOMRIGHT:
				case HTLEFT:
				case HTRIGHT:
				case HTTOP:
				case HTTOPLEFT:
				case HTTOPRIGHT:
					return HTBORDER;
			}
			return result;
		}

		case WM_SYSCOMMAND: {
			if (wParam == SC_MAXIMIZE)
				return 0;

			if (wParam == SC_CLOSE)
				exit(0);

			if (wParam == SC_KEYMENU)
				return 0;

			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		case WM_ACTIVATE:
		case WM_ACTIVATEAPP: {
			const bool fps_capped = !App.vsync && App.foreground_fps.active;

			if (wParam) {
				App.context->setFpsLimit(fps_capped, App.foreground_fps.range.value);
				CallWindowProcA(App.wndproc, hWnd, WM_SYSKEYUP, VK_MENU, 0);

				if (!Menu::instance().isVisible())
					setCursorLock();
			} else {
				App.context->setFpsLimit(App.background_fps.active || fps_capped, App.background_fps.active ? App.background_fps.range.value : App.foreground_fps.range.value);
				if (App.window.fullscreen && App.window.auto_minimize)
					PostMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);

				auto cursor_pos = d2::getCursorPos();
				cursor_pos.x = glm::max((int)glm::round((cursor_pos.x + App.viewport.offset.x * App.cursor.unscale.x) * App.cursor.scale.x), 0);
				cursor_pos.y = glm::max((int)glm::round((cursor_pos.y + App.viewport.offset.y * App.cursor.unscale.y) * App.cursor.scale.y), 0);
				cursor_pos.x = glm::min(cursor_pos.x, (int)App.window.size.x);
				cursor_pos.y = glm::min(cursor_pos.y, (int)App.window.size.y);
				LPARAM xy = MAKELPARAM(cursor_pos.x, cursor_pos.y);
				//SendMessage(hWnd, WM_LBUTTONUP, 0, xy);
				//SendMessage(hWnd, WM_RBUTTONUP, 0, xy);

				setCursorUnlock();
			}
			return 0;
		}

		case WM_SYSKEYDOWN: {
			BOOL context_code = (lParam & (1 << 29)) != 0;
			BOOL key_state = (lParam & (1 << 30)) != 0;

			if (wParam == VK_RETURN && context_code && !key_state) {
				if (!Menu::instance().isVisible()) {
					App.window.fullscreen = !App.window.fullscreen;
					if (!App.window.fullscreen)
						App.window.size = App.window.size_save;

					setWindowRect();
					windowResize();
				}
				return 0;
			}
			break;
		}
		case WM_SYSKEYUP: {
			if (wParam == VK_TAB || wParam == VK_RETURN)
				return DefWindowProc(hWnd, uMsg, wParam, lParam);

			break;
		}
		case WM_KEYDOWN: {
			if (wParam == VK_CONTROL || wParam == VK_TAB) {
				if (GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState(VK_TAB) & 0x8000) {
					setCursorUnlock();
					return 0;
				}
			} else if (wParam == VK_CONTROL || wParam == VK_MENU) {
				if ((GetAsyncKeyState(VK_RMENU) & 0x8000) && GetAsyncKeyState(VK_RCONTROL) & 0x8000) {
					setCursorUnlock();
					return 0;
				}
			} else {
				if (wParam == 0x4F && GetAsyncKeyState(VK_CONTROL) & 0x8000)
					return 0;

				if (Menu::instance().isVisible()) {
					if (wParam == VK_ESCAPE || (wParam >= 0x30 && wParam <= 0x39))
						return 0;
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (wParam == 0x4F && GetAsyncKeyState(VK_CONTROL) & 0x8000) {
				Menu::instance().toggle();
				if (Menu::instance().isVisible())
					setCursorUnlock();
				else
					setCursorLock();

				return 0;
			}
			if (Menu::instance().isVisible()) {
				if (wParam == VK_ESCAPE) {
					Menu::instance().toggle();
					setCursorLock();
					return 0;
				}
				if (wParam >= 0x30 && wParam <= 0x39)
					return 0;
			}
			break;
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP: {
			if (Menu::instance().isVisible())
				return DefWindowProc(hWnd, uMsg, wParam, lParam);

			if (!App.cursor.locked) {
				setCursorLock();
				if (!App.cursor.unlock)
					return 0;
			}

			[[fallthrough]];
		}
		case WM_XBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_MOUSEHOVER:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_MOUSEMOVE: {
			if (Menu::instance().isVisible())
				return DefWindowProc(hWnd, uMsg, wParam, lParam);

			if (!App.cursor.unlock && !App.cursor.locked)
				return 0;

			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);

			x = (int)((float)glm::max(x - App.viewport.offset.x, 0) * App.cursor.unscale.x);
			y = (int)((float)glm::max(y - App.viewport.offset.y, 0) * App.cursor.unscale.y);
			x = glm::min(x, (int)App.game.size.x);
			y = glm::min(y, (int)App.game.size.y);

			lParam = MAKELPARAM(x, y);

			modules::HDCursor::Instance().mouseProc(uMsg);
			break;
		}
		case WM_MOUSEWHEEL: {
			if (Menu::instance().isVisible())
				return DefWindowProc(hWnd, uMsg, wParam, lParam);

			if (!App.cursor.locked)
				return 0;

			int x = *d2::mouse_x;
			int y = *d2::mouse_y;
			if (App.game.screen == GameScreen::Menu) {
				x = *d2::mouse_x_menu;
				y = *d2::mouse_y_menu;
			}

			lParam = MAKELPARAM(x, y);
			break;
		}
	}

	return CallWindowProcA(App.wndproc, hWnd, uMsg, wParam, lParam);
}

void setWindow(HWND hwnd)
{
	App.hwnd = hwnd;
	App.hdc = GetDC(App.hwnd);
	win32::toggleDarkmode();

	App.window.style = GetWindowLong(App.hwnd, GWL_STYLE);
	App.window.style &= ~(WS_POPUP | WS_GROUP | WS_DLGFRAME | WS_CLIPSIBLINGS | WS_TABSTOP);
	App.window.style |= WS_OVERLAPPEDWINDOW & ~(WS_SIZEBOX | WS_MAXIMIZEBOX);
}

void setWindowRect()
{
	HMONITOR monitor = MonitorFromWindow(App.hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitor_info;
	monitor_info.cbSize = sizeof(MONITORINFO);

	GetMonitorInfo(monitor, &monitor_info);
	RECT wr = monitor_info.rcMonitor;
	LONG screen_w = wr.right - wr.left;
	LONG screen_h = wr.bottom - wr.top;

	if (!App.window.fullscreen) {
		if (App.window.size.x > (DWORD)screen_w)
			App.window.size.x = screen_w;

		if (App.window.size.y > (DWORD)screen_h)
			App.window.size.y = screen_h;

		int x = App.window.centered ? wr.left + (screen_w / 2) - (App.window.size.x / 2) : App.window.position.x;
		int y = App.window.centered ? wr.top + (screen_h / 2) - (App.window.size.y / 2) : App.window.position.y;
		x = (x > wr.left + screen_w - 50) ? wr.left + screen_w - 50 : x;
		y = (y > wr.top + screen_h - 50) ? wr.top + screen_h - 50 : y;
		wr = { x, y, (LONG)App.window.size.x + x, (LONG)App.window.size.y + y };

		SetWindowLong(App.hwnd, GWL_STYLE, App.window.style);

		RECT wr_test = { 0 };
		AdjustWindowRect(&wr_test, App.window.style, FALSE);

		wr.left -= wr_test.left;
		wr.right -= wr_test.left;
		wr.top -= wr_test.top;
		wr.bottom -= wr_test.top;

		AdjustWindowRect(&wr, App.window.style, FALSE);
		SetWindowPos_Og(App.hwnd, HWND_NOTOPMOST, wr.left, wr.top, (wr.right - wr.left), (wr.bottom - wr.top), SWP_SHOWWINDOW);
		trace_log("Switched to windowed mode: %d x %d", App.window.size.x, App.window.size.y);
	} else {
		SetWindowLong(App.hwnd, GWL_STYLE, App.window.style & ~WS_OVERLAPPEDWINDOW);
		App.window.size = { screen_w, screen_h };

		SetWindowPos_Og(App.hwnd, HWND_NOTOPMOST, wr.left, wr.top, (wr.right - wr.left), (wr.bottom - wr.top), 0);
		SetWindowPos_Og(App.hwnd, HWND_NOTOPMOST, wr.left, wr.top, (wr.right - wr.left), (wr.bottom - wr.top), SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
		trace_log("Switched to fullscreen mode: %d x %d", App.window.size.x, App.window.size.y);
	}
}

void setWindowMetrics()
{
	App.viewport.size.x = App.window.size.x;
	App.viewport.size.y = (uint32_t)(((float)App.game.size.y / App.game.size.x) * App.window.size.x);

	if (App.viewport.size.y > App.window.size.y) {
		App.viewport.size.x = (uint32_t)(((float)App.game.size.x / App.game.size.y) * App.window.size.y);
		App.viewport.size.y = App.window.size.y;
	}

	App.viewport.offset.x = App.window.size.x / 2 - App.viewport.size.x / 2;
	App.viewport.offset.y = App.window.size.y / 2 - App.viewport.size.y / 2;
	App.viewport.scale = { (float)App.viewport.size.x / App.game.size.x, (float)App.viewport.size.y / App.game.size.y };

	// clang-format off
	App.game.tex_size.x = App.game.size.x <= 1024 ? 1024 : App.game.size.x <= 2048 ? 2048 : App.game.size.x <= 4096 ? 4096 : App.game.size.x;
	App.game.tex_size.y = App.game.size.y <=  512 ?  512 : App.game.size.y <= 1024 ? 1024 : App.game.size.y <= 2048 ? 2048 : App.game.size.y <= 4096 ? 4096 : App.game.size.y;
	App.game.tex_scale = glm::vec2((float)App.game.size.x / App.game.tex_size.x, (float)App.game.size.y / App.game.tex_size.y);
	// clang-format off

	App.cursor.scale = { (float)App.viewport.size.x / App.game.size.x, (float)App.viewport.size.y / App.game.size.y };
	App.cursor.unscale = { (float)App.game.size.x / App.viewport.size.x, (float)App.game.size.y / App.viewport.size.y };
}

void setCursorLock()
{
	if (!App.cursor.locked) {
		if (!App.cursor.unlock) {
			RECT rc = { App.viewport.offset.x, App.viewport.offset.y, (int)App.viewport.size.x + App.viewport.offset.x, (int)App.viewport.size.y + App.viewport.offset.y };
			MapWindowPoints(App.hwnd, NULL, (LPPOINT)&rc, 2);
			ClipCursor(&rc);
		}

		while(ShowCursor_Og(false) >= 0);
		App.cursor.locked = true;
	}
}

void setCursorUnlock()
{
	if (App.cursor.locked) {
		if (!App.cursor.unlock)
			ClipCursor(NULL);
		
		while(ShowCursor_Og(true) <= 0);
		App.cursor.locked = false;
	}
}

void windowResize()
{
	bool cursor_lock = App.cursor.locked;
	setCursorUnlock();

	setWindowMetrics();
	App.window.resized = true;
	App.context->getCommandBuffer()->resize();

	if (cursor_lock)
		setCursorLock();
}

void toggleDarkmode()
{
	typedef BOOL(WINAPI* SetWindowCompositionAttribute_t)(HWND hWnd, WINDCOMPATTRDATA*);
	SetWindowCompositionAttribute_t SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)helpers::getProcOffset(DLL_USER32, "SetWindowCompositionAttribute");

	if (SetWindowCompositionAttribute) {
		BOOL dark = (BOOL)App.window.dark_mode;
		WINDCOMPATTRDATA data = { WINDCOMPATTR::WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
		SetWindowCompositionAttribute(App.hwnd, &data);
		trace_log("Window style switched to dark mode.");
	} else {
		if (App.window.dark_mode)
			trace_log("Dark mode is not available!");
	}
}

void setDPIAwareness()
{
	auto info = getOSVersion();
	trace_log("Windows version %d.%d (build: %d) detected.", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);

	if (info.dwMajorVersion >= HIBYTE(_WIN32_WINNT_WIN10)) { // Win10+
		typedef BOOL(WINAPI* SetProcessDpiAwareness_t)(DPI_AWARENESS_CONTEXT);
		SetProcessDpiAwareness_t SetProcessDpiAwareness = (SetProcessDpiAwareness_t)helpers::getProcOffset(DLL_USER32, "SetProcessDpiAwarenessContext");
		if (SetProcessDpiAwareness)
			SetProcessDpiAwareness(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	} else if (info.dwMajorVersion == HIBYTE(_WIN32_WINNT_WINBLUE) && info.dwMinorVersion == LOBYTE(_WIN32_WINNT_WINBLUE)) { // Win8.1
		typedef HRESULT(WINAPI* SetProcessDpiAwareness_t)(PROCESS_DPI_AWARENESS);
		SetProcessDpiAwareness_t SetProcessDpiAwareness = (SetProcessDpiAwareness_t)helpers::getProcOffset(DLL_SHCORE, "SetProcessDpiAwareness");
		if (SetProcessDpiAwareness)
			SetProcessDpiAwareness(PROCESS_DPI_AWARENESS::PROCESS_PER_MONITOR_DPI_AWARE);
	} else
		SetProcessDPIAware();
}

RTL_OSVERSIONINFOW getOSVersion()
{
	typedef LONG NTSTATUS, *PNTSTATUS;
	typedef NTSTATUS (WINAPI* RtlGetVersion_t)(PRTL_OSVERSIONINFOW);
	static RtlGetVersion_t RtlGetVersion = (RtlGetVersion_t)helpers::getProcOffset(DLL_NTDLL, "RtlGetVersion");

	RTL_OSVERSIONINFOW info = { 0 };
	info.dwOSVersionInfoSize = sizeof(info);
	if (RtlGetVersion)
		RtlGetVersion(&info);

	return info;
}

}