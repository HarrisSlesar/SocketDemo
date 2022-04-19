#include "Game.h"
#include "Unit.h"
#include "UnitManager.h"
#include "GraphicsBufferManager.h"
#include "NetworkManager.h"
#include "MemoryManager.h"

#include <System.h>
#include <GraphicsSystem.h>
#include <GraphicsBuffer.h>
#include <Animation.h>

#include <timer.h>
#include <PerformanceTracker.h>
#include <iostream>

using namespace std;

Game* Game::mspInstance = NULL;


Game::Game()
{
	mpSystem = new System;
}

Game::~Game()
{
	cleanup();
	delete mpSystem;
}

Game* Game::getInstance()
{
	assert(mspInstance != NULL);
	return mspInstance;
}

void Game::initInstance()
{
	assert(mspInstance == NULL);
	if (mspInstance == NULL)
	{
		mspInstance = new Game();
	}
}

void Game::deleteInstance()
{
	assert(mspInstance != NULL);
	delete mspInstance;
	mspInstance = nullptr;
}

bool Game::init(unsigned int width, unsigned int height, double targetTimePerFrame /*= 16.7*/)
{
	if (mIsInitted)
	{
		cleanup();
	}
	SocketUtil::StaticInit();

	string destination = StringUtils::GetCommandLineArg(1);
	string name = StringUtils::GetCommandLineArg(2);
	if (destination == "" || name == "")
	{
		destination = "8000";
		name = "Harris";
		// LOG("ERROR: Missing command line arguments.");
		//return false;
	}
	mpNetworkManager = new NetworkManager;
	//assume no colon implies this is just the port, which implies that this is the master peer
	if (destination.find_first_of(':') == string::npos)
	{
		mpNetworkManager->InitAsMasterPeer(stoi(destination), name);
	}
	else
	{
		SocketAddressPtr targetAddress = SocketAddressFactory::CreateIPv4FromString(destination);
		if (!targetAddress)
		{
			LOG("ERROR: Unable to create target address from destination.");
			return false;
		}
		mpNetworkManager->InitAsPeer(*targetAddress, name);
	}
	mpSystem->init(width, height);
	mTargetTimePerFrame = targetTimePerFrame;

	mpMemoryManager = new MemoryManager;
	mpMemoryManager->init();

	mpGraphicsBufferManager = new GraphicsBufferManager;
	mpUnitManager = new UnitManager;
	
	

	loadBuffers();

	mIsInitted = true;

	return true;
}

void Game::cleanup()
{
	if (mIsInitted)
	{
		
		
		delete mpUnitManager;
		delete mpGraphicsBufferManager;
		delete mpMemoryManager;
		delete mpNetworkManager;
		mpSystem->cleanup();
	}
	SocketUtil::CleanUp();
	mIsInitted = false;
}

void Game::doLoop()
{
	Timer timer;
	PerformanceTracker tracker;
	
	float packetTimer = 0.f;
	while (mShouldContinue)
	{
		tracker.clearTracker("loop");
		tracker.startTracking("loop");
		timer.start();

		getInput();

		if (mpNetworkManager->GetState() != NetworkManager::NetworkManagerState::NMS_Delay)
		{
			
			update(mTargetTimePerFrame);
			mpNetworkManager->ProcessIncomingPackets();

			packetTimer += mTargetTimePerFrame;
			if (packetTimer >= 500.f)
			{
				mpNetworkManager->SendOutgoingPackets();
				packetTimer = 0.f;
			}
		}
		else
		{
			//only grab the incoming packets because if I'm in delay,
			//the only way I'm getting out is if an incoming packet saves me
			mpNetworkManager->ProcessIncomingPackets();
		}

		
		render();

		timer.sleepUntilElapsed(mTargetTimePerFrame);
		tracker.stopTracking("loop");
		//cout << tracker.getElapsedTime("loop") << endl;

		//cout << "Allocated memory: " << mpMemoryManager->getTotalAllocated() << endl;
		//cout << "Current available memory: " << mpMemoryManager->getTotalCapacity() << endl;
	}
}

