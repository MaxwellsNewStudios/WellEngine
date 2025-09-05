#pragma once

#include <map>
#include "Input/Input.h"
#include "Timing/TimeUtils.h"

namespace InputBindings
{
	enum class InputAction
	{
		// NULL
		None,

		// General
		Exit,
		LockCursor,
		Fullscreen,
		CopySelected,
		AdditiveSelect,
		Save,
		Load,
		Pause,

		// Player movement
		WalkForward,
		WalkBackward,
		StrafeLeft,
		StrafeRight,
		Sprint,
		Sneak,

		// Player actions
		Flashlight,
		Interact,
		ShowPicture,
		PlaceBreadcrumb,

		// Scene Editor
		Delete,
		CreateEntity,
		Select,

		CycleTransform,
		SetTransformTranslate,
		SetTransformRotate,
		SetTransformScale,


		// Used for counting the number of actions. DO NOT USE. KEEP AS LAST ELEMENT!
		LAST
	};

	enum class InputBindingStepType
	{
		None, // NULL

		Simple,
		Sustained,


		// Used for counting the number of binding step types. DO NOT USE. KEEP AS LAST ELEMENT!
		LAST
	};

	enum class InputBindingType
	{
		None, // NULL

		Single,
		Exclusive,
		Combo,
		Sequential,


		// Used for counting the number of binding types. DO NOT USE. KEEP AS LAST ELEMENT!
		LAST
	};


	const std::unordered_map<std::string, InputAction> InputActionNames = {
		{ "NULL",						InputAction::None				    },
		{ "Exit",						InputAction::Exit				    },
		{ "Lock Cursor",				InputAction::LockCursor			    },
		{ "Fullscreen",					InputAction::Fullscreen			    },
		{ "Copy Selected",				InputAction::CopySelected		    },
		{ "Additive Select",			InputAction::AdditiveSelect			},
		{ "Save",						InputAction::Save				    },
		{ "Load",						InputAction::Load				    },
		{ "Pause",						InputAction::Pause				    },
		{ "Move Forward",				InputAction::WalkForward		    },
		{ "Move Backward",				InputAction::WalkBackward		    },
		{ "Move Left",					InputAction::StrafeLeft			    },
		{ "Move Right",					InputAction::StrafeRight		    },
		{ "Sprint",						InputAction::Sprint				    },
		{ "Sneak",						InputAction::Sneak				    },
		{ "Flashlight",					InputAction::Flashlight			    },
		{ "Interact",					InputAction::Interact			    },
		{ "Show Picture",				InputAction::ShowPicture		    },
		{ "Place Breadcrumb",			InputAction::PlaceBreadcrumb	    },
		{ "Delete Entity",				InputAction::Delete				    },
		{ "Create Entity",				InputAction::CreateEntity		    },
		{ "Select",						InputAction::Select				    },
		{ "Cycle Transform Tools",		InputAction::CycleTransform		    },
		{ "Translate Tool",				InputAction::SetTransformTranslate  },
		{ "Rotate Tool",				InputAction::SetTransformRotate	    },
		{ "Scale Tool",					InputAction::SetTransformScale	    }
	};

	const std::unordered_map<std::string, InputBindingStepType> InputBindingStepNames = {
		{ "NULL",						InputBindingStepType::None			},
		{ "Simple",						InputBindingStepType::Simple	    },
		{ "Sustained",					InputBindingStepType::Sustained	    }
	};

	const std::unordered_map<std::string, InputBindingType> InputBindingNames = {
		{ "NULL",						InputBindingType::None				},
		{ "Single",						InputBindingType::Single			},
		{ "Exclusive",					InputBindingType::Exclusive			},
		{ "Combo",						InputBindingType::Combo				},
		{ "Sequential",					InputBindingType::Sequential	    },
	};


	class InputBindingStep
	{
	protected:
		KeyCode _key = KeyCode::None;
		bool _ignoreAbsorb = false;

		float _timeToDeactivate = 0.0f;
		float _deactivationTimer = 0.0f;

	public:
		InputBindingStep() = default;
		~InputBindingStep() = default;

		virtual void Update() {};
		virtual bool IsTriggered() { return false; };
		virtual void Reset()
		{
			_deactivationTimer = 0.0f;
		};

		TESTABLE()
	};

	// Active if the key has the given state.
	class InputBindingStepSimple : public InputBindingStep
	{
	private:
		KeyState _state = KeyState::Pressed;

	public:
		InputBindingStepSimple(KeyCode key, KeyState state, float timeToDeactivate = -1.0f, bool ignoreAbsorb = false)
		{
			_key = key;
			_state = state;
			_timeToDeactivate = timeToDeactivate;
			_ignoreAbsorb = ignoreAbsorb;
		};

		void Update() override
		{
			if (_timeToDeactivate <= 0.0f)
			{
				return;
			}

			if (Input::Instance().GetKey(_key, _ignoreAbsorb) == _state)
			{
				_deactivationTimer = _timeToDeactivate;
			}
			else
			{
				_deactivationTimer -= TimeUtils::GetInstance().GetRealDeltaTime();
			}
		};

