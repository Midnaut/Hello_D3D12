#pragma once
#include <SDL.h>
#include <Windows.h>
#include <memory>
#include "../Graphics/D3D12Implementation.h"

class Application {
	private:


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
		void Destroy();

		static int windowWidth;
		static int windowHeight;
};