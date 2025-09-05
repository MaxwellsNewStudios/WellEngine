#pragma once

#include <vector>
#include "Debug/ErrMsg.h"
#ifdef TRACY_REFS
#include "Math/ConstRand.h"
#endif

//#define STRICT_CHECKS

// Forward declaration
template<class T>
class IRefTarget;
template<class T>
class Ref;


// Interface for all referenceable types
template<class T>
class IRefTarget
{
private:
	friend class Ref<T>;
	std::vector<Ref<T> *> _refs;

	inline void AddRef(Ref<T> *ref) noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]: %d", this, _refs.size(), ref);
#endif
#ifdef STRICT_CHECKS
		if (!ref)
		{
			Warn("Trying to add null reference!");
			return;
		}

		if (std::find(_refs.begin(), _refs.end(), ref) != _refs.end())
		{
			Warn("Trying to add duplicate reference!");
			return;
		}

		if (ref->Get() != this)
		{
			Warn("Trying to add a reference that doesn't point to this target!");
			return;
		}
#endif

		_refs.push_back(ref);
	}

	inline void RemoveRef(const Ref<T> *ref) noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]: %d", this, _refs.size(), ref);
#endif
#ifdef STRICT_CHECKS
		if (!ref)
		{
			Warn("Trying to remove null reference!");
			return;
		}

		if (ref->Get() == this)
		{
			Warn("Trying to remove a reference that still points to this target!");
			return;
		}
#endif

		auto it = std::find(_refs.begin(), _refs.end(), ref);
		if (it != _refs.end())
			_refs.erase(it);
	}

protected:
	// Inherit all references from another IRefTarget.
	// Allows for replacing an object without losing existing references.
	void ReplaceTarget(IRefTarget<T> &other) noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]: %d", this, _refs.size(), &other);
#endif
		for (Ref<T> *ref : other._refs)
			ref->_ref = this; // Update reference to point to this new target

		_refs.insert(_refs.end(), other._refs.begin(), other._refs.end());
		other._refs.clear(); // Clear the old target's references
	}

public:
	IRefTarget() noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]", this, _refs.size());
#endif
		static_assert(std::is_base_of_v<IRefTarget<T>, T>);
		_refs.reserve(8); // Reserve space for references to avoid frequent reallocations
	}
	virtual ~IRefTarget() noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]", this, _refs.size());
#endif
		for (Ref<T> *ref : _refs)
		{
			ZoneNamedXNC(dereferenceRefZone, "Dereference Ref", RandomUniqueColor(), true);

			if (!ref)
				continue;

			if (!ref->IsValid())
			{
				Warn("Unhandelled null reference found! References aren't being cleaned properly somewhere in this file.");
				continue;
			}

			ref->TargetDestructed(); // Notify references that the target is being destructed
		}
	}

	[[nodiscard]] inline Ref<T> AsRef(const char *identifier = nullptr) noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]", this, _refs.size());
#endif
		// Avoid wasting time on linking and unlinking the locally constructed reference
#ifndef _DEBUG
		// Rely on return value optimization to do this efficiently
		return Ref<T>(*this, identifier);
#else
		// Without optimizations, use std::move to force move semantics
		return std::move(Ref<T>(*this, identifier));
#endif
	}

	[[nodiscard]] const std::vector<Ref<T> *> &GetRefs() const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		ZoneTextF("(%d) [%d]", this, _refs.size());
#endif
		return _refs; 
	}

	TESTABLE()
};


template<class T>
class Ref
{
private:
	friend class IRefTarget<T>;
	IRefTarget<T> *_ref = nullptr;

	std::vector<std::function<void(Ref<T> *refPtr)>> _destructCallbacks;

#ifdef DEBUG_BUILD
	std::unique_ptr<std::string> _identifier = nullptr;
#endif

protected:
	inline void TargetDestructed()
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		for (auto &callback : _destructCallbacks)
			callback(this);
		_destructCallbacks.clear();
		_ref = nullptr;
	}

public:
	Ref() noexcept = default;
	Ref(std::nullptr_t) noexcept : Ref() 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
	}
	Ref(IRefTarget<T> &ref, const char *identifier = nullptr) noexcept : _ref(&ref)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
#ifdef DEBUG_BUILD
		if (identifier)
			_identifier = std::make_unique<std::string>(identifier);
#endif
		_ref->AddRef(this); 
	}
	Ref(T &ref, const char *identifier = nullptr) noexcept : _ref(&static_cast<IRefTarget<T> &>(ref))
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
#ifdef DEBUG_BUILD
		if (identifier)
			_identifier = std::make_unique<std::string>(identifier);
