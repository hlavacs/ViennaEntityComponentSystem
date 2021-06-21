
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <atomic>
#include <numeric>
#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VGJS.h"
#include "VGJSCoro.h"

#include "VECS.h"
#include "parallel.h"	//define user components


using namespace vecs;


std::atomic<size_t> cnt = 0;
std::atomic<double> outer = 0;

volatile double u = 0;


auto t0 = high_resolution_clock::now();
auto t1 = high_resolution_clock::now();
auto t2 = high_resolution_clock::now();
auto t3 = high_resolution_clock::now();


void work( size_t num ) {
	for (int j = 0; j < num; ++j) {
		u = sqrt(j * u + 1);
	}
}


template<typename R>
void do_work(R range ) {
	size_t i = 0;

	for (auto [mutex, handle, pos] : range) {
		if (!handle.is_valid()) continue;	//is_valid() is enough here!
		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
		++i;
	}
	//std::cout << i << "\n";
}


template<typename R>
void do_work2(R range, bool sync = false) {
	size_t i = 0;

	range.for_each([&](auto& mutex, auto handle, auto& pos) {
		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
		++i;
	}, sync);
	//std::cout << i << "\n";

}




void init(size_t num) {

	for (int i = 0; i < num; ++i) {
		MyComponentName name;
		MyComponentPosition pos{ glm::vec3{1.0f + i, 2.0f + i, 3.0f + i} };
		MyComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f + i}} };
		MyComponentTransform trans{ glm::mat4{ 1.0f } };
		MyComponentMaterial mat{ 99 };
		MyComponentGeometry geo{ 11 };

		auto h1 = VecsRegistry<MyEntityTypeNode>().insert(name, pos, orient, trans);
		auto h2 = VecsRegistry<MyEntityTypeDraw>().insert(name, pos, orient, trans, mat, geo);
	}

	/*for (int i = 0; i < 100; i++) {
		vgjs::schedule( [=]() { work(num); });
	}*/

}


vgjs::Coro<std::tuple<double,double,double,double,double, double>> clock( size_t num ) {
    //std::cout << "Start \n";

	int thr = 12;
	std::pmr::vector<vgjs::Function> vec1;
	std::pmr::vector<vgjs::Function> vec2;
	std::pmr::vector<vgjs::Function> vec3;

	auto ranges = VecsRange<MyComponentPosition>{}.split(thr);
	for (int i = 0; i < ranges.size(); ++i) {
		vec1.push_back(vgjs::Function([=]() { do_work(ranges[i]); }, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ i }));
	}

	for (int i = 0; i < ranges.size(); ++i) {
		vec2.push_back(vgjs::Function([=]() { do_work2(ranges[i]); }, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ i }));
	}

	for (int i = 0; i < ranges.size(); ++i) {
		vec3.push_back(vgjs::Function([=]() { do_work2(ranges[i], true); }, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ i }));
	}

	auto lin1 =
		vgjs::Function([&]() {
				do_work(VecsRange<MyComponentPosition>{});
			}
			, vgjs::thread_index_t{}, vgjs::thread_type_t{1}, vgjs::thread_id_t{100});


	auto lin2 =
		vgjs::Function([&]() {
		do_work2(VecsRange<MyComponentPosition>{});
			}
	, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ 100 });

	auto lin3 =
		vgjs::Function([&]() {
		do_work2(VecsRange<MyComponentPosition>{}, true);
			}
	, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ 100 });


	auto t0 = high_resolution_clock::now();

	co_await lin1;

	auto t1 = high_resolution_clock::now();

	co_await lin2;

	auto t2 = high_resolution_clock::now();

	co_await lin3;

	auto t3 = high_resolution_clock::now();
	
	co_await vec1;

	auto t4 = high_resolution_clock::now();

	co_await vec2;

	auto t5 = high_resolution_clock::now();

	co_await vec3;

	auto t6 = high_resolution_clock::now();

	auto d1 = duration_cast<nanoseconds>(t1 - t0);
	auto d2 = duration_cast<nanoseconds>(t2 - t1);
	auto d3 = duration_cast<nanoseconds>(t3 - t2);
	auto d4 = duration_cast<nanoseconds>(t4 - t3);
	auto d5 = duration_cast<nanoseconds>(t5 - t4);
	auto d6 = duration_cast<nanoseconds>(t6 - t5);

	double dt1 = d1.count() / 1.0;
	double dt2 = d2.count() / 1.0;
	double dt3 = d3.count() / 1.0;
	double dt4 = d4.count() / 1.0;	
	double dt5 = d5.count() / 1.0;
	double dt6 = d6.count() / 1.0;

	size_t size = VecsRegistry{}.size();

	//std::cout << "Num " << 2 * num << " Size " << size << "\n";
	//std::cout << "Linear " << dt1        << " ns Parallel 1 " << dt2        << " ns Speedup " << dt1 / dt2 << "\n\n";
	//std::cout << "Linear " << std::setprecision(5) << std::setw(10) << dt1 / size << " ns Parallel 1 " << std::setw(10) << dt2 / size << " ns \n";

    co_return std::make_tuple(dt1 / size, dt2 / size, dt3 / size, dt4 / size, dt5 / size, dt6 / size);
}


vgjs::Coro<> start(size_t num) {

	init(num);

	std::vector<double> v1;
	std::vector<double> v2;
	std::vector<double> v3;
	std::vector<double> v4;
	std::vector<double> v5;
	std::vector<double> v6;
	for (int i = 0; i < 1000; ++i) {
		auto p = co_await clock(num);
		v1.push_back(std::get<0>(p));
		v2.push_back(std::get<1>(p));
		v3.push_back(std::get<2>(p));
		v4.push_back(std::get<3>(p));
		v5.push_back(std::get<4>(p));
		v6.push_back(std::get<5>(p));
	}
	std::sort(v1.begin(), v1.end());
	std::sort(v2.begin(), v2.end());
	std::sort(v3.begin(), v3.end());
	std::sort(v4.begin(), v4.end());
	std::sort(v5.begin(), v5.end());
	std::sort(v6.begin(), v6.end());

	std::cout << "Linear\n";

	auto& v = v1;
	std::cout << "Range based\n";
	std::cout << "Average " << std::accumulate(v.begin(), v.end(), 0.0) / v.size() << "\n";
	std::cout << "Median " << v[v.size()/2] << "\n";
	std::cout << "Min " << *std::min_element(v.begin(), v.end()) << "\n";
	std::cout << "Max " << *std::max_element(v.begin(), v.end()) << "\n\n";

	v = v2;
	std::cout << "for_each sync=false\n";
	std::cout << "Average " << std::accumulate(v.begin(), v.end(), 0.0) / v.size() << "\n";
	std::cout << "Median " << v[v.size() / 2] << "\n";
	std::cout << "Min " << *std::min_element(v.begin(), v.end()) << "\n";
	std::cout << "Max " << *std::max_element(v.begin(), v.end()) << "\n\n";

	v = v3;
	std::cout << "for_each sync=true\n";
	std::cout << "Average " << std::accumulate(v.begin(), v.end(), 0.0) / v.size() << "\n";
	std::cout << "Median " << v[v.size() / 2] << "\n";
	std::cout << "Min " << *std::min_element(v.begin(), v.end()) << "\n";
	std::cout << "Max " << *std::max_element(v.begin(), v.end()) << "\n\n";

	std::cout << "Parallel\n";

	v = v4;
	std::cout << "Range based\n";
	std::cout << "Average " << std::accumulate(v.begin(), v.end(), 0.0) / v.size() << "\n";
	std::cout << "Median " << v[v.size() / 2] << "\n";
	std::cout << "Min " << *std::min_element(v.begin(), v.end()) << "\n";
	std::cout << "Max " << *std::max_element(v.begin(), v.end()) << "\n\n";

	v = v5;
	std::cout << "for_each sync=false\n";
	std::cout << "Average " << std::accumulate(v.begin(), v.end(), 0.0) / v.size() << "\n";
	std::cout << "Median " << v[v.size() / 2] << "\n";
	std::cout << "Min " << *std::min_element(v.begin(), v.end()) << "\n";
	std::cout << "Max " << *std::max_element(v.begin(), v.end()) << "\n\n";

	v = v6;
	std::cout << "for_each sync=true\n";
	std::cout << "Average " << std::accumulate(v.begin(), v.end(), 0.0) / v.size() << "\n";
	std::cout << "Median " << v[v.size() / 2] << "\n";
	std::cout << "Min " << *std::min_element(v.begin(), v.end()) << "\n";
	std::cout << "Max " << *std::max_element(v.begin(), v.end()) << "\n\n";

	vgjs::terminate();
	co_return;
}



int main() {

	vgjs::JobSystem();

	//vgjs::JobSystem().enable_logging();

	size_t num = 100000;

	auto st = start( num )(vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ 999 });
    vgjs::schedule( st );
	
    vgjs::wait_for_termination();

    return 0;
}

