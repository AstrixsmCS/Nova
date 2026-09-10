#include "EditorApplication.hpp"

#include "Core/Log.hpp"

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	m_Renderer = SDL_CreateRenderer(GetWindow().GetNativeWindow(), nullptr);

	if (!SDL_SetRenderVSync(m_Renderer, 1))
		NV_WARN("Failed to enable VSync: {}", SDL_GetError());
}

void EditorApplication::OnUpdate()
{
	int width  = 0;
	int height = 0;

	SDL_GetCurrentRenderOutputSize(m_Renderer, &width, &height);

	const float centerX = static_cast<float>(width) * 0.5f;
	const float centerY = static_cast<float>(height) * 0.5f;
	const float size    = 250.0f;

	const SDL_Vertex vertices[] =
	{
		{
			.position = { centerX, centerY - size },
			.color = { 1.0f, 0.2f, 0.2f, 1.0f },
			.tex_coord = { 0.0f, 0.0f }
		},
		{
			.position = { centerX - size, centerY + size },
			.color = { 0.2f, 1.0f, 0.2f, 1.0f },
			.tex_coord = { 0.0f, 0.0f }
		},
		{
			.position = { centerX + size, centerY + size },
			.color = { 0.2f, 0.4f, 1.0f, 1.0f },
			.tex_coord = { 0.0f, 0.0f }
		}
	};

	SDL_SetRenderDrawColor(m_Renderer, 18, 18, 22, 255);
	SDL_RenderClear(m_Renderer);

	if (!SDL_RenderGeometry(m_Renderer, nullptr, vertices, 3, nullptr, 0))
	{
		NV_ERROR("Failed to render triangle: {}", SDL_GetError());
	}

	SDL_RenderPresent(m_Renderer);
}

void EditorApplication::OnShutdown()
{
	if (m_Renderer)
	{
		SDL_DestroyRenderer(m_Renderer);
		m_Renderer = nullptr;
	}
}
