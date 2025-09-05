#pragma once

#include <DirectXMath.h>
#include <unordered_map>
#include <Windows.h>
#include <SDL3/SDL.h>
#include "Window/Window.h"

namespace dx = DirectX;


enum class KeyCode : unsigned char
{
	None = 0x0,
	M1 = 0x1,
	M2 = 0x2,
	M3 = 0x4,
	M4 = 0x5,
	M5 = 0x6,
	Back = 0x8,
	Tab = 0x9,
	Enter = 0xd,
	Pause = 0x13,
	CapsLock = 0x14,
	Kana = 0x15,
	ImeOn = 0x16,
	Kanji = 0x19,
	ImeOff = 0x1a,
	Escape = 0x1b,
	ImeConvert = 0x1c,
	ImeNoConvert = 0x1d,
	Space = 0x20,
	PageUp = 0x21,
	PageDown = 0x22,
	End = 0x23,
	Home = 0x24,
	Left = 0x25,
	Up = 0x26,
	Right = 0x27,
	Down = 0x28,
	Select = 0x29,
	Print = 0x2a,
	Execute = 0x2b,
	PrintScreen = 0x2c,
	Insert = 0x2d,
	Delete = 0x2e,
	Help = 0x2f,
	D0 = 0x30,
	D1 = 0x31,
	D2 = 0x32,
	D3 = 0x33,
	D4 = 0x34,
	D5 = 0x35,
	D6 = 0x36,
	D7 = 0x37,
	D8 = 0x38,
	D9 = 0x39,
	A = 0x41,
	B = 0x42,
	C = 0x43,
	D = 0x44,
	E = 0x45,
	F = 0x46,
	G = 0x47,
	H = 0x48,
	I = 0x49,
	J = 0x4a,
	K = 0x4b,
	L = 0x4c,
	M = 0x4d,
	N = 0x4e,
	O = 0x4f,
	P = 0x50,
	Q = 0x51,
	R = 0x52,
	S = 0x53,
	T = 0x54,
	U = 0x55,
	V = 0x56,
	W = 0x57,
	X = 0x58,
	Y = 0x59,
	Z = 0x5a,
	LeftWindows = 0x5b,
	RightWindows = 0x5c,
	Apps = 0x5d,
	Sleep = 0x5f,
	NumPad0 = 0x60,
	NumPad1 = 0x61,
	NumPad2 = 0x62,
	NumPad3 = 0x63,
	NumPad4 = 0x64,
	NumPad5 = 0x65,
	NumPad6 = 0x66,
	NumPad7 = 0x67,
	NumPad8 = 0x68,
	NumPad9 = 0x69,
	Multiply = 0x6a,
	Add = 0x6b,
	Separator = 0x6c,
	Subtract = 0x6d,
	Decimal = 0x6e,
	Divide = 0x6f,
	F1 = 0x70,
	F2 = 0x71,
	F3 = 0x72,
	F4 = 0x73,
	F5 = 0x74,
	F6 = 0x75,
	F7 = 0x76,
	F8 = 0x77,
	F9 = 0x78,
	F10 = 0x79,
	F11 = 0x7a,
	F12 = 0x7b,
	F13 = 0x7c,
	F14 = 0x7d,
	F15 = 0x7e,
	F16 = 0x7f,
	F17 = 0x80,
	F18 = 0x81,
	F19 = 0x82,
	F20 = 0x83,
	F21 = 0x84,
	F22 = 0x85,
	F23 = 0x86,
	F24 = 0x87,
	NumLock = 0x90,
	Scroll = 0x91,
	LeftShift = 0xa0,
	RightShift = 0xa1,
	LeftControl = 0xa2,
	RightControl = 0xa3,
	LeftAlt = 0xa4,
	RightAlt = 0xa5,
	BrowserBack = 0xa6,
	BrowserForward = 0xa7,
	BrowserRefresh = 0xa8,
	BrowserStop = 0xa9,
	BrowserSearch = 0xaa,
	BrowserFavorites = 0xab,
	BrowserHome = 0xac,
	VolumeMute = 0xad,
	VolumeDown = 0xae,
	VolumeUp = 0xaf,
	MediaNextTrack = 0xb0,
	MediaPreviousTrack = 0xb1,
	MediaStop = 0xb2,
	MediaPlayPause = 0xb3,
	LaunchMail = 0xb4,
	SelectMedia = 0xb5,
	LaunchApplication1 = 0xb6,
	LaunchApplication2 = 0xb7,
	OemSemicolon = 0xba,
	OemPlus = 0xbb,
	OemComma = 0xbc,
	OemMinus = 0xbd,
	OemPeriod = 0xbe,
	OemQuestion = 0xbf,
	OemTilde = 0xc0,
	OemOpenBrackets = 0xdb,
	OemPipe = 0xdc,
	OemCloseBrackets = 0xdd,
	OemQuotes = 0xde,
	Oem8 = 0xdf,
	OemBackslash = 0xe2,
	ProcessKey = 0xe5,
	OemCopy = 0xf2,
	OemAuto = 0xf3,
	OemEnlW = 0xf4,
	Attn = 0xf6,
	Crsel = 0xf7,
	Exsel = 0xf8,
	EraseEof = 0xf9,
	Play = 0xfa,
	Zoom = 0xfb,
	Pa1 = 0xfd,
	OemClear = 0xfe,
};