		bool IsTriggered() override
		{
			if (_timeToDeactivate <= 0.0f)
			{
				return Input::Instance().GetKey(_key, _ignoreAbsorb) == _state;
			}

			return _deactivationTimer > 0.0f;
		};

		TESTABLE()
	};
	
	// Active if the key has been held for a certain amount of time.
	class InputBindingStepSustained : public InputBindingStep
	{
	private:
		float _timeToActivate = 0.0f;
		float _activationTimer = 0.0f;

	public:
		InputBindingStepSustained(KeyCode key, float timeToActivate, float timeToDeactivate = -1.0f, bool ignoreAbsorb = false)
		{
			_key = key;
			_timeToActivate = timeToActivate;
			_timeToDeactivate = timeToDeactivate;
			_ignoreAbsorb = ignoreAbsorb;
		};

		void Update() override
		{
			if (Input::Instance().GetKey(_key, _ignoreAbsorb) == KeyState::Held)
			{
				_activationTimer += TimeUtils::GetInstance().GetRealDeltaTime();
			}
			else
			{
				_activationTimer = 0.0f;
			}

			if (_activationTimer >= _timeToActivate)
			{
				_deactivationTimer = _timeToDeactivate;
			}
			else if (_timeToDeactivate > 0.0f)
			{
				_deactivationTimer -= TimeUtils::GetInstance().GetRealDeltaTime();
			}
		};

		bool IsTriggered() override
		{
			if (_timeToDeactivate <= 0.0f)
			{
				return _activationTimer >= _timeToActivate;
			}

			return _deactivationTimer > 0.0f;
		};

		void Reset() override
		{
			this->InputBindingStep::Reset();
			_activationTimer = 0.0f;
		};

		TESTABLE()
	};


	class InputBinding
	{
	protected:
		std::vector<std::unique_ptr<InputBindingStep>> _steps;
		bool _resetOnTrigger = false;

	public:
		InputBinding() = default;
		~InputBinding() = default;

		virtual inline void AddStep(std::unique_ptr<InputBindingStep> step)
		{
			_steps.emplace_back(std::move(step));
		};
		virtual inline void AddSteps(std::unique_ptr<InputBindingStep> *steps, int count)
		{
			for (int i = 0; i < count; i++)
			{
				_steps.emplace_back(std::move(steps[i]));
			}
		};

		virtual void Update() 
		{
			for (auto &step : _steps)
			{
				step->Update();
			}
		};
		virtual bool IsTriggered() { return false; };
		virtual void Reset() 
		{
			for (auto &step : _steps)
			{
				step->Reset();
			}
		};

		TESTABLE()
	};
	
	/// Active if any binding step is active.
	class InputBindingSingle : public InputBinding
	{
	private:

	public:
		InputBindingSingle() = default;
		~InputBindingSingle() = default;

		InputBindingSingle(KeyCode code, KeyState state, bool resetOnTrigger = false, float deactivationTime = -1.0f, bool ignoreAbsorb = false)
		{
			_resetOnTrigger = resetOnTrigger;
			_steps.emplace_back(std::move(std::make_unique<InputBindingStepSimple>(code, state, deactivationTime, ignoreAbsorb)));
		};
		InputBindingSingle(std::unique_ptr<InputBindingStep> *steps, int count, bool resetOnTrigger = false)
		{
			_resetOnTrigger = resetOnTrigger;
			AddSteps(steps, count);
		};

		bool IsTriggered() override
		{
			if (_steps.size() == 0)
				return false;

			for (auto &step : _steps)
			{
				if (step->IsTriggered())
				{
					if (_resetOnTrigger)
					{
						Reset();
					}

					return true;
				}
			}

			return false;
		};

		TESTABLE()
	};
	
	/// Active if any binding step is active and no disallowed steps are active.
	class InputBindingExclusive : public InputBinding
	{
	private:
		std::vector<std::unique_ptr<InputBindingStep>> _disallowedSteps;

	public:
		InputBindingExclusive() = default;
		~InputBindingExclusive() = default;

		InputBindingExclusive(std::unique_ptr<InputBindingStep> *steps, int count, bool resetOnTrigger = false)
		{
			_resetOnTrigger = resetOnTrigger;
			AddSteps(steps, count);
		};
		InputBindingExclusive(std::unique_ptr<InputBindingStep> *steps, int count, 
			std::unique_ptr<InputBindingStep> *disallowedSteps, int disallowedCount, bool resetOnTrigger = false)
		{
			_resetOnTrigger = resetOnTrigger;
			AddSteps(steps, count);
			AddDisallowedSteps(disallowedSteps, disallowedCount);
		};

		inline void AddDisallowedStep(std::unique_ptr<InputBindingStep> step)
		{
			_disallowedSteps.emplace_back(std::move(step));
		};
		inline void AddDisallowedSteps(std::unique_ptr<InputBindingStep> *steps, int count)
		{
			for (int i = 0; i < count; i++)
			{
				_disallowedSteps.emplace_back(std::move(steps[i]));
			}
		};

