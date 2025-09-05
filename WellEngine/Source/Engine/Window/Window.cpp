#include "stdafx.h"
#include "Window.h"
#include "Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool Window::SetupWindow()
{
	SDL_InitFlags initFlags = 0;
	initFlags |= SDL_INIT_AUDIO;
	initFlags |= SDL_INIT_VIDEO;
	initFlags |= SDL_INIT_EVENTS;

	if (!SDL_Init(initFlags))
	{
		ErrMsgF("Error initializing SDL: {}", SDL_GetError());
		return false;
	}

	SDL_WindowFlags windowFlags = 0;
	windowFlags |= SDL_WINDOW_RESIZABLE;

#ifdef DEBUG_BUILD
	DebugData &debugData = DebugData::Get();
	if (debugData.windowMaximized)
		windowFlags |= SDL_WINDOW_MAXIMIZED;
#endif

	_window = SDL_CreateWindow(_name.c_str(), _size.x, _size.y, windowFlags);
	if (!_window) 
	{
		ErrMsgF("Couldn't create window: {}", SDL_GetError());
		return false;
	}

	SDL_PropertiesID props = SDL_GetWindowProperties(_window);
	_hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	if (!_hwnd) 
	{
		ErrMsgF("Couldn't get window handle: {}", SDL_GetError());
		return false;
	}
    return true;
}

Window::Window(std::string name, dx::XMINT2 size, WindowType windowType)
{
	Initialize(name, size, windowType);
}

bool Window::Initialize(std::string name, dx::XMINT2 size, WindowType windowType)
{
	ZoneScopedC(RandomUniqueColor());

	_name = std::move(name);
	_size = size;
	_windowType = windowType;

	if (!SetupWindow())
	{
		ErrMsg("Failed to setup window!");
		return false;
	}

	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.Width = static_cast<float>(size.x);
	_viewport.Height = static_cast<float>(size.y);
	_viewport.MinDepth = 0;
	_viewport.MaxDepth = 1;

	UpdateWindowSize();
	ValidateWindowedSize();
	_isDirty = false;

	return true;
}

bool Window::ToggleFullscreen()
{
	if (!_isFullscreen)
	{
		// Set window size to screen size before entering fullscreen
		_preFullscreenSize = _size;

		SDL_Rect screenRect{};
		SDL_GetDisplayBounds(SDL_GetDisplayForWindow(_window), &screenRect);
		dx::XMINT2 screenSize = { screenRect.w, screenRect.h };

		SetWindowSize(screenSize);
	}

	if (!SDL_SetWindowFullscreen(_window, !_isFullscreen))
		return false;

	_isDirty = true;
	_isFullscreen = !_isFullscreen;

	if (!_isFullscreen && _preFullscreenSize.x > 0)
	{
		// Restore previous window size after exiting fullscreen
		SetWindowSize(_preFullscreenSize);
		_preFullscreenSize = { -1, -1 };
	}

	UpdateWindowSize();

	// If exiting fullscreen and the window size is the same as the screen size, 
	// we must reduce it manually to avoid being unable to resize the window.
	ValidateWindowedSize();

#ifdef DEBUG_BUILD
	DebugData::Get().windowFullscreen = _isFullscreen;
#endif

	return true;
}
bool Window::SetFullscreen(bool fullscreen)
{
	if (_isFullscreen == fullscreen)
		return true;

	_isDirty = true;
	return ToggleFullscreen();
}

bool Window::SetWindowSize(dx::XMINT2 size)
{
/*#ifdef DEBUG_BUILD
	DebugData &debugData = DebugData::Get();
	debugData.windowSizeX = size.x;
	debugData.windowSizeY = size.y;
#endif*/
	_physicalSize = ToUint(size);
	_isDirty = true;
	return SDL_SetWindowSize(_window, size.x, size.y);
}

bool Window::UpdateWindowSize()
{
	int x = 0, y = 0;
	bool result = SDL_GetWindowSize(_window, &x, &y);

	_physicalSize = { (UINT)x, (UINT)y };
	_isDirty = true;
	return result;
}

void Window::ValidateWindowedSize()
{
	if (_isFullscreen)
		return;

	SDL_Rect screenRect{};
	SDL_GetDisplayBounds(SDL_GetDisplayForWindow(_window), &screenRect);
	dx::XMUINT2 screenSize = { (UINT)screenRect.w, (UINT)screenRect.h };

	bool hasChanged = false;
	dx::XMUINT2 newSize = _physicalSize;
	
	if (newSize.x >= screenSize.x)
	{
		newSize.x = screenSize.x - 32u;
		hasChanged = true;
	}
	
	if (newSize.y >= screenSize.y)
	{
		newSize.y = screenSize.y - 64u;
		hasChanged = true;
	}
	
	if (hasChanged)
	{
		_physicalSize = newSize;
		SDL_SetWindowSize(_window, (int)newSize.x, (int)newSize.y);
		SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		_isDirty = true;
	}
}

HWND Window::GetHWND() const
{
	return _hwnd;
}

dx::XMINT2 Window::GetSize() const
{
	return _size;
}
UINT Window::GetWidth() const
{
	return _size.x;
}
UINT Window::GetHeight() const
{
	return _size.y;
}

dx::XMUINT2 Window::GetPhysicalSize() const
{
	return _physicalSize;
}
UINT Window::GetPhysicalWidth() const
{
	return _physicalSize.x;
}
UINT Window::GetPhysicalHeight() const
{
	return _physicalSize.y;
}

SDL_Surface* Window::GetSurface() const
{
	return _surface;
}

SDL_Window* Window::GetWindow() const
{
	return _window;
}

const D3D11_VIEWPORT* Window::GetViewport() const
{
	return &_viewport;
}

WindowType Window::GetWindowType() const
{
	return _windowType;
}

bool Window::IsFullscreen() const
{
	return _isFullscreen;
}

bool Window::IsDirty() const
{
	return _isDirty;
}

void Window::MarkDirty(bool state)
{
	_isDirty = state;
}