enum class KeyState : unsigned char
{
	None        = 0b000,
	Pressed     = 0b001,
	Held        = 0b010,
	Released    = 0b100,

	PressedHeld = Pressed | Held,
};

const std::unordered_map<std::string, KeyCode> KeyCodeNames = {
	{ "None",                   (KeyCode)0x0    },
	{ "M1",                     (KeyCode)0x1    },
	{ "M2",                     (KeyCode)0x2    },
	{ "M3",                     (KeyCode)0x4    },
	{ "M4",                     (KeyCode)0x5    },
	{ "M5",                     (KeyCode)0x6    },
	{ "Back",                   (KeyCode)0x8    },
	{ "Tab",                    (KeyCode)0x9    },
	{ "Enter",                  (KeyCode)0xd    },
	{ "Pause",                  (KeyCode)0x13   },
	{ "CapsLock",               (KeyCode)0x14   },
	{ "Kana",                   (KeyCode)0x15   },
	{ "ImeOn",                  (KeyCode)0x16   },
	{ "Kanji",                  (KeyCode)0x19   },
	{ "ImeOff",                 (KeyCode)0x1a   },
	{ "Escape",                 (KeyCode)0x1b   },
	{ "ImeConvert",             (KeyCode)0x1c   },
	{ "ImeNoConvert",           (KeyCode)0x1d   },
	{ "Space",                  (KeyCode)0x20   },
	{ "PageUp",                 (KeyCode)0x21   },
	{ "PageDown",               (KeyCode)0x22   },
	{ "End",                    (KeyCode)0x23   },
	{ "Home",                   (KeyCode)0x24   },
	{ "Left",                   (KeyCode)0x25   },
	{ "Up",                     (KeyCode)0x26   },
	{ "Right",                  (KeyCode)0x27   },
	{ "Down",                   (KeyCode)0x28   },
	{ "Select",                 (KeyCode)0x29   },
	{ "Print",                  (KeyCode)0x2a   },
	{ "Execute",                (KeyCode)0x2b   },
	{ "PrintScreen",            (KeyCode)0x2c   },
	{ "Insert",                 (KeyCode)0x2d   },
	{ "Delete",                 (KeyCode)0x2e   },
	{ "Help",                   (KeyCode)0x2f   },
	{ "D0",                     (KeyCode)0x30   },
	{ "D1",                     (KeyCode)0x31   },
	{ "D2",                     (KeyCode)0x32   },
	{ "D3",                     (KeyCode)0x33   },
	{ "D4",                     (KeyCode)0x34   },
	{ "D5",                     (KeyCode)0x35   },
	{ "D6",                     (KeyCode)0x36   },
	{ "D7",                     (KeyCode)0x37   },
	{ "D8",                     (KeyCode)0x38   },
	{ "D9",                     (KeyCode)0x39   },
	{ "A",                      (KeyCode)0x41   },
	{ "B",                      (KeyCode)0x42   },
	{ "C",                      (KeyCode)0x43   },
	{ "D",                      (KeyCode)0x44   },
	{ "E",                      (KeyCode)0x45   },
	{ "F",                      (KeyCode)0x46   },
	{ "G",                      (KeyCode)0x47   },
	{ "H",                      (KeyCode)0x48   },
	{ "I",                      (KeyCode)0x49   },
	{ "J",                      (KeyCode)0x4a   },
	{ "K",                      (KeyCode)0x4b   },
	{ "L",                      (KeyCode)0x4c   },
	{ "M",                      (KeyCode)0x4d   },
	{ "N",                      (KeyCode)0x4e   },
	{ "O",                      (KeyCode)0x4f   },
	{ "P",                      (KeyCode)0x50   },
	{ "Q",                      (KeyCode)0x51   },
	{ "R",                      (KeyCode)0x52   },
	{ "S",                      (KeyCode)0x53   },
	{ "T",                      (KeyCode)0x54   },
	{ "U",                      (KeyCode)0x55   },
	{ "V",                      (KeyCode)0x56   },
	{ "W",                      (KeyCode)0x57   },
	{ "X",                      (KeyCode)0x58   },
	{ "Y",                      (KeyCode)0x59   },
	{ "Z",                      (KeyCode)0x5a   },
	{ "LeftWindows",            (KeyCode)0x5b   },
	{ "RightWindows",           (KeyCode)0x5c   },
	{ "Apps",                   (KeyCode)0x5d   },
	{ "Sleep",                  (KeyCode)0x5f   },
	{ "NumPad0",                (KeyCode)0x60   },
	{ "NumPad1",                (KeyCode)0x61   },
	{ "NumPad2",                (KeyCode)0x62   },
	{ "NumPad3",                (KeyCode)0x63   },
	{ "NumPad4",                (KeyCode)0x64   },
	{ "NumPad5",                (KeyCode)0x65   },
	{ "NumPad6",                (KeyCode)0x66   },
	{ "NumPad7",                (KeyCode)0x67   },
	{ "NumPad8",                (KeyCode)0x68   },
	{ "NumPad9",                (KeyCode)0x69   },
	{ "Multiply",               (KeyCode)0x6a   },
	{ "Add",                    (KeyCode)0x6b   },
	{ "Separator",              (KeyCode)0x6c   },
	{ "Subtract",               (KeyCode)0x6d   },
	{ "Decimal",                (KeyCode)0x6e   },
	{ "Divide",                 (KeyCode)0x6f   },
	{ "F1",                     (KeyCode)0x70   },
	{ "F2",                     (KeyCode)0x71   },
	{ "F3",                     (KeyCode)0x72   },
	{ "F4",                     (KeyCode)0x73   },
	{ "F5",                     (KeyCode)0x74   },
	{ "F6",                     (KeyCode)0x75   },
	{ "F7",                     (KeyCode)0x76   },
	{ "F8",                     (KeyCode)0x77   },
	{ "F9",                     (KeyCode)0x78   },
	{ "F10",                    (KeyCode)0x79   },
	{ "F11",                    (KeyCode)0x7a   },
	{ "F12",                    (KeyCode)0x7b   },
	{ "F13",                    (KeyCode)0x7c   },
	{ "F14",                    (KeyCode)0x7d   },
	{ "F15",                    (KeyCode)0x7e   },
	{ "F16",                    (KeyCode)0x7f   },
	{ "F17",                    (KeyCode)0x80   },
	{ "F18",                    (KeyCode)0x81   },
	{ "F19",                    (KeyCode)0x82   },
	{ "F20",                    (KeyCode)0x83   },
	{ "F21",                    (KeyCode)0x84   },
	{ "F22",                    (KeyCode)0x85   },
	{ "F23",                    (KeyCode)0x86   },
	{ "F24",                    (KeyCode)0x87   },
	{ "NumLock",                (KeyCode)0x90   },
	{ "Scroll",                 (KeyCode)0x91   },
	{ "LeftShift",              (KeyCode)0xa0   },
	{ "RightShift",             (KeyCode)0xa1   },
	{ "LeftControl",            (KeyCode)0xa2   },
	{ "RightControl",           (KeyCode)0xa3   },
	{ "LeftAlt",                (KeyCode)0xa4   },
	{ "RightAlt",               (KeyCode)0xa5   },
	{ "BrowserBack",            (KeyCode)0xa6   },
	{ "BrowserForward",         (KeyCode)0xa7   },
	{ "BrowserRefresh",         (KeyCode)0xa8   },
	{ "BrowserStop",            (KeyCode)0xa9   },
	{ "BrowserSearch",          (KeyCode)0xaa   },
	{ "BrowserFavorites",       (KeyCode)0xab   },
	{ "BrowserHome",            (KeyCode)0xac   },
	{ "VolumeMute",             (KeyCode)0xad   },
	{ "VolumeDown",             (KeyCode)0xae   },
	{ "VolumeUp",               (KeyCode)0xaf   },
	{ "MediaNextTrack",         (KeyCode)0xb0   },
	{ "MediaPreviousTrack",     (KeyCode)0xb1   },
	{ "MediaStop",              (KeyCode)0xb2   },
	{ "MediaPlayPause",         (KeyCode)0xb3   },
	{ "LaunchMail",             (KeyCode)0xb4   },
	{ "SelectMedia",            (KeyCode)0xb5   },
	{ "LaunchApplication1",     (KeyCode)0xb6   },
	{ "LaunchApplication2",     (KeyCode)0xb7   },
	{ "OemSemicolon",           (KeyCode)0xba   },
	{ "OemPlus",                (KeyCode)0xbb   },
	{ "OemComma",               (KeyCode)0xbc   },
	{ "OemMinus",               (KeyCode)0xbd   },
	{ "OemPeriod",              (KeyCode)0xbe   },
	{ "OemQuestion",            (KeyCode)0xbf   },
	{ "OemTilde",               (KeyCode)0xc0   },
	{ "OemOpenBrackets",        (KeyCode)0xdb   },
	{ "OemPipe",                (KeyCode)0xdc   },
	{ "OemCloseBrackets",       (KeyCode)0xdd   },
	{ "OemQuotes",              (KeyCode)0xde   },
	{ "Oem8",                   (KeyCode)0xdf   },
	{ "OemBackslash",           (KeyCode)0xe2   },
	{ "ProcessKey",             (KeyCode)0xe5   },
	{ "OemCopy",                (KeyCode)0xf2   },
	{ "OemAuto",                (KeyCode)0xf3   },
	{ "OemEnlW",                (KeyCode)0xf4   },
	{ "Attn",                   (KeyCode)0xf6   },
	{ "Crsel",                  (KeyCode)0xf7   },
	{ "Exsel",                  (KeyCode)0xf8   },
	{ "EraseEof",               (KeyCode)0xf9   },
	{ "Play",                   (KeyCode)0xfa   },
	{ "Zoom",                   (KeyCode)0xfb   },
	{ "Pa1",                    (KeyCode)0xfd   },
	{ "OemClear",               (KeyCode)0xfe   }
};

