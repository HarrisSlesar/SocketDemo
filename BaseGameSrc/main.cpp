#include <iostream>
#include <cassert>
#include <string>
#include <random>
#include <ctime>

#include <PerformanceTracker.h>
#include <MemoryTracker.h>

#include <GraphicsSystem.h>
#include <System.h>
#include <GraphicsBuffer.h>
#include <Sprite.h>
#include <Color.h>
#include <Font.h>

#include "Game.h"

using namespace std;

int main()
{
	const unsigned int WIDTH = 800;
	const unsigned int HEIGHT = 600;
	
	Game::initInstance();
	Game* pGame = Game::getInstance();

	pGame->init(WIDTH, HEIGHT);
	pGame->doLoop();
	pGame->cleanup();
	
	Game::deleteInstance();

	MemoryTracker::getInstance()->reportAllocations(cout);
	system("pause");

	return 0;

}