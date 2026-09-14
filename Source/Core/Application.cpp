#include "Application.hpp"

#include "Log.hpp"
#include "Version.hpp"

#include "Events/EventBus.hpp"
#include "Asset/AssetManager.hpp"

#include "Renderer/Renderer.hpp"

Application* Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification &specification)
	: m_Specification(specification)
{
	s_Instance = this;

	Log::Initialize();

	NV_TRACE("Nova Engine {}", NV_VERSION);
	NV_TRACE("Initializing...");

	m_Specification.Window.Title = m_Specification.Name;
	m_Window = Window::Create(m_Specification.Window);

	if (m_Specification.Window.Mode == WindowMode::Windowed)
		m_Window->CenterWindow();

	Renderer::Initialize(m_Window->GetNativeWindow());

	AssetManager::Initialize("Assets");

	EventBus::Subscribe<WindowResizeEvent>([this](WindowResizeEvent& e) { e.m_Handled |= OnWindowResize(e); });
	EventBus::Subscribe<WindowMinimizeEvent>([this](WindowMinimizeEvent& e) { e.m_Handled |= OnWindowMinimize(e); });
	EventBus::Subscribe<WindowCloseEvent>([this](WindowCloseEvent& e) { e.m_Handled |= OnWindowClose(e); });
}

Application::~Application()
{
	NV_TRACE("Shutting down...");

	EventBus::Clear();

	AssetManager::Shutdown();
	Renderer::Shutdown();

	m_Window.reset();

	Log::Shutdown();
}

void Application::Run()
{
	OnInitialize();
	while(m_Running)
	{
		ProcessEvents();

		if (!m_Minimized)
		{
			if (Renderer::BeginFrame())
			{
				OnUpdate();
				Renderer::EndFrame();
				Renderer::Present();
			}
		}
	}
	OnShutdown();
}

void Application::Close()
{
	m_Running = false;
}

void Application::ProcessEvents()
{
	m_Window->ProcessEvents();

	EventBus::Process();
}

bool Application::OnWindowResize(WindowResizeEvent& e)
{
	if (e.GetWidth() == 0 || e.GetHeight() == 0)
		return false;

	Renderer::GetSwapChain().RequestResize();

	return false;
}

bool Application::OnWindowMinimize(WindowMinimizeEvent& e)
{
	m_Minimized = e.IsMinimized();
	return false;
}

bool Application::OnWindowClose(WindowCloseEvent& e)
{
	Close();
	return false; // Allow remaining subscribers to react to the close event.
}

const char* Application::GetConfigurationName()
{
	return NV_BUILD_CONFIG_NAME;
}

const char* Application::GetPlatformName()
{
	return NV_BUILD_PLATFORM_NAME;
}
