#ifndef INTTYPE_H
#define INTTYPE_H


/**
* \brief Strong type for integers.
* 
* T...the integer type
* P...phantom type as unique ID
* D...default value (=null value)
*/
template<typename T, typename P, auto D = -1>
struct int_type {
	using type_name = T;
	static const T null = static_cast<T>(D);

	T value{};

	/**
	* \brief Empty constructor.
	*/
	int_type() {
		static_assert(!(std::is_unsigned_v<T> && std::is_signed_v<decltype(D)> && static_cast<int>(D) < 0));
		value = static_cast<T>(D);
	};

	/**
	* \brief Constructor.
	* \param[in] u A POD type that is used for setting the value.
	*/
	template<typename U>
	requires (std::is_convertible_v<std::decay_t<U>, T> && std::is_pod_v<std::decay_t<U>>)
	explicit int_type(U&& u) noexcept : value{ static_cast<T>(u) } {};

	/**
	* \brief Copy constructor.
	* \param[in] t Another int type.
	*/
	int_type(const int_type<T, P, D>& t) noexcept : value{ t.value } {};

	/**
	* \brief Move constructor.
	* \param[in] t Another int type.
	*/
	int_type(int_type<T, P, D>&& t) noexcept : value{ std::move(t.value) } {};

	/**
	* \brief Copy assignment.
	* \param[in] rhs Another int type.
	*/
	void operator=(const int_type<T, P, D>& rhs) noexcept { value = rhs.value; };

	/**
	* \brief Move assignment.
	* \param[in] rhs Another int type.
	*/
	void operator=(int_type<T, P, D>&& rhs) noexcept { value = std::move(rhs.value); };

	/**
	* \brief Copy assignment.
	* \param[in] rhs Any POD int type.
	*/
	template<typename U>
	requires (std::is_convertible_v<std::decay_t<U>, T> && std::is_pod_v<std::decay_t<U>>)
	void operator=(U&& rhs) noexcept { value = static_cast<T>(rhs); };

	/**
	* \brief Yield the int value.
	* \returns the int value.
	*/
	operator const T& () const { return value; }

	/**
	* \brief Yield the int value.
	* \returns the int value.
	*/
	operator T& () { return value; }

	/**
	* \brief Comparison operator.
	* \returns the default comparison.
	*/
	auto operator<=>(const int_type<T, P, D>& v) const = default;

	/**
	* \brief Comparison operator.
	* \returns the comparison between this value and another int.
	*/
	template<typename U>
	requires std::is_convertible_v<U, T>
	auto operator<(const U& v) noexcept { return value <=> static_cast<T>(v); };

	/**
	* \brief Left shift operator.
	* \param[in] L Number of bits to left shift.
	* \returns the value left shifted.
	*/
	T operator<<(const size_t L) noexcept { return value << L; };

	/**
	* \brief Right shift operator.
	* \param[in] L Number of bits to right shift.
	* \returns the value left shifted.
	*/
	T operator>>(const size_t L) noexcept { return value >> L; };

	/**
	* \brief And operator.
	* \param[in] L Number that should be anded bitwise.
	* \returns the bew value that was anded to the old value.
	*/
	T operator&(const size_t L) noexcept { return value & L; };

	/**
	* \brief Pre-increment operator.
	* \returns the value increased by 1.
	*/
	int_type<T, P, D> operator++() noexcept {
		value++; 
		if( !has_value() ) value = 0;
		return *this;
	};

	/**
	* \brief Post-increment operator.
	* \returns the old value before increasing by 1.
	*/
	int_type<T, P, D> operator++(int) noexcept {
		int_type<T, P, D> res = *this;
		value++;
		if (!has_value()) value = 0;
		return res;
	};

	/**
	* \brief Pre-decrement operator.
	* \returns the value decreased by 1.
	*/
	int_type<T, P, D> operator--() noexcept { 
		--value; 
		if (!has_value()) --value;
		return *this;
	};

	/**
	* \brief Post-decrement operator.
	* \returns the value before decreasing by 1.
	*/
	int_type<T, P, D> operator--(int) noexcept { 
		int_type<T, P, D> res = *this;
		value--;
		if (!has_value()) value--;
		return res;
	};

	/**
	* \brief Create a hash value.
	*/
	struct hash {
		/**
		* \param[in] tg The input int value.
		* \returns the hash of the int value.
		*/
		std::size_t operator()(const int_type<T, P, D>& tg) const { return std::hash<T>()(tg.value); };
	};

	/**
	* \brief Equality comparison as function.
	* \returns true if the value is not null (the default value).
	*/
	struct equal_to {
		constexpr bool operator()(const T& lhs, const T& rhs) const { return lhs == rhs; };
	};

	/**
	* \brief Determine whether the value is not null.
	* \returns true if the value is not null (the default value).
	*/
	bool has_value() const {
		return value != null;
	}
};


#endif
