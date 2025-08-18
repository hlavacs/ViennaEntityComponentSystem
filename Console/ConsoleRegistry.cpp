#include "ConsoleRegistry.h"

namespace Console {

	void Archetype::SetRegistry(Registry* r) {
		registry = r;
		if (registry)
			for (auto& e : entities)
				registry->addEntity(e.second, hash);
	}
	int Archetype::addEntity(Entity& e) {
		entities[e.GetValue()] = e;
		entities[e.GetValue()].SetArchetype(this);

		if (registry) {
			registry->addEntity(e, hash);
		}

		return 0;
	}

}