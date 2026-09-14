#include "Window.hpp"

#include "Log.hpp"

#include "Events/EventBus.hpp"
#include "Events/ApplicationEvent.hpp"
#include "Events/KeyEvent.hpp"
#include "Events/MouseEvent.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#include <cassert>

static void SDLErrorCallback(const char* context)
{
	NV_ERROR("SDL Error ({}): {}", context, SDL_GetError());
}

std::unique_ptr<Window> Window::Create(const WindowSpecification& specification)
{
	return std::make_unique<Window>(specification);
}

Window::Window(const WindowSpecification& specification)
	: m_Specification(specification)
{
	NV_INFO("Creating window {} ({}, {})", specification.Title, specification.Width, specification.Height);

	SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	const Uint32 windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

	switch (specification.Mode)
	{
		case WindowMode::Windowed:
			m_Window = SDL_CreateWindow(specification.Title.c_str(), (int)specification.Width, (int)specification.Height, windowFlags);
			break;

		case WindowMode::BorderlessFullscreen:
		{
			const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
			m_Window = SDL_CreateWindow(specification.Title.c_str(), mode->w, mode->h, windowFlags | SDL_WINDOW_BORDERLESS);
			SDL_SetWindowPosition(m_Window, 0, 0);
			break;
		}

		case WindowMode::ExclusiveFullscreen:
		{
			const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
			m_Window = SDL_CreateWindow(specification.Title.c_str(), mode->w, mode->h, windowFlags | SDL_WINDOW_FULLSCREEN);
			break;
		}

		case WindowMode::Maximized:
			m_Window = SDL_CreateWindow(specification.Title.c_str(), (int)specification.Width, (int)specification.Height, windowFlags | SDL_WINDOW_MAXIMIZED);
			break;
	}

	assert(m_Window && "Could not create window");

	if (!SDL_SetWindowResizable(m_Window, specification.Resizable))
		SDLErrorCallback("SDL_SetWindowResizable");

	int w, h;
	SDL_GetWindowSize(m_Window, &w, &h);
	m_Specification.Width  = static_cast<uint32_t>(w);
	m_Specification.Height = static_cast<uint32_t>(h);
}

