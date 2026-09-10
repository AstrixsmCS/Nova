#include "Application.hpp"

#include "Log.hpp"
#include "Version.hpp"

#include "Events/EventBus.hpp"

Application::Application(const ApplicationSpecification &specification)
	: m_Specification(specification)
{
	Log::Initialize();

	NV_TRACE("Nova Engine {}", NV_VERSION);
	NV_TRACE("Initializing...");

	m_Specification.Window.Title = m_Specification.Name;
	m_Window = Window::Create(m_Specification.Window);

	if (m_Specification.Window.Mode == WindowMode::Windowed)
		m_Window->CenterWindow();

	EventBus::Subscribe<WindowResizeEvent>([this](WindowResizeEvent& e)
	{
		e.m_Handled |= OnWindowResize(e);
	});

	EventBus::Subscribe<WindowMinimizeEvent>([this](WindowMinimizeEvent& e)
	{
		e.m_Handled |= OnWindowMinimize(e);
	});

	EventBus::Subscribe<WindowCloseEvent>([this](WindowCloseEvent& e)
	{
		e.m_Handled |= OnWindowClose(e);
	});
}

Application::~Application()
{
	NV_TRACE("Shutting down...");

	m_Window.reset();
	EventBus::Clear();

	Log::Shutdown();
}

void Application::Run()
{
	OnInitialize();
	while(m_Running)
	{
		ProcessEvents();

		OnUpdate();

		SDL_Delay(1); // Frame pacing for now
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
