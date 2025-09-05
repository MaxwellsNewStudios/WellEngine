#pragma once

#include "Rendering/RendererInfo.h"
#include "Behaviours/CameraBehaviour.h"
#include "Behaviours/CameraCubeBehaviour.h"

class RenderQueuer
{
public:
	virtual void QueueGeometry(const RenderQueueEntry &entry) const = 0;
	virtual void QueueTransparent(const RenderQueueEntry &entry) const = 0;
};

class CamRenderQueuer final : public RenderQueuer
{
private:
	CameraBehaviour *_cameraBehaviour = nullptr;

public:
	CamRenderQueuer(CameraBehaviour *cameraBehaviour)
	{
		_cameraBehaviour = cameraBehaviour;
	}

	void QueueGeometry(const RenderQueueEntry &entry) const override
	{
		_cameraBehaviour->QueueGeometry(entry);
	}
	void QueueTransparent(const RenderQueueEntry &entry) const override
	{
		_cameraBehaviour->QueueTransparent(entry);
	}

	TESTABLE()
};

class CubeRenderQueuer final : public RenderQueuer
{
private:
	CameraCubeBehaviour *_cameraCubeBehaviour = nullptr;

public:
	CubeRenderQueuer(CameraCubeBehaviour *cameraCubeBehaviour)
	{
		_cameraCubeBehaviour = cameraCubeBehaviour;
	}

	void QueueGeometry(const RenderQueueEntry &entry) const override
	{
		_cameraCubeBehaviour->QueueGeometry(entry);
	}
	void QueueTransparent(const RenderQueueEntry &entry) const override
	{
		_cameraCubeBehaviour->QueueTransparent(entry);
	}

	TESTABLE()
};
