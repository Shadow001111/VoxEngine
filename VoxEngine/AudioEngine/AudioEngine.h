#pragma once
#include "Player.h"
#include "ResourceManager.h"

namespace AudioEngine
{
	class GlobalInstances
	{
	public:
		static Player& getPlayer()
		{
			static Player player;
			return player;
		}

		static ResourceManager& getResourceManager()
		{
			static ResourceManager resourceManager;
			return resourceManager;
		}
	};
}