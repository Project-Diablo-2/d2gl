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

namespace d2gl {

class Texture;

struct FrameBufferAttachment {
	uint32_t slot = 0;
	std::array<float, 4> clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
	std::pair<GLint, GLint> filter = { GL_NEAREST, GL_NEAREST };
	std::pair<GLint, GLenum> format = { GL_RGBA8, GL_RGBA };
};

struct FrameBufferCreateInfo {
	glm::uvec2 size = { 0, 0 };
	std::vector<FrameBufferAttachment> attachments;
};

class FrameBuffer {
	GLuint m_id = 0;
	uint32_t m_width, m_height, m_attachment_count;
	std::vector<std::unique_ptr<Texture>> m_textures;
	std::vector<std::array<float, 4>> m_clear_colors;
	bool m_complete = true;

public:
	FrameBuffer(const FrameBufferCreateInfo& info);
	~FrameBuffer();

	void bind(bool clear = true);
	static void unBind();
	static void setDrawBuffers(uint32_t count);

	inline const GLuint getId() const { return m_id; }
	inline Texture* getTexture(uint32_t index = 0) { return m_textures[index].get(); }
	inline const uint32_t getWidth() const { return m_width; }
	inline const uint32_t getHeight() const { return m_height; }
	inline const uint32_t getAttachmentCount() const { return m_attachment_count; }
	inline bool isComplete() { return m_complete; }

private:
	void clearBuffer();
};

}