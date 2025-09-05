#pragma once
#include <Windows.h>
#include <iostream>
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <wrl/client.h>


enum class WindowType
{
	MAIN,
	SECONDARY
};

class Window 
{
private:
	std::string _name = "Window";
	HWND _hwnd = NULL;
	dx::XMINT2 _size{0, 0};
	dx::XMUINT2 _physicalSize{0, 0};
	dx::XMINT2 _preFullscreenSize{ -1, -1 };
	bool _isFullscreen = false;
	bool _isDirty = false;

	WindowType _windowType = WindowType::MAIN;

	SDL_Surface *_surface = nullptr;
	SDL_Window *_window = nullptr;

	D3D11_VIEWPORT _viewport = { };

	bool SetupWindow();

public:
	Window() = default;
	Window(std::string name, dx::XMINT2 size, WindowType windowType = WindowType::MAIN);
	~Window() = default;

	bool Initialize(std::string name, dx::XMINT2 size, WindowType windowType = WindowType::MAIN);
	bool ToggleFullscreen();
	bool SetFullscreen(bool fullscreen);
	bool SetWindowSize(dx::XMINT2 size);
	bool UpdateWindowSize();

	void ValidateWindowedSize();

	HWND GetHWND() const;

	dx::XMINT2 GetSize() const;
	UINT GetWidth() const;
	UINT GetHeight() const;

	dx::XMUINT2 GetPhysicalSize() const;
	UINT GetPhysicalWidth() const;
	UINT GetPhysicalHeight() const;

	SDL_Surface *GetSurface() const;
	SDL_Window *GetWindow() const;
	WindowType GetWindowType() const;

	const D3D11_VIEWPORT *GetViewport() const;

	bool IsFullscreen() const;
	bool IsDirty() const;
	void MarkDirty(bool state);

	TESTABLE()
};
