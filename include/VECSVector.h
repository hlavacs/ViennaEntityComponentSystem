#pragma once


namespace vecs {

	//----------------------------------------------------------------------------------------------
	//Segmented Vector

	template<VecsPOD T> class Vector;

	class VectorBase {

	public:
		VectorBase() = default; //constructor
		virtual ~VectorBase() = default; //destructor

		/// @brief Insert a new component value.
		/// @param v The component value.
		/// @return The index of the new component value.
		template<typename U>
		auto push_back(U&& v) -> size_t {
			using T = std::decay_t<U>;
			return static_cast<Vector<T>*>(this)->push_back(std::forward<U>(v));
		}

		virtual auto push_back() -> size_t = 0;
		virtual auto pop_back() -> void = 0;
		virtual auto erase(size_t index) -> size_t = 0;
		virtual void copy(VectorBase* other, size_t from) = 0;
		virtual void swap(size_t index1, size_t index2) = 0;
		virtual auto size() const->size_t = 0;
		virtual auto clone() -> std::unique_ptr<VectorBase> = 0;
		virtual void clear() = 0;
		virtual void print() = 0;

		virtual auto toJSON() -> std::string = 0;
		virtual auto toJSON(size_t index) -> std::string = 0;
		virtual size_t getType() = 0;
		virtual size_t elemSize() = 0;
	}; //end of VectorBase


	/// @brief A vector that stores elements in segments to avoid reallocations. The size of a segment is 2^segmentBits.
	template<VecsPOD T>
	class Vector : public VectorBase {

		using Segment_t = std::shared_ptr<std::vector<T>>;
		using Vector_t = std::vector<Segment_t>;

	public:

		/// @brief Iterator for the vector.
		class Iterator {
		public:
			Iterator(Vector<T>& data, size_t index) : m_data{ data }, m_index{ index } {}
			Iterator& operator++() { ++m_index; return *this; }
			bool operator!=(const Iterator& other) const { return m_index != other.m_index; }
			T& operator*() { return m_data[m_index]; }

		private:
			Vector<T>& m_data;
			size_t m_index{ 0 };
		};

		/// @brief Constructor, creates the vector.
		/// @param segmentBits The number of bits for the segment size.
		Vector(size_t segmentBits = 6) : m_size{ 0 }, m_segmentBits(segmentBits), m_segmentSize{ 1ull << segmentBits }, m_segments{} {
			assert(segmentBits > 0);
			m_segments.emplace_back(std::make_shared<std::vector<T>>(m_segmentSize));
		}

		~Vector() = default;

		Vector(const Vector& other) : m_size{ other.m_size }, m_segmentBits(other.m_segmentBits), m_segmentSize{ other.m_segmentSize }, m_segments{} {
			m_segments.emplace_back(std::make_shared<std::vector<T>>(m_segmentSize));
		}

		/// @brief Push a value to the back of the vector.
		/// @param value The value to push.
		template<typename U>
		auto push_back(U&& value) -> size_t {
			while (Segment(m_size) >= m_segments.size()) {
				m_segments.emplace_back(std::make_shared<std::vector<T>>(m_segmentSize));
			}
			++m_size;
			(*this)[m_size - 1] = std::forward<U>(value);
			return m_size - 1;
		}

		auto push_back() -> size_t override {
			return push_back(T{});
		}

		/// @brief Pop the last value from the vector.
		void pop_back() override {
			assert(m_size > 0);
			--m_size;
			if (Offset(m_size) == 0 && m_segments.size() > 1) {
				m_segments.pop_back();
			}
		}

		/// @brief Get the value at an index.
		/// @param index The index of the value.
		auto operator[](size_t index) const -> T& {
			assert(index < m_size);
			auto seg = Segment(index);
			auto off = Offset(index);
			return (*m_segments[Segment(index)])[Offset(index)];
		}

		/// @brief Get the value at an index.
		auto size() const -> size_t override { return m_size; }

		/// @brief Clear the vector. Make sure that one segment is always available.
		void clear() override {
			m_size = 0;
			m_segments.clear();
			m_segments.emplace_back(std::make_shared<std::vector<T>>(m_segmentSize));
		}

		/// @brief Erase an entity from the vector.
		auto erase(size_t index) -> size_t override {
			size_t last = size() - 1;
			assert(index <= last);
			if (index < last) {
				(*this)[index] = std::move((*this)[last]); //move the last entity to the erased one
			}
			pop_back(); //erase the last entity
			return last; //if index < last then last element was moved -> correct mapping 
		}

		/// @brief Copy an entity from another vector to this.
		void copy(VectorBase* other, size_t from) override {
			push_back((static_cast<Vector<T>*>(other))->operator[](from));
		}

		/// @brief Swap two entities in the vector.
		void swap(size_t index1, size_t index2) override {
			std::swap((*this)[index1], (*this)[index2]);
		}

		/// @brief Clone the vector.
		auto clone() -> std::unique_ptr<VectorBase> override {
			return std::make_unique<Vector<T>>();
		}

		/// @brief Print the vector.
		void print() override {
			std::cout << "Name: " << typeid(T).name() << " ID: " << Type<T>();
		}

		auto begin() -> Iterator { return Iterator{ *this, 0 }; }
		auto end() -> Iterator { return Iterator{ *this, m_size }; }

	private:

		/// @brief Compute the segment index of an entity index.
		/// @param index Entity index.
		/// @return Index of the segment.
		inline size_t Segment(size_t index) const { return index >> m_segmentBits; }

		/// @brief Compute the offset of an entity index in a segment.
		/// @param index Entity index.
		/// @return Offset in the segment.
		inline size_t Offset(size_t index) const { return index & (m_segmentSize - 1ul); }

		size_t m_size{ 0 };	///< Size of the vector.
		size_t m_segmentBits;	///< Number of bits for the segment size.
		size_t m_segmentSize; ///< Size of a segment.
		Vector_t m_segments{ 10 };	///< Vector holding unique pointers to the segments.


	public:
		auto toJSON(size_t index) -> std::string override { return ::vecs::toJSON(operator[](index)); }

		auto toJSON() -> std::string override {
			std::string json = std::string("{\"name\":\"") + typeid(T).name() + "\",\"id\":" + std::to_string(Type<T>());
			json += "}";
			return json;
		}
		virtual size_t getType() { return Type<T>(); }
		size_t elemSize() override { return sizeof(T); }

	}; //end of Vector

}
