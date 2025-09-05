#pragma once
#include <functional>
#include <string>

// ImGui utilities for simplifying window management, setting style presets
namespace ImGuiUtils
{
#ifdef USE_IMGUI
	enum class StyleType
	{
		Green,
		Red,
	};


	void WrapMousePosEx(int axises_mask, const ImRect &wrap_rect);
	void WrapMousePos(int axises_mask);
	void LockMouseOnActive();

	void BeginButtonStyle(StyleType style);
	void EndButtonStyle();

	// Pushes ImGui ID on creation, popped automatically when going out of scope.
	class ImGuiAutoID
	{
	public:
		ImGuiAutoID(const std::string &id);
		~ImGuiAutoID();
	};

	// ImGui style preset, popped automatically when going out of scope.
	class ImGuiAutoStyle
	{
	private:
		UINT _varStyles = 0;
		UINT _colorStyles = 0;

	public:
		ImGuiAutoStyle();
		~ImGuiAutoStyle();
	};

	// ImGui window, rendered at top level.
	class ImGuiAutoWindow
	{
	private:
		std::function<bool(void)> _func = nullptr;
		std::string _name = "";
		std::string _id = "";

		bool _open = true;

	public:
		ImGuiAutoWindow() = default;
		~ImGuiAutoWindow() = default;
		ImGuiAutoWindow(const std::string &name, const std::string &id, std::function<bool(void)> &func) : _name(name), _id(id), _func(func) {};

		void Create(const std::string &name, const std::string &id, std::function<bool(void)> &func)
		{
			this->_name = name;
			this->_id = id;
			this->_func = func;
		};

		[[nodiscard]] const std::string &GetID() const;
		[[nodiscard]] bool IsClosed() const;

		[[nodiscard]] bool Render();
	};


	class Utils
	{
	private:
		std::vector<ImGuiAutoWindow> windows;

	public:
		[[nodiscard]] static Utils *GetInstance();

		[[nodiscard]] static UINT GetWindowCount();
		[[nodiscard]] static bool GetWindow(const std::string &id, ImGuiAutoWindow **window);
		[[nodiscard]] static const std::string *GetWindowID(UINT index);
		[[nodiscard]] static bool OpenWindow(const ImGuiAutoWindow &window);
		[[nodiscard]] static bool OpenWindow(const std::string &name, const std::string &id, std::function<bool(void)> func);
		[[nodiscard]] static bool CloseWindow(const std::string &id);

		[[nodiscard]] static bool Render();

		TESTABLE()
	};
#endif
}