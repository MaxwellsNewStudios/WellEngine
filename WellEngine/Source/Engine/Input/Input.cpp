#include "stdafx.h"
#include "Input/Input.h"
#include <winuser.h>
#include "Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

Input::Input()
{
	for (int i = 0; i < 255; i++)
	{
		_vKeys[i] = (GetAsyncKeyState(i) & 0x8000) ? true : false;
	}

	SDL_GetMouseState(&_mousePos.x, &_mousePos.y);

#ifdef DEBUG_BUILD
	DebugData &dbgData = DebugData::Get();

	_sceneRenderSize = { 
		(UINT)dbgData.sceneViewSizeX,
		(UINT)dbgData.sceneViewSizeY
	};

	_mouseSensitivity = dbgData.mouseSensitivity;
#endif
}

bool Input::Update(Window &window)
{
	ZoneScopedC(RandomUniqueColor());
	
	_window = &window;
	
	SDL_Window *sdlWindow = _window->GetWindow();

	_windowFlags = SDL_GetWindowFlags(sdlWindow);
	_realWindowSize = _window->GetPhysicalSize();
	_isFullscreen = _window->IsFullscreen();
	SDL_GetWindowPosition(sdlWindow, &_windowPos.x, &_windowPos.y);

	SDL_Rect screenRect{};
	SDL_GetDisplayBounds(SDL_GetDisplayForWindow(sdlWindow), &screenRect);
	_screenPos = { screenRect.x, screenRect.y };
	_screenSize = { (UINT)screenRect.w, (UINT)screenRect.h };

	_hasMouseFocus = SDL_GetMouseFocus() == sdlWindow;
	_hasKeyboardFocus = SDL_GetKeyboardFocus() == sdlWindow;

	if (_hasMouseFocus)
	{
		if (_cursorLocked)
		{
			SDL_GetRelativeMouseState(&_mousePos.x, &_mousePos.y);
		}
		else
		{
			SDL_GetMouseState(&_mousePos.x, &_mousePos.y);
			_lMousePos.x = 0;
			_lMousePos.y = 0;
		}
	}
	else
	{
		_mousePos.x = 0;
		_mousePos.y = 0;
	}

#ifdef DEBUG_BUILD
	if (_wrapMouse && _hasMouseFocus && !_cursorLocked)
		TryWrapMouse();
#endif

	_localMousePos = _mousePos;
#ifdef USE_IMGUI
	_localMousePos.x -= _sceneViewPos.x;
	_localMousePos.y -= _sceneViewPos.y;

	_localMousePos.x /= (float)_sceneViewSize.x;
	_localMousePos.y /= (float)_sceneViewSize.y;
#else
	_localMousePos.x /= (float)_realWindowSize.x;
	_localMousePos.y /= (float)_realWindowSize.y;
#endif

	{
		ZoneNamedXNC(getAsyncKeyStateZone, "Input Update Key States", RandomUniqueColor(), true);

		for (int i = 0; i < _mouseButtonsEnd; i++)
		{
			_lvKeys[i] = _vKeys[i];
			_vKeys[i] = _hasMouseFocus ? (GetAsyncKeyState(i) & 0x8000) : false;
		}

		for (int i = _mouseButtonsEnd; i < 255; i++)
		{
			_lvKeys[i] = _vKeys[i];
			_vKeys[i] = _hasKeyboardFocus ? (GetAsyncKeyState(i) & 0x8000) : false;
		}
	}

	if (_cursorLocked)
	{
		_isKeyboardAbsorbed = false;
		_isMouseAbsorbed = false;
	}

	_keyboardWasAbsorbed = _isKeyboardAbsorbed;
	_isKeyboardAbsorbed = false;

	_mouseWasAbsorbed = _isMouseAbsorbed;
	_isMouseAbsorbed  = false;

	if (!_hasMouseFocus)
		SetMouseScroll({ 0.0f, 0.0f });

	return true;
}

