# Vienna Entity Component System (VECS)

The Vienna Entity Component System (VECS) is a C++20 based ECS for game engines. An ECS is a data structure storing *entities* of different types.
Entity types consist of a number of *components*.
Different entity types can be composed of different components.

An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. In a game engine, typically a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a *physics* system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system *animation* is only interested into position, orientation, and animation data.
Systems do not have to be specially defined, anything that specifies a number of components it is interested in and then loops over all entities that contain these components can be seen as a system.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* Easy to use
* Supports multithreading and parallel accesses.

VECS is a container for entities. Entities are composed of an arbitrary number of components of simple plain old data types (PODs). PODs can be copied, moved, but cannot be pointers or references. They can also be copyable or movable classes like *std::string*.
The standard operation in each game loop is to loop over a *subset* of such components concurrently. 
Data layout is organized in such a way to make optimal use of caches for exactly this main use case.
All entities containing the same component types are grouped into *archetypes*, i.e., separate data structures 
storing these components, each component in continuous memory. Here, empty slots are never allowed,
i.e., if an entity and all its components is erased, the components at the end of the data structure are moved into the now 
empty slots to fill up the space. 
Thus when iterating over *N* components, accessing each component can make optimal use of cache prefetching, i.e.,
hiding slow data transfers from main memory by loading the data up front before being actually accessed.

VECS internally uses the following data structures:
* *Vector*: a container like a *std::vector*, but using segments to store data. Inside a segment, data is stored contiguously. Pointers to data are invalidated only if data is moved or erased. 
* *SlotMap*: a map that maps an integer index to an archetype and an index inside the archetype. *SlotMap* is based on *Vector* and **never shrinks**. Each entry also contains a *version* number, which is increased 
each time an entity is erased from VECS.
* *Handle*: Handles identify entities. For this, they contain an integer *index* into the SlotMap, and a *version* number. Handles point to existing entities only if their version numbers match. A handle points to an erased entity if its version number does not match the SlotMap version number.
* *ComponentMap*: is based on *Vector* and stores one specific data type.
* *Archetype*: Contains all component maps of entities having the same set of component types.
* *View*: allows to select a subset of component types and entities and can create Iterators for looping.
* *Iterator*: can be used to loop over a subset of entities and component types.
* *Ref\<T>* is like a C++ reference to a data component, but is based on the SlotMap entry and autmatically finds components, even if their entities have been moved to other archetypes.
* *Registry*: the main class representing a container for entities. Programs can hold an arbitrary number of
Registry instances any time, they do not interfere with each other.
* *LockGuard* and *LockGuardShared*: used to protect data structures when compiled for parallel mode. Are empty when compiled for sequential mode.


## The VECS Include Files

VECS is a header only library, consisting only of header files. Include *VECS.h* into your source code to use VECS. VECS depends on other header only projects:
* Vienna Type List Library (VTLL), see https://github.com/hlavacs/ViennaTypeListLibrary.
* Vienna Strong Type (VSTY), see https://github.com/hlavacs/ViennaStrongType.

Both are automatically downloaded through *CMake* configuration:

```
cmake -S . -Bbuild -A x64
cd build
cmake --build . --config Release
cmake --build . --config Debug
cd ..
```


## VECS Usage

You can create an ECS with such a statement:

```C
#define REGISTRYTYPE_SEQUENTIAL //is the defualt, so you dont need it
#include "VECS.h"

int main() {
    vecs::Registry system; 
}
```
Now *system* is a container of entities and their components. 
VECS can be compiled in two versions by defining a macro before including it. In REGISTRYTYPE_SEQUENTIAL mode (default if none is defined) all operations are supposed to be done sequentially, there is no internal synchronization taking place. In REGISTRYTYPE_PARALLEL mode VECS uses mutexes to protect internal data structures from corruption, when access are done from multiple threads concurrently (currently NOT implemented). 

Entities can be created by calling *Insert()*:

```C
vecs::Handle h1 = system.Insert(5, 3.0f, 4.0);
```

Component values and their types are automatically determined by the function parameter list. In the example an entity is created that holds an integer, a float and a double as components, with the respective values. Note that you can use types only **once**, since access to components is type based. Thus, if you want to include components with the same type, wrap them into *structs* like so:

```C
//vecs::Handle hx = system.Insert(5, 6); //compile error beacuse of two ints
struct height_t { int i; }; 
using weight_t = vsty::strong_type_t<int, vsty::counter<>>; 
vecs::Handle hx1 = system.Insert(height_t{5}, weight_t{6}); //compiles
```
In the second case, the Vienny Strong Type is used, which is a convenience wrapper needing explicit construction and allowing implicit conversion, integer value partitioning, default values, testing for NULL values, etc. See the VSTY library for its usage. The counter makes sure that internally always a new type is created.

Entities always contain their handle as a component. Thus it is not possible to additionally insert components of type *Handle*. If you need to insert handles, wrap them into a struct.

