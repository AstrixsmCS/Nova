#pragma once

#include "Core/Application.hpp"

#include "Renderer/Shader.hpp"
#include "Renderer/GraphicsPipeline.hpp"
#include "Renderer/Buffer.hpp"

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
    Pipeline                m_TrianglePipeline;
    VertexBuffer            m_VertexBuffer;
};