bool Input::TryWrapMouse()
{
	// Check if mouse is on the outermost edge of the screen.
	// If it is, warp it to the opposite side plus one pixel.

	bool isOnEdge = false;
	dx::XMFLOAT2 wrappedPos = _mousePos;
	dx::XMINT2 relativeScreenPos = { _screenPos.x - _windowPos.x, _screenPos.y - _windowPos.y};

	if ((int)std::floor(_mousePos.x) <= relativeScreenPos.x)
	{
		isOnEdge = true;
		wrappedPos.x += (int)_screenSize.x - 2.0f; // Wrap to right edge
	}
	else if ((int)std::ceil(_mousePos.x) >= relativeScreenPos.x + (int)_screenSize.x - 1)
	{
		isOnEdge = true;
		wrappedPos.x -= (int)_screenSize.x - 2.0f; // Wrap to left edge
	}

	if ((int)std::floor(_mousePos.y) <= relativeScreenPos.y)
	{
		isOnEdge = true;
		wrappedPos.y += (int)_screenSize.y - 2.0f; // Wrap to lower edge
	}
	else if ((int)std::ceil(_mousePos.y) >= relativeScreenPos.y + (int)_screenSize.y - 1)
	{
		isOnEdge = true;
		wrappedPos.y -= (int)_screenSize.y - 2.0f; // Wrap to upper edge
	}

	if (isOnEdge)
	{
		_mousePos = wrappedPos;
		_lMousePos = wrappedPos;

		SDL_WarpMouseGlobal(wrappedPos.x + _windowPos.x, wrappedPos.y + _windowPos.y);
	}

	return isOnEdge;
}

KeyState Input::GetKey(const KeyCode keyCode, bool ignoreAbsorb) const
{
	const unsigned char key = static_cast<unsigned char>(keyCode);
	return GetKey(static_cast<UCHAR>(keyCode), ignoreAbsorb);
}
KeyState Input::GetKey(const UCHAR keyCode, bool ignoreAbsorb) const
{
#ifndef DEBUG_BUILD
	if (_disable) 
		return KeyState::None;
#endif 

	if (!ignoreAbsorb)
	{
		if (keyCode < _mouseButtonsEnd && _mouseWasAbsorbed)
			return KeyState::None;

		if (keyCode >= _mouseButtonsEnd && _keyboardWasAbsorbed)
			return KeyState::None;
	}

	const bool
		thisFrame = _vKeys[keyCode],
		lastFrame = _lvKeys[keyCode];

	if (thisFrame)
	{
		if (lastFrame)
			return KeyState::Held;

		return KeyState::Pressed;
	}

	if (lastFrame)
		return KeyState::Released;

	return KeyState::None;
}

void Input::GetPressedKeys(std::vector<KeyCode> &keys, bool ignoreAbsorb) const
{
#ifndef DEBUG_BUILD
	if (_disable) return;
#endif

	for (int i = 0; i < 255; i++)
	{
		if (!ignoreAbsorb)
		{
			if (i < _mouseButtonsEnd && _mouseWasAbsorbed)
				continue;

			if (i >= _mouseButtonsEnd && _keyboardWasAbsorbed)
				break;
		}

		if (_vKeys[i])
			keys.push_back(static_cast<KeyCode>(i));
	}
}
KeyCode Input::GetPressedKey(bool ignoreAbsorb) const
{
#ifndef DEBUG_BUILD
	if (_disable) return KeyCode::None;
#endif

	for (int i = 0; i < 255; i++)
	{
		if (!ignoreAbsorb)
		{
			if (i < _mouseButtonsEnd && _mouseWasAbsorbed)
				continue;

			if (i >= _mouseButtonsEnd && _keyboardWasAbsorbed)
				break;
		}

		if (_vKeys[i])
			return static_cast<KeyCode>(i);
	}
	return KeyCode::None;
}

MouseState Input::GetMouse() const
{
	return {
		_mousePos,
		{ _mousePos.x - _lMousePos.x, _mousePos.y - _lMousePos.y },
		_scroll
	};
}
dx::XMFLOAT2 Input::GetLocalMousePos() const
{
	return _localMousePos;
}