const std::unordered_map<std::string, KeyState> KeyStateNames = {
	{ "None",                   (KeyState)0b000 },
	{ "Pressed",                (KeyState)0b001 },
	{ "Held",                   (KeyState)0b010 },
	{ "Released",               (KeyState)0b100 }
};

struct MouseState
{
	dx::XMFLOAT2 pos;
	dx::XMFLOAT2 delta;
	dx::XMFLOAT2 scroll;
};


// Manages input from keyboard, mouse and window.
class Input
{
private:
	const UCHAR _mouseButtonsEnd = static_cast<UCHAR>(KeyCode::M5) + 1;

	bool
		_vKeys[256] = { },
		_lvKeys[256] = { };

	dx::XMFLOAT2 _mousePos{0, 0};
	dx::XMFLOAT2 _lMousePos{ 0, 0 };
#ifdef USE_IMGUI
	dx::XMFLOAT2 _dragLockMousePos{ 0, 0 };
#endif
	dx::XMFLOAT2 _localMousePos{0, 0};
	dx::XMFLOAT2 _scroll{ 0, 0 };

	dx::XMINT2 _windowPos{0, 0};
	dx::XMUINT2 _windowSize{ 0, 0 };
	dx::XMUINT2 _realWindowSize{ 0, 0 };

