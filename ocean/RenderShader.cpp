#include "pch.h"
#include "RenderShader.h"

RenderShader::RenderShader(std::string VertexShaderFilePath, std::string FragShaderFilePath) :
	RenderInterface(),
	Vertices(),
	VAO(),
	VBO(),
	MyShader(VertexShaderFilePath.c_str(), FragShaderFilePath.c_str())
{
	Vertices = {
		0.5f, -0.5f, 0.0f,   0.4f, 0.8f, 0.0f,
		-0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
		0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f
	};
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * Vertices.size(), &Vertices[0], GL_STATIC_DRAW);
	
	glBindAttribLocation(MyShader.Program, 0, "position");
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glBindAttribLocation(MyShader.Program, 1, "color");
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

RenderShader::~RenderShader()
{
}

void RenderShader::render()
{
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	MyShader.Use();
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glFlush();
	glutSwapBuffers();
}