#endif
		_ref->AddRef(this);
	}

	~Ref() noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("~(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("~(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		if (!_ref)
			return;
		
		if (!this)
			return;

		auto *tempRef = _ref;
		_ref = nullptr;
		tempRef->RemoveRef(this);
	}

	Ref(const Ref<T> &other) noexcept : _ref(other._ref), _destructCallbacks(other._destructCallbacks)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &other);
		else
			ZoneTextF("(%d - %d) [%d] = %d", this, _ref, _destructCallbacks.size(), &other);
#endif
#ifdef DEBUG_BUILD
		if (other._identifier)
			_identifier = std::make_unique<std::string>(*other._identifier);
#endif

		if (!other.IsValid())
			return; // If the other reference is not valid, nothing needs to be done
		_ref->AddRef(this); // Add a reference to the target
	}
	Ref(Ref<T> &&other) noexcept : _ref(other._ref), _destructCallbacks(std::move(other._destructCallbacks))
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = &%d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &other);
		else
			ZoneTextF("(%d - %d) [%d] = &%d", this, _ref, _destructCallbacks.size(), &other);
#endif
#ifdef DEBUG_BUILD
		if (other._identifier)
			_identifier = std::make_unique<std::string>(*other._identifier);
#endif

		other._ref = nullptr;
		other._destructCallbacks.clear();

		if (!_ref)
			return; // If null reference, nothing needs to be done

		_ref->RemoveRef(&other); // Remove the reference target's link to the other reference
		_ref->AddRef(this);
	}

	Ref<T> &operator=(std::nullptr_t)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), nullptr);
		else
			ZoneTextF("(%d - %d) [%d] = %d", this, _ref, _destructCallbacks.size(), nullptr);
#endif
		_destructCallbacks.clear(); // Clear destruct callbacks
		if (!_ref)
			return *this; // If already null, nothing to do
			
		auto *tempRef = _ref;
		_ref = nullptr;
		tempRef->RemoveRef(this); // Remove current reference
		return *this;
	}
	Ref<T> &operator=(const Ref<T> &other)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &other);
		else
			ZoneTextF("(%d - %d) [%d] = %d", this, _ref, _destructCallbacks.size(), &other);
#endif
		if (this == &other)
			return *this; // Self-assignment check

#ifdef DEBUG_BUILD
		if (other._identifier)
			_identifier = std::make_unique<std::string>(*other._identifier);
#endif

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = other._ref; // Copy the reference
		_destructCallbacks = other._destructCallbacks; // Copy destruct callbacks

		if (_ref)
			_ref->AddRef(this); // Add a reference to the target

		return *this;
	}
	Ref<T> &operator=(Ref<T> &other)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &other);
		else
			ZoneTextF("(%d - %d) [%d] = %d", this, _ref, _destructCallbacks.size(), &other);
#endif
		if (this == &other)
			return *this; // Self-assignment check

#ifdef DEBUG_BUILD
		if (other._identifier)
			_identifier = std::make_unique<std::string>(*other._identifier);
#endif

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = other._ref; // Copy the reference
		_destructCallbacks = other._destructCallbacks; // Copy destruct callbacks

		if (_ref)
			_ref->AddRef(this); // Add a reference to the target

		return *this;
	}
	Ref<T> &operator=(Ref<T> &&other) noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = &%d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &other);
		else
			ZoneTextF("(%d - %d) [%d] = &%d", this, _ref, _destructCallbacks.size(), &other);
#endif
		if (this == &other)
			return *this; // Self-assignment check

#ifdef DEBUG_BUILD
		if (other._identifier)
			_identifier = std::make_unique<std::string>(*other._identifier);
#endif

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = other._ref; // Move the reference
		other._ref = nullptr; 

		_destructCallbacks = std::move(other._destructCallbacks); // Move destruct callbacks
		other._destructCallbacks.clear();

		if (_ref)
		{
			_ref->RemoveRef(&other); // Remove targets reference to other 
			_ref->AddRef(this); // Add a reference to the target
		}

		return *this;
	}
	Ref<T> &operator=(T &ref)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &ref);
		else
			ZoneTextF("(%d - %d) [%d] = %d", this, _ref, _destructCallbacks.size(), &ref);
#endif
		if (_ref == &ref)
			return *this; // Self-assignment check

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = &ref; // Get the reference target
		if (_ref)
			_ref->AddRef(this); // Add this reference to the target

		return *this;
	}
	Ref<T> &operator=(T *ref)
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] = %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), ref);
		else
			ZoneTextF("(%d - %d) [%d] = %d", this, _ref, _destructCallbacks.size(), ref);