void Game::getInput()
{
	if (mpSystem->isKeyPressed(System::ESCAPE_KEY))
	{
		mShouldContinue = false;
	}
	if (gameStarted)
	{
		if (mpSystem->isKeyPressed(System::ENTER_KEY))
		{
			Unit* pUnit = mpUnitManager->getLastUnitCreated();
			if (pUnit)
			{
				pUnit->toggleAnimation();
				mpNetworkManager->addAction(ActionTypes::ToggleAnimSingle, pUnit->getCenterPosition());
			}
		}
		if (mpSystem->isKeyPressed(System::SPACE_KEY))
		{
			mpUnitManager->togglePauseStateForAllAnimations();
			mpNetworkManager->addAction(ActionTypes::ToggleAnimAll, Vector2D(0, 0));
		}
		if (mpSystem->isMouseButtonPressed(System::LEFT))
		{
			Vector2D mousePos = mpSystem->getCurrentMousePos();
			createUnit(mousePos, UnitTypes::BASE_UNIT);
			mpNetworkManager->addAction(ActionTypes::CreateUnit, mousePos);
		}
		if (mpSystem->isMouseButtonPressed(System::RIGHT))
		{
			Vector2D mousePos = mpSystem->getCurrentMousePos();
			mpUnitManager->deleteAllUnitsAt2DPosition(mousePos);
			mpNetworkManager->addAction(ActionTypes::DestroyUnit, mousePos);
		}
		if (mpSystem->isKeyPressed(System::ONE_KEY))
		{
			Vector2D mousePos = mpSystem->getCurrentMousePos();

			createUnit(mousePos, UnitTypes::RAND_DIR);
			mpNetworkManager->addAction(ActionTypes::CreateUnitMove, mousePos);
		}
		if (mpSystem->isKeyPressed(System::TWO_KEY))
		{

			int randX = rand() % 800;
			int randY = rand() % 600;
			Vector2D position(randX, randY);

			createUnit(position, UnitTypes::RAND_SPAWN);
			mpNetworkManager->addAction(ActionTypes::CreateUnitRand, position);
		}
	}
	if (mpSystem->isKeyPressed(System::S_KEY) && !gameStarted && mpNetworkManager->IsMasterPeer())
	{
		mpNetworkManager->TryStartGame();
		gameStarted = true;
	}
}

void Game::update(double dt)
{
	mpUnitManager->update(dt);
	Timing::sInstance.Update();
}

void Game::render()
{
	//draw background
	GraphicsSystem::draw(ZERO_VECTOR2D, *(mpGraphicsBufferManager->getBuffer(WOODS)), 0.5f);

	//draw units
	mpUnitManager->draw();

	mpSystem->getGraphicsSystem()->flip();
}

void Game::loadBuffers()
{
	const string ASSET_PATH = "..\\..\\..\\assets\\";
	const string BACKGROUND_FILENAME = "woods.png";
	const string SMURF_FILENAME = "smurf_sprites.png";
	const string DEAN_FILENAME = "dean_sprites.png";
	const string NUMBERED_FILENAME = "smurf_sprites_numbered.png";

	mpGraphicsBufferManager->createBuffer(WOODS, ASSET_PATH + BACKGROUND_FILENAME);
	mpGraphicsBufferManager->createBuffer(SMURFS, ASSET_PATH + SMURF_FILENAME);
	mpGraphicsBufferManager->createBuffer(DEAN, ASSET_PATH + DEAN_FILENAME);
	mpGraphicsBufferManager->createBuffer(NUMBERED, ASSET_PATH + NUMBERED_FILENAME);


}

const int PIXEL_WIDTH = 60;
const int PIXEL_HEIGHT = 60;
const int SPRITES_ACROSS = 4;
const int SPRITES_DOWN = 4;
const float TIME_PER_FRAME_MULTIPLE = 5;