float Input::GetMouseSensitivity() const
{
	return _mouseSensitivity;
}
void Input::SetMouseSensitivity(float sensitivity)
{
	_mouseSensitivity = sensitivity;

#ifdef DEBUG_BUILD
	DebugData::Get().mouseSensitivity = sensitivity;
#endif
}

bool Input::GetKeyboardAbsorbed() const
{
	return _isKeyboardAbsorbed;
}
void Input::SetKeyboardAbsorbed(bool absorbed)
{
	_isKeyboardAbsorbed = absorbed;
}

bool Input::GetMouseAbsorbed() const
{
	return _isMouseAbsorbed;
}
void Input::SetMouseAbsorbed(bool absorbed)
{
	_isMouseAbsorbed = absorbed;
}

dx::XMINT2 Input::GetScreenPos() const
{
	return _screenPos;
}
dx::XMUINT2 Input::GetScreenSize() const
{
	return _screenSize;
}

Window *Input::GetWindow() const
{
	return _window;
}
dx::XMUINT2 Input::GetWindowSize() const
{
	return _windowSize;
}
dx::XMUINT2 Input::GetRealWindowSize() const
{
	return _realWindowSize;
}
dx::XMUINT2 Input::GetActiveWindowSize() const
{
	if (_isFullscreen)
		return GetWindowSize();
	else
		return GetRealWindowSize();
}
dx::XMINT2 Input::GetWindowPos() const
{
	return _windowPos;
}

void Input::SetWindowSize(dx::XMUINT2 size)
{
	_windowSize = size;
}
void Input::SetWindowPos(Window &window, dx::XMINT2 pos)
{
	_windowPos = pos;
	SDL_SetWindowPosition(window.GetWindow(), pos.x, pos.y);
}
void Input::SetMouseScroll(dx::XMFLOAT2 scroll)
{
	_scroll = scroll;
}
void Input::SetMousePosition(Window &window, dx::XMFLOAT2 pos, bool teleport)
{
	_mousePos = pos;
	if (teleport)
		_lMousePos = pos;

	SDL_WarpMouseInWindow(window.GetWindow(), pos.x, pos.y);
}

#ifdef USE_IMGUI
dx::XMUINT2 Input::GetSceneViewSize() const
{
	return _sceneViewSize;
}
dx::XMINT2 Input::GetSceneViewPos() const
{
	return _sceneViewPos;
}
void Input::SetSceneViewSize(dx::XMUINT2 size)
{
	_sceneViewSize = size;
}
void Input::SetSceneViewPos(dx::XMINT2 pos)
{
	_sceneViewPos = pos;
}

dx::XMUINT2 Input::GetSceneRenderSize() const
{
	return _sceneRenderSize;
}
bool Input::HasResizedSceneView()
{
	auto &debugData = DebugData::Get();
	UINT newWidth = debugData.sceneViewSizeX;
	UINT newHeight = debugData.sceneViewSizeY;

	if (_sceneRenderSize.x != newWidth || _sceneRenderSize.y != newHeight)
	{
		_sceneRenderSize.x = newWidth;
		_sceneRenderSize.y = newHeight;
		return true;
	}

	return false;
}
#endif

bool Input::IsPressedOrHeld(KeyCode keyCode, bool ignoreAbsorb) const
{
#ifndef DEBUG_BUILD
	if (_disable) return false;
#endif

	const KeyState state = GetKey(keyCode, ignoreAbsorb);
	return (state == KeyState::Pressed || state == KeyState::Held);
}

SDL_WindowFlags Input::GetSDLWindowFlags() const
{
	return _windowFlags;
}

bool Input::IsInFocus() const 
{
	return _windowFlags & SDL_WINDOW_INPUT_FOCUS; 
}

bool Input::HasKeyboardFocus() const
{
	return _hasKeyboardFocus && !_isKeyboardAbsorbed;
}
bool Input::HasMouseFocus() const
{
	return _hasMouseFocus && !_isMouseAbsorbed;
}

bool Input::IsCursorLocked() const 
{ 
	return _cursorLocked; 
}

