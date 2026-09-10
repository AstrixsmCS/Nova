#pragma once

#include "Core/Application.hpp"

#include <SDL3/SDL.h>

class EditorApplication final : public Application
{
public:
	explicit EditorApplication(const ApplicationSpecification& specification);
	~EditorApplication();

protected:
	void OnInitialize() override;
	void OnUpdate() override;
	void OnShutdown() override;
private:
	SDL_Renderer* m_Renderer = nullptr; //TODO: temp until I add vulkan
};
