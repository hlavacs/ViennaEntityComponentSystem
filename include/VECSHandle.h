#pragma once

namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Handles

	/// @brief A handle for an entity or a component.
	template<size_t INDEX_BITS=32, size_t VERSION_BITS=24, size_t STORAGE_BITS=8>
	struct HandleT {
		using type_t = typename vsty::strong_type_t<size_t, vsty::counter<>, 
							std::integral_constant<size_t, std::numeric_limits<size_t>::max()>>; ///< Strong type for the handle type.

	public:
		HandleT() = default; ///< Default constructor.

		HandleT(size_t index, size_t version, size_t storageIndex=0) : 
			m_value{std::numeric_limits<size_t>::max()} { 
			m_value.set_bits(index, 0, INDEX_BITS);
			m_value.set_bits(version, INDEX_BITS, VERSION_BITS);
			m_value.set_bits(storageIndex, INDEX_BITS + VERSION_BITS, STORAGE_BITS);
		};

		HandleT(size_t v) : m_value{type_t{v}} {};

		size_t GetIndex() const { return m_value.get_bits(0, INDEX_BITS); }
		size_t GetVersion() const { return m_value.get_bits(INDEX_BITS, VERSION_BITS); }
		size_t GetStorageIndex() const { return m_value.get_bits(INDEX_BITS + VERSION_BITS); }
		size_t GetVersionedIndex() const { return (GetVersion() << VERSION_BITS) + GetIndex(); }
		bool IsValid() const { return m_value != std::numeric_limits<size_t>::max(); }
		HandleT& operator=(const HandleT& other) { m_value = other.m_value; return *this; }
		bool operator==(const HandleT& other) const { return GetIndex() == other.GetIndex() && GetVersion() == other.GetVersion(); }
		bool operator!=(const HandleT& other) const { return !(*this == other); }
		bool operator<(const HandleT& other) const { return GetIndex() < other.GetIndex(); }

	private:
		type_t m_value; ///< Strong type for the handle.
	};

	using Handle = HandleT<32,24,8>; ///< Type of the handle.

	inline bool IsValid(const Handle& handle) {
		return handle.IsValid();
	}
	
}

inline std::ostream& operator<<(std::ostream& os, const vecs::Handle& handle) {
	return os << "{" <<  handle.GetIndex() << ", " << handle.GetVersion() << ", " << handle.GetStorageIndex() << "}"; 
}
