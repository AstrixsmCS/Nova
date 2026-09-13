#include "Renderer.hpp"

#include "Core/Log.hpp"

bool Renderer::Initialize(SDL_Window* window)
{
	m_Renderer = SDL_CreateRenderer(window, nullptr);
	if (!m_Renderer)
	{
		NV_ERROR("Renderer: failed to create SDL renderer: {}", SDL_GetError());
		return false;
	}

	if (!SDL_SetRenderVSync(m_Renderer, 1))
		NV_WARN("Renderer: failed to enable VSync: {}", SDL_GetError());

	NV_TRACE("Renderer initialized (SDL)");
	return true;
}

void Renderer::Shutdown()
{
	if (m_Renderer)
	{
		SDL_DestroyRenderer(m_Renderer);
		m_Renderer = nullptr;
		NV_TRACE("Renderer shut down.");
	}
}

void Renderer::BeginFrame()
{
	SDL_SetRenderDrawColor(m_Renderer, 18, 18, 22, 255);
	SDL_RenderClear(m_Renderer);
}

void Renderer::DrawTestTriangle()
{
	int width  = 0;
	int height = 0;
	SDL_GetCurrentRenderOutputSize(m_Renderer, &width, &height);

	const float centerX = static_cast<float>(width)  * 0.5f;
	const float centerY = static_cast<float>(height) * 0.5f;
	const float size    = 250.0f;

	const SDL_Vertex vertices[] =
	{
		{
			.position  = { centerX, centerY - size },
			.color     = { 1.0f, 0.2f, 0.2f, 1.0f },
			.tex_coord = { 0.0f, 0.0f }
		},
		{
			.position  = { centerX - size, centerY + size },
			.color     = { 0.2f, 1.0f, 0.2f, 1.0f },
			.tex_coord = { 0.0f, 0.0f }
		},
		{
			.position  = { centerX + size, centerY + size },
			.color     = { 0.2f, 0.4f, 1.0f, 1.0f },
			.tex_coord = { 0.0f, 0.0f }
		}
	};

	if (!SDL_RenderGeometry(m_Renderer, nullptr, vertices, 3, nullptr, 0))
		NV_ERROR("Renderer: DrawTestTriangle failed: {}", SDL_GetError());
}

void Renderer::EndFrame()
{
	SDL_RenderPresent(m_Renderer);
}
