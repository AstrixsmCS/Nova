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

	inline Window& GetWindow() { return *m_Window; }

	static inline Application& Get() { return *s_Instance; }

	static const char* GetConfigurationName();
	static const char* GetPlatformName();

	const ApplicationSpecification& GetSpecification() const { return m_Specification; }
protected:
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

	bool m_Running = true, m_Minimized = false;

	static Application* s_Instance;
};
