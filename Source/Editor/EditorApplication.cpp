#include "EditorApplication.hpp"

#include "Core/Log.hpp"
#include "Asset/AssetManager.hpp"

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	m_Renderer.Initialize(GetWindow().GetNativeWindow());
}

void EditorApplication::OnUpdate()
{
	m_Renderer.BeginFrame();
	m_Renderer.DrawTestTriangle();
	m_Renderer.EndFrame();
}

void EditorApplication::OnShutdown()
{
	m_Renderer.Shutdown();
}
