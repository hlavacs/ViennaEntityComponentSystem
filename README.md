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
vecs::Handle h1 = system.create(5, 3.0f, 4.0);
```

Component values and their types are automatically determined by the function parameter list. In the example an entity is created that holds an integer, a float and a double as components, with the respective values. Note that you can use types only **once**, since access to components is type based. Thus, if you want to include components with the same type, wrap them into *structs* like so:

```C
//vecs::Handle hx = system.create(5, 6); //compile error
struct height_t { int i; }; 
struct weight_t { int i; }; 
vecs::Handle hx1 = system.create(5, height_t{6}, weight_t{7}); //compiles
```

Do not forget to use type names that describe the intent of the new type, not its type itself. So if you need another integer for storing the height of a person, name it *height_t* rather than *myint_t*.

You can check whether an entity still exists by calling *exists()*. You can get a vector with type_index integers holding a list of component types that a given entity has by calling *types()*. You can check whether an entity has a specific component type *T* by calling *has<T>()*. You can erase an entity and all its components by calling *erase()*. You can also erase individual components *T1, T2, ...* by calling *erase<T1, T2, ...>()*. Note that it is perfectly fine to remove all components from an entity. This does not remove the entity itself, and you can afterwards add new components to it. Call *clear()* to remove all entities from the system.

```C
vecs::Handle h1 = system.create(5); //create a new entity with one int component

assert( system.exists(h1) ); //check that it exists
auto t1 = system.types(h1); //get list of types (vector with type_index)
assert( system.has<int>(h1) ); //check whether an entity has a component with a given type
system.erase(h1); //erase an entity
assert( !system.exists(h1) ); //

vecs::Handle h2 = system.create(5, 6.9f, 7.3);; //create a new entity with int, float and double components
system.erase<int, float>(h2); //erase integer and float components
assert( !system.has<int>(h2) ); //check whether the components are gone
assert( !system.has<float>(h2) );
assert( system.has<double>(h2) );
system.erase<double>(h2); //remove also the last component
assert( system.exists(h2) ); //check that the entity still exists

assert( system.size() > 0 );
system.clear();
assert( system.size() == 0 );
```

Accessing components can be done by calling *get()*. This causes the values of the components to be copied. If you access only one component, the return value is not embedded into a *tuple*. If you access more than one component, then a tuple is returned holding the component values. You can easily access all component values by using C++17 "structured binding".

```C
vecs::Handle h2 = system.create(5, 6.9f, 7.3);; //create a new entity with int, float and double components
float value = system.get<float>(h2);    //get float value directly
std::tuple<float,double> tup = system.get<float, double>(h2); //returns a std::tuple<float,double>
float value2 = std::get<float>(tup);    //access the value from the tuple
auto [fvalue, dvalue] = system.get<float, double>(h2); //structured binding
```

You can update the value of a component by calling *put()*. You can call put using the new values directly, or by using a tuple as single value parameter. This way, you can reuse the same tuple that you previously extracted by calling *get()*.

```C
vecs::Handle h2 = system.create(5, 6.9f, 7.3);; //create a new entity with int, float and double components
assert( system.get<float>(h2) == 6.9f && system.get<double>(h2) == 7.3 );    //check float and double values
system.put(h2, 66.9f, 77.3);    //change float and double values
assert( system.get<float>(h2) == 66.9f && system.get<double>(h2) == 77.3 ); //check new values

std::tuple<float,double> tup = system.get<float, double>(h2); //returns a std::tuple<float,double>
assert( std::get<float>(tup) == 66.9f && std::get<double>(tup) == 77.3);    //access the values from the tuple
std::get<float>(tup) == 666.9f;  //change tuple values
std::get<double>(tup) == 777.3;
system.put(h2, tup); //store new values in the ECS
auto [fvalue, dvalue] = system.get<float,double>(h2); //access the same entity
assert( fvalue == 666.9f && dvalue == 777.3);    //check the values
```

You can iterate over all components of a given type creating a view. The view covers all entities that hold all components of the specified types, and you can iterate over them using a standard C++ for loop. The following example creates views with one or three tyes and then iterates over all entities having these components:

```C
for( auto handle : system.view<vecs::Handle>() ) { //iterate over ALL entities
    std::cout << "Handle: "<< handle << std::endl;
}

for( auto [handle, i, f] : system.view<vecs::Handle, int, float>() ) {
    std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
}
```

Note that if you want the handle of the entity you are currently accessing, you have to explicitly include the *vecs::Handle* type also into the view. 


