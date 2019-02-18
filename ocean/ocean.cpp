// ocean.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <functional>
#include <iostream>

#include "Shader.h"
#include "RenderShader.h"

#include <time.h>

#include "WaterFFT.h"

RenderInterface* RenderPointer = nullptr;

static void DisplayFunc() {
	if (RenderPointer == nullptr) {
		return;
	}

	while (true) {
		RenderPointer->render();
	}
}

static void test() {
	WaterFFT fft(256, 256, 5, true);
	fft.UseIntensityMap("intensity2.png");

	while (true){
		fft.Update(clock());
		fft.SwapBuffers();
		std::cout << "Location 0, 0 " << fft.HeightAtLocation({ 15054,0 }) << std::endl;
	}
}

int main(int argc, char** argv)
{
	test();
	
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
