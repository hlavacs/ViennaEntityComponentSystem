
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include "VECS.h"


int test1() {

	{
		vecs::SlotMap<int, vecs::SLOTMAPTYPE_SEQUENTIAL> sm;
		auto [i1, v1] = sm.Insert(1);
		auto [i2, v2] = sm.Insert(2);
		auto [i3, v3] = sm.Insert(3);
		assert( sm.Size() == 3 );

		assert( sm[i1].m_value == 1 );
		assert( sm[i2].m_value == 2 );
		assert( sm[i3].m_value == 3 );

		sm.Erase(1,0);
		sm.Erase(2,0);

		assert( sm.Size() == 1 );
		assert( sm[i3].m_value == 3 );
	}

    vecs::Registry<vecs::REGISTRYTYPE_PARALLEL> system;

    vecs::Handle h1 = system.Insert(5, 5.5f);
    assert( system.Exists(h1) );
    auto t1 = system.Types(h1);
    auto v1 = system.Get<int>(h1);
    assert( system.Has<int>(h1) );
    system.Erase(h1);
    assert( !system.Exists(h1) );

    //vecs::Handle hx = system.Insert(5, 6); //compile error
    struct height_t { int i; }; 
    struct weight_t { int i; }; 
    vecs::Handle hx1 = system.Insert(5, height_t{6}, weight_t{6}); //works

    auto h2 = system.Insert(5, 6.9f, 7.3);
    assert( system.Exists(h2) );
    auto t2 = system.Types(h2);
    auto [v2a, v2b] = system.Get<float, double>(h2);
    auto [v3a, v3b] = system.Get<float&, double&>(h2);
	v3a = 100.0f;
	v3b = 101.0;
    auto [v4a, v4b] = system.Get<float, double>(h2);

    system.Put(h2, 50, 69.0f, 73.0);
	auto [v5a, v5b, v5c] = system.Get<int, float, double>(h2);
    assert( v5b == 69.0f && v5c == 73.0 );
    std::tuple<float,double> tup = system.Get<float, double>(h2);
    std::get<float>(tup) = 101.0f;    
    std::get<double>(tup) = 102.0;    
    system.Put(h2, tup);
	auto [v6a, v6b] = system.Get<float, double>(h2);
    assert( v6a == 101.0f && v6b == 102.0 );

    auto tup2 = system.Get<int, float, double>(h2);
    int ii = std::get<int>(tup2);
    auto [ivalue, fvalue, dvalue] = system.Get<int, float, double>(h2);

    assert( system.Has<int>(h2) );
    assert( system.Has<float>(h2) );
    assert( system.Has<double>(h2) );

    system.Erase<int, float>(h2); //remove two components
    assert( !system.Has<int>(h2) );
    assert( !system.Has<float>(h2) );
    assert( system.Has<double>(h2) );
    system.Erase<double>(h2); //remove also the last component
    assert( system.Exists(h2)); //check that the entity still exists
	system.Put(h2, 3.9); //add a component of type double
    assert( system.Exists(h2)); //check that the entity still exists
	auto d = system.Get<double>(h2);
	auto cc = system.Get<char&>(h2); //
	cc = 'A';
	auto dd = system.Get<char>(h2); //
	std::string s = "AAA";
	struct T1 {
		const char* m_str;
	};

	system.Put(h2, s, T1{"BBB"}); //
	auto [ee, ff] = system.Get<std::string, T1>(h2); //

    system.Erase(h2);
    assert( !system.Exists(h2) );
 
    auto hd1 = system.Insert(1, 10.0f, 10.0);
    auto hd2 = system.Insert(2, 20.0f );
    auto hd3 = system.Insert(3, 30.0, "AAA");
    auto hd4 = system.Insert(4, 40.0f, 40.0);
    auto hd5 = system.Insert(5);
    auto hd6 = system.Insert(6, 60.0f, 60.0);

	int a = 0;
	float b = 1.0f;
	std::tuple<int&, float> tup3 = {a, b};
	std::get<int&>(tup3) = 100;

	//auto hhh = system.Get<vecs::Handle&>(hd1); //compile error

    for( auto handle : system.template GetView<vecs::Handle>() ) {
        std::cout << "Handle: "<< handle << std::endl;
    }

    for( auto [handle, i, f] : system.template GetView<vecs::Handle, int&, float>() ) {
        std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
		i = 100;
		f = 100.0f;
		auto h1 = system.Insert(5, 5.5f);
		auto h2 = system.Insert(5, 5.5f);
    }
    for( auto [handle, i, f] : system.template GetView<vecs::Handle, int&, float&>() ) {
        std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
	}

    assert( system.Size() > 0 );
    system.Clear();
    assert( system.Size() == 0 );

    return 0;   
}


size_t test_insert_iterate( auto& system, int m ) {

	auto t1 = std::chrono::high_resolution_clock::now();

	for( int i=0; i<m; ++i ) {
		auto h = system.Insert(i, (float)i, (double)i, 'A', std::string("AAAAAA") );
	}

	for( auto [handle, i, f, d] : system.template GetView<vecs::Handle, int&, float, double>() ) {
		i = (int)(f + d);
	}

	auto t2 = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
}


size_t test_insert( auto& system, int m ) {

	auto t1 = std::chrono::high_resolution_clock::now();

	for( int i=0; i<m; ++i ) {
		auto h = system.Insert(i, (float)i, (double)i, 'A', std::string("AAAAAA") );
	}
	auto t2 = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
}


void test3(auto&& job ) {

	size_t duration;
	int num = 2000000;
	{
		vecs::Registry<vecs::REGISTRYTYPE_SEQUENTIAL> system;
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
		system.Clear();
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
	}

	{
		vecs::Registry<vecs::REGISTRYTYPE_PARALLEL> system;
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
		system.Clear();
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
	}
}


void test4( auto&& job ) {

	vecs::Registry<vecs::REGISTRYTYPE_PARALLEL> system;

	int num = 1000000;
	auto work = [&](auto& system) {
		size_t duration = job(system, num);
	};

	auto t1 = std::chrono::high_resolution_clock::now();
	{
		std::jthread t1{ [&](){ work(system);} };
		std::jthread t2{ [&](){ work(system);} };
		std::jthread t3{ [&](){ work(system);} };
		std::jthread t4{ [&](){ work(system);} };
	}
	{
		auto t2 = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)system.Size() << std::endl;
	}

	system.Clear();
	std::cout << "Size: " << system.Size() << std::endl;

	t1 = std::chrono::high_resolution_clock::now();

	{
		std::jthread t1{ [&](){ work(system);} };
		std::jthread t2{ [&](){ work(system);} };
		std::jthread t3{ [&](){ work(system);} };
		std::jthread t4{ [&](){ work(system);} };
	}
	{
		auto t2 = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)system.Size() << std::endl;
	}
}


template<typename S>
auto SelectRandom(const S &s, size_t n) {
 	auto it = std::begin(s);
 	std::advance(it,n);
 	return it;
}


void test5() {
	using system_t = vecs::Registry<vecs::REGISTRYTYPE_PARALLEL>;
	using handles_t = std::set<vecs::Handle>;
	system_t system;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);

	auto GetInt = [&]() -> int { return dis(gen); };
	auto GetFloat = [&]() -> float { return (float)dis(gen)*1000.0f; };
	auto GetDouble = [&]() -> double { return (double)dis(gen)*1000.0; };
	auto GetChar = [&]() -> char { return (char)(dis(gen)*100.0); };

	std::vector<std::function<void(handles_t&)>> jobs;
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetInt()) ); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetFloat())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetDouble())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetChar())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetInt(), GetFloat())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetInt(), GetFloat(), GetDouble())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetInt(), GetFloat(), GetDouble(), GetChar())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetInt(), GetFloat(), GetDouble(), GetChar(), std::string("1"))); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetFloat(), GetDouble())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetFloat(), GetDouble(), GetChar())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetFloat(), GetDouble(), GetChar(), std::string("1"))); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetDouble(), GetChar())); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetDouble(), GetChar(), std::string("1"))); } );
	jobs.push_back( [&](handles_t& handles) { handles.insert( system.Insert(GetChar(), std::string("1"))); } );
	
	/*jobs.push_back( [&](handles_t& handles) { if( handles.size()) { 
		auto h = SelectRandom(handles, dis(gen)*handles.size()); 
		handles.erase(h);}; 
	} );*/

	jobs.push_back( [&](handles_t& handles) { 
		if( handles.size()) { 
			auto h = SelectRandom(handles, dis(gen)*handles.size());
			system.Get<int&>(*h) = GetInt();
		};
	} );

	jobs.push_back( [&](handles_t& handles) { 
		if( handles.size()) { 
			auto h = SelectRandom(handles, dis(gen)*handles.size());
			system.Get<float&>(*h) = GetFloat();
		};
	} );

	int num = 20000;
	auto work = [&](auto& system) {
		std::set<vecs::Handle> handles;

		for( int i=0; i<num; ++i ) {
			jobs[dis(gen)*jobs.size()](handles);
		};
	};

	{
		std::jthread t1{ [&](){ work(system);} };
		std::jthread t2{ [&](){ work(system);} };
		std::jthread t3{ [&](){ work(system);} };
		std::jthread t4{ [&](){ work(system);} };
		std::jthread t5{ [&](){ work(system);} };
		std::jthread t6{ [&](){ work(system);} };
		std::jthread t7{ [&](){ work(system);} };
		std::jthread t8{ [&](){ work(system);} };
	}

	std::cout << "Size: " << system.Size() << std::endl;

}



int main() {
	/*test3( [&](auto& system, int num){ return test_insert(system, num); } );
	test3( [&](auto& system, int num){ return test_insert_iterate(system, num); } );
	test4( [&](auto& system, int num){ return test_insert(system, num); } );
	test4( [&](auto& system, int num){ return test_insert_iterate(system, num); } );
	*/
	test5();

	return 0;
}

