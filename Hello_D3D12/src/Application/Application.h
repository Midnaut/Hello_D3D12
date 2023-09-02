#pragma once
#include <SDL.h>
#include <Windows.h>
#include <memory>
#include "../Graphics/D3D12Implementation.h"

const int TARGET_FPS = 120;
const int TARGET_MILLISECONDS_PER_FRAME = 1000 / TARGET_FPS;

class Application {
	private:
		
		bool uncappedFrameRate = false;
		int millisecondsPreviousFrame = 0;

		bool isRunning;
		SDL_Window* window = nullptr;
		HWND windowHandle = nullptr;
		std::unique_ptr<D3D12Implementation> d3d12_imp;

	public:
		Application();
		~Application();

		void Initialize();
		void Run();
		void ProcessInput();
		void Update();
		void Render();
		void Destroy();

		static int windowWidth;
		static int windowHeight;
};