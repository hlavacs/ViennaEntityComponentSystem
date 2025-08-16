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

	using Mutex_t = std::shared_mutex; ///< Shared mutex type
	using namespace std::chrono_literals;

	template<typename T>
	concept VecsPOD = (!std::is_reference_v<T> && !std::is_pointer_v<T>);

	//----------------------------------------------------------------------------------------------
	//Convenience 

	template<typename>
	struct is_std_vector : std::false_type {};

	template<typename T, typename A>
	struct is_std_vector<std::vector<T, A>> : std::true_type {};

	template <typename> struct is_tuple : std::false_type {};
	template <typename ...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};

	template<typename... Ts>
	struct Yes {};

	template<typename... Ts>
	struct No {};

	/// @brief Turn a type into a hash.
	/// @tparam T The type to hash.
	/// @return The hash of the type.
	template<typename T>
	inline auto Type() -> std::size_t {
		return std::type_index(typeid(T)).hash_code();
	}

	/// @brief Compute the hash of a list of hashes. If stored in a vector, make sure that hashes are sorted.
	/// @tparam T Container type of the hashes.
	/// @param hashes Reference to the container of the hashes.
	/// @return Overall hash made from the hashes.
	template <typename T>
	inline size_t Hash(T& hashes) {
		std::size_t seed = 0;
		if constexpr (is_std_vector<std::decay_t<T>>::value) {
			std::sort(hashes.begin(), hashes.end());
		}
		for (auto& v : hashes) {
			seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}

	template <typename T>
	inline size_t Hash(T&& hashes) {
		std::size_t seed = 0;
		if constexpr (is_std_vector<std::decay_t<T>>::value) {
			std::sort(hashes.begin(), hashes.end());
		}
		for (auto& v : hashes) {
			seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}


	/// @brief convert a string into a transmittable JSON string
	inline std::string toJSONString(std::string s) {
		std::string d("\"");
		// mini-massager to get a string into JSON format
		for (auto c : s) {
			switch (c) {
			case '\b':
				d += "\\b";
				break;
			case '\f':
				d += "\\f";
				break;
			case '\n':
				d += "\\n";
				break;
			case '\r':
				d += "\\r";
				break;
			case '\t':
				d += "\\t";
				break;
			case '\\':
			case '\"':
				d += '\\';
				d += c;
				break;
			default:
				// this might require conversion of all other control characters (below 0x20) to \u00xx,
				// but let's ignore that for now, shall we?
				d += c;
				break;
			}
		}
		return d + "\"";
	}

	template<typename T>
	concept HasToString = requires(T a) {
		{ a.to_string() } -> std::convertible_to<std::string>;
	};
	template<typename T>
	concept Streamable = requires(std::ostream & os, T a) {
		{ os << a } -> std::same_as<std::ostream&>;
	};

	/// @brief convert a value into a transmittable JSON string
	template<typename T>
	std::string toJSON(const T& value) {
		// This is less terrible code, but, to my knowledge, there's no better way to get
		// the template types and hence the data
		// handle primitive types
		if constexpr (std::is_same_v<T, char> || std::is_same_v<T, unsigned char>)
			return toJSONString(std::string(1, value));
		else if constexpr (std::is_same_v<T, int> ||
			std::is_same_v<T, unsigned int> ||
			std::is_same_v<T, long> ||
			std::is_same_v<T, unsigned long> ||
			std::is_same_v<T, long long> ||
			std::is_same_v<T, unsigned long long> ||
			std::is_same_v<T, float> ||
			std::is_same_v<T, double> ||
			std::is_same_v<T, long double>)
			return std::to_string(value);
		// handle strings themselves
		else if constexpr (std::is_same_v<T, std::string>)
			return toJSONString(value);
		// handle objects with a to_string member
		else if constexpr (HasToString<T>)
			return toJSONString(value.to_string());
		// handle objects that can be serialized to a stream
		else if constexpr (Streamable<T>) {
			std::ostringstream oss;
			oss << value;
			return toJSONString(oss.str());
		}
		// anything else, sadly, cannot be JSONified
		return toJSONString("<unknown>");
	}


}

#if !defined(REGISTRYTYPE_SEQUENTIAL) || !defined(REGISTRYTYPE_PARALLEL)
#define REGISTRYTYPE_SEQUENTIAL
#endif

#ifdef REGISTRYTYPE_SEQUENTIAL
using Size_t = std::size_t;
#else
using Size_t = std::atomic<std::size_t>;
#endif

namespace vecs {
	class Registry;
	class VECSConsoleComm;
	VECSConsoleComm* getConsoleComm(Registry* reg = nullptr);
}
#include <VTLL.h>
#include <VSTY.h>
#include "VECSHandle.h"
#include "VECSMutex.h"
#include "VECSVector.h"
#include "VECSSlotMap.h"
#include "VECSArchetype.h"
#include "VECSRegistry.h"
#include "VECSConsoleComm.h"

