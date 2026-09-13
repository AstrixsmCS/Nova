#pragma once

#include "Core/Application.hpp"
#include "Renderer/Renderer.hpp"

class EditorApplication final : public Application
{
public:
	explicit EditorApplication(const ApplicationSpecification& specification);
	~EditorApplication();

protected:
	void OnInitialize() override;
	void OnUpdate()     override;
	void OnShutdown()   override;
private:
	Renderer m_Renderer;
};
