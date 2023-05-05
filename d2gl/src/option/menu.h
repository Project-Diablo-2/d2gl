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

#pragma once

#include <imgui/imgui.h>

namespace d2gl {

enum class Color {
	Default,
	Orange,
	White,
	Gray,
};

class Menu {
	bool m_visible = false;
	bool m_visible_t = false;
	std::unordered_map<int, ImFont*> m_fonts;
	std::unordered_map<Color, ImVec4> m_colors;
	ImGuiCond window_pos_cond;

	Menu();
	~Menu() = default;

	void updateSelectedQualityPreset();
	void applyQualityPreset();
	void applyWindowChanges();
	bool tabBegin(const char* title, int tab_num, int* active_tab);
	void tabEnd();

	void childBegin(const char* id, bool half_width = false, bool with_nav = false);
	void childSeparator(const char* id, bool with_nav = false);
	void childEnd();
	bool drawNav(const char* btn_label);

	template <typename T>
	bool drawCombo(const char* title, Select<T>* select, const char* desc, const char* btn_label, int* opt);
	bool drawCheckbox(const char* title, bool* option, const char* desc, bool* opt);
	template <typename T>
	bool drawSlider(const std::string& id, const char* title, Range<T>* range, const char* format, const char* desc, T* opt);
	void drawInput2(const std::string& id, const char* desc, glm::ivec2* input, glm::ivec2 min = { 0, 0 }, glm::ivec2 max = { 10000, 10000 });

	void drawSeparator(float y_padd = 5.0f, float alpha = 1.0f);
	void drawLabel(const char* title, const ImVec4& color, int size = 17);
	void drawDescription(const char* desc, const ImVec4& color, int size = 14);

public:
	static Menu& instance()
	{
		static Menu instance;
		return instance;
	}

	void toggle(bool force = false);
	void draw();

	inline bool isVisible() { return m_visible; }
};

}