#include "Application.h"
#include <SDL.h>
#include <Windows.h>
#include <spdlog/spdlog.h>

int Application::windowWidth;
int Application::windowHeight;

Application::Application() {
	isRunning = false;
	spdlog::info("Application Constructor Called");
}

Application::~Application() {
	spdlog::info("Application Destructor Called");
}

void Application::Initialize() {

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		std::string errorMsg = SDL_GetError();
		spdlog::critical("Error initializing SDL: " + errorMsg);
		return;
	}

	windowWidth = 800;
	windowHeight = 600;

	window = SDL_CreateWindow(
		"DirectX 12 Test Window", 
		SDL_WINDOWPOS_CENTERED, 
		SDL_WINDOWPOS_CENTERED, 
		windowWidth, 
		windowHeight, 
		0);
	
	if (!window) {
		spdlog::critical("Error initializing SDL Window.");
		return;
	}

	HWND activeWindowHandle = GetActiveWindow();
	windowHandle = activeWindowHandle;

	d3d12_imp = std::make_unique<D3D12Implementation>(windowHandle, windowWidth, windowHeight);
	d3d12_imp->Initialize();

	isRunning = true;
}

void Application::Run() {
	while (isRunning) {
		ProcessInput();
		Update();
		Render();
	}
}

void Application::ProcessInput() {
	SDL_Event sdlEvent;
	while (SDL_PollEvent(&sdlEvent)) {

		switch (sdlEvent.type)
		{
		case SDL_QUIT:
			isRunning = false;
			break;

		default:
			break;
		}

	}
}

void Application::Update() {
	
	if (!uncappedFrameRate) {
		int timeToWait = TARGET_MILLISECONDS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame);

		// Release control to OS
		if (timeToWait > 0 && timeToWait <= millisecondsPreviousFrame) {
			SDL_Delay(timeToWait);
		}
	}

}

void Application::Render() {
	d3d12_imp->Render();
}

void Application::Destroy() {
	d3d12_imp->Shutdown();
	SDL_DestroyWindow(window);
	SDL_Quit();
}