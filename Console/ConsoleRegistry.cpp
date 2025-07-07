#include "ConsoleRegistry.h"

namespace Console {

	// Console::Archetype members

	void Archetype::SetRegistry(Registry* r) {
		registry = r;
		if (registry)
			for (auto& e : entities)
				registry->addEntity(e.second.GetIndex(), hash);
	}
	int Archetype::addEntity(Entity& e) {
		entities[e.GetIndex()] = e;
		entities[e.GetIndex()].SetArchetype(this);
 
		if (registry) {
			registry->addEntity(e.GetIndex(), hash);
		}

		return 0;
	}

}