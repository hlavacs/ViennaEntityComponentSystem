#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include <iostream>
#include <string>

#include "VECS.h"


void check( bool b, std::string_view msg = "" ) {
	if( b ) {
		//std::cout << "\x1b[32m passed\n";
	} else {
		std::cout << "\x1b[31m failed: " << msg << "\n";
		exit(1);
	}
}

void test_handle() {
	std::cout << "\x1b[37m testing handle...";

	{
		vecs::Handle h0;
		vecs::Handle h1{1,2};
		vecs::Handle h2{1,2};
		vecs::Handle h3{1,3};

		vecs::Vector<vecs::Handle> v;
		v.push_back( h0 );

		check( h0.IsValid() == false );
		check( h1.IsValid() == true );
		check( h1.GetIndex() == 1 );
		check( h1.GetVersion() == 2 );
		check( h1.GetStorageIndex() == 0 );
		check( h1 == h2 );
		check( h1 != h3 );
		check( h2 != h3 );
	}

	std::cout << "\x1b[32m passed\n";
}

void test_vector() {
	std::cout << "\x1b[37m testing vector...";
	{
		vecs::Vector<int> vec;
		std::vector<vecs::Vector<int>> v;
		v.push_back( vec );

		vec.push_back( 0 );
		check( vec[0] == 0 );
		check( vec.size() == 1 );
		for( int i=1; i<10000; ++i ) { vec.push_back( i ); }
		check( vec.size() == 10000 );
		for( int i=0; i<10000; ++i ) { check( vec[i] == i ); }
		int j=0;
		for( auto& i : vec ) { check( i == j++ ); }
		while( vec.size() > 0 ) { vec.pop_back(); }
		check( vec.size() == 0 );
		for( int i=0; i<20000; ++i ) { vec.push_back( i ); }
		check( vec.size() == 20000 );
		vec.clear();
		check( vec.size() == 0 );
		for( int i=0; i<30000; ++i ) { vec.push_back( i ); }
		check( vec.size() == 30000 );
		for( int i=0; i<1000; ++i ) { 
			vec.erase( 0 ); 
			check( vec[0] == 30000 - i - 1); 
		}
		check( vec.size() == 29000 );
		for( int i=0; i<1000; ++i ) { 
			vec.erase( i ); 
			check( vec.size() == 29000 - i - 1 ); 
		}
		check( vec.size() == 28000 );

		vec.clear();
		check( vec.size() == 0 );
		for( int i=0; i<30000; ++i ) { vec.push_back( i ); }

		vecs::Vector<int> vec2;
		for( int i=0; i<10000; ++i ) { vec2.copy( &vec, i ); }
		for( int i=0; i<10000; ++i ) { 
			check( vec2[i] == i ); 
		}

		vec.swap( 0, 1 );
		check( vec[0] == 1 );
		check( vec[1] == 0 );

		auto newvec = vec.clone();
		check( newvec->size() == 0 );

		vecs::VectorBase* vb = &vec;
		for( int i=0; i<10000; ++i ) { vb->push_back(); }
	}
	std::cout << "\x1b[32m passed\n";
}

void test_slotmap() {
	std::cout << "\x1b[37m testing slot map...";
	{
		vecs::SlotMap<int> sm(0,6);
		std::vector<vecs::SlotMap<int>> v;
		v.push_back( sm );

		auto [h1, v1] = sm.Insert(1);
		auto [h2, v2] = sm.Insert(2);
		auto [h3, v3] = sm.Insert(3);
		check( sm.Size() == 3 );

		check( sm[h1].m_value == 1 );
		check( sm[h1].m_version == 0 );

		check( sm[h2].m_value == 2 );
		check( sm[h1].m_version == 0 );

		check( sm[h3].m_value == 3 );
		check( sm[h1].m_version == 0 );

		sm.Erase( h1 );
		sm.Erase( h2);
		check( sm.Size() == 1 );
		check( sm[h3].m_value == 3 );

		sm.Clear();
		check( sm.Size() == 0 );

		std::vector<vecs::Handle> handles;
		for( int i=0; i<10000; ++i ) { handles.push_back( sm.Insert(i).first ); }
		check( sm.Size() == 10000 );

		for( auto& h : handles ) { sm.Erase( h ); }
		check( sm.Size() == 0 );

		handles.clear();
		for( int i=0; i<10000; ++i ) { handles.push_back( sm.Insert(i).first ); }
		check( sm.Size() == 10000 );
		for( int i=0; i<10000; ++i ) { check( sm[handles[i]].m_value == i ) ; }

	}
	std::cout << "\x1b[32m passed\n";
}

void test_archetype() {
	std::cout << "\x1b[37m testing archetype...";

	{
		vecs::Archetype arch;
		arch.AddComponent<int>();
		arch.AddComponent<float>();
		arch.AddComponent<char>();
		arch.AddComponent<double>();
		arch.AddComponent<std::string>();

		arch.AddValue( vecs::Handle{1,2} );
		arch.AddValue( 1 );
		arch.AddValue( 2.0f );
		arch.AddValue( 'a' );
		arch.AddValue( 3.0 );
		arch.AddValue( std::string("hello") );

		check( arch.Size() == 1 );
		check( arch.Get<int>(0) == 1 );
		check( arch.Get<float>(0) == 2.0f );
		check( arch.Get<char>(0) == 'a' );
		check( arch.Get<double>(0) == 3.0 );
		check( arch.Get<std::string>(0) == "hello" );

		arch.Clear();
		check( arch.Size() == 0 );

		arch.Insert( vecs::Handle{1,2}, 1, 2.0f, 'a', 3.0, std::string("hello") );
		check( arch.Size() == 1 );
		check( arch.Get<int>(0) == 1 );
		check( arch.Get<float>(0) == 2.0f );
		check( arch.Get<char>(0) == 'a' );
		check( arch.Get<double>(0) == 3.0 );
		check( arch.Get<std::string>(0) == "hello" );

		arch.Insert( vecs::Handle{2,3}, 2, 3.0f, 'b', 4.0, std::string("world") );
		check( arch.Size() == 2 );
		check( arch.Get<int>(0) == 1 );
		check( arch.Get<float>(0) == 2.0f );
		check( arch.Get<char>(0) == 'a' );
		check( arch.Get<double>(0) == 3.0 );
		check( arch.Get<std::string>(0) == "hello" );

		check( arch.Get<int>(1) == 2 );
		check( arch.Get<float>(1) == 3.0f );
		check( arch.Get<char>(1) == 'b' );
		check( arch.Get<double>(1) == 4.0 );
		check( arch.Get<std::string>(1) == "world" );

		arch.Erase( 0 );
		check( arch.Size() == 1 );
		check( arch.Get<int>(0) == 2 );
		check( arch.Get<float>(0) == 3.0f );
		check( arch.Get<char>(0) == 'b' );

		arch.Erase( 0 );
		check( arch.Size() == 0 );
	}

	{
		vecs::Archetype arch;
		arch.AddComponent<int>();
		arch.AddComponent<float>();
		arch.AddComponent<char>();
		arch.AddComponent<double>();
		arch.AddComponent<std::string>();

		vecs::Archetype::m_iteratingArchetype = &arch;
		vecs::Archetype::m_iteratingIndex = 5;

		auto add = [&](int i){
			arch.AddValue( vecs::Handle{1,(size_t)i} );
			arch.AddValue( i );
			arch.AddValue( 2.0f*i );
			arch.AddValue( 'c' );
			arch.AddValue( 3.0*i );
			arch.AddValue( std::string("hello....") );
		};

		for( int i=0; i<10; ++i ) { 
			add(i); 
		}
		check( arch.Size() == 10 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		arch.Erase( 6 );
		check( arch.Size() == 9 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		arch.Erase( 6 );
		check( arch.Size() == 8 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		arch.Erase( 6 );
		check( arch.Size() == 7 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		arch.Erase( 6 );
		check( arch.Size() == 6 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();

		vecs::Archetype::m_iteratingIndex = 5;
		arch.Erase( 1 );
		check( arch.Size() == 5 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		arch.Erase( 2 );
		check( arch.Size() == 4 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		arch.Erase( 0 );
		check( arch.Size() == 3 );
		std::cout << "\nArchetype size: " << arch.Size() << std::endl;
		arch.Print();
		vecs::Archetype::m_gaps.clear();
	}

	{
		vecs::Archetype arch, arch2;
		arch.AddComponent<int>();
		arch.AddComponent<float>();
		arch.AddComponent<char>();
		arch.AddComponent<double>();
		arch.AddComponent<std::string>();

		arch2.AddComponent<int>();
		arch2.AddComponent<float>();
		arch2.AddComponent<char>();
		arch2.AddComponent<double>();

		arch2.Insert( vecs::Handle{1,2}, 1, 2.0f, 'a', 3.0 );
		check( arch2.Size() == 1 );
		check( arch2.Get<int>(0) == 1 );
		check( arch2.Get<float>(0) == 2.0f );
		check( arch2.Get<char>(0) == 'a' );
		check( arch2.Get<double>(0) == 3.0 );

		arch2.Insert( vecs::Handle{2,3}, 2, 3.0f, 'b', 4.0 );
		check( arch2.Size() == 2 );
		check( arch2.Get<int>(1) == 2 );
		check( arch2.Get<float>(1) == 3.0f );
		check( arch2.Get<char>(1) == 'b' );
		check( arch2.Get<double>(1) == 4.0 );

		auto [index, handle] = arch.Move( arch2, 0 );
		check( arch.Size() == 1 );
		check( arch2.Size() == 1 );
		check( index == 0 );
		check( handle == vecs::Handle{2,3} );
		check( arch.Get<int>(0) == 1 );
		check( arch.Get<float>(0) == 2.0f );
		check( arch.Get<char>(0) == 'a' );
		check( arch.Get<double>(0) == 3.0 );

		auto [index1, handle1] = arch.Move( arch2, 0 );
		check( arch.Size() == 2 );
		check( arch2.Size() == 0 );	
		check( index1 == 1 );
		check( handle1 == vecs::Handle{} );
		check( arch.Get<int>(1) == 2 );
		check( arch.Get<float>(1) == 3.0f );
		check( arch.Get<char>(1) == 'b' );
		check( arch.Get<double>(1) == 4.0 );

		vecs::Archetype arch3;
		arch3.Clone( arch, std::vector<size_t>{} );
		check( arch3.Size() == 0 );
		check( arch3.Has( vecs::Type<vecs::Handle>() ) == true );
		check( arch3.Has( vecs::Type<int>() ) == true );
		check( arch3.Has( vecs::Type<float>() ) == true );
		check( arch3.Has( vecs::Type<char>() ) == true );
		check( arch3.Has( vecs::Type<double>() ) == true );
		check( arch3.Has( vecs::Type<std::string>() ) == true );

		vecs::Archetype arch4;
		arch4.Clone( arch, std::vector<size_t>{vecs::Type<int>(), vecs::Type<double>()} );
		check( arch4.Size() == 0 );
		check( arch4.Has( vecs::Type<vecs::Handle>() ) == true );
		check( arch4.Has( vecs::Type<int>() ) == false );
		check( arch4.Has( vecs::Type<float>() ) == true );
		check( arch4.Has( vecs::Type<char>() ) == true );
		check( arch4.Has( vecs::Type<double>() ) == false );
		check( arch4.Has( vecs::Type<std::string>() ) == true );

	}
	std::cout << "\x1b[32m passed\n";

}

void test_mutex() {

}

void test_vecs();

void test_registry() {
	std::cout << "\x1b[37m testing registry...";

	test_vecs();
	std::cout << "\x1b[32m passed\n"; 

}

void test_conn() {
	std::cout << "\x1b[37m testing Connection!...\n";

	//add usage here?
	// 
	// create a populated registry
	vecs::Registry system;
	vecs::Handle h1 = system.Insert(5, 3.0f, 4.0);
	vecs::Handle h2 = system.Insert(1, 23.0f, 3.0);

	system.AddTags(h1, 47ul);
	system.AddTags(h2, 666ul);

	vecs::Handle h3 = system.Insert(6, 7.0f, 8.0);
	vecs::Handle h4 = system.Insert(2, 24.0f, 4.0);

	struct height_t { int i; };
	using weight_t = vsty::strong_type_t<int, vsty::counter<>>;
	vecs::Handle hx1 = system.Insert(height_t{ 5 }, weight_t{ 6 });

	std::vector<vecs::Handle> handles;
	// create 20 handles
	for (int i = 10; i < 30; i++) {
		handles.push_back(system.Insert(i, static_cast<float>(i * 2)));
	}
	// erase one of them, leaving 19
	system.Erase(handles[4]);
	handles.erase(handles.begin() + 4);

	std::cout << "\x1b[37m isConnected: " << system.isConnected() << "\n";
	SOCKET testval = system.connectToServer();
	std::cout << "\x1b[37m isConnected: " << system.isConnected() << "\n";

	if (system.isConnected()) {

		// do nothing for 600 seconds, let background task work
		for (int secs = 0; secs < 600; secs++) {
			// test for dynamic scaling of entity graph in Console LiveView
			if (secs == 80) {
				for (auto hit = std::prev(handles.end()); hit > handles.begin() + 18; hit--)
					system.Erase(*hit);
				handles.erase(handles.begin() + 19, handles.end());
			}
			else if (secs < 80) {
				handles.push_back(system.Insert(secs + 1000, static_cast<float>(secs * 7)));
				handles.push_back(system.Insert(secs + 1000, static_cast<float>(secs * 7)));
			}
			// alternating "add 2 at end, remove 2 at front"
			if (secs & 1) {
				system.Erase(handles[0]); handles.erase(handles.begin());
				system.Erase(handles[1]); handles.erase(handles.begin() + 1);
			}
			else {
				handles.push_back(system.Insert(secs + 20, static_cast<float>(secs * 2)));
				handles.push_back(system.Insert(secs + 15, static_cast<float>(secs * 3)));
			}

#ifdef WIN32
			Sleep(1000);
#else
			usleep(1000 * 1000);
#endif
			// ... but get out if console has decided to leave the building
			if (!system.isConnected())
				break;
		}

		system.disconnectFromServer();
	}
	std::cout << "\x1b[37m I hope it works? ...\n";
}

int main() {
	std::cout << "testing VECS...\n";
	test_conn();
	
	test_handle();
	test_vector();
	test_slotmap();
	test_archetype();
	test_mutex();
	test_registry();
	
}

