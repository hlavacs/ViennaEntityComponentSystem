#pragma once

#include <iostream>
#include <list>
#include <vector>

namespace Console {

	class Component {
	private:
		std::tuple<size_t, std::string> data;

	public:
		Component() {}
		Component(Component const& org) {
			data = org.data;
		}
		Component& operator=(Component const& org) {
			clear();
			data = org.data;
			return *this;
		}

		void clear() {
			/*data.clear();*/
		}

		int addData(std::tuple<size_t, std::string> x) {
			data = x;
			return 0;
		}

		std::tuple<size_t, std::string>& getData() {
			return data;
		}

		size_t getType() { return std::get<0>(data); }

		std::string toString() {
			return std::get<1>(data);
		}
	};

} // namespace Console
