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

		int AddData(std::tuple<size_t, std::string> x) {
			data = x;
			return 0;
		}

		//std::tuple<size_t, std::string>& GetData() {
		//	return data;
		//}

		size_t GetType() { return std::get<0>(data); }

		std::string ToString() {
			return std::get<1>(data);
		}
		void SetString(std::string s) {
			std::get<1>(data) = s;
		}
	};

} // namespace Console
