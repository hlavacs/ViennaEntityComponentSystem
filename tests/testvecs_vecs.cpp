
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include "VECS.h"

bool boolprint = false;
void check( bool b, std::string_view msg = "" );


int test1() {


	if(boolprint) std::cout << "test 1.2 system" << std::endl;

	{
	    vecs::Registry system;

		//Valid
		{
			vecs::Handle handle;
			check( !handle.IsValid() );
			if(boolprint) std::cout << "Handle size: " << sizeof(handle) << std::endl;
		}

		//Insert, Types, Get, Has, Erase, Exists
		{
			std::string s = "AAA";
			auto hhh = system.Insert(s);
			check( system.Exists(hhh) );

		    vecs::Handle handle = system.Insert(5, 5.5f);
		    check( system.Exists(handle) );
		    auto t1 = system.Types(handle);
			check( t1.size() == 3 ); //also handle!
			std::set<size_t> types{vecs::Type<vecs::Handle>(), vecs::Type<int>(), vecs::Type<float>()};
			std::ranges::for_each( t1, [&](auto& t){ check( types.contains(t) ); } );
		    auto v1 = system.Get<int>(handle);
			check( v1 == 5 );
		    check( system.Has<int>(handle) );
		    system.Erase(handle);
		    check( !system.Exists(handle) );

		    //vecs::Handle hx = system.Insert(5, 6); //compile error
		    struct height_t { int i; }; 
		    struct weight_t { int i; }; 
		    vecs::Handle hx1 = system.Insert(5, height_t{6}, weight_t{6}); //works
		}

		//Exists
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
		    check( system.Exists(handle) );
		    auto t2 = system.Types(handle);
			std::set<size_t> types{vecs::Type<vecs::Handle>(), vecs::Type<int>(), vecs::Type<float>(), vecs::Type<double>()};
			std::ranges::for_each( t2, [&](auto& t){ check( types.contains(t) ); } );	
		}

		//Get
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
			auto value = system.Get<float&>(handle);
			auto f1 = value; 
			value = 10.0f; 
			check( system.Get<float>(handle) == 10.0f );
			auto c = system.Get<char&>(handle); //new component -> change counter goes up
			//float val = value; //access old reference -> error
			//value = 5.0f; //access old reference -> error
			c = 'A';
			check( system.Get<char>(handle) == 'A'); //new component -> change counter goes up

		    auto [v2a, v2b] = system.Get<float, double>(handle);
		    auto [v3a, v3b] = system.Get<float&, double&>(handle);
			v3a = 100.0f;
			v3b = 101.0;
		    auto [v4a, v4b] = system.Get<float, double>(handle);
			check( v4a == 100.0f && v4b == 101.0 );
		}

		//Put
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
		    system.Put(handle, 50, 69.0f, 73.0);
			auto [v5a, v5b, v5c] = system.Get<int, float, double>(handle);
		    check( v5b == 69.0f && v5c == 73.0 );
		    std::tuple<float,double> tup = system.Get<float, double>(handle);
		    std::get<float>(tup) = 101.0f;    
		    std::get<double>(tup) = 102.0;    
		    system.Put(handle, tup);
			auto [v6a, v6b] = system.Get<float, double>(handle);
		    check( v6a == 101.0f && v6b == 102.0 );

		    auto tup2 = system.Get<int, float, double>(handle);
		    int ii = std::get<int>(tup2);
		    auto [ivalue, fvalue, dvalue] = system.Get<int, float, double>(handle);
			check( ivalue == 50 && fvalue == 101.0f && dvalue == 102.0 );
		}

		//Has
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
		    system.Put(handle, 50, 69.0f, 73.0);
		    check( system.Has<int>(handle) );
		    check( system.Has<float>(handle) );
		    check( system.Has<double>(handle) );
		}

		//Erase components
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
		    system.Erase<int, float>(handle); //remove two components
		    check( !system.Has<int>(handle) );
		    check( !system.Has<float>(handle) );
		    check( system.Has<double>(handle) );
		    system.Erase<double>(handle); //remove also the last component
		    check( system.Exists(handle)); //check that the entity still exists
		    check( !system.Has<double>(handle) );
		}

		//Add components with Put
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
			system.Erase<int, float, double>(handle);
		    check( !system.Has<int>(handle) );
		    check( !system.Has<float>(handle) );
		    check( !system.Has<double>(handle) );
			system.Put(handle, 3.9); //add a component of type double
		    check( system.Exists(handle)); //check that the entity still exists
		    check( system.Has<double>(handle) );
			auto d = system.Get<double>(handle);
			auto cc = system.Get<char&>(handle); //
			cc = 'A';
		}

		//Add components with Get
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
			auto dd = system.Get<char>(handle); //
			std::string s = "AAA";
			struct T1 {
				const char* m_str;
			};

			system.Put(handle, s, T1{"BBB"}); //
			auto [ee, ff] = system.Get<std::string, T1>(handle); //
			check( ee == "AAA" && ff.m_str == std::string{"BBB"} );
		}

		//Erase entity
		{
		    auto handle = system.Insert(5, 6.9f, 7.3);
	    	check( system.Exists(handle) );
		    system.Erase(handle);
	    	check( !system.Exists(handle) );
		}
	
		//Add Tags
		{
			auto handle1 = system.Insert(5, 6.9f, 7.3);
			auto handle2 = system.Insert(6, 7.9f, 8.3);
			auto handle3 = system.Insert(7, 8.9f, 9.3);
			system.AddTags(handle1, 1ull, 2ull, 3ull);
			system.AddTags(handle2, 1ull, 3ull);
			system.AddTags(handle3, 2ull, 3ull);
			auto tags = system.Types(handle1);
			check( tags.size() == 7 );
			for( auto handle : system.template GetView<vecs::Handle>(std::vector<size_t>{1}) ) {
				if(boolprint) std::cout << "Handle (yes 1): "<< handle << std::endl;
			}
			for( auto handle : system.template GetView<vecs::Handle>(std::vector<size_t>{1ull}, std::vector<size_t>{2}) ) {
				if(boolprint) std::cout << "Handle (yes 1 no 2): "<< handle << std::endl;
			}
		}

		//Erase Tags
		{
			auto handle = system.Insert(5, 6.9f, 7.3);
			system.AddTags(handle, 1ull, 2ull, 3ull);
			auto tags = system.Types(handle);
			check( tags.size() == 7 );
			system.EraseTags(handle, 1ull);
			tags = system.Types(handle);
			check( tags.size() == 6 );
			system.EraseTags(handle, 2ull, 3ull);
			tags = system.Types(handle);
			check( tags.size() == 4 );
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

		//auto hhh = system.Get<vecs::Handle&>(hd1); //compile error

		if(boolprint) std::cout << "Loop Handle: " << std::endl;
	    for( auto handle : system.template GetView<vecs::Handle>() ) {
	        if(boolprint) std::cout << "Handle: "<< handle << std::endl;
	    }

		if(boolprint) std::cout << "Loop Handle int& float " << std::endl;
	    for( auto [handle, i, f] : system.template GetView<vecs::Handle, int&, float>() ) {
	        if(boolprint) std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
			i = 100;
			f = 100.0f;
			//auto h2 = system.Insert(5, 5.5f);
	    }

		if(boolprint) std::cout << "Loop Handle int& float& " << std::endl;
	    for( auto [handle, i, f] : system.template GetView<vecs::Handle, int&, float&>() ) {
	        if(boolprint) std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
		}

		check( system.Size() > 0 );
	    system.Clear();
	    check( system.Size() == 0 );
	}

	{
	    vecs::Registry system;

		std::vector<vecs::Handle> handles;
		for( int i=0; i<10; ++i ) {
			auto h = system.Insert(i, (float)i, (double)i, 'A', std::string("AAAAAA") );
			handles.push_back(h);
		}
		system.Print();
		for(auto [h, i, f, d] : system.template GetView<vecs::Handle, int&, float, double>() ) {
			if (i==1) {
				system.Erase(handles[2]);
			}
			
			if( i == 5 || i == 6) {
				system.Erase(h);
			}
			system.Print();
		}
		system.Print();
	}


	return 0;   
}


size_t test_insert_iterate( vecs::Registry& system, int m ) {

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


size_t test_insert( vecs::Registry& system, int m ) {

	auto t1 = std::chrono::high_resolution_clock::now();

	for( int i=0; i<m; ++i ) {
		auto h = system.Insert(i, (float)i, (double)i, 'A', std::string("AAAAAA") );
	}
	auto t2 = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
}


size_t test_iterate( vecs::Registry& system, int m ) {

	auto t1 = std::chrono::high_resolution_clock::now();

    for( auto [handle, i, f] : system.template GetView<vecs::Handle, int&, float&>() ) {
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
	int num = 500000;

	{
		if(boolprint) std::cout << "test 3.1 sequential " + name << std::endl;
		vecs::Registry system;
		if(insert) test_insert(system, num);
		duration = job(system, num);
		if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
		system.Clear();
		if(insert) test_insert(system, num);
		duration = job(system, num);
		if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
	}

	{
		if(boolprint) std::cout << "test 3.2 sequential " + name << std::endl;
		vecs::Registry system;
		if(insert) test_insert(system, num);
		duration = job(system, num);
		if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
		system.Clear();
		if(insert) test_insert(system, num);
		duration = job(system, num);
		if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)num << std::endl;
	}

}


void test4( std::string name, bool insert, auto&& job ) {

	vecs::Registry system;

	int num = 500000;
	auto work = [&](auto& system) {
		size_t duration = job(system, num);
	};

	if(insert) test_insert(system, 4*num);

	if(boolprint) std::cout << "test 4.1 parallel " + name << std::endl;
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
		if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)system.Size() << std::endl;
	}

	system.Clear();
	if(insert) test_insert(system, 4*num);

	if(boolprint) std::cout << "test 4.2 parallel " + name << std::endl;
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
		if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)system.Size() << std::endl;
	}
}


template<typename S>
auto SelectRandom(const S &s, size_t n) {
 	auto it = std::begin(s);
 	std::advance(it,n);
 	return it;
}


void test5() {

	if(boolprint) std::cout << "test 5 parallel" << std::endl;

	using system_t = vecs::Registry;
	using handles_t = std::set<vecs::Handle>;
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
		std::set<vecs::Handle> hs;

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
	if(boolprint) std::cout << "Size: " << system.Size() << " us: " << duration << " us/entity: " << (double)duration/(double)(8*num) << std::endl;

}


void test_vecs() {
	test1();
	
	test3( "Insert", false, [&](auto& system, int num){ return test_insert(system, num); } );
	test3( "Iterate", true, [&](auto& system, int num){ return test_iterate(system, num); } );
	test3( "Insert + Iterate", false, [&](auto& system, int num){ return test_insert_iterate(system, num); } );

	//test4( "Insert", false, [&](auto& system, int num){ return test_insert(system, num); } );
	//test4( "Iterate", true, [&](auto& system, int num){ return test_iterate(system, num); } );
	//test4( "Insert + Iterate", false, [&](auto& system, int num){ return test_insert_iterate(system, num); } );
	
	/*for( int i=0; i<1000; ++i ) {
		std::cout << "test 5 " << i << std::endl;
		test5();
	}*/
}

