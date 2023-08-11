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
#include "frame_buffer.h"
#include "texture.h"

namespace d2gl {

GLuint current_binded_fbo = 0;

FrameBuffer::FrameBuffer(const FrameBufferCreateInfo& info)
	: m_width(info.size.x), m_height(info.size.y), m_attachment_count(info.attachments.size())
{
	glGenFramebuffers(1, &m_id);
	glBindFramebuffer(GL_FRAMEBUFFER, m_id);

	TextureCreateInfo texture_ci;
	texture_ci.size = info.size;

	for (size_t i = 0; i < m_attachment_count; i++) {
		texture_ci.slot = info.attachments[i].slot;
		texture_ci.format = info.attachments[i].format;
		texture_ci.filter = info.attachments[i].filter;
		m_textures.push_back(std::make_unique<Texture>(texture_ci));

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_textures[i]->getId(), 0);
		m_clear_colors.push_back(info.attachments[i].clear_color);
	}
	setDrawBuffers(m_attachment_count);

	auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		error_log("(%d) Framebuffer is not complete!", status);
		for (size_t i = 0; i < m_attachment_count; i++)
			trace_log("Attachment #%d size: %d x %d (%d)", i, m_textures[i]->getWidth(), m_textures[i]->getHeight(), m_textures[i]->getSlot());
		m_complete = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	current_binded_fbo = 0;
}

FrameBuffer::~FrameBuffer()
{
	for (auto& texture : m_textures)
		texture.reset();
	glDeleteFramebuffers(1, &m_id);
}

void FrameBuffer::bind(bool clear)
{
	if (current_binded_fbo == m_id) {
		if (clear)
			clearBuffer();
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_id);
	current_binded_fbo = m_id;

	if (clear)
		clearBuffer();
}

void FrameBuffer::unBind()
{
	if (current_binded_fbo == 0)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	current_binded_fbo = 0;
}

void FrameBuffer::clearBuffer()
{
	const auto count = m_clear_colors.size();
	if (count == 1) {
		const auto color = m_clear_colors[0];
		glClearColor(color[0], color[1], color[2], color[3]);
		glClear(GL_COLOR_BUFFER_BIT);
	} else {
		for (size_t i = 0; i < count; i++)
			glClearBufferfv(GL_COLOR, i, m_clear_colors[i].data());
	}
}

void FrameBuffer::setDrawBuffers(uint32_t count)
{
	GLenum* attachments = new GLenum[count];
	for (size_t i = 0; i < count; i++)
		attachments[i] = GL_COLOR_ATTACHMENT0 + i;

	glDrawBuffers(count, attachments);
	delete[] attachments;
}

}