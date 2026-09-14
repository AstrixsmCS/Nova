#include "EditorApplication.hpp"

#include <chrono>
#include <cmath>

#include "Renderer/Renderer.hpp"

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
}

void EditorApplication::OnUpdate()
{
	using Clock = std::chrono::steady_clock;

	static const auto startTime = Clock::now();

	const float seconds = std::chrono::duration<float>(Clock::now() - startTime).count();

	const float pulse = std::sin(seconds * 3.0f) * 0.5f + 0.5f;

	Renderer::ClearColor(0.05f + pulse * 0.45f, 0.02f, 0.15f + pulse * 0.35f);
}

void EditorApplication::OnShutdown()
{
}
