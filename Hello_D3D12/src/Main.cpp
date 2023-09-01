#include <SDL.h>
#include "Application/Application.h"


int main(int argc, char* args[]) {
	Application app;

	app.Initialize();
	app.Run();
	app.Destroy();

	return 0;
}