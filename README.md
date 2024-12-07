# Vienna Entity Component System (VECS)

The Vienna Entity Component System (VECS) is a C++20 based ECS for game engines. An ECS is a data structure storing *entities* of different types.
Entity types consist of a number of *components*.
Different entity types can be composed of different components.

An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. Additionally, a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a physics system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system animation is only interested into position, orientation, and animation data.
Systems do not have to be specially defined, anything that specifies a number of components it is interested in and then loops over all entities that contain these components can be seen as a system.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* Easy to use


## The VECS Include File

VECS is a header only library, consisting of the header file *VECS.h*.  Include this file into your source code.
VECS depends on another projects named the Vienna Type List Library (VTLL), see https://github.com/hlavacs/ViennaTypeListLibrary. This project is included as Git submodule. When cloning the project make sure to run 

git submodule init\
git submodule update

in order to also clone VTLL. In tests\testlib.cpp you can find examples on how to use VECS.


## VECS Usage

You can create an ECS with such a statement:

```C
 #include "VECS.h"

int main() {
    vecs::Registry<vecs::REGISTRYTYPE_SEQUENTIAL> system; //Either REGISTRYTYPE_SEQUENTIAL or REGISTRYTYPE_PARALLEL
}
```
Now *system* is a container of entities and their components. 
VECS can be compiled in two versions. In REGISTRYTYPE_SEQUENTIAL mode all operations are supposed to be done sequentially, there is no internal synchronization taking place. In REGISTRYTYPE_PARALLEL mode VECS uses mutexes to protect internal data structures from corruption, when access are done from multiple threads concurrently. 

Entities can be created by calling *Insert()*:

```C
vecs::Handle h1 = system.Insert(5, 3.0f, 4.0);
```

Component values and their types are automatically determined by the function parameter list. In the example an entity is created that holds an integer, a float and a double as components, with the respective values. Note that you can use types only **once**, since access to components is type based. Thus, if you want to include components with the same type, wrap them into *structs* like so:

```C
//vecs::Handle hx = system.Insert(5, 6); //compile error
struct height_t { int i; }; 
struct weight_t { int i; }; 
vecs::Handle hx1 = system.Insert(height_t{5}, weight_t{6}); //compiles
```

Entities always contain their handle as a component. Thus it is not possible to additionally insert components of type *Handle*. If you need to insert handles, wrap them into a struct.

Do not forget to use type names that describe the intent of the new type, not its type itself. So if you need another integer for storing the height of a person, name it *height_t* rather than *myint_t*. You can check whether an entity still exists by calling *Exists(handle)*. You can get a reference to a *std::set* holding *std::size_t* representing the component types that a given entity has by calling *Types()*. You can check whether an entity has a specific component type *T* by calling *Has<T>(handle)*. You can erase an entity and all its components by calling *Erase(handle)*. You can also erase individual components *T1, T2, ...* by calling *Erase<T1, T2, ...>(handle)*. Note that it is perfectly fine to remove all components from an entity. This does not remove the entity itself, and you can afterwards add new components to it. Call *Clear()* to remove all entities from the system.

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

You can get the current value of type *T* of an entity by calling *Get<T>(handle)*. You can get a reference to a component by calling *Get<T&>(handle)*. 
Note that such a reference is not a standard C++ reference but instead an instance of Class *Ref<T>*, which is a wrapper around *T&*. The wrapper ensures that as long as an entity is not erased, a reference to one of its components is never invalidated. This is due to the fact that components may change their place in memory due to the following operations:
* Components are added to or erased from an entity.
* Erasing an entity might cause another entity to be moved in order to fill the gap left by the erased entity.
A *Ref<T>* automatically detects such operations and resets the reference to the correct memory location. 
Note that *Ref<T>* offers an *operator()* and a conversion operator that in most cases conceal the fact that this is not a C++ reference. However, in some cases, it might be nevessary to call either this operator or the function *Get()*.

