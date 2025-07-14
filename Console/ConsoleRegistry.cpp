#include "ConsoleRegistry.h"

namespace Console {

	// Console::Archetype members

	void Archetype::SetRegistry(Registry* r) {
		registry = r;
		if (registry)
			for (auto& e : entities)
				registry->addEntity(e.second.GetValue(), hash);
	}
	int Archetype::addEntity(Entity& e) {
		entities[e.GetValue()] = e;
		entities[e.GetValue()].SetArchetype(this);

		if (registry) {
			registry->addEntity(e.GetValue(), hash);
		}

		return 0;
	}

}