		void Update() override
		{
			this->InputBinding::Update();

			for (auto &disallowedStep : _disallowedSteps)
			{
				disallowedStep->Update();
			}
		};

		bool IsTriggered() override
		{
			if (_steps.size() == 0)
				return false;

			for (auto &step : _steps)
			{
				if (step->IsTriggered())
				{
					for (auto &disallowedStep : _disallowedSteps)
					{
						if (disallowedStep->IsTriggered())
							return false;
					}

					if (_resetOnTrigger)
					{
						Reset();
					}

					return true;
				}
			}

			return false;
		};

		void Reset() override
		{
			this->InputBinding::Reset();

			for (auto &disallowedStep : _disallowedSteps)
			{
				disallowedStep->Reset();
			}
		};

		TESTABLE()
	};

	/// Active if all binding steps are active at once.
	class InputBindingCombo : public InputBinding
	{
	private:

	public:
		InputBindingCombo() = default;
		~InputBindingCombo() = default;
		InputBindingCombo(const InputBindingCombo &other) = default;

		InputBindingCombo(std::unique_ptr<InputBindingStep> *steps, int count, bool resetOnTrigger = false)
		{
			_resetOnTrigger = resetOnTrigger;
			AddSteps(steps, count);
		};

		bool IsTriggered() override
		{
			if (_steps.size() == 0)
				return false;

			for (auto &step : _steps)
			{
				if (!step->IsTriggered())
					return false;
			}

			if (_resetOnTrigger)
			{
				Reset();
			}

			return true;
		};

		TESTABLE()
	};

	/// Active if all binding steps were activated in sequence.
	class InputBindingSequential : public InputBinding
	{
	private:
		float _timeToRestart = 0.0f; // Restart the sequence if no input is received for this amount of time.
		float _restartTimer = 0.0f;
		int _currentStep = 0;

	public:
		InputBindingSequential() = default;
		~InputBindingSequential() = default;

		InputBindingSequential(std::unique_ptr<InputBindingStep> *steps, int count, float timeToRestart, bool resetOnTrigger = false)
		{
			_resetOnTrigger = resetOnTrigger;
			_timeToRestart = timeToRestart;
			AddSteps(steps, count);
		};

		void Update() override
		{
			if (_steps.size() == 0)
				return;

			this->InputBinding::Update();

			if (_currentStep < _steps.size())
			{
				if (_steps[_currentStep]->IsTriggered())
				{
					_currentStep++;
					_restartTimer = _timeToRestart;
					return;
				}
			}

			if (_timeToRestart <= 0.0f)
				return;

			if (_restartTimer > 0.0f)
			{
				_restartTimer -= TimeUtils::GetInstance().GetRealDeltaTime();

				if (_restartTimer <= 0.0f)
				{
					Reset();
				}
			}
		};

		bool IsTriggered() override
		{
			if (_steps.size() == 0)
				return false;

			if (_currentStep < _steps.size())
				return false;

			if (_resetOnTrigger)
			{
				Reset();
			}

			return true;
		};

		void Reset() override
		{
			this->InputBinding::Reset();
			_currentStep = 0;
			_restartTimer = 0.0f;
		};

		TESTABLE()
	};
}


/// Singleton class for handling input bindings. Stores all input bindings for the game.
/// 
/// How it works:
/// Every action that can be performed in the game is defined as an InputAction.
/// Each InputAction is either unbound or bound to an InputBinding.
/// InputActions are triggered when their InputBinding is triggered.
/// An InputBinding represents a string of keypresses and has a vector of InputBindingSteps.
/// An InputBindingStep represents a single key and how to press it.
/// 
/// Ex:
/// I want [Shift] + [F5] to trigger a quicksave.
/// To set this up, I create an InputBindingCombo with two InputBindingSteps.
/// The first is an InputBindingStepSimple, with KeyCode::LeftShift and KeyState::Held.
/// The second is an InputBindingStepSimple, with KeyCode::F5 and KeyState::Pressed.
class BindingCollection
{
private:
	std::map<InputBindings::InputAction, std::unique_ptr<InputBindings::InputBinding>> _inputs;

public:
	BindingCollection() = default;
	~BindingCollection() = default;

	static inline BindingCollection *GetInstance()
	{
		static BindingCollection instance;
		return &instance;
	}

	void SaveBindings(const std::string &path);
	void LoadBindings(const std::string &path);

	static inline void SetBinding(InputBindings::InputAction action, std::unique_ptr<InputBindings::InputBinding> binding)
	{
		auto instance = GetInstance();
		instance->_inputs.try_emplace(action, std::move(binding));
	}

	static inline void Update()
	{
		auto instance = GetInstance();

		for (auto &input : instance->_inputs)
		{
			input.second->Update();
		}
	}

	static inline bool IsTriggered(InputBindings::InputAction action)
	{
		auto instance = GetInstance();
		auto input = instance->_inputs.find(action);

		if (input != instance->_inputs.end())
		{
			return input->second->IsTriggered();
		}

		// Action is unbound. Always return false.
		return false;
	}

	TESTABLE()
};
