#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include <print>

#include "VECS.h"

#include <VECSHandle.h>
#include <VECSVector.h>
#include <VECSSlotMap.h>

void check( bool b, std::string_view msg = "" ) {
	if( b ) {
		//std::cout << "\x1b[32m passed\n";
	} else {
		//std::cout << "\x1b[31m failed: " << msg << "\n";
		assert(false);
	}
}

void test_handle() {
	std::print("\x1b[37m testing handle...");

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

	std::print("\x1b[32m passed\n");
}

void test_vector() {
	std::print("\x1b[37m testing vector...");
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
	std::print("\x1b[32m passed\n");
}

void test_slotmap() {
	std::print("\x1b[37m testing slot map...");
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
	std::print("\x1b[32m passed\n");
}

void test_hashmap() {
	std::print("\x1b[37m testing hash map...");
	{
		vecs::HashMap<int> hm;
		hm[1] = 1;
		hm[2] = 2;
		hm[3] = 3;

		int h1 = hm[1];
		int h2 = hm[2];
		int h3 = hm[3];


	}
	std::print("\x1b[32m passed\n");
}

void test_archetype() {

}

void test_mutex() {

}

void test_registry() {

}


int main() {
	test_handle();
	test_vector();
	test_slotmap();
	test_hashmap();
	test_archetype();
	test_mutex();
	test_registry();
}