Window::~Window()
{
	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

void Window::PollEvents()
{
	const SDL_WindowID windowID = SDL_GetWindowID(m_Window);
	if (windowID == 0)
	{
		SDLErrorCallback("SDL_GetWindowID");
		return;
	}

	while (SDL_PollEvent(&m_Event))
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData)
		{
			ImGui_ImplSDL3_ProcessEvent(&m_Event);
		}

		switch (m_Event.type)
		{
			case SDL_EVENT_QUIT:
			{
				EventBus::Publish<WindowCloseEvent>();
				break;
			}

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			{
				if (m_Event.window.windowID == windowID)
					EventBus::Publish<WindowCloseEvent>();

				break;
			}

			case SDL_EVENT_WINDOW_RESIZED:
			{
				if (m_Event.window.windowID != windowID)
					break;

				const uint32_t width = static_cast<uint32_t>(m_Event.window.data1);
				const uint32_t height = static_cast<uint32_t>(m_Event.window.data2);

				m_Specification.Width = width;
				m_Specification.Height = height;

				EventBus::Publish<WindowResizeEvent>(width, height);

				break;
			}

			case SDL_EVENT_WINDOW_MINIMIZED:
			{
				if (m_Event.window.windowID == windowID)
					EventBus::Publish<WindowMinimizeEvent>(true);

				break;
			}

			case SDL_EVENT_WINDOW_MAXIMIZED:
			case SDL_EVENT_WINDOW_RESTORED:
			{
				if (m_Event.window.windowID == windowID)
					EventBus::Publish<WindowMinimizeEvent>(false);

				break;
			}

			case SDL_EVENT_KEY_DOWN:
			{
				if (m_Event.key.windowID != windowID)
					break;

				const uint32_t key = m_Event.key.scancode;

				if (m_Event.key.repeat)
				{
					EventBus::Publish<KeyPressedEvent>(key, 1);
				}
				else
				{
					EventBus::Publish<KeyPressedEvent>(key, 0);
				}

				break;
			}

			case SDL_EVENT_KEY_UP:
			{
				if (m_Event.key.windowID != windowID)
					break;

				const uint32_t key = m_Event.key.scancode;

				EventBus::Publish<KeyReleasedEvent>(key);

				break;
			}

			case SDL_EVENT_TEXT_INPUT:
			{
				if (m_Event.text.windowID != windowID)
					break;

				const auto* text = reinterpret_cast<const unsigned char*>(m_Event.text.text);

				uint32_t codepoint = 0;

				if (text[0] < 0x80)
				{
					codepoint = text[0];
				}
				else if (text[0] < 0xE0)
				{
					codepoint = ((text[0] & 0x1F) << 6) | (text[1] & 0x3F);
				}
				else if (text[0] < 0xF0)
				{
					codepoint = ((text[0] & 0x0F) << 12) | ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
				}
				else
				{
					codepoint = ((text[0] & 0x07) << 18) | ((text[1] & 0x3F) << 12) | ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
				}

				EventBus::Publish<KeyTypedEvent>(codepoint);

				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
				if (m_Event.button.windowID != windowID)
					break;

				const int button = m_Event.button.button;

				EventBus::Publish<MouseButtonPressedEvent>(button);

				break;
			}

			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				if (m_Event.button.windowID != windowID)
					break;

				const int button = m_Event.button.button;

				EventBus::Publish<MouseButtonReleasedEvent>(button);

				break;
			}

			case SDL_EVENT_MOUSE_WHEEL:
			{
				if (m_Event.wheel.windowID != windowID)
					break;

				EventBus::Publish<MouseScrolledEvent>(m_Event.wheel.x, m_Event.wheel.y);

				break;
			}

			case SDL_EVENT_MOUSE_MOTION:
			{
				if (m_Event.motion.windowID != windowID)
					break;

				if (SDL_GetWindowRelativeMouseMode(m_Window))
				{
					EventBus::Publish<MouseMovedEvent>(m_Event.motion.xrel, m_Event.motion.yrel);
				}
				else
				{
					EventBus::Publish<MouseMovedEvent>(m_Event.motion.x, m_Event.motion.y);
				}

				break;
			}
		}
	}
}

std::pair<float, float> Window::GetWindowPos() const
{
	int x, y;
	if (!SDL_GetWindowPosition(m_Window, &x, &y))
	{
		NV_WARN("Failed to get window position: {}", SDL_GetError());
		return { 0.0f, 0.0f };
	}
	return { static_cast<float>(x), static_cast<float>(y) };
}

void Window::ProcessEvents()
{
	PollEvents();
}

void Window::SetVSync(bool enabled)
{
	m_Specification.VSync = enabled;
	// swapchain recreate/update
}

void Window::SetResizable(bool resizable)
{
	m_Specification.Resizable = resizable;
	if (!SDL_SetWindowResizable(m_Window, resizable))
		SDLErrorCallback("SDL_SetWindowResizable");
}

void Window::Maximize()
{
	if (m_Specification.Mode == WindowMode::Windowed || m_Specification.Mode == WindowMode::Maximized)
		SDL_MaximizeWindow(m_Window);
}

void Window::CenterWindow()
{
	SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Window::SetTitle(const std::string& title)
{
	m_Specification.Title = title;
	if (!SDL_SetWindowTitle(m_Window, title.c_str()))
		SDLErrorCallback("SDL_SetWindowTitle");
}

float Window::GetTime()
{
	static const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
	static const double start     = static_cast<double>(SDL_GetPerformanceCounter()) / frequency;
	return static_cast<float>(static_cast<double>(SDL_GetPerformanceCounter()) / frequency - start);
}
