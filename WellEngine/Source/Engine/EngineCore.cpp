#include "stdafx.h"
#include "EngineCore.h"
#include "Engine/Debug/DebugData.h"
#include "Game/Behaviours/Rendering/Mesh/B_Mesh.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

EngineCore::EngineCore()
{
	ZoneScopedC(RandomUniqueColor());
	DbgMsg("========| Start |==========================================================================");
}

EngineCore::~EngineCore()
{
	ZoneScopedC(RandomUniqueColor());
	DbgMsg("========| Close |==========================================================================\n");
}

int EngineCore::Init()
{
	DbgMsg("========| Initialization |=================================================================");

	// Seed random number generator
	srand(static_cast<unsigned>(time(0)));

#ifdef TRACY_ENABLE
	DbgMsg("Tracy Profiler detected! Wait to see if a connection is made...");

	SDL_Delay(500);

	if (TracyIsConnected)
	{
		DbgMsg("Tracy Profiler connected!");

		// Assign names to OMP threads
		constexpr int threadCount = (PARALLEL_THREADS < 4) ? 4 : PARALLEL_THREADS;
#pragma omp parallel for num_threads(threadCount)
		for (int i = 0; i < threadCount; i++)
		{
			if (i == 0)
			{
				TracySetThreadNameWithHint("Main Thread", 123456789);
			}
			else
			{
				TracySetThreadNameWithHint("OpenMP Thread", 983464687); // Magic number, who cares
			}
		}
	}
#endif

	ZoneScopedC(RandomUniqueColor());

#ifdef DEBUG_BUILD
	DbgMsg("Loading Debug Data..."); LogIndentIncr();
	DebugData::LoadState();
	LogIndentDecr();
#endif

	TimeUtils &time = TimeUtils::GetInstance();

	DbgMsg("Input Setup..."); LogIndentIncr();
	Input &input = Input::Instance();
#ifdef DEBUG_BUILD
	DebugData &debugData = DebugData::Get();
	dx::XMINT2 wndSize = { debugData.windowSizeX, debugData.windowSizeY };
	input.SetWindowSize({ (UINT)wndSize.x, (UINT)wndSize.y });
#else
	dx::XMINT2 wndSize = { WINDOW_WIDTH, WINDOW_HEIGHT };
	input.SetWindowSize({ (UINT)wndSize.x, (UINT)wndSize.y });
#endif
	LogIndentDecr();

	DbgMsg("Bindings Setup..."); LogIndentIncr();
	BindingCollection *inputBindings = BindingCollection::GetInstance();
	inputBindings->LoadBindings(INTERNAL_FILE_BINDINGS);
	LogIndentDecr();

	DbgMsg("Window Setup..."); LogIndentIncr();
	// Setup of window and input
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		ErrMsg("Failed to CoInitializeEx!");
		return 0;
	}
	LogIndentDecr();

	DbgMsg("Window Initialization..."); LogIndentIncr();
	Window window{};
	std::string windowName = GAME_TITLE;
#ifndef _DEPLOY
	windowName = "Well Engine " ENGINE_VERSION;
#endif
	if (!window.Initialize(windowName, wndSize))
	{
		ErrMsg("Failed to setup window!");
		return -1;
	}
	LogIndentDecr();

#ifdef DEBUG_BUILD
	// Register window handle for message boxes
	MsgLogger::SetHWnd(window.GetHWND());
#endif

	DbgMsg("Game Setup..."); LogIndentIncr();
	// Setup of game. Loads all assets into memory like meshes, textures, shaders etc. Also creates the graphics manager.
	if (!_game.Setup(time, std::move(window)))
	{
		ErrMsg("Failed to setup game!");
		return -1;
	}
	LogIndentDecr();
	DbgMsgF("Loading content took {} s", time.CompareSnapshots("LoadContent"));
	DbgMsgF("Loading Systems took {} s", time.CompareSnapshots("SetupSystems"));
	DbgMsgF("Loading Scenes took {} s", time.CompareSnapshots("AddScenes"));

	_game.GetWindow().UpdateWindowSize();
