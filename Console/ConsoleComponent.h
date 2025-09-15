#pragma once

#include <iostream>
#include <list>
#include <vector>

namespace Console {
	//Representation of Components for the Console
	class Component {
	private:
		std::tuple<size_t, std::string> data;

	public:
		Component() {}
		Component(Component const& org) {
			data = org.data;
		}
		Component& operator=(Component const& org) {
			data = org.data;
			return *this;
		}

		/// @brief add Data to a component
		/// @param x tuple consisting of type as a hash and the value
		void AddData(std::tuple<size_t, std::string> x) {
			data = x;
		}

		/// @brief retrieve datatype
		/// @return datatype as hash
		size_t GetType() { return std::get<0>(data); }

		/// @brief retrieve data
		/// @return data as string
		std::string ToString() {
			return std::get<1>(data);
		}

		/// @brief set data 
		/// @param s string value
		void SetString(std::string s) {
			std::get<1>(data) = s;
		}
	};

} // namespace Console
