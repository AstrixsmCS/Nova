#pragma once

#include "Core/Application.hpp"

#include "Renderer/GraphicsState.hpp"
#include "Renderer/Shader.hpp"
#include "Renderer/Buffer.hpp"

#include "Renderer/Camera.hpp"

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
	std::shared_ptr<Shader> m_TriangleShader;
	GraphicsState           m_TriangleState;
	VertexBuffer            m_VertexBuffer;

	Camera m_Camera;
};