#ifdef DEBUG_BUILD
	_game.GetWindow().SetFullscreen(debugData.windowFullscreen);
#endif

	return 0;
}

int EngineCore::Run()
{
	ZoneScopedC(RandomUniqueColor());
	DbgMsg("========| Game Loop |======================================================================");
	FrameMark;

	// Tracy Plot Configuration
	{
		TracyPlotConfig("Frame Time (ns)", tracy::PlotFormatType::Number, false, true, 0xBBFF11);
		TracyPlotConfig("View Cull Count", tracy::PlotFormatType::Number, true, true, 0xFF5533);
	}

	TimeUtils &time = TimeUtils::GetInstance();
	Input &input = Input::Instance();
	Window &window = _game.GetWindow();
	int returnCode = 0;

	while (!_game.IsExiting())
	{
		_frameCount++;
		ZoneNamedNC(tracyFrameZone, "Frame", RandomUniqueColor(), true);
		ZoneValueXV(tracyFrameZone, _frameCount);

#pragma omp parallel for num_threads(PARALLEL_THREADS)
		for (int i = 0; i < PARALLEL_THREADS; i++)
		{
			if (i == 0)
			{
				TracySetThreadNameWithHint("Main Thread", 123456789);
			}
			else
			{
				TracySetThreadNameWithHint("OpenMP Thread", 983464687); // Magic number, who cares
			}
		}

		// Update time
		time.Update();

		TracyPlot("Frame Time (ns)", (int64_t)(time.GetDeltaTime() * 1000000.0f));

		DebugData::Update(time.GetDeltaTime());

		// SDL poll events
		{
			ZoneNamedXNC(pollSDLEventsZone, "SDL Poll Events", RandomUniqueColor(), true);

			input.SetMouseScroll({ 0, 0 });
			window.MarkDirty(false);

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
#ifdef USE_IMGUI
				ImGui_ImplSDL3_ProcessEvent(&event);
#endif

				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					_game.Exit();
					break;

				case SDL_EVENT_WINDOW_RESIZED:
					DbgMsg("Window Resized.");
					window.UpdateWindowSize();

					input.SetWindowSize({
						(UINT)window.GetPhysicalWidth(),
						(UINT)window.GetPhysicalHeight()
						});
					break;

				case SDL_EVENT_MOUSE_WHEEL: // Scroll wheel event because it couldn't be fetched from Input's update.
					input.SetMouseScroll({ event.wheel.x, event.wheel.y });
					break;

				default:
					break;
				}
			}
		}

		// Toggle fullscreen (default [Left Control] + [Enter])
		if (BindingCollection::IsTriggered(InputBindings::InputAction::Fullscreen))
		{
			if (!window.ToggleFullscreen())
			{
				ErrMsg("Failed to toggle fullscreen!");
			}
		}

		if (BindingCollection::IsTriggered(InputBindings::InputAction::Exit))
			_game.Exit();

		// Lock cursor to window (default [Left Control] + [Tab])
		if (BindingCollection::IsTriggered(InputBindings::InputAction::LockCursor))
			input.ToggleLockCursor(window);

		if (!input.Update(window))
		{
			ErrMsg("Failed to update input!");
			returnCode = -1;
			_game.Exit();
			continue;
		}

		// Update binding collections
		{
			ZoneNamedXNC(updateBindingsZone, "Binding Collection Update", RandomUniqueColor(), true);
			BindingCollection::Update();
		}

		if (!_game.Update(time, input))
		{
			ErrMsg("Failed to update game logic!");
			returnCode = -1;
			_game.Exit();
			continue;
		}

		if (!_game.Render(time, input))
		{
			ErrMsg("Failed to render frame!");
			returnCode = -1;
			_game.Exit();
			continue;
		}

		FrameMark;
	}

	return returnCode;
}