Do not forget to use type names that describe the intent of the new type, not its type itself. So if you need another integer for storing the height of a person, name it *height_t* rather than *myint_t*. You can check whether an entity still exists by calling *Exists(handle)*. You can get a reference to a *std::set* holding *std::size_t* representing the component types that a given entity has by calling *Types()*. You can check whether an entity has a specific component type *T* by calling *Has\<T>(handle)*. You can erase an entity and all its components by calling *Erase(handle)*. You can also erase individual components *T1, T2, ...* by calling *Erase<T1, T2, ...>(handle)*. Note that it is perfectly fine to remove all components from an entity. This does not remove the entity itself, and you can afterwards add new components to it. Call *Clear()* to remove all entities from the registry.

```C
vecs::Handle h1 = system.create(5); //create a new entity with one int component

assert( system.Exists(h1) ); //check that it exists
auto t1 = system.Types(h1); //get list of types (vector with type_index)
assert( system.Has<int>(h1) ); //check whether an entity has a component with a given type
system.Erase(h1); //erase an entity
assert( !system.Exists(h1) ); //

vecs::Handle h2 = system.Insert(5, 6.9f, 7.3);; //create a new entity with int, float and double components
system.Erase<int, float>(h2); //erase integer and float components
assert( !system.Has<int>(h2) ); //check whether the components are gone
assert( !system.Has<float>(h2) );
assert( system.Has<double>(h2) );
system.Erase<double>(h2); //remove also the last component
assert( system.Exists(h2) ); //check that the entity still exists

assert( system.Size() > 0 );
system.Clear(); //clear the system
assert( system.Size() == 0 );
```

## References

You can get the current value of type *T* of an entity by calling *Get\<T>(handle)*. Here *T* is neither a pointer nor a reference.

Obtaining pure C++ references is not possible in VECS. Instead, you can get a reference object *Ref\<T>* to a component by calling *Get<T&>(handle)*. Reference objects can be used like normal references. They track the component's location are react accordingly. 
However, if it accesses an erased entity or erased component, the program is aborted with an error.

```C
vecs::Handle h2 = system.Insert(5, 6.9f, 7.3);; //create a new entity with int, float and double components
auto value = system.Get<float&>(h2);    //get Ref<float>
float f1 = value; //get value
value = 10.0f; //set value
auto c = system.Get<char&>(handle); //new component -> all components are moved but reference is still valid
```

If you specify more than one type, you can get a tuple holding the specified types or references (reference objects). You can easily access all component values by using C++17 *structured binding*. Calling *Get\<T>(handle)* on a type *T* that is not yet part of the entity will also create an empty new component for the entity.

```C
vecs::Handle h2 = system.Insert(5, 6.9f, 7.3);; //create a new entity with int, float and double components
float value = system.Get<float>(h2);    //get float value directly
std::tuple<float,double> tup = system.Get<float, double>(h2); //returns a std::tuple<float,double>
float value2 = std::get<float>(tup);    //access the value from the tuple
auto [fvalue, dvalue] = system.Get<float&, double>(h2); //structured binding. fvalue is now a Ref to the component!!
auto cc = system.Get<char&>(h2); 	//Create char component and return a reference to it (note the &)!
cc = 'A'; //can change values
auto dd = system.Get<char>(h2); 	//the value has changed
```

You can update the value of a component by calling *Put(handle, values...)*. You can call *Put(handle, values...)* using the new values directly, or by using a tuple as single value parameter. This way, you can reuse the same tuple that you previously extracted by calling *Get<T1,T2,...>(handle)*. 

```C
vecs::Handle h2 = system.Insert(5, 6.9f, 7.3); //create a new entity with int, float and double components
assert( system.Get<float>(h2) == 6.9f && system.Get<double>(h2) == 7.3 );    //check float and double values
system.Put(h2, 66.9f, 77.3);    //change float and double values
assert( system.Get<float>(h2) == 66.9f && system.Get<double>(h2) == 77.3 ); //check new values

std::tuple<float,double> tup = system.Get<float, double>(h2); //returns a std::tuple<float,double>
assert( std::get<float>(tup) == 66.9f && std::get<double>(tup) == 77.3);    //access the values from the tuple
std::get<float>(tup) == 666.9f;  //change tuple values
std::get<double>(tup) == 777.3;
system.Put(h2, tup); //store new values in the ECS
auto [fvalue, dvalue] = system.Get<float,double>(h2); //access the same entity
assert( fvalue == 666.9f && dvalue == 777.3);    //check the values

auto h2 = system.Insert(5, 6.9f, 7.3);
assert( system.Exists(h2) );
auto t2 = system.Types(h2);
```

Of course you can always change component values by getting and changing references calling *Get<T1&,T2&,...>(handle)*, which returns a tuple with Ref objects. Calling it on a new type will again create this component for the entity and set its value accordingly.

