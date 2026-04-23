#pragma once

#include "RendererInfo.h"
#include "Engine/Utils/ReferenceHelper.h"

namespace WellEngine
{
	class Behaviour;

	struct RenderInstance
	{
		Behaviour *subject;
		size_t subjectSize;
	};

	struct RenderQueueEntry
	{
		ResourceGroup resourceGroup;
		RenderInstance instance;

		RenderQueueEntry(const ResourceGroup &resourceGroup, RenderInstance instance) : resourceGroup(resourceGroup), instance(instance) {}
		RenderQueueEntry(const RenderQueueEntry &other) : resourceGroup(other.resourceGroup), instance(other.instance) {}

		bool operator==(const RenderQueueEntry &other) const
		{
			return resourceGroup == other.resourceGroup;
		}
		bool operator<(const RenderQueueEntry &other) const
		{
			return resourceGroup < other.resourceGroup;
		}
		bool operator>(const RenderQueueEntry &other) const
		{
			return !(resourceGroup < other.resourceGroup || resourceGroup == other.resourceGroup);
		}
	};


	// Abstract class for behaviours that queue geometry for rendering, eg. cameras
	class RenderQueuer : public IRefTarget<RenderQueuer>
	{
	public:
		virtual void QueueGeometry(const RenderQueueEntry &entry) = 0;
		virtual void QueueTransparent(const RenderQueueEntry &entry) = 0;
		virtual void ResetRenderQueue() = 0;

		[[nodiscard]] virtual const std::vector<RenderQueueEntry> &GetGeometryQueue() const = 0;
		[[nodiscard]] virtual const std::vector<RenderQueueEntry> &GetTransparentQueue() const = 0;

		TESTABLE
	};
}
