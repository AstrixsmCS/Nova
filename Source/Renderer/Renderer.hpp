#pragma once

#include <SDL3/SDL.h>

// Temporary SDL renderer. Replaced entirely when Vulkan is added.
class Renderer
{
public:
	bool Initialize(SDL_Window* window);
	void Shutdown();

	void BeginFrame();
	void DrawTestTriangle(); // TODO: remove when Vulkan is added
	void EndFrame();

private:
	SDL_Renderer* m_Renderer = nullptr;
};
