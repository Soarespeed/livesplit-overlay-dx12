#define ImTextureID ImU64
#include <imgui.h>
#include <reshade.hpp>
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using namespace reshade::api;

namespace
{
	constexpr const char *k_section = "LIVESPLIT";

	bool g_show = true;
	float g_align_x = 0.17f;
	float g_align_y = 0.0f;
	int g_offset_x = 0;
	int g_offset_y = 0;
	float g_scale = 1.0f;
	float g_opacity = 1.0f;
	int g_capture_hz = 30;
	int g_upload_hz = 15;
	bool g_chroma = true;
	float g_key[3] = { 0.0f, 0.0f, 0.0f };
	int g_key_tolerance = 12;

	device *g_device = nullptr;
	resource g_texture = {};
	resource_view g_view = {};
	uint32_t g_tex_width = 0;
	uint32_t g_tex_height = 0;

	std::mutex g_frame_mutex;
	std::vector<uint8_t> g_frame;
	uint32_t g_frame_width = 0;
	uint32_t g_frame_height = 0;

	struct dirty_rect
	{
		uint32_t left = 0, top = 0, right = 0, bottom = 0;
		bool valid = false;
	};
	dirty_rect g_dirty;

	std::atomic<bool> g_running { false };
	std::atomic<int> g_capture_hz_atomic { 30 };
	std::atomic<bool> g_chroma_atomic { true };
	std::atomic<int> g_key_r { 0 }, g_key_g { 0 }, g_key_b { 0 }, g_key_tol_atomic { 12 };
	std::thread g_capture_thread;

	BOOL CALLBACK find_livesplit(HWND hwnd, LPARAM lparam)
	{
		wchar_t title[256];
		if (GetWindowTextW(hwnd, title, 256) > 0 && wcsstr(title, L"LiveSplit") != nullptr && IsWindowVisible(hwnd))
		{
			*reinterpret_cast<HWND *>(lparam) = hwnd;
			return FALSE;
		}
		return TRUE;
	}

	struct capture_surface
	{
		HWND window = nullptr;
		HDC dc = nullptr;
		HBITMAP bitmap = nullptr;
		HGDIOBJ previous = nullptr;
		uint8_t *bits = nullptr;
		int width = 0;
		int height = 0;

		void release()
		{
			if (dc != nullptr)
			{
				SelectObject(dc, previous);
				DeleteDC(dc);
			}
			if (bitmap != nullptr)
				DeleteObject(bitmap);
			dc = nullptr;
			bitmap = nullptr;
			bits = nullptr;
			width = height = 0;
		}

		bool resize(int w, int h)
		{
			if (dc != nullptr && w == width && h == height)
				return true;
			release();

			HDC screen = GetDC(window);
			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = -h;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;
			bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, reinterpret_cast<void **>(&bits), nullptr, 0);
			dc = bitmap != nullptr ? CreateCompatibleDC(screen) : nullptr;
			ReleaseDC(window, screen);

			if (dc == nullptr)
			{
				release();
				return false;
			}
			previous = SelectObject(dc, bitmap);
			width = w;
			height = h;
			return true;
		}
	};

	void apply_chroma_key(std::vector<uint8_t> &pixels)
	{
		const bool chroma = g_chroma_atomic.load();
		const int key[3] = { g_key_b.load(), g_key_g.load(), g_key_r.load() };
		const float tol = static_cast<float>(g_key_tol_atomic.load());
		for (size_t i = 0; i + 3 < pixels.size(); i += 4)
		{
			if (!chroma)
			{
				pixels[i + 3] = 255;
				continue;
			}

			float alpha = 0.0f;
			for (int c = 0; c < 3; ++c)
			{
				const int v = pixels[i + c];
				const int span = v >= key[c] ? 255 - key[c] : key[c];
				if (span > 0)
					alpha = (std::max)(alpha, static_cast<float>(std::abs(v - key[c])) / static_cast<float>(span));
			}
			alpha = (std::max)(0.0f, (alpha * 255.0f - tol) / (255.0f - tol));
			if (alpha <= 0.0f)
			{
				pixels[i + 3] = 0;
				continue;
			}

			for (int c = 0; c < 3; ++c)
			{
				const float v = pixels[i + c];
				pixels[i + c] = static_cast<uint8_t>(std::clamp(key[c] + (v - key[c]) / alpha, 0.0f, 255.0f) + 0.5f);
			}
			pixels[i + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
		}
	}

	bool capture_livesplit(capture_surface &surface, std::vector<uint8_t> &pixels, uint32_t &out_w, uint32_t &out_h)
	{
		if (surface.window == nullptr || !IsWindow(surface.window))
		{
			surface.release();
			surface.window = nullptr;
			EnumWindows(find_livesplit, reinterpret_cast<LPARAM>(&surface.window));
			if (surface.window == nullptr)
				return false;
		}
		if (IsIconic(surface.window))
			return false;

		RECT rc;
		GetClientRect(surface.window, &rc);
		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;
		if (w <= 0 || h <= 0 || !surface.resize(w, h))
			return false;
		if (PrintWindow(surface.window, surface.dc, PW_RENDERFULLCONTENT) == 0)
			return false;

		const size_t size = static_cast<size_t>(w) * h * 4;
		pixels.assign(surface.bits, surface.bits + size);
		apply_chroma_key(pixels);

		out_w = static_cast<uint32_t>(w);
		out_h = static_cast<uint32_t>(h);
		return true;
	}

	dirty_rect compute_dirty(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b, uint32_t w, uint32_t h)
	{
		dirty_rect r;
		r.left = w;
		r.top = h;
		const size_t row = static_cast<size_t>(w) * 4;
		for (uint32_t y = 0; y < h; ++y)
		{
			const size_t off = y * row;
			if (std::memcmp(&a[off], &b[off], row) == 0)
				continue;
			uint32_t x0 = 0, x1 = w;
			while (x0 < w && std::memcmp(&a[off + x0 * 4], &b[off + x0 * 4], 4) == 0)
				++x0;
			while (x1 > x0 && std::memcmp(&a[off + (x1 - 1) * 4], &b[off + (x1 - 1) * 4], 4) == 0)
				--x1;
			r.left = (std::min)(r.left, x0);
			r.right = (std::max)(r.right, x1);
			r.top = (std::min)(r.top, y);
			r.bottom = (std::max)(r.bottom, y + 1);
			r.valid = true;
		}
		return r;
	}

	void merge_dirty(dirty_rect &into, const dirty_rect &add)
	{
		if (!add.valid)
			return;
		if (!into.valid)
		{
			into = add;
			return;
		}
		into.left = (std::min)(into.left, add.left);
		into.top = (std::min)(into.top, add.top);
		into.right = (std::max)(into.right, add.right);
		into.bottom = (std::max)(into.bottom, add.bottom);
	}

	void capture_loop()
	{
		capture_surface surface;
		std::vector<uint8_t> pixels;
		while (g_running.load())
		{
			uint32_t w = 0, h = 0;
			if (capture_livesplit(surface, pixels, w, h))
			{
				const bool same_size = g_frame_width == w && g_frame_height == h && g_frame.size() == pixels.size();
				const dirty_rect changed = same_size ? compute_dirty(g_frame, pixels, w, h) : dirty_rect{ 0, 0, w, h, true };
				if (changed.valid)
				{
					std::lock_guard<std::mutex> lock(g_frame_mutex);
					g_frame.swap(pixels);
					g_frame_width = w;
					g_frame_height = h;
					merge_dirty(g_dirty, changed);
				}
			}
			const int hz = (std::max)(1, g_capture_hz_atomic.load());
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 / hz));
		}
		surface.release();
	}

	void destroy_texture()
	{
		if (g_device == nullptr)
			return;
		if (g_view.handle != 0)
			g_device->destroy_resource_view(g_view);
		if (g_texture.handle != 0)
			g_device->destroy_resource(g_texture);
		g_view = {};
		g_texture = {};
		g_tex_width = g_tex_height = 0;
	}

	bool ensure_texture(uint32_t w, uint32_t h)
	{
		if (g_texture.handle != 0 && g_tex_width == w && g_tex_height == h)
			return true;

		destroy_texture();

		const resource_desc desc(static_cast<int32_t>(w), static_cast<int32_t>(h), 1, 1,
			format::b8g8r8a8_unorm, 1, memory_heap::gpu_only, resource_usage::shader_resource);
		if (!g_device->create_resource(desc, nullptr, resource_usage::shader_resource, &g_texture))
			return false;
		if (!g_device->create_resource_view(g_texture, resource_usage::shader_resource,
			resource_view_desc(format::b8g8r8a8_unorm), &g_view))
		{
			g_device->destroy_resource(g_texture);
			g_texture = {};
			return false;
		}
		g_tex_width = w;
		g_tex_height = h;
		return true;
	}

	void upload_latest_frame()
	{
		using clock = std::chrono::steady_clock;
		static clock::time_point last_upload = {};

		std::lock_guard<std::mutex> lock(g_frame_mutex);
		if (!g_dirty.valid || g_frame.empty())
			return;

		// Texture freshly (re)created this call needs a full upload; otherwise
		// only the rectangle that changed since the last upload is sent.
		const bool had_texture = g_texture.handle != 0 && g_tex_width == g_frame_width && g_tex_height == g_frame_height;
		dirty_rect rect = g_dirty;

		// Split transitions change a tall region and are rare, so send them at
		// once; the timer ticking only changes a short band, so rate-limit those
		// to keep GPU syncs down without delaying what the eye notices.
		const uint32_t changed_rows = rect.valid ? rect.bottom - rect.top : 0;
		const bool small_change = had_texture && changed_rows * 3 < g_frame_height;
		const int hz = (std::max)(1, g_upload_hz);
		const auto min_gap = std::chrono::milliseconds(1000 / hz);
		const clock::time_point now = clock::now();
		if (small_change && now - last_upload < min_gap)
			return;

		g_dirty.valid = false;
		last_upload = now;
		if (!ensure_texture(g_frame_width, g_frame_height))
			return;
		if (!had_texture)
			rect = dirty_rect{ 0, 0, g_frame_width, g_frame_height, true };

		const size_t row = static_cast<size_t>(g_frame_width) * 4;
		subresource_data data = {};
		data.data = g_frame.data() + static_cast<size_t>(rect.top) * row + static_cast<size_t>(rect.left) * 4;
		data.row_pitch = static_cast<uint32_t>(row);
		data.slice_pitch = static_cast<uint32_t>(row * (rect.bottom - rect.top));
		const subresource_box box = { rect.left, rect.top, 0, rect.right, rect.bottom, 1 };
		g_device->update_texture_region(data, g_texture, 0, &box);
	}

	int to_byte(float v)
	{
		return static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
	}

	void publish_chroma_settings()
	{
		g_chroma_atomic = g_chroma;
		g_key_r = to_byte(g_key[0]);
		g_key_g = to_byte(g_key[1]);
		g_key_b = to_byte(g_key[2]);
		g_key_tol_atomic = g_key_tolerance;
	}

	void load_config(effect_runtime *runtime)
	{
		reshade::get_config_value(runtime, k_section, "Show", g_show);
		reshade::get_config_value(runtime, k_section, "AlignmentX", g_align_x);
		reshade::get_config_value(runtime, k_section, "AlignmentY", g_align_y);
		reshade::get_config_value(runtime, k_section, "OffsetX", g_offset_x);
		reshade::get_config_value(runtime, k_section, "OffsetY", g_offset_y);
		reshade::get_config_value(runtime, k_section, "Scale", g_scale);
		reshade::get_config_value(runtime, k_section, "Opacity", g_opacity);
		reshade::get_config_value(runtime, k_section, "CaptureHz", g_capture_hz);
		reshade::get_config_value(runtime, k_section, "UploadHz", g_upload_hz);
		reshade::get_config_value(runtime, k_section, "ChromaKey", g_chroma);
		reshade::get_config_value(runtime, k_section, "KeyTolerance", g_key_tolerance);
		reshade::get_config_value(runtime, k_section, "KeyR", g_key[0]);
		reshade::get_config_value(runtime, k_section, "KeyG", g_key[1]);
		reshade::get_config_value(runtime, k_section, "KeyB", g_key[2]);
		g_capture_hz = std::clamp(g_capture_hz, 1, 60);
		g_upload_hz = std::clamp(g_upload_hz, 1, 60);
		g_key_tolerance = std::clamp(g_key_tolerance, 0, 64);
		g_capture_hz_atomic = g_capture_hz;
		publish_chroma_settings();
	}

	void save_config(effect_runtime *runtime)
	{
		reshade::set_config_value(runtime, k_section, "Show", g_show);
		reshade::set_config_value(runtime, k_section, "AlignmentX", g_align_x);
		reshade::set_config_value(runtime, k_section, "AlignmentY", g_align_y);
		reshade::set_config_value(runtime, k_section, "OffsetX", g_offset_x);
		reshade::set_config_value(runtime, k_section, "OffsetY", g_offset_y);
		reshade::set_config_value(runtime, k_section, "Scale", g_scale);
		reshade::set_config_value(runtime, k_section, "Opacity", g_opacity);
		reshade::set_config_value(runtime, k_section, "CaptureHz", g_capture_hz);
		reshade::set_config_value(runtime, k_section, "UploadHz", g_upload_hz);
		reshade::set_config_value(runtime, k_section, "ChromaKey", g_chroma);
		reshade::set_config_value(runtime, k_section, "KeyTolerance", g_key_tolerance);
		reshade::set_config_value(runtime, k_section, "KeyR", g_key[0]);
		reshade::set_config_value(runtime, k_section, "KeyG", g_key[1]);
		reshade::set_config_value(runtime, k_section, "KeyB", g_key[2]);
	}

	void draw_settings(effect_runtime *runtime)
	{
		bool changed = false;
		changed |= ImGui::Checkbox("Show LiveSplit", &g_show);
		changed |= ImGui::SliderFloat("Vertical/Horizontal Alignment", &g_align_x, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("##alignment_y", &g_align_y, 0.0f, 1.0f);
		changed |= ImGui::InputInt2("Vertical/Horizontal Offsets", &g_offset_x);
		changed |= ImGui::SliderFloat("Scale", &g_scale, 0.25f, 2.0f, "%.2f");
		changed |= ImGui::SliderFloat("Overlay opacity", &g_opacity, 0.0f, 1.0f, "%.2f");
		if (ImGui::SliderInt("Capture rate (Hz)", &g_capture_hz, 1, 60))
		{
			g_capture_hz_atomic = g_capture_hz;
			changed = true;
		}
		changed |= ImGui::SliderInt("Upload rate (Hz) - lower = more FPS", &g_upload_hz, 1, 60);
		changed |= ImGui::Checkbox("Chroma key (remove background)", &g_chroma);
		changed |= ImGui::ColorEdit3("Background color", g_key, ImGuiColorEditFlags_NoInputs);
		changed |= ImGui::SliderInt("Key tolerance", &g_key_tolerance, 0, 64);
		if (ImGui::Button("Reset position"))
		{
			g_align_x = 0.17f; g_align_y = 0.0f; g_offset_x = 0; g_offset_y = 0; g_scale = 1.0f;
			changed = true;
		}
		if (changed)
		{
			publish_chroma_settings();
			save_config(runtime);
		}
	}

	void on_reshade_overlay(effect_runtime *runtime)
	{
		device *current = runtime->get_device();
		if (g_device == nullptr)
		{
			g_device = current;
			load_config(runtime);
		}
		else if (current != g_device)
		{
			// Device really changed: drop GPU resources tied to the old one.
			destroy_texture();
			g_device = current;
			std::lock_guard<std::mutex> lock(g_frame_mutex);
			g_dirty.valid = true;
		}
		if (!g_show)
			return;

		upload_latest_frame();
		if (g_view.handle == 0)
			return;

		const ImVec2 screen_size = ImGui::GetIO().DisplaySize;
		const float width = g_tex_width * g_scale;
		const float height = g_tex_height * g_scale;
		const float pos_x = std::floor((screen_size.x - width) * g_align_x) + static_cast<float>(g_offset_x);
		const float pos_y = std::floor((screen_size.y - height) * g_align_y) + static_cast<float>(g_offset_y);

		ImDrawList *draw = ImGui::GetBackgroundDrawList(nullptr);
		draw->AddImage(ImTextureRef(static_cast<ImTextureID>(g_view.handle)),
			ImVec2(pos_x, pos_y),
			ImVec2(pos_x + width, pos_y + height),
			ImVec2(0, 0), ImVec2(1, 1),
			IM_COL32(255, 255, 255, static_cast<int>(g_opacity * 255.0f)));
	}

	// The ReShade runtime is recreated repeatedly in exclusive fullscreen, each
	// time firing destroy_device for the SAME underlying device. Recreating the
	// texture on every one of those is the expensive part the overlay adds. So
	// only tear down when the process is actually ending; on_reshade_overlay
	// detects a genuine device swap by comparing the pointer instead.
	void on_destroy_device(device *dev)
	{
		if (!g_running.load() && dev == g_device)
		{
			destroy_texture();
			g_device = nullptr;
		}
	}
}

extern "C" BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(module);
		if (!reshade::register_addon(module))
			return FALSE;
		reshade::register_overlay(nullptr, draw_settings);
		reshade::register_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
		reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
		g_running = true;
		g_capture_thread = std::thread(capture_loop);
		break;
	case DLL_PROCESS_DETACH:
		g_running = false;
		if (g_capture_thread.joinable())
			g_capture_thread.join();
		reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
		reshade::unregister_event<reshade::addon_event::reshade_overlay>(on_reshade_overlay);
		reshade::unregister_overlay(nullptr, draw_settings);
		destroy_texture();
		reshade::unregister_addon(module);
		break;
	}
	return TRUE;
}
 
