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
#include "pipeline.h"
#include "frame_buffer.h"
#include "texture.h"
#include "uniform_buffer.h"

namespace d2gl {

Pipeline::Pipeline(const PipelineCreateInfo& info)
	: m_name(info.name), m_attachment_blends(info.attachment_blends), m_bindings(info.bindings), m_compute(info.compute)
{
	m_id = glCreateProgram();

	GLuint vs = 0, fs = 0, cs = 0;
	if (m_compute) {
		cs = createShader(info.shader, GL_COMPUTE_SHADER, info.version, m_name);
		glAttachShader(m_id, cs);
		if (cs == 0)
			m_compile_success = false;
	} else {
		vs = createShader(info.shader, GL_VERTEX_SHADER, info.version, m_name);
		glAttachShader(m_id, vs);
		if (vs == 0)
			m_compile_success = false;

		fs = createShader(info.shader, GL_FRAGMENT_SHADER, info.version, m_name);
		glAttachShader(m_id, fs);
		if (fs == 0)
			m_compile_success = false;
	}

	glLinkProgram(m_id);
	glValidateProgram(m_id);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glDeleteShader(cs);

	glUseProgram(m_id);

	if (m_compile_success && m_bindings.size() > 0) {
		for (auto& binding : m_bindings) {
			switch (binding.type) {
				case BindingType::UniformBuffer: {
					GLuint ubo_index = glGetUniformBlockIndex(m_id, binding.name.c_str());
					glUniformBlockBinding(m_id, ubo_index, binding.value);
					break;
				}
				case BindingType::Texture:
				case BindingType::Image:
				case BindingType::FBTexture: {
					setUniform1i(binding.name, binding.value);
					break;
				}
			};
		}
	}
}

Pipeline::~Pipeline()
{
	glDeleteProgram(m_id);
}

void Pipeline::bind(uint32_t index)
{
	static GLuint current_program = 0;
	static uint32_t current_blend_index = 10;

	if (current_program == m_id && current_blend_index == index)
		return;

	if (current_program != m_id) {
		glUseProgram(m_id);
		current_program = m_id;
		current_blend_index = 10;
	}

	if (current_blend_index != index) {
		setBlendState(index);
		current_blend_index = index;
	}

	if (m_bindings.size() > 0) {
		for (auto& binding : m_bindings) {
			if (binding.ptr) {
				if (binding.type == BindingType::Texture) {
					if (const auto& ptr = *binding.tex_ptr)
						ptr->bind();
				} else if (binding.type == BindingType::FBTexture) {
					if (const auto& ptr = *binding.fbo_ptr)
						ptr->getTexture(binding.index)->bind();
				}
			}
		}
	}
}

void Pipeline::setBlendState(uint32_t index)
{
	static bool blend_enabled = true;

	const auto& blends = m_attachment_blends[index];
	if (blends[0] == BlendType::NoBlend) {
		if (blend_enabled) {
			glDisable(GL_BLEND);
			blend_enabled = false;
		}
		return;
	}

	if (!blend_enabled) {
		glEnable(GL_BLEND);
		blend_enabled = true;
	}
	if (App.gl_caps.independent_blending) {
		for (size_t i = 0; i < blends.size(); i++) {
			const auto factor = blendFactor(blends[i]);
			glBlendFuncSeparatei(i, factor.src_color, factor.dst_color, factor.src_alpha, factor.dst_alpha);
		}
	} else {
		const auto factor = blendFactor(blends[0]);
		glBlendFuncSeparate(factor.src_color, factor.dst_color, factor.src_alpha, factor.dst_alpha);
	}
}

BlendFactors Pipeline::blendFactor(BlendType type)
{
	switch (type) {
		case BlendType::One_Zero: return { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
		case BlendType::Zero_SColor: return { GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_SRC_COLOR };
		case BlendType::One_One: return { GL_ONE, GL_ONE, GL_ZERO, GL_ONE };
		case BlendType::SAlpha_OneMinusSAlpha: return { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
	}
	return { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
}

GLint Pipeline::getUniformLocation(const std::string& name)
{
	bind();
	if (m_uniform_cache.find(name) != m_uniform_cache.end())
		return m_uniform_cache[name];

	int location = glGetUniformLocation(m_id, name.c_str());
	if (location == -1) {
		error("Uniform (%s) not found!", name.c_str());
		return -1;
	}

	m_uniform_cache.insert({ name, location });
	return location;
}

void Pipeline::setUniform1i(const std::string& name, int value)
{
	glUniform1i(getUniformLocation(name), value);
}

void Pipeline::setUniform1u(const std::string& name, uint32_t value)
{
	glUniform1ui(getUniformLocation(name), value);
}

void Pipeline::setUniform1f(const std::string& name, float value)
{
	glUniform1f(getUniformLocation(name), value);
}

void Pipeline::setUniformVec2f(const std::string& name, const glm::vec2& value)
{
	glUniform2fv(getUniformLocation(name), 1, &value.x);
}

void Pipeline::setUniformVec4f(const std::string& name, const glm::vec4& value)
{
	glUniform4fv(getUniformLocation(name), 1, &value.x);
}

void Pipeline::setUniformMat4f(const std::string& name, const glm::mat4& matrix)
{
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &matrix[0][0]);
}

void Pipeline::dispatchCompute(int flag, glm::ivec2 work_size, GLbitfield barrier)
{
	setUniform1u("u_Flag", flag);
	glDispatchCompute(work_size.x, work_size.y, 1);

	if (barrier)
		glMemoryBarrier(barrier);
}

GLuint Pipeline::createShader(const char* source, int type, glm::vec<2, uint8_t> version, const std::string& name)
{
	std::string shader_src = "#version " + std::to_string(version.x) + std::to_string(version.y) + "0\n";
	switch (type) {
		case GL_VERTEX_SHADER: shader_src += "#define VERTEX 1\n"; break;
		case GL_FRAGMENT_SHADER: shader_src += "#define FRAGMENT 1\n"; break;
		case GL_COMPUTE_SHADER: shader_src += "#define COMPUTE 1\n"; break;
	}
	shader_src += source;

	GLuint id = glCreateShader(type);
	GLint length = shader_src.length();
	const char* src = shader_src.c_str();

	glShaderSource(id, 1, &src, &length);
	glCompileShader(id);

	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int lenght;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &lenght);
		char* message = (char*)_malloca(lenght * sizeof(char));
		glGetShaderInfoLog(id, lenght, &lenght, message);
		const char* stype = (type == GL_VERTEX_SHADER ? "VERTEX" : (type == GL_FRAGMENT_SHADER ? "FRAGMENT" : "COMPUTE"));
		error_log("Shader compile failed! %s (%s) | %s", name.c_str(), stype, message);
		trace_log("%s", src);

		glDeleteShader(id);
		return 0;
	}

	return id;
}

const char* g_shader_glide = {
#include "shaders/glide.glsl.h"
};

const char* g_shader_ddraw = {
#include "shaders/ddraw.glsl.h"
};

const char* g_shader_movie = {
#include "shaders/movie.glsl.h"
};

const char* g_shader_prefx = {
#include "shaders/prefx.glsl.h"
};

const char* g_shader_postfx = {
#include "shaders/postfx.glsl.h"
};

const char* g_shader_mod = {
#include "shaders/mod.glsl.h"
};

const std::map<uint32_t, std::pair<uint32_t, BlendType>> g_blend_types = {
	{ 0, { 0, BlendType::One_Zero } },
	{ 2, { 1, BlendType::Zero_SColor } },
	{ 4, { 2, BlendType::One_One } },
	{ 5, { 3, BlendType::SAlpha_OneMinusSAlpha } },
};

}