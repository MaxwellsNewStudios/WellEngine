#pragma once
#include "Dependencies/ImGui/imgui.h"
#include "Dependencies/ImGui/imgui_internal.h"

#include <functional>
#include <string>
#include <map>

#ifdef USE_IMGUI

// ImGui utilities for simplifying window management, setting style presets
namespace ImGuiUtils
{
	void WrapMousePosEx(int axises_mask, const ImRect &wrap_rect);
	void WrapMousePos(int axises_mask);
	void LockMouseOnActive();

	enum class StyleType
	{
		Green,
		Red,
		Yellow,
		Cornflower,
	};
	void BeginButtonStyle(StyleType style);
	void EndButtonStyle();

	bool BeginFont(const std::string &name, float scale = 0.0f);
	void EndFont();
	bool SetDefaultFont(const std::string &name);

	void TextWithFont(const char *text, const std::string &font, float scale = 0.0f);
	bool ButtonWithFont(const char *text, const std::string &font, float scale = 0.0f, ImVec2 size = ImVec2(0, 0));


	// ImGui window, rendered at top level.
	class ImGuiAutoWindow
	{
	private:
		std::function<bool(void)> _func = nullptr;
		std::string _name = "";
		std::string _id = "";
		ImRect _initialRect = ImRect(0,0,0,0);

		bool _open = true;

	public:
		ImGuiAutoWindow() = default;
		~ImGuiAutoWindow() = default;
		ImGuiAutoWindow(const std::string &name, const std::string &id, std::function<bool(void)> &func, ImRect rect = ImRect(0,0,0,0)) : _name(name), _id(id), _func(func), _initialRect(rect) {};

		void Create(const std::string &name, const std::string &id, std::function<bool(void)> &func, ImRect rect = ImRect(0,0,0,0))
		{
			this->_name = name;
			this->_id = id;
			this->_func = func;
			this->_initialRect = rect;
		};

		[[nodiscard]] const std::string &GetID() const;
		[[nodiscard]] bool IsClosed() const;

		[[nodiscard]] bool Render();
	};

	class Utils
	{
	private:
		std::map<std::string, ImFont*> _fonts;
		std::vector<ImGuiAutoWindow> _windows;

	public:
		[[nodiscard]] static Utils *GetInstance();

		[[nodiscard]] static bool AddFont(const std::string &name, ImFont *font)
		{
			Utils *utils = GetInstance();
			if (utils->_fonts.find(name) != utils->_fonts.end())
				return false;

			utils->_fonts[name] = font;
			return true;
		}
		[[nodiscard]] static UINT GetFontCount()
		{
			return static_cast<UINT>(GetInstance()->_fonts.size());
		}
		[[nodiscard]] static bool HasFont(const std::string &name)
		{
			Utils *utils = GetInstance();
			return utils->_fonts.find(name) != utils->_fonts.end();
		}
		[[nodiscard]] static bool GetFont(const std::string &name, ImFont *&font)
		{
			Utils *utils = GetInstance();

			auto it = utils->_fonts.find(name);
			if (it == utils->_fonts.end())
				return false;

			font = it->second;
			return true;
		}
		[[nodiscard]] static const std::string &GetFontName(ImFont *font)
		{
			Utils *utils = GetInstance();

			for (const auto &pair : utils->_fonts)
			{
				if (pair.second == font)
					return pair.first;
			}

			static const std::string empty = "";
			return empty;
		}
		[[nodiscard]] static std::vector<std::pair<std::string, ImFont*>> GetFonts()
		{
			Utils *utils = GetInstance();

			std::vector<std::pair<std::string, ImFont*>> fonts;
			fonts.reserve(utils->_fonts.size());

			for (const auto &pair : utils->_fonts)
				fonts.push_back(pair);

			return fonts;
		}

		[[nodiscard]] static UINT GetWindowCount();
		[[nodiscard]] static bool GetWindow(const std::string &id, ImGuiAutoWindow **window);
		[[nodiscard]] static const std::string *GetWindowID(UINT index);
		[[nodiscard]] static bool OpenWindow(const ImGuiAutoWindow &window);
		[[nodiscard]] static bool OpenWindow(const std::string &name, const std::string &id, std::function<bool(void)> func, ImRect rect = ImRect(0, 0, 0, 0));
		[[nodiscard]] static bool CloseWindow(const ImGuiAutoWindow *window);
		[[nodiscard]] static bool CloseWindow(const std::string &id);

		[[nodiscard]] static bool Render();

		TESTABLE
	};
}

#endif // USE_IMGUI
