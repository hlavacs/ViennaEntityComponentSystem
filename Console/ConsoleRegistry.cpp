#include "ConsoleRegistry.h"

namespace Console {

	void Archetype::SetRegistry(Registry* r) {
		registry = r;
		if (registry)
			for (auto& e : entities)
				registry->AddEntity(e.second, hash);
	}
	int Archetype::AddEntity(Entity& e) {
		entities[e.GetValue()] = e;
		entities[e.GetValue()].SetArchetype(this);

		if (registry) {
			registry->AddEntity(e, hash);
		}

		return 0;
	}

}