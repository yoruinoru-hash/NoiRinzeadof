// ReshadeFocusBridge - ReShade add-on
// 从共享内存读取 BepInEx 插件算好的角色屏幕坐标，写进 qUINT_dof 的 fADOF_AutofocusCenter。
// 编译：见 CMakeLists.txt（header-only 模式，不需要 ReShade 的 import library）。

#include <Windows.h>
#include <cstdint>
#include <cstring>

#include <reshade.hpp>

using namespace reshade::api;

// ---- 与 C# 侧必须完全一致的布局（16 字节）----
#pragma pack(push, 4)
struct FocusBridgeData
{
	int32_t version;   // 1
	float   u;         // 0..1
	float   v;         // 0..1
	int32_t valid;     // 1 = 有目标
};
#pragma pack(pop)

static const char *SHARED_MEMORY_NAME = "Local\\ReshadeFocusBridge";
static const char *UNIFORM_NAME = "fADOF_AutofocusCenter";

static HANDLE  s_mapping = nullptr;
static FocusBridgeData *s_view = nullptr;
static bool    s_logged_connect = false;

// 回调不能捕获，用文件作用域变量传递目标 UV
static float s_target_uv[2] = { 0.5f, 0.5f };

static void try_connect()
{
	if (s_view != nullptr)
		return;

	if (s_mapping == nullptr)
		s_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, SHARED_MEMORY_NAME);

	if (s_mapping == nullptr)
		return;

	s_view = static_cast<FocusBridgeData *>(MapViewOfFile(s_mapping, FILE_MAP_READ, 0, 0, sizeof(FocusBridgeData)));
	if (s_view != nullptr && !s_logged_connect)
	{
		s_logged_connect = true;
		reshade::log::message(reshade::log::level::info, "FocusBridge: connected to the BepInEx shared memory.");
	}
}

static void apply_focus(effect_runtime *runtime)
{
	if (s_view == nullptr || s_view->valid == 0)
		return;

	// 夹一下，防止异常值把采样区域甩到画面外
	float u = s_view->u;
	float v = s_view->v;
	s_target_uv[0] = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
	s_target_uv[1] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);

	// 必须用 lambda（可调用对象）而不是函数指针：API 内部会对 F* 解引用调用
	runtime->enumerate_uniform_variables(nullptr, [](effect_runtime *rt, effect_uniform_variable variable)
	{
		char name[128] = "";
		rt->get_uniform_variable_name(variable, name);
		if (std::strcmp(name, UNIFORM_NAME) != 0)
			return;

		float value[2] = { s_target_uv[0], s_target_uv[1] };
		rt->set_uniform_value_float(variable, value, 2);
	});
}

static void on_reshade_begin_effects(effect_runtime *runtime, command_list *, resource_view, resource_view)
{
	try_connect();
	apply_focus(runtime);
}

static void on_reshade_reloaded_effects(effect_runtime *runtime)
{
	(void)runtime;
	reshade::log::message(reshade::log::level::debug, "FocusBridge: effects reloaded, handles re-enumerated next frame.");
}

static void on_init_effect_runtime(effect_runtime *)
{
	try_connect();
}

static void on_destroy_effect_runtime(effect_runtime *)
{
	if (s_view != nullptr) { UnmapViewOfFile(s_view); s_view = nullptr; }
	if (s_mapping != nullptr) { CloseHandle(s_mapping); s_mapping = nullptr; }
	s_logged_connect = false;
}

extern "C" __declspec(dllexport) const char *NAME = "Reshade Focus Bridge";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Feeds a BepInEx-tracked character position into the qUINT ADOF autofocus point.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
		reshade::register_event<reshade::addon_event::reshade_begin_effects>(on_reshade_begin_effects);
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
		reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(on_reshade_begin_effects);
		reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
		reshade::unregister_addon(hModule);
		break;
	}
	return TRUE;
}