void Game::createUnit(const Vector2D& pos, UnitTypes type)
{
	switch (type)
	{
	case UnitTypes::BASE_UNIT:
	{
		float timePerFrame = (float)mTargetTimePerFrame * TIME_PER_FRAME_MULTIPLE;
		const GraphicsBuffer* pSmurfs = mpGraphicsBufferManager->getBuffer(SMURFS);
		assert(pSmurfs);
		const GraphicsBuffer* pDean = mpGraphicsBufferManager->getBuffer(DEAN);
		if (pSmurfs)
		{
			Animation smurfAnimation(*pSmurfs, PIXEL_WIDTH, PIXEL_HEIGHT, SPRITES_ACROSS, SPRITES_DOWN, timePerFrame);
			Animation deanAnimation(*pDean, PIXEL_WIDTH, PIXEL_HEIGHT, SPRITES_ACROSS, SPRITES_DOWN, timePerFrame);

			mpUnitManager->createUnit(pos, smurfAnimation, deanAnimation);
		}
		break;
	}
	case UnitTypes::RAND_SPAWN:
	{
		float timePerFrame = (float)mTargetTimePerFrame * TIME_PER_FRAME_MULTIPLE;
		const GraphicsBuffer* pSmurfs = mpGraphicsBufferManager->getBuffer(SMURFS);
		assert(pSmurfs);
		const GraphicsBuffer* pDean = mpGraphicsBufferManager->getBuffer(DEAN);
		assert(pDean);
		Animation smurfAnimation(*pSmurfs, PIXEL_WIDTH, PIXEL_HEIGHT, SPRITES_ACROSS, SPRITES_DOWN, timePerFrame);
		Animation deanAnimation(*pDean, PIXEL_WIDTH, PIXEL_HEIGHT, SPRITES_ACROSS, SPRITES_DOWN, timePerFrame);

		mpUnitManager->createUnit(pos, deanAnimation, smurfAnimation);
		break;
	}
	case UnitTypes::RAND_DIR:
	{
		float timePerFrame = (float)mTargetTimePerFrame * TIME_PER_FRAME_MULTIPLE;
		const GraphicsBuffer* pNumbered = mpGraphicsBufferManager->getBuffer(NUMBERED);
		assert(pNumbered);
		const GraphicsBuffer* pDean = mpGraphicsBufferManager->getBuffer(DEAN);
		assert(pDean);
		Animation numberedAnimation(*pNumbered, PIXEL_WIDTH, PIXEL_HEIGHT, SPRITES_ACROSS, SPRITES_DOWN, timePerFrame);
		Animation deanAnimation(*pDean, PIXEL_WIDTH, PIXEL_HEIGHT, SPRITES_ACROSS, SPRITES_DOWN, timePerFrame);

		mpUnitManager->createUnit(pos, numberedAnimation, deanAnimation);
		break;
	}
	}

}

void Game::HandleAction(ActionTypes type, Vector2D pos)
{
	switch (type)
	{
	case ActionTypes::ToggleAnimAll:
	{
		mpUnitManager->togglePauseStateForAllAnimations();
		break;
	}
	case ActionTypes::ToggleAnimSingle:
	{
		Unit* pUnit = mpUnitManager->getLastUnitCreated();
		if (pUnit)
		{
			pUnit->toggleAnimation();
		}
		break;
	}
	case ActionTypes::CreateUnit:
	{
		createUnit(pos, UnitTypes::BASE_UNIT);
		break;
	}
	case ActionTypes::CreateUnitRand:
	{
		rand();
		rand();
		createUnit(pos, UnitTypes::RAND_SPAWN);
		break;
	}
	case ActionTypes::CreateUnitMove:
	{
		createUnit(pos, UnitTypes::RAND_DIR);
		break;
	}
	case ActionTypes::DestroyUnit:
	{
		mpUnitManager->deleteAllUnitsAt2DPosition(pos);
		break;
	}
	}
}
