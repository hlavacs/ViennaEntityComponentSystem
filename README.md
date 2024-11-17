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
    vecs::Registry system; //create an ECS
}
```

Now *system* is a container of entities and their components. Entities can be created by calling *create()*:

```C
vecs::Handle h1 = system.Insert(5, 3.0f, 4.0);
```

Component values and their types are automatically determined by the function parameter list. In the example an entity is created that holds an integer, a float and a double as components, with the respective values. Note that you can use types only **once**, since access to components is type based. Thus, if you want to include components with the same type, wrap them into *structs* like so:

```C
//vecs::Handle hx = system.create(5, 6); //compile error
struct height_t { int i; }; 
struct weight_t { int i; }; 
vecs::Handle hx1 = system.Insert(5, height_t{6}, weight_t{7}); //compiles
```

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
system.Clear();
assert( system.Size() == 0 );
```


You can get the current value of type *T* of an entity by calling *Get<T>(handle)*. You can get a reference to a component by calling *Get<T&>(handle)*. Note that such a reference is invalid after any entity of the same archetype has been erased after getting the reference. If you specify more than one type, you can get a tuple holding the specified types or references. You can easily access all component values by using C++17 "structured binding". Calling *Get<T>(handle)* on a type *T* that is not yet past of the entity will also create an empty new component for the entity.

```C
vecs::Handle h2 = system.Insert(5, 6.9f, 7.3);; //create a new entity with int, float and double components
float value = system.Get<float>(h2);    //get float value directly
std::tuple<float,double> tup = system.Get<float, double>(h2); //returns a std::tuple<float,double>
float value2 = std::get<float>(tup);    //access the value from the tuple
auto [fvalue, dvalue] = system.Get<float&, double>(h2); //structured binding. fvalue is now a reference to the component!!
auto& cc = system.Get<char&>(h2); 	//Create char component and return a reference to it (note the &)!
cc = 'A'; //can change values
auto dd = system.Get<char>(h2); 	//the value has changed
```11

You can update the value of a component by calling *Put(handle, value)*. You can call *Put(handle, value)* using the new values directly, or by using a tuple as single value parameter. This way, you can reuse the same tuple that you previously extracted by calling *Get<T1,T2,...>(handle)*. 

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

Of course you can always change component values by getting and changing references calling *Get<T1&,T2&,...>(handle)*, which returns a tuple with references. Calling it on a new type will again create this component for the entity and set its value accordingly.

```C
auto [v2a, v2b] = system.Get<float, double>(h2);
auto [v3a, v3b] = system.Get<float&, double&>(h2); //tuple with references
v3a = 100.0f;
v3b = 101.0;
auto [v4a, v4b] = system.Get<float, double>(h2); //check new values
```

You can iterate over all components of a given type creating a *View*. The view covers all entities that hold all components of the specified types, and you can iterate over them using a standard C++ for loop. The following example creates views with one or three types and then iterates over all entities having these components. In the second loop, we get references and thus could also update the component values by iterating over them.

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
}
```