	dx::XMINT2 _screenPos{ 0, 0 };
	dx::XMUINT2 _screenSize{ 0, 0 };

#ifdef USE_IMGUI
	dx::XMINT2 _sceneViewPos{0, 0};
	dx::XMUINT2 _sceneViewSize{0, 0};
	dx::XMUINT2 _sceneRenderSize{0, 0};
#endif

	bool
		_cursorLocked = false,
		_isFullscreen = false,
		_disable = false,
		_hasMouseFocus = false,
		_hasKeyboardFocus = false,
		_isMouseAbsorbed = false,
		_isKeyboardAbsorbed = false,
		_mouseWasAbsorbed = false,
		_keyboardWasAbsorbed = false,
		_wrapMouse = false;

	float _mouseSensitivity = 1.0f;

	SDL_WindowFlags _windowFlags = 0;
	Window *_window = nullptr;

public:
	Input();
	~Input() = default;

	static inline Input &Instance()
	{
		static Input instance;
		return instance;
	}

	[[nodiscard]] bool Update(Window &window);

	bool TryWrapMouse();

	[[nodiscard]] KeyState GetKey(KeyCode keyCode, bool ignoreAbsorb = false) const;
	[[nodiscard]] KeyState GetKey(UCHAR keyCode, bool ignoreAbsorb = false) const;
	[[nodiscard]] KeyCode GetPressedKey(bool ignoreAbsorb = false) const;
	void GetPressedKeys(std::vector<KeyCode> &keys, bool ignoreAbsorb = false) const;
	[[nodiscard]] MouseState GetMouse() const;

