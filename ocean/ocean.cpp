// ocean.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <functional>
#include <iostream>

#include "Shader.h"
#include "RenderShader.h"

RenderInterface* RenderPointer = nullptr;

static void DisplayFunc() {
	if (RenderPointer == nullptr) {
		return;
	}

	RenderPointer->render();

}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(1024, 768);
	glutInitWindowPosition(10, 10);
	glutCreateWindow("ocean");
	auto GlewInitRes = glewInit();
	glViewport(0, 0, 1024, 768);
	

	RenderShader Render("./shader.vs", "./shader.frag");
	RenderPointer = &Render;
	glutDisplayFunc(DisplayFunc);

	glutMainLoop();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