void Input::DisableAllInput()
{
#ifndef DEBUG_BUILD
	_disable = true;
#endif
}
void Input::EnableAllInput()
{
#ifndef DEBUG_BUILD
	_disable = false;
#endif
}

bool Input::ToggleLockCursor(Window &window)
{
	SDL_Window* sdl_window = window.GetWindow();

	// Warp cursor to center of window if unlocked.
	if(_cursorLocked)
		SDL_WarpMouseInWindow(sdl_window, window.GetWidth()/2.0f, window.GetHeight()/2.0f);

	// Toggle cursor lock.
	SDL_SetWindowRelativeMouseMode(sdl_window, !_cursorLocked);
	_cursorLocked = SDL_GetWindowRelativeMouseMode(sdl_window);

	return _cursorLocked;
}

#ifdef USE_IMGUI
bool Input::RenderUI()
{
	ImGui::SeparatorText("Options");
	ImGui::Checkbox("Mouse Wrap", &_wrapMouse);
	ImGui::Dummy({ 0, 4 });

	ImGui::SeparatorText("General");
	ImGui::Text(std::format("Screen Pos:    ({}, {})", _screenPos.x, _screenPos.y).c_str());
	ImGui::Text(std::format("Screen Size:   ({}, {})", _screenSize.x, _screenSize.y).c_str());
	ImGui::Dummy({ 0, 2 });
	ImGui::Text(std::format("Window Pos:    ({}, {})", _windowPos.x, _windowPos.y).c_str());
	ImGui::Text(std::format("Window Size:   ({}, {})", _windowSize.x, _windowSize.y).c_str());
	ImGui::Dummy({ 0, 2 });
	ImGui::Text(std::format("Scene Pos:    ({}, {})", _sceneViewPos.x, _sceneViewPos.y).c_str());
	ImGui::Text(std::format("Scene Size:   ({}, {})", _sceneViewSize.x, _sceneViewSize.y).c_str());
	ImGui::Text(std::format("Scene Render: ({}, {})", _sceneRenderSize.x, _sceneRenderSize.y).c_str());
	ImGui::Dummy({ 0, 4 });
	ImGui::Text(std::format("Fullscreen:    {}", _isFullscreen).c_str());
	ImGui::Text(std::format("Cursor Locked: {}", _cursorLocked).c_str());
	ImGui::Text(std::format("Input Enabled: {}", !_disable).c_str());
	ImGui::Text(std::format("Input Focus:   {}", IsInFocus()).c_str());
	ImGui::Dummy({ 0, 4 });

	ImGui::SeparatorText("Keyboard");
	ImGui::Text(std::format("Absorbed:  {}", _keyboardWasAbsorbed).c_str());
	ImGui::Text(std::format("Has Focus: {}", _hasKeyboardFocus).c_str());
	ImGui::Dummy({ 0, 4 });

	ImGui::SeparatorText("Mouse");
	ImGui::Text(std::format("Pos:		  ({}, {})", _mousePos.x, _mousePos.y).c_str());
	ImGui::Text(std::format("Last Pos:	  ({}, {})", _lMousePos.x, _lMousePos.y).c_str());
	ImGui::Text(std::format("Local Pos:	  ({}, {})", _localMousePos.x, _localMousePos.y).c_str());
	ImGui::Text(std::format("Absorbed:    {}", _mouseWasAbsorbed).c_str());
	ImGui::Text(std::format("Has Focus:   {}", _hasMouseFocus).c_str());
	ImGui::Dummy({ 0, 4 });

	if (ImGui::TreeNode("Active Keys"))
	{
		ImGui::Separator();
		for (int i = 0; i < 255; i++)
		{
			KeyCode code = static_cast<KeyCode>(i);
			KeyState state = GetKey(code);

			if (state == KeyState::None)
				continue;

			std::string keyName = "?";
			for (const auto &[key, value] : KeyCodeNames)
			{
				if (value == code)
				{
					keyName = key;
					break;
				}
			}
			ImGui::Text(keyName.c_str());
		}
		ImGui::Separator();
		ImGui::TreePop();
	}

	return true;
}
#endif