```C
vecs::Handle h2 = system.Insert(5, 6.9f, 7.3);; //create a new entity with int, float and double components
auto value = system.Get<float&>(h2);    //get Ref<float>
float f1 = value; //implicit conversion
float f2 = value(); //call operator() explicitly
float f3 = value.Get(); //call Get() instead
value = 10.0f; //implicit operator()
```

If you specify more than one type, you can get a tuple holding the specified types or references. You can easily access all component values by using C++17 "structured binding". Calling *Get<T>(handle)* on a type *T* that is not yet part of the entity will also create an empty new component for the entity.

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

Inside the for loop you can do everything as long as VECS is running in sequential mode. In the next section the parallel mode is discussed.

## Parallel Processing

VECS can be compiled to run in parallel mode. This means that it can be accessed from several threads in parallel. VECS uses two types of mutexes to ensure no corruption of its data structures is done:
* The registry uses a mutex to protect its global structures, i.e., the slot map and the map of archetypes.
* Each archetype has its own mutex.

Mutexes can be used in *shared mode* or in *exclusive mode*. Shared mode creates a read lock, i.e., many readers are allows to access the data in parallel, but no writer is allowed. A writer must lock in exclusive mode, and during this period, no othr reader or writer is allowed. Shared locks are fast and thus VECS prioritises them as much as possible for frequent operations, like iterating through entities. When using range based for loops, VECS read locks each archetype when the iterator enters the archetype, and unlocks it when leaving. This make iterating fast, but bears the potential for deadlocks when entities in the current archetype are accessed by others. E.g., if thread A runs a range based for loop and enters an archetype, it is immediately read locked. Thread B might want to add a new entity to the same archetype, and gets a write lock to the system, then waits for a write lock to the archetype. Thread A now wants to read lock the system, but the system is already locked by thread B. Thus both threads wait for each other.

To mitigate this, VECS provides the possibility for scheduling a *delayed transaction*, i.e., a function that is carried out right after the iterator unlocks the current archetype. This is done in the following example:

```C
//create view and iterator, start with the first non-empty archetype that holds the data types, make read lock
for( auto [handle, i, f] : system.template GetView<vecs::Handle, int&, float>() ) {
    std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
	i = 100;
	f = 100.0f;
	system.DelayTransaction( [&](){  //carried out when archetype is changed or end is reached
		std::cout << "Delayed Insert" << std::endl;
		auto h = system.Insert(5, 5.5f);
	} );
	//auto h2 = system.Insert(5, 5.5f); // can cause deadlocks in parallel mode
}
```
Of course this code also works in sequential mode, and it makes sense to use such operations in both modes to keep the code consistent.

Note also that a new element is added each loop, which bears the potential of staying in the loop forever if the element is added to the same archetype the iterator is currently in. VECS mitigates this problem by limiting the loop count for each archetype to the number of entities in them when the view is created. 

VECS mutexes protect agains data structure corruption, but they do not protect each component from data races. Parallel writes to the same components can occur and must be synchronized externally. In a video game this is traditionally done with a *directed acyclic graph (DAG)* that interconnects each system working on the ECS. In order to create this graph, each system must announce beforehand which resources it wants to read from and write to. With VECS two ways are possible.

* In the easy way, each system only declares the component types it wants to read/write to coming from the *iterator in the for loop*.
	** If the system does nothing else than iterating (no entity erased, no components added or erased), then those systems are not in a conflict with each other, i.e., they can read from the same components, but write to different components, can easily be determined and run in parallel. In these cases, delaying transactions is not necessary, since the corresponding operations do not occur.
	** If systems do carry out operations other than iterating, then deadlocks can occur and delaying transactions ist necessary.

* In the sophisticated mode, systems would also declare which entity types they would erase, or add/erase components from. Since these are writing operations, it would severly restrict the number of systems that can run in parallel, but would not need any delayed transaction since one system would not write over or change/move entities that currently belong to another system. 


