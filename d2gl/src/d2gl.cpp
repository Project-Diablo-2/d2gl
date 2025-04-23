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
#include "d2gl.h"
#include "d2/common.h"
#include "modules/motion_prediction.h"
#include "option/menu.h"
#include "win32.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) BOOL __stdcall d2glConfigQueryImpl(D2GLConfigId config_id)
{
	switch (config_id) {
		case D2GL_CONFIG_VSYNC: return d2gl::App.vsync;
		case D2GL_CONFIG_CURSOR_UNLOCKED: return d2gl::App.cursor.unlock;
		case D2GL_CONFIG_HD_CURSOR: return d2gl::App.hd_cursor;
		case D2GL_CONFIG_HD_TEXT: return d2gl::App.hd_text.active;
		case D2GL_CONFIG_MOTION_PREDICTION: return d2gl::App.motion_prediction;
		case D2GL_CONFIG_MINI_MAP: return d2gl::App.mini_map.active;
		case D2GL_CONFIG_SHOW_ITEM_QUANTITY: return d2gl::App.show_item_quantity;
		case D2GL_CONFIG_SHOW_MONSTER_RES: return FALSE;
		case D2GL_CONFIG_SHOW_FPS: return d2gl::App.show_fps;
		case D2GL_CONFIG_DRAW_CURSOR: return d2gl::App.cursor.draw;
	}
	return FALSE;
}

__declspec(dllexport) BOOL __stdcall d2glConfigSetImpl(D2GLConfigId config_id, bool configVal)
{
	BOOL bSave = FALSE;
	switch (config_id) {
		case D2GL_CONFIG_HD_CURSOR:
			d2gl::App.hd_cursor = configVal;
			bSave = TRUE;
			break;
		case D2GL_CONFIG_HD_TEXT:
			d2gl::App.hd_text.active = configVal;
			bSave = TRUE;
			break;
		case D2GL_CONFIG_MOTION_PREDICTION:
			d2gl::App.motion_prediction = configVal;
			d2gl::modules::MotionPrediction::Instance().toggle(d2gl::App.motion_prediction);
			bSave = TRUE;
			break;
		case D2GL_CONFIG_MINI_MAP:
			d2gl::App.mini_map.active = configVal;
			d2gl::d2::patch_minimap->toggle(d2gl::App.mini_map.active);
			bSave = TRUE;
			break;
		case D2GL_CONFIG_SHOW_ITEM_QUANTITY:
			d2gl::App.show_item_quantity = configVal;
			bSave = TRUE;
			break;
		case D2GL_CONFIG_DRAW_CURSOR:
			d2gl::App.cursor.draw = configVal;
			break;
	}
	if (bSave) {
		d2gl::App.config.SaveConfig();
		return TRUE;
	}
	return FALSE;
}

__declspec(dllexport) void __stdcall setCustomScreenSize(uint32_t width, uint32_t height)
{
	d2gl::App.game.custom_size = { width, height };
}

__declspec(dllexport) int __cdecl d2glIsReady()
{
	return d2gl::App.ready;
}

__declspec(dllexport) void __cdecl d2glToggleInterface()
{
	d2gl::Menu::instance().toggle();
	if (d2gl::Menu::instance().isVisible())
		d2gl::win32::setCursorUnlock();
	else
		d2gl::win32::setCursorLock();
}

__declspec(dllexport) void __cdecl d2glCopyTooltipScreenShot()
{
	if (d2gl::App.tooltip.active)
		d2gl::helpers::copyTooltipScreenShot();
}

#ifdef __cplusplus
}
#endif