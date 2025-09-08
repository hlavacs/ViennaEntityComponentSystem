#include "ConsoleRegistry.h"

namespace Console {

	/// @brief set the registry for the console
	/// @param r Representation of a Registry for the Console
	void Archetype::SetRegistry(Registry* r) {
		registry = r;
		if (registry)
			for (auto& e : entities)
				registry->AddEntity(e.second, hash);
	}

	/// @brief add an Entity to an Archetype
    /// @param e Representation of an Entity for the Console
	void Archetype::AddEntity(Entity& e) {
		entities[e.GetValue()] = e;
		entities[e.GetValue()].SetArchetype(this);

		if (registry) {
			registry->AddEntity(e, hash);
		}
	}

}