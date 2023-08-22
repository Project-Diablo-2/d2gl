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
#include "wrapper.h"
#include "d2/common.h"
#include "helpers.h"
#include "modules/motion_prediction.h"
#include "win32.h"

namespace d2gl {

std::unique_ptr<Wrapper> GlideWrapper;

Wrapper::Wrapper()
	: ctx(App.context.get())
{
	g_glide_texture.memory = new uint8_t[GLIDE_TEX_MEMORY * GLIDE_MAX_NUM_TMU];

	SubTextureCounts sub_texture_counts = { { 256, 256 }, { 128, 154 }, { 64, 64 }, { 32, 32 }, { 16, 5 }, { 8, 1 } };
	m_texture_manager = std::make_unique<TextureManager>(sub_texture_counts);
}

Wrapper::~Wrapper()
{
	delete[] g_glide_texture.memory;
}

void Wrapper::onBufferClear()
{
	if (!m_swapped)
		return;
	m_swapped = false;

	ctx->beginFrame();
}

void Wrapper::onBufferSwap()
{
	if (m_swapped)
		return;
	m_swapped = true;

#ifdef _DEBUG
	App.var[0] = m_texture_manager->getUsage(256);
	App.var[1] = m_texture_manager->getUsage(128);
	App.var[2] = m_texture_manager->getUsage(64);
	App.var[3] = m_texture_manager->getUsage(32);
	App.var[4] = m_texture_manager->getUsage(16);
	App.var[5] = m_texture_manager->getUsage(8);
#endif

	ctx->presentFrame();
}

void Wrapper::grDrawPoint(const void* pt)
{
	GlideVertex* vertex = (GlideVertex*)pt;

	ctx->pushVertex(vertex);
	vertex->x += 1.0f;
	ctx->pushVertex(vertex);
	vertex->y += 1.0f;
	ctx->pushVertex(vertex);
	vertex->x -= 1.0f;
	ctx->pushVertex(vertex);
}

void Wrapper::grDrawLine(const void* v1, const void* v2)
{
	GlideVertex* vertex1 = (GlideVertex*)v1;
	GlideVertex* vertex2 = (GlideVertex*)v2;

	glm::vec2 widening_vec = { vertex1->y - vertex2->y, vertex2->x - vertex1->x };
	const float len = (2.0f * glm::length(widening_vec));
	const float half_inv_len = 1.0f / len;
	widening_vec *= half_inv_len;

	vertex1->x -= widening_vec.x;
	vertex1->y -= widening_vec.y;
	ctx->pushVertex(vertex1);
	vertex1->x += widening_vec.x * 2.0f;
	vertex1->y += widening_vec.y * 2.0f;
	ctx->pushVertex(vertex1);

	vertex2->x += widening_vec.x;
	vertex2->y += widening_vec.y;
	ctx->pushVertex(vertex2);
	vertex2->x -= widening_vec.x * 2.0f;
	vertex2->y -= widening_vec.y * 2.0f;
	ctx->pushVertex(vertex2);
}

void Wrapper::grDrawVertexArray(FxU32 mode, FxU32 count, void** pointers)
{
	if (mode == GR_TRIANGLE_STRIP) {
		const auto offset = modules::MotionPrediction::Instance().getGlobalOffset();
		for (FxU32 i = 0; i < count - 2; i += 2) {
			ctx->pushVertex((const GlideVertex*)pointers[i + 0], { -0.0001f, -0.0001f }, offset);
			ctx->pushVertex((const GlideVertex*)pointers[i + 1], { +0.0001f, -0.0001f }, offset);
			ctx->pushVertex((const GlideVertex*)pointers[i + 3], { +0.0001f, +0.0001f }, offset);
			ctx->pushVertex((const GlideVertex*)pointers[i + 2], { -0.0001f, +0.0001f }, offset);
		}
	} else {
		for (FxU32 i = 0; i < count; i++)
			ctx->pushVertex((const GlideVertex*)pointers[i]);
	}
}

void Wrapper::grDrawVertexArrayContiguous(FxU32 mode, FxU32 count, void* pointers)
{
	const auto offset = modules::MotionPrediction::Instance().getGlobalOffsetPerspective();
	for (FxU32 i = 0; i < count; i++)
		ctx->pushVertex(&((const GlideVertex*)pointers)[i], { 0.0f, 0.0f }, offset);
}

void Wrapper::grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_df)
{
	ctx->setBlendState(static_cast<uint32_t>(rgb_df));
}

void Wrapper::grAlphaCombine(GrCombineFunction_t function)
{
	ctx->setVertexFlagZ(function == GR_COMBINE_FUNCTION_LOCAL);
}

void Wrapper::grChromakeyMode(GrChromakeyMode_t mode)
{
	ctx->setVertexFlagX(mode == GR_CHROMAKEY_ENABLE);
}

void Wrapper::grColorCombine(GrCombineFunction_t function)
{
	ctx->setVertexFlagY(function == GR_COMBINE_FUNCTION_LOCAL);
}

void Wrapper::grConstantColorValue(GrColor_t value)
{
	ctx->setVertexColor(value);
}

void Wrapper::grLoadGammaTable(FxU32 nentries, FxU32* red, FxU32* green, FxU32* blue)
{
	glm::vec4 gamma[256];
	for (int32_t i = 0; i < glm::min((int)nentries, 256); i++) {
		gamma[i].r = (float)red[i] / 255;
		gamma[i].g = (float)green[i] / 255;
		gamma[i].b = (float)blue[i] / 255;
	}

	const uint32_t hash = helpers::hash(&gamma[0], sizeof(glm::vec4) * 256);
	if (m_gamma_hash == hash)
		return;

	ctx->flushVertices();
	ctx->getCommandBuffer()->colorUpdate(UBOType::Gamma, &gamma[0]);
	m_gamma_hash = hash;
}

void Wrapper::guGammaCorrectionRGB(FxFloat red, FxFloat green, FxFloat blue)
{
	glm::vec4 gamma[256];
	for (int32_t i = 0; i < 256; i++) {
		float v = i / 255.0f;
		gamma[i].r = powf(v, 1.0f / red);
		gamma[i].g = powf(v, 1.0f / green);
		gamma[i].b = powf(v, 1.0f / blue);
	}

	const uint32_t hash = helpers::hash(&gamma[0], sizeof(glm::vec4) * 256);
	if (m_gamma_hash == hash)
		return;

	ctx->flushVertices();
	ctx->getCommandBuffer()->colorUpdate(UBOType::Gamma, &gamma[0]);
	m_gamma_hash = hash;
}

void Wrapper::grTexSource(GrChipID_t tmu, FxU32 start_address, GrTexInfo* info)
{
	uint32_t width, height;
	uint32_t size = Wrapper::getTexSize(info, width, height);
	start_address += GLIDE_TEX_MEMORY * tmu;

	const auto frame_index = ctx->getFrameIndex();
	const auto frame_count = ctx->getFrameCount();
	const auto sub_tex_info = m_texture_manager->getSubTextureInfo(start_address, size, width, height, frame_count);
	if (sub_tex_info) {
		ctx->setVertexTexNum({ sub_tex_info->tex_num, 0 });
		ctx->setVertexOffset(sub_tex_info->offset);
		ctx->setVertexTexShift(sub_tex_info->shift);
	}
}

void Wrapper::grTexDownloadMipMap(GrChipID_t tmu, FxU32 start_address, GrTexInfo* info)
{
	uint32_t width, height;
	uint32_t size = Wrapper::getTexSize(info, width, height);
	start_address += GLIDE_TEX_MEMORY * tmu;

	memcpy(g_glide_texture.memory + start_address, info->data, width * height);
	g_glide_texture.hash[start_address] = helpers::hash(info->data, width * height);
}

void Wrapper::grTexDownloadTable(void* data)
{
	static uint32_t old_hash = 0;
	const uint32_t hash = helpers::hash(data, sizeof(uint32_t) * 256);
	if (old_hash == hash)
		return;

	ctx->flushVertices();

	glm::vec4 palette[256];
	uint32_t* pal = (uint32_t*)data;

	for (int32_t i = 0; i < 256; i++) {
		palette[i].a = (float)((pal[i] >> 24) & 0xFF) / 255;
		palette[i].r = (float)((pal[i] >> 16) & 0xFF) / 255;
		palette[i].g = (float)((pal[i] >> 8) & 0xFF) / 255;
		palette[i].b = (float)((pal[i]) & 0xFF) / 255;
	}
	ctx->getCommandBuffer()->colorUpdate(UBOType::Palette, &palette[0]);
	old_hash = hash;
}

FxBool Wrapper::grLfbLock(GrLfbWriteMode_t write_mode, GrOriginLocation_t origin, GrLfbInfo_t* info)
{
	if (write_mode == GR_LFBWRITEMODE_8888 && origin == GR_ORIGIN_UPPER_LEFT && info) {
		if (!m_movie_buffer.lfbPtr) {
			m_movie_buffer.lfbPtr = malloc(640 * 480 * 4);
			m_movie_buffer.strideInBytes = 640 * 4;
			m_movie_buffer.writeMode = write_mode;
			m_movie_buffer.origin = origin;
			if (m_movie_buffer.lfbPtr)
				memset(m_movie_buffer.lfbPtr, 0, 640 * 480 * 4);
		}
		*info = m_movie_buffer;

		return FXTRUE;
	}

	return FXFALSE;
}

FxBool Wrapper::grLfbUnlock()
{
	App.game.screen = GameScreen::Movie;
	ctx->getCommandBuffer()->gameTextureUpdate((uint8_t*)m_movie_buffer.lfbPtr, { 640, 480 }, 4);

	onBufferClear();
	onBufferSwap();

	return FXTRUE;
}

GrContext_t Wrapper::grSstWinOpen(FxU32 hwnd, GrScreenResolution_t screen_resolution)
{
	if (App.video_test)
		return 1;

	App.game.screen = (GameScreen)(*d2::is_in_game);
	if (App.game.screen == GameScreen::InGame) {
		App.game.screen = GameScreen::Loading;

		GlideWrapper->m_texture_manager->clearCache();
	}

	glm::uvec2 old_size = App.game.size;
	if (screen_resolution == GR_RESOLUTION_640x480)
		App.game.size = { 640, 480 };
	else if (screen_resolution == GR_RESOLUTION_800x600)
		App.game.size = { 800, 600 };
	else if (screen_resolution == GR_RESOLUTION_1024x768)
		App.game.size = { 1024, 768 };
	else {
		if (App.game.custom_size.x != 0) {
			App.game.size = App.game.custom_size;
			trace_log("Applying custom size.");
		} else
			App.game.size = { *d2::screen_width, *d2::screen_height };
	}
	trace_log("Game requested screen size: %d x %d", App.game.size.x, App.game.size.y);

	if (App.hwnd) {
		if (old_size != App.game.size) {
			win32::setWindowMetrics();
			win32::windowResize();
		}
		return 1;
	}

	win32::setWindow((HWND)hwnd);
	win32::setWindowRect();
	win32::setWindowMetrics();

	App.context = std::make_unique<Context>();
	GlideWrapper = std::make_unique<Wrapper>();
	App.ready = true;

	helpers::loadDlls(App.dlls_late, true);

	return 1;
}

FxU32 Wrapper::grGet(FxU32 pname, FxI32& params)
{
	switch (pname) {
	case GR_MAX_TEXTURE_SIZE: params = 256; break;
	case GR_MAX_TEXTURE_ASPECT_RATIO: params = 3; break;
	case GR_TEXTURE_ALIGN: params = 256; break;
	case GR_NUM_BOARDS: params = 1; break;
	case GR_NUM_FB: params = 1; break;
	case GR_NUM_TMU: params = GLIDE_MAX_NUM_TMU; break;
	case GR_MEMORY_UMA: params = 0; break;
	case GR_GAMMA_TABLE_ENTRIES: params = 256; break;
	case GR_BITS_GAMMA: params = 8; break;
	}
	return 4;
}

const char* Wrapper::grGetString(FxU32 pname)
{
	switch (pname) {
	case GR_EXTENSION: return " ";
	case GR_HARDWARE: return "Spectre 3000";
	case GR_RENDERER: return "D2GL";
	case GR_VENDOR: return "3Dfx Interactive";
	case GR_VERSION: return "3.1";
	}
	return "";
}

uint32_t Wrapper::getTexSize(GrTexInfo* info, uint32_t& width, uint32_t& height)
{
	if (info->aspectRatioLog2 < 0) {
		height = 1 << info->largeLodLog2;
		width = height >> -info->aspectRatioLog2;
		return height;
	} else {
		width = 1 << info->largeLodLog2;
		height = width >> info->aspectRatioLog2;
		return width;
	}
}

}