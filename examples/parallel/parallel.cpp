
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


template<template<typename...> typename R, typename... Cs>
void do_work(R<Cs...> range ) {
	size_t i = 0;

	for (auto [handle, pos] : range) {
		if (!handle.is_valid()) continue;	//is_valid() is enough here!
		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
		++i;
	}

	/*range.for_each([&](auto& handle, auto& pos) {
		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
		++i;
	}, true);*/

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


vgjs::Coro<std::pair<double,double>> clock( size_t num ) {
    //std::cout << "Start \n";

	int thr = 12;
	std::pmr::vector<vgjs::Function> vec;

	auto ranges = VecsRange<MyComponentPosition>{}.split(thr);
	for (int i = 0; i < ranges.size(); ++i) {
		vec.push_back(vgjs::Function([=]() { do_work(ranges[i]); }, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ i }));
	}

	auto lin =
		vgjs::Function([&]() {
				do_work(VecsRange<MyComponentPosition>{});
			}
			, vgjs::thread_index_t{}, vgjs::thread_type_t{1}, vgjs::thread_id_t{100});

	auto t0 = high_resolution_clock::now();

	co_await lin;

	auto t1 = high_resolution_clock::now();

	//for (auto& func : vec) {
		//func.m_function();
	//}
	
	co_await vec;

	auto t2 = high_resolution_clock::now();

	auto d1 = duration_cast<nanoseconds>(t1 - t0);
	auto d2 = duration_cast<nanoseconds>(t2 - t1);

	double dt1 = d1.count() / 1.0;
	double dt2 = d2.count() / 1.0;

	size_t size = VecsRegistry{}.size();

	//std::cout << "Num " << 2 * num << " Size " << size << "\n";
	//std::cout << "Linear " << dt1        << " ns Parallel 1 " << dt2        << " ns Speedup " << dt1 / dt2 << "\n\n";
	//std::cout << "Linear " << std::setprecision(5) << std::setw(10) << dt1 / size << " ns Parallel 1 " << std::setw(10) << dt2 / size << " ns \n";

    co_return std::make_pair(dt1 / size, dt2 / size);
}


vgjs::Coro<> start(size_t num) {

	init(num);

	std::vector<double> v1;
	std::vector<double> v2;
	for (int i = 0; i < 10000; ++i) {
		auto p = co_await clock(num);
		v1.push_back(std::get<0>(p));
		v2.push_back(std::get<1>(p));
	}
	std::sort(v1.begin(), v1.end());
	std::sort(v1.begin(), v1.end());

	std::cout << "Average " << std::accumulate(v1.begin(), v1.end(), 0.0) / v1.size() << "\n";
	std::cout << "Median " << v1[v1.size()/2] << "\n";
	std::cout << "Min " << *std::min_element(v1.begin(), v1.end()) << "\n";
	std::cout << "Max " << *std::max_element(v1.begin(), v1.end()) << "\n\n";

	std::cout << "Average " << std::accumulate(v2.begin(), v2.end(), 0.0) / v2.size() << "\n";
	std::cout << "Median " << v2[v2.size() / 2] << "\n";
	std::cout << "Min " << *std::min_element(v2.begin(), v2.end()) << "\n";
	std::cout << "Max " << *std::max_element(v2.begin(), v2.end()) << "\n";

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

