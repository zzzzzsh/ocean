#pragma once

#include "pch.h"
#include "RenderInterface.h"
#include "Shader.h"

class RenderShader : public RenderInterface {
public:

	RenderShader(std::string VertexShaderFilePath, std::string FragShaderFilePath);

	~RenderShader() override;

	void render() override;
private:
	std::vector<GLfloat>	Vertices;
	GLuint VBO;
	GLuint VAO;
	
	Shader MyShader;
};