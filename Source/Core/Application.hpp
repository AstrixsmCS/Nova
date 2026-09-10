#pragma once

#include "Window.hpp"

#include "Events/ApplicationEvent.hpp"

#include <string>
#include <memory>

struct ApplicationSpecification
{
	std::string Name = "Nova";
	WindowSpecification Window;
};

class Application
{
public:
	explicit Application(const ApplicationSpecification& specification);
	~Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void Run();
	void Close();

	static const char* GetConfigurationName();
	static const char* GetPlatformName();
protected:
	Window& GetWindow() { return *m_Window; }
	const Window& GetWindow() const { return *m_Window; }

	virtual void OnInitialize() {}
	virtual void OnUpdate() {}
	virtual void OnShutdown() {}
private:
	void ProcessEvents();

		bool OnWindowResize(WindowResizeEvent& e);
		bool OnWindowMinimize(WindowMinimizeEvent& e);
		bool OnWindowClose(WindowCloseEvent& e);
private:
	std::unique_ptr<Window> m_Window;
	ApplicationSpecification m_Specification;

	bool m_Running = true;
	bool m_Minimized = false;
};