	[[nodiscard]] dx::XMFLOAT2 GetLocalMousePos() const;

	[[nodiscard]] float GetMouseSensitivity() const;
	void SetMouseSensitivity(float sensitivity);

	[[nodiscard]] bool GetKeyboardAbsorbed() const;
	void SetKeyboardAbsorbed(bool absorbed);

	[[nodiscard]] bool GetMouseAbsorbed() const;
	void SetMouseAbsorbed(bool absorbed);

	[[nodiscard]] dx::XMINT2 GetScreenPos() const;
	[[nodiscard]] dx::XMUINT2 GetScreenSize() const;

	Window *GetWindow() const;
	[[nodiscard]] dx::XMUINT2 GetWindowSize() const;
	[[nodiscard]] dx::XMUINT2 GetRealWindowSize() const;
	[[nodiscard]] dx::XMUINT2 GetActiveWindowSize() const;
	dx::XMINT2 GetWindowPos() const;

	void SetWindowSize(dx::XMUINT2 size);
	void SetWindowPos(Window &window, dx::XMINT2 pos);
	void SetMouseScroll(dx::XMFLOAT2 scroll);
	void SetMousePosition(Window &window, dx::XMFLOAT2 pos, bool teleport = false);

#ifdef USE_IMGUI
	[[nodiscard]] dx::XMUINT2 GetSceneViewSize() const;
	[[nodiscard]] dx::XMINT2 GetSceneViewPos() const;
	void SetSceneViewSize(dx::XMUINT2 size);
	void SetSceneViewPos(dx::XMINT2 pos);

	[[nodiscard]] dx::XMUINT2 GetSceneRenderSize() const;
	[[nodiscard]] bool HasResizedSceneView();
#endif

	[[nodiscard]] bool IsPressedOrHeld(KeyCode keyCode, bool ignoreAbsorb = false) const;

	[[nodiscard]] SDL_WindowFlags GetSDLWindowFlags() const;

	[[nodiscard]] bool IsInFocus() const;
	[[nodiscard]] bool HasKeyboardFocus() const;
	[[nodiscard]] bool HasMouseFocus() const;
	[[nodiscard]] bool IsCursorLocked() const;

	void DisableAllInput();
	void EnableAllInput();

	bool ToggleLockCursor(Window& window);

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI();
#endif

	TESTABLE()
};