```C
auto [v2a, v2b] = system.Get<float, double>(h2);
auto [v3a, v3b] = system.Get<float&, double&>(h2); //tuple with Refs
v3a = 100.0f;
v3b = 101.0;
auto [v4a, v4b] = system.Get<float, double>(h2); //check new values
```

*std::string* can be included directly, *const char** must be wrapped into a struct.

```C
std::string s = "AAA";
struct T1 {
	char* m_str;
};
system.Put(h2, s, T1{"BBB"}); //
auto [ee, ff] = system.Get<std::string, T1>(h2); //
```

## Strong Types in Ref\<T> Objects
If you use *VSTY* strong types, this means another onion layer of containment for the true value. Accessing it inside a *Ref\<T>* object follows some rules. The call *operator()* now returns the strong type *value*, not the strong type itself. This way, you only need one () instead of two. The *Value()* function does the same, while the *Get()* function returns the *strong type*, not the value of the strong type. If you want to read or write struct members, the compiler may force you to use teh call operator(), in order to make sure that you nmean the value of the Ref\<T> object.


```C
    struct test_struct {
    	int i;
    	float f;
    };
    using strong_struct = vsty::strong_type_t<test_struct, vsty::counter<>>;
    using strong_int = vsty::strong_type_t<int, vsty::counter<>>;

    ...

	strong_int si{5};
	strong_struct ss{{10, 6.9f}};
	auto handle = system.Insert(si, ss);
	check( system.Has<vecs::Handle>(handle) );
	auto [rsi, rss] = system.Get<strong_int&, strong_struct&>(handle);
	int i = rsi; //implicit type conversion
	strong_struct ss2 = rss;
	rss().i = 100; //use call operator()
	check( system.Get<strong_struct>(handle)().i == 100 );
	rss.Value().i = 101; //use Value() for strong type
	check( system.Get<strong_struct>(handle)().i == 101 );
	rss().f = 101.0f;
	check( system.Get<strong_struct>(handle)().f == 101.0f );
	rss.Value().f = 102.0f;
	check( system.Get<strong_struct>(handle)().f == 102.0f );
	auto mewss = system.Get<strong_struct&>(handle);
	mewss.Value().i = 103;
	check( system.Get<strong_struct>(handle)().i == 103 );
	mewss.Get() = {104, 204.0f}; //access to strong type
	check( system.Get<strong_struct>(handle)().i == 104 );
	check( system.Get<strong_struct>(handle)().f == 204.0f );
```


## Iteration

You can iterate over all components of a given type creating a *View*. The view covers all entities that hold all components of the specified types, and you can iterate over them using a standard C++ range based for loop. The following example creates views with one or three types and then iterates over all entities having these components. In the second loop, we get references and thus could also update the component values by iterating over them.

```C
for( auto handle : system.GetView<vecs::Handle>() ) { //iterate over ALL entities
    std::cout << "Handle: "<< handle << std::endl;
}

for( auto [handle, i, f] : system.GetView<vecs::Handle, int&, float&>() ) {
    std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
	i = 10; //write new value
}
```

Note that if you want the handle of the entity you are currently accessing, you have to explicitly include the *vecs::Handle* type also into the view. Also note that trying to get a handle reference results in a compile error.

```C
for( auto [handle, i, f] : system.GetView<vecs::Handle, int&, float&>() ) { //works
    std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
}

for( auto [handle, i, f] : system.GetView<vecs::Handle&, int&, float&>() ) { //compile error
    std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
}
```

Inside the for loop you can do everything as long as VECS is running in *sequential mode*. Nevertheless, of course erasing entities might result in crashes if systems still try to access them. Systems can check if entities still exist using the *Exists(handle)* function, this also works for Ref\<T> objects. VECS does not use C++ *std::optional* intentionally since accessing erased entities should never occur which lies in the responsibility of the programmer.

## Tags

Entities can be decorated with tags, which are simply *uint64_t* numbers. You can add tags to an entity with the function *AddTag()*, you can remove tags using *EraseTags()*. The *GetView()* function allows for zero, 1 or 2 parameters. If one parameter is given, then this is reference to a *std::vector<uint64_t>* having a positive tag list, i.e., only those entities that have these tags attached will be iterated over. If a second parameter is given, then this is a negative tag list, i.e., only those entities that do not have these tags will be iterated over. 

```C
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

for( auto handle : system.template GetView<vecs::Handle>(std::vector<size_t>{1ul}) ) {
	std::cout << "Handle (yes 1): "<< handle << std::endl; //finds two entities 
}

for( auto handle : system.template GetView<vecs::Handle>(std::vector<size_t>{1ul}, std::vector<size_t>ul}) ) {
	std::cout << "Handle (yes 1 no 2): "<< handle << std::endl; //finds only one entity
}

auto handle = system.Insert(5, 6.9f, 7.3);
system.AddTags(handle, 1ul, 2ul, 3ul);
system.EraseTags(handle, 1ul);

```

## Parallel Usage
Parallel usage at this point is not possible. Make sure to externally synchronize VECS.

