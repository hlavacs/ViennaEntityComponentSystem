
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include "VECS2.h"


int test1() {


	std::cout << "test 1.2 system" << std::endl;

    vecs2::Registry<vecs2::REGISTRYTYPE_PARALLEL> system;

	//Valid
	{
		vecs2::Handle handle;
		assert( !handle.IsValid() );
		std::cout << "Handle size: " << sizeof(handle) << std::endl;
	}

	//Insert, Types, Get, Has, Erase, Exists
	{
		std::string s = "AAA";
		auto hhh = system.Insert(s);
		assert( system.Exists(hhh) );

	    vecs2::Handle handle = system.Insert(5, 5.5f);
		system.Print();
	    assert( system.Exists(handle) );
	    auto t1 = system.Types(handle);
		assert( t1.size() == 3 ); //also handle!
		std::set<size_t> types{vecs2::Type<vecs2::Handle>(), vecs2::Type<int>(), vecs2::Type<float>()};
		std::ranges::for_each( t1, [&](auto& t){ assert( types.contains(t) ); } );
	    auto v1 = system.Get<int>(handle);
		assert( v1 == 5 );
	    assert( system.Has<int>(handle) );
	    system.Erase(handle);
	    assert( !system.Exists(handle) );

	    //vecs2::Handle hx = system.Insert(5, 6); //compile error
	    struct height_t { int i; }; 
	    struct weight_t { int i; }; 
	    vecs2::Handle hx1 = system.Insert(5, height_t{6}, weight_t{6}); //works
	}

	//Exists
	{
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
	    assert( system.Exists(handle) );
	    auto t2 = system.Types(handle);
		std::set<size_t> types{vecs2::Type<vecs2::Handle>(), vecs2::Type<int>(), vecs2::Type<float>(), vecs2::Type<double>()};
		std::ranges::for_each( t2, [&](auto& t){ assert( types.contains(t) ); } );	}

	//Get
	{
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
		decltype(auto) value = system.Get<float&>(handle);
		float& f1 = value; 
		value = 10.0f; 
		assert( system.Get<float>(handle) == 10.0f );

	    auto [v2a, v2b] = system.Get<float, double>(handle);
	    auto [v3a, v3b] = system.Get<float&, double&>(handle);
		v3a = 100.0f;
		v3b = 101.0;
	    auto [v4a, v4b] = system.Get<float, double>(handle);
		assert( v4a == 100.0f && v4b == 101.0 );
	}

	//Put
	{
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
	    system.Put(handle, 50, 69.0f, 73.0);
		auto [v5a, v5b, v5c] = system.Get<int, float, double>(handle);
	    assert( v5b == 69.0f && v5c == 73.0 );
	    std::tuple<float,double> tup = system.Get<float, double>(handle);
	    std::get<float>(tup) = 101.0f;    
	    std::get<double>(tup) = 102.0;    
	    system.Put(handle, tup);
		auto [v6a, v6b] = system.Get<float, double>(handle);
	    assert( v6a == 101.0f && v6b == 102.0 );

	    auto tup2 = system.Get<int, float, double>(handle);
	    int ii = std::get<int>(tup2);
	    auto [ivalue, fvalue, dvalue] = system.Get<int, float, double>(handle);
		assert( ivalue == 50 && fvalue == 101.0f && dvalue == 102.0 );
	}

	//Has
	{
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
	    system.Put(handle, 50, 69.0f, 73.0);
	    assert( system.Has<int>(handle) );
	    assert( system.Has<float>(handle) );
	    assert( system.Has<double>(handle) );
	}

	//Erase components
	{
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
	    system.Erase<int, float>(handle); //remove two components
	    assert( !system.Has<int>(handle) );
	    assert( !system.Has<float>(handle) );
	    assert( system.Has<double>(handle) );
		system.Print();
	    system.Erase<double>(handle); //remove also the last component
	    assert( system.Exists(handle)); //check that the entity still exists
	    assert( !system.Has<double>(handle) );
	}

	//Add components with Put
	{
		system.Print();
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
		system.Erase<int, float, double>(handle);
	    assert( !system.Has<int>(handle) );
	    assert( !system.Has<float>(handle) );
	    assert( !system.Has<double>(handle) );
		system.Print();
		system.Put(handle, 3.9); //add a component of type double
	    assert( system.Exists(handle)); //check that the entity still exists
	    assert( system.Has<double>(handle) );
		system.Print();
		auto d = system.Get<double>(handle);
		auto cc = system.Get<char&>(handle); //
		cc = 'A';
		system.Print();
	}

	//Add components with Get
	{
	    auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
		auto dd = system.Get<char>(handle); //
		system.Print();
		std::string s = "AAA";
		struct T1 {
			const char* m_str;
		};

		system.Put(handle, s, T1{"BBB"}); //
		system.Print();
		auto [ee, ff] = system.Get<std::string, T1>(handle); //
		assert( ee == "AAA" && ff.m_str == std::string{"BBB"} );
	}

	//Erase entity
	{
		system.Print();
	    auto handle = system.Insert(5, 6.9f, 7.3);
    	assert( system.Exists(handle) );
		system.Print();
	    system.Erase(handle);
    	assert( !system.Exists(handle) );
		system.Print();
	}
 
	/*
	//Add Tags
	{
		auto handle1 = system.Insert(5, 6.9f, 7.3);
		auto handle2 = system.Insert(6, 7.9f, 8.3);
		auto handle3 = system.Insert(7, 8.9f, 9.3);
		system.Print();
		system.AddTags(handle1, 1ul, 2ul, 3ul);
		system.AddTags(handle2, 1ul, 3ul);
		system.AddTags(handle3, 2ul, 3ul);
		system.Print();
		auto tags = system.Types(handle1);
		assert( tags.size() == 7 );
		for( auto handle : system.template GetView<vecs2::Handle>(std::vector<size_t>{1ul}) ) {
			std::cout << "Handle (yes 1): "<< handle << std::endl;
		}
		for( auto handle : system.template GetView<vecs2::Handle>(std::vector<size_t>{1ul}, std::vector<size_t>{2ul}) ) {
			std::cout << "Handle (yes 1 no 2): "<< handle << std::endl;
		}
	}

	//Erase Tags
	{
		auto handle = system.Insert(5, 6.9f, 7.3);
		system.Print();
		system.AddTags(handle, 1ul, 2ul, 3ul);
		system.Print();
		auto tags = system.Types(handle);
		assert( tags.size() == 7 );
		system.EraseTags(handle, 1ul);
		system.Print();
		tags = system.Types(handle);
		assert( tags.size() == 6 );
		system.EraseTags(handle, 2ul, 3ul);
		system.Print();
		tags = system.Types(handle);
		assert( tags.size() == 4 );
	}

	//for loop
    auto hd1 = system.Insert(1, 10.0f, 10.0);
    auto hd2 = system.Insert(2, 20.0f );
    auto hd3 = system.Insert(3, 30.0, std::string("AAA"));
    auto hd4 = system.Insert(4, 40.0f, 40.0);
    auto hd5 = system.Insert(5);
    auto hd6 = system.Insert(6, 60.0f, 60.0);

	int a = 0;
	float b = 1.0f;
	std::tuple<int&, float> tup3 = {a, b};
	std::get<int&>(tup3) = 100;

	//auto hhh = system.Get<vecs2::Handle&>(hd1); //compile error

	std::cout << "Loop Handle: " << std::endl;
    for( auto handle : system.template GetView<vecs2::Handle>() ) {
        std::cout << "Handle: "<< handle << std::endl;
    }

	std::cout << "Loop Handle int& float " << std::endl;
    for( auto [handle, i, f] : system.template GetView<vecs2::Handle, int&, float>() ) {
        std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
		i = 100;
		f = 100.0f;
		system.DelayTransaction( [&](){ 
			std::cout << "Delayed Insert" << std::endl;
			auto h = system.Insert(5, 5.5f);
		} );
		//auto h2 = system.Insert(5, 5.5f);
    }

	std::cout << "Loop Handle int& float& " << std::endl;
    for( auto [handle, i, f] : system.template GetView<vecs2::Handle, int&, float&>() ) {
        std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
	}

	std::cout << "Loop Yes No " << std::endl;
    for( auto [handle, i] : system.template GetView<vecs2::Yes<vecs2::Handle, int>, vecs2::No<float>>() ) {
        std::cout << "Handle: "<< handle << " int: " << i << std::endl;
	}
    */

	
	assert( system.Size() > 0 );
    system.Clear();
    assert( system.Size() == 0 );

    return 0;   
}

/*
size_t test_insert_iterate( auto& system, int m ) {

	auto t1 = std::chrono::high_resolution_clock::now();

	for( int i=0; i<m; ++i ) {
		auto h = system.Insert(i, (float)i, (double)i, 'A', std::string("AAAAAA") );
	}

	for( auto [handle, i, f, d] : system.template GetView<vecs2::Handle, int&, float, double>() ) {
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


size_t test_iterate( auto& system, int m ) {

	auto t1 = std::chrono::high_resolution_clock::now();

    for( auto [handle, i, f] : system.template GetView<vecs2::Handle, int&, float&>() ) {
        i = i + (int)f;
		f = (float)i;
		//system.template Has<int>(handle);
		//system.Exists(handle);
	}

	auto t2 = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
}


void test3( std::string name, bool insert, auto&& job ) {

	size_t duration;
	int num = 2000000;

	{
		std::cout << "test 3.1 sequential " + name << std::endl;
		vecs2::Registry<vecs2::REGISTRYTYPE_SEQUENTIAL> system;
		if(insert) test_insert(system, num);
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
		system.Clear();
		if(insert) test_insert(system, num);
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
	}

	{
		std::cout << "test 3.2 sequential " + name << std::endl;
		vecs2::Registry<vecs2::REGISTRYTYPE_PARALLEL> system;
		if(insert) test_insert(system, num);
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
		system.Clear();
		if(insert) test_insert(system, num);
		duration = job(system, num);
		std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
	}

}


void test4( std::string name, bool insert, auto&& job ) {

	vecs2::Registry<vecs2::REGISTRYTYPE_PARALLEL> system;

	int num = 500000;
	auto work = [&](auto& system) {
		size_t duration = job(system, num);
	};

	if(insert) test_insert(system, 4*num);

	std::cout << "test 4.1 parallel " + name << std::endl;
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
	if(insert) test_insert(system, 4*num);

	std::cout << "test 4.2 parallel " + name << std::endl;
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

	std::cout << "test 5 parallel" << std::endl;

	using system_t = vecs2::Registry<vecs2::REGISTRYTYPE_PARALLEL>;
	using handles_t = std::set<vecs2::Handle>;
	system_t system;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);

	auto GetInt = [&]() -> int { return (int)(dis(gen)*1000.0); };
	auto GetFloat = [&]() -> float { return (float)dis(gen)*1000.0f; };
	auto GetDouble = [&]() -> double { return (double)dis(gen)*1000.0; };
	auto GetChar = [&]() -> char { return (char)(dis(gen)*100.0); };

	std::vector<std::function<void(handles_t&)>> jobs;
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetInt()) ); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetFloat())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetDouble())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetChar())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetInt(), GetFloat())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetInt(), GetFloat(), GetDouble())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetInt(), GetFloat(), GetDouble(), GetChar())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetInt(), GetFloat(), GetDouble(), GetChar(), std::string("1"))); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetFloat(), GetDouble())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetFloat(), GetDouble(), GetChar())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetFloat(), GetDouble(), GetChar(), std::string("1"))); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetDouble(), GetChar())); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetDouble(), GetChar(), std::string("1"))); } );
	jobs.push_back( [&](handles_t& hs) { hs.insert( system.Insert(GetChar(), std::string("1"))); } );
	
	jobs.push_back( [&](handles_t& handles) { 
		if( handles.size()>0) { 
			auto h = SelectRandom(handles, (size_t)(dis(gen))*handles.size());
			auto v = system.Get<int&>(*h);
			v = GetInt();			
		};
	} );

	jobs.push_back( [&](handles_t& hs) { 
		if( hs.size()>0) {
			auto h = SelectRandom(hs, (size_t)(dis(gen)*hs.size()));
			auto v = system.Get<float&>(*h);
			v = GetFloat();
		};
	} );

	jobs.push_back( [&](handles_t& hs) { 
		if( hs.size()>0) {
			auto h = SelectRandom(hs, (size_t)(dis(gen)*hs.size()));
			auto db = system.Get<double&>(*h);
			db = GetDouble();
		};
	} );

	jobs.push_back( [&](handles_t& hs) { 
		if( hs.size()>0) {
			auto h = SelectRandom(hs, (size_t)(dis(gen)*hs.size()));
			auto db = system.Get<double&>(*h);
		};
	} );


	int num = 1000000;
	auto work = [&](auto& system) {
		std::set<vecs2::Handle> hs;

		for( int i=0; i<num; ++i ) {
			size_t idx = std::min( (size_t)(dis(gen)*jobs.size()), jobs.size()-1);
			jobs[idx](hs);
		};
	};

	auto t1 = std::chrono::high_resolution_clock::now();

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

	auto t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)(8*num) << std::endl;

}

*/

void test_vecs() {
	test1();
	
	/*test3( "Insert", false, [&](auto& system, int num){ return test_insert(system, num); } );
	test3( "Iterate", true, [&](auto& system, int num){ return test_iterate(system, num); } );
	test3( "Insert + Iterate", false, [&](auto& system, int num){ return test_insert_iterate(system, num); } );

	test4( "Insert", false, [&](auto& system, int num){ return test_insert(system, num); } );
	test4( "Iterate", true, [&](auto& system, int num){ return test_iterate(system, num); } );
	test4( "Insert + Iterate", false, [&](auto& system, int num){ return test_insert_iterate(system, num); } );
	
	for( int i=0; i<1000; ++i ) {
		std::cout << "test 5 " << i << std::endl;
		test5();
	}
	*/
}