#endif
		if (_ref == ref)
			return *this; // Self-assignment check

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = ref; // Get the reference target
		if (ref)
			ref->IRefTarget<T>::AddRef(this); // Add this reference to the target

		return *this;
	}

	bool operator==(const Ref<T> &other) const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] == %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &other);
		else
			ZoneTextF("(%d - %d) [%d] == %d", this, _ref, _destructCallbacks.size(), &other);
#endif
		return this == &other;
	}
	bool operator==(const IRefTarget<T> &refTarget) const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] == %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &refTarget);
		else
			ZoneTextF("(%d - %d) [%d] == %d", this, _ref, _destructCallbacks.size(), &refTarget);
#endif
		return _ref == &refTarget;
	}
	bool operator==(const IRefTarget<T> *refTarget) const noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] == %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), refTarget);
		else
			ZoneTextF("(%d - %d) [%d] == %d", this, _ref, _destructCallbacks.size(), refTarget);
#endif
		return _ref == refTarget;
	}
	bool operator==(const T &refTarget) const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] == %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &refTarget);
		else
			ZoneTextF("(%d - %d) [%d] == %d", this, _ref, _destructCallbacks.size(), &refTarget);
#endif
		return _ref == &refTarget;
	}
	bool operator==(const T *refTarget) const noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d] == %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), refTarget);
		else
			ZoneTextF("(%d - %d) [%d] == %d", this, _ref, _destructCallbacks.size(), refTarget);
#endif
		return _ref == refTarget;
	}

	[[nodiscard]] explicit operator bool() const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		return (bool)(_ref != nullptr);
	}

	[[nodiscard]] inline void Set(IRefTarget<T> *ref, const char *identifier = nullptr) noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]: %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), ref);
		else
			ZoneTextF("(%d - %d) [%d]: %d", this, _ref, _destructCallbacks.size(), ref);
#endif
#ifdef DEBUG_BUILD
		if (identifier)
			_identifier = std::make_unique<std::string>(identifier);
#endif

		if (_ref == ref)
			return;

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = ref;
		if (ref)
			ref->AddRef(this);
	}
	[[nodiscard]] inline void Set(IRefTarget<T> &ref, const char *identifier = nullptr) noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]: %d", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), &ref);
		else
			ZoneTextF("(%d - %d) [%d]: %d", this, _ref, _destructCallbacks.size(), &ref);
#endif
#ifdef DEBUG_BUILD
		if (identifier)
			_identifier = std::make_unique<std::string>(identifier);
#endif

		if (_ref == &ref)
			return;

		if (_ref)
		{
			auto *tempRef = _ref;
			_ref = nullptr;
			tempRef->RemoveRef(this); // Remove current reference
		}
		_destructCallbacks.clear(); // Clear destruct callbacks

		_ref = &ref;
		ref.AddRef(this);
	}

	[[nodiscard]] inline T *Get() const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		return dynamic_cast<T *>(_ref); 
	}
	[[nodiscard]] inline bool TryGet(T *&out) const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		out = Get(); 
		return out != nullptr;
	}
	[[nodiscard]] inline bool IsValid() const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		return (bool)*this; 
	}

	template<class C>
	[[nodiscard]] inline C *GetAs() const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		static_assert(std::is_base_of_v<T, C>);
		return dynamic_cast<C *>(_ref); 
	}
	template<class C>
	[[nodiscard]] inline bool TryGetAs(C *&out) const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		static_assert(std::is_base_of_v<T, C>);
		out = GetAs<C>(); 
		return (bool)out; 
	}

	void AddDestructCallback(std::function<void(Ref<T> *refPtr)> callback) noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		if (!_ref)
			return;
		_destructCallbacks.push_back(std::move(callback));
	}

#ifdef DEBUG_BUILD
	[[nodiscard]] inline void SetIdentifier(const std::string &identifier) noexcept
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]: %s", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size(), identifier.c_str());
		else
			ZoneTextF("(%d - %d) [%d]: %s", this, _ref, _destructCallbacks.size(), identifier.c_str());
#endif
		_identifier = std::make_unique<std::string>(identifier);
	}
	[[nodiscard]] inline const std::string *GetIdentifier() const noexcept 
	{
#ifdef TRACY_REFS
		ZoneScopedC(RandomUniqueColor());
		if (_identifier)
			ZoneTextF("(%d - %d - %s) [%d]", this, _ref, _identifier.get()->c_str(), _destructCallbacks.size());
		else
			ZoneTextF("(%d - %d) [%d]", this, _ref, _destructCallbacks.size());
#endif
		return _identifier.get(); 
	}
#endif

	TESTABLE()
};
