#pragma once

#include <shared_mutex>
#include <map>
#include <unordered_map>
#include <set>
#include <chrono>
#include <functional>
#include <typeindex>
#include <cassert>

namespace vecs {

    using mutex_t = std::shared_mutex; ///< Shared mutex type

    //----------------------------------------------------------------------------------------------
	//Convenience functions

	template<typename>
	struct is_std_vector : std::false_type {};

	template<typename T, typename A>
	struct is_std_vector<std::vector<T,A>> : std::true_type {};

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	template<typename... Ts>
	struct Yes {};

	template<typename... Ts>
	struct No {};

    /// @brief Compute the hash of a list of hashes. If stored in a vector, make sure that hashes are sorted.
	/// @tparam T Container type of the hashes.
	/// @param hashes Reference to the container of the hashes.
	/// @return Overall hash made from the hashes.
	template <typename T>
	inline size_t Hash( T& hashes ) {
		std::size_t seed = 0;
		if constexpr ( is_std_vector<std::decay_t<T>>::value ) {
			std::ranges::sort(hashes);
		}
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	}

	template <typename T>
	inline size_t Hash( T&& hashes ) {
		std::size_t seed = 0;
		if constexpr ( is_std_vector<std::decay_t<T>>::value ) {
			std::ranges::sort(hashes);
		}
		for( auto& v : hashes ) {
			seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
		}
		return seed;
	}

}

#include <VTLL.h>
#include <VSTY.h>
#include "VECSHandle.h"
#include "VECSMutex.h"
#include "VECSVector.h"
#include "VECSSlotMap.h"
#include "VECSArchetype.h"
#include "VECSRegistry.h"
