# Vienna Entity Component System (VECS)

The Vienna Entity Component System (VECS) is a C++20 based ECS for game engines. An ECS is a data structure storing *entities* of different types.
Entity types consist of a number of *components*.
Different entity types can be composed of different components.

An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. Additionally, a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a physics system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system animation is only interested into position, orientation, and animation data.
Systems do not have to be specially defined, anything that specifies a number of components it is interested in and then loops over all entities that contain these components can be seen as a system.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* Generic compile time definition of entities
* Designed to be used by multiple threads in parallel
* Easy to use, various ways to access the entities and their components
* High performance, components are ordered sequentially for cache friendly update access


## The VECS Include Files

VECS is a header only library, consisting of the following header files:
* *IntType.h*: Template for strong integral types like *index_t* or *counter16_t*. Such types enforce to use them explicitly as function parameters and prevent users from mixing them up with *size_t, int* or *unsigned long*. Also such a type can store a NULL value, and can be tested with *is_valid()*.
* *VECSUtil.h*: Contains utility classes such as a class for implementing CRTP, mono state, and simple low-overhead read and write locks.
* *VECS.h*: The main include file containing most of VECS functionality.
* *VECSIterator.h*: Functionality for iterating over entities in VECS.
* *VECSCompSystem.h*: Examples for component types and entity types used in a game engine itself.
* *VECSCompUser.h*: Examples for components and entities that could be defined by the engine users to store their own entities.
* *VTLL.h*: The Vienna Type List Library is a collection of meta-algorithms for treating type lists and value lists.

VECS depends on two projects, the Vienna Type List Library (VTLL) and Vienna Game Job System (VGJS).

### Vienna Type List Library (VTLL)

As stated, VECS is a compile time ECS. Compositions of entities are defined in source code only. VECS thus heavily relies on its partner project Vienna Type List Library (VTLL), a header-only meta-programming library that helps creating entity types, type lists, compositions of components, accessing list elements, mapping data, etc. Especially, entities are defined as *vtll::type_list<...>* and therefore are nothing more than a list of types. For more information goto https://github.com/hlavacs/ViennaTypeListLibrary.


### Vienna Game Job System (VGJS)

In the parallelization examples, VECS makes use of another partner project, the Vienna Game Job System (VGJS). VGJS is a header-only library for helping spawning threads and submitting jobs to them. VGJS is unique in the sense that you can submit C++ functions, lambdas, function pointers, *std::function*s, vectors containing them, and more notably co-routines enabling easy dependencies between successive jobs. For more information see https://github.com/hlavacs/ViennaGameJobSystem.
VGJS is not necessary to run VECS, and as stated it is only used as an example for multithreading.


## The VECS API

An *Entity Component System* is a data structure that stores data components in some flat way, meaning that data is stored linearly in memory as a vector or table. The stored data is called *components*. Components can by in principle any type that in this case is *movable* and *default constructible*.
This can be plain old data types, structs, classes, (smart) pointers etc.
but not references since they must be bound to something when created. An example is given by

    struct VeUserComponentName {
      std::string m_name;
    };

    struct VeUserComponentPosition {
      float x;
      float y;
      float z;
    };

As can be seen, these two components consist of a std::string and floats, and thus satisfy the conditions on components. Components are then used to define *entities*. Entities are composed of one or more components. This composition is defined using type lists, like

    using VeSystemEntityTypeNode
      = vtll::type_list<VeSystemComponentName, VeSystemComponentPosition>;

which defines the entity type *VeSystemEntityTypeNode* to be composed of the two components.

For defining components and entities, there are two major use cases. If you *create a game engine* yourself, you should define components and entities as belonging to the *system* in file *VECSCompSystem.h*.

If you are a *user of a VECS based engine*, then you can define your own types either in *VECSCompUser.h*, or in your own include files and by defining a macro prevent *VECSCompUser.h* from being loaded. Both cases are internally treated the same way and are discussed in the following.

### Using VECS in the Core of your Game engine

If you create your own game engine and use VECS as its core ECS, simply include *VECS.h* in your CPP files. Also edit *VECSCompSystem.h* to define the components you use, and how entity types are composed of them. Initially, VECS comes with an empty list of system components, and you are free to define them as you like. Components should be structs or other data types that are *movable* and *default constructible*.

    struct VeSystemComponentName {
      std::string m_name;
    };

    struct VeSystemComponentPosition {
      float x;
      float y;
      float z;
    };

    //...

You can define arbitrarily many components like this. All component types must be put into this list:

    using VeSystemComponentTypeList = vtll::type_list<
        VeSystemComponentName
      , VeSystemComponentPosition
      //, ...
    >;

Once you have defined your component types, you can compose entities with them:

    using VeSystemEntityTypeNode
      = vtll::type_list<VeSystemComponentName, VeSystemComponentPosition>;

In this example, an entity type VeSystemEntityTypeNode is composed of the components VeSystemComponentName and VeSystemComponentPosition.
You can define arbitrarily many entities like this. All entity
types must be put into this list:

    using VeSystemEntityTypeList = vtll::type_list<
      VeSystemEntityTypeNode
      // ,...
    >;

Components of entities are stored in tables, one table for each entity type. Tables are organized in segments, each segment is able to store *S = 2^A* rows of component data. Furthermore, there is always a limit on how many entities of a given type E can be stored in VECS, this being *N = 2^B*. The latter can be changed any time, but must be externally synchronized, i.e., at the time this number is changed, no parallel accesses are allowed for the given table. You can define *A* and a default value for *B* in a map:

    using VeSystemTableSizeMap = vtll::type_list<
      vtll::type_list< VeSystemEntityTypeNode, vtll::value_list< 15, 20 > >
      //, ...
    >;

In this example, *A=15* (this is fixed), and a default value for *B=20*. If there is no entry for a specific entity type E, then the default values used are *A=10* and *B=16*. Be careful not to set *A* to a too high value.

Another choice relates to the layout of segments. Layouts of segments can be row wise or column wise. In row wise, the components of each entity are stored next to each other. Thus when accessing all components at once, cache efficiency is best. In column wise, components are stored in separate arrays, and cache performance is optimal if only single components are accessed in for loops. You can choose the layout for a specific entity type E like so:

    using VeSystemTableLayoutMap = vtll::type_list<
      vtll::type_list< VeSystemEntityTypeNode, VECS_LAYOUT_COLUMN >
      //, ...
    >;

Possible values are VECS_LAYOUT_COLUMN or VECS_LAYOUT_ROW, the default is VECS_LAYOUT_COLUMN.

### You are a User of a Game Engine that is based on VECS

If you are a user of a given game engine using VECS, you can edit the file *VECSCompUser.h* for defining user based components and entity types. Like with system related types, there are lists holding user related types:

    struct VeComponentName {
      std::string m_name;
    };

    struct VeComponentPosition {
      glm::vec3 m_position;
    };

    struct VeComponentOrientation {
      glm::quat m_orientation;
    };

    using VeUserComponentTypeList = vtll::type_list<
        VeComponentName
      , VeComponentPosition
      , VeComponentOrientation
      //, ...
    >;

    using VeEntityTypeNode = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation >;

    using VeUserEntityTypeList = vtll::type_list<
      VeEntityTypeNode
      , VeEntityTypeDraw
      , VeEntityTypeAnimation
      // ,...
    >;

    using VeUserTableSizeMap = vtll::type_list<
      vtll::type_list< VeEntityTypeNode, vtll::value_list< 15, 20 > >
      //, ...
    >;

    using VeUserTableLayoutMap = vtll::type_list<
      vtll::type_list< VeEntityTypeNode, VECS_LAYOUT_COLUMN >
      //, ...
    >;

The names for component types and entity types can be arbitrarily chosen, but must be unique amongst all components and entity types.

Alternatively, you can define this in your own include file, and define  VECS_USER_DATA in order to prevent later on using the definitions from *VECSCompUser.h*. The example projects use this approach, for example in *test.h*:

    #ifndef TEST_H
    #define TEST_H

    #include <limits>
    #include <typeinfo>
    #include <typeindex>
    #include "VECSUtil.h"

    #include "VECSCompSystem.h" //get basic type list

    #define VECS_USER_DATA   //define macro to prevent loading VECSCompUser.h

    namespace vecs {

    	/** \brief Example for a user component*/
    	struct VeComponentName {
    		std::string m_name;
    	};

      //define the rest of the user components and entities
      //...

    };

    #endif

In your CPP file, make sure to include first your own include file, then afterwards *VECS.h*:

    #include <iostream>
    #include <utility>
    #include "glm.hpp"
    #include "gtc/quaternion.hpp"

    #include "basic_test.h" //your own definitions, including VECS_USER_DATA

    #include "VECS.h"       //VECS does not load VECSCompUser.h since you defined VECS_USER_DATA

    using namespace vecs;

    int main() {
        std::atomic_flag flag;

        //...
    }


### The VECS Registry

Entities are stored in the *VecsRegistry*. This data structure uses the *mono state* pattern, so if you want to use it you simply instantiate it:

    VecsRegistry reg{};

There is only one version, and you can instantiate it any number of times, the result is always the same version. In the first instantiation you can pass a parameter specifying the maximum number of entities to be stored in the ECS. If no parameter is given, then *VecsTableMaxSize::value* is used, which is the smallest power of 2 larger than the sum of the number of entities of any type E.
If you just want to initialize the registry you can do it like this:

    VecsRegistry{ 1 << 20 };

Using *VecsRegistry* is not bound to a specific entity type, but commands evenmtually need this information. However, all calls are eventually passed on to *VecsRegistry<E>*, where *E* is an entity type. This is a specialized version of the registry made only for entity type *E*. It is recommended to always use this specialized version, if possible. For instance, if you want to create an entity of type *E*, you have at some point to specify the entity type. In the following, we define an *example* entity type *VeEntityTypeNode* like so:

    using VeEntityTypeNode = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation >;
    using VeEntityTypeDraw = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentMaterial, VeComponentGeometry>;


We assume that is has been registered as described in the previous section. Of course, any other similarly defined type can be used for the following examples as well. You can create an entity of type *VeEntityTypeNode* using:

    VecsHandle handle = VecsRegistry<VeEntityTypeNode>{}.insert("Node2", VeComponentPosition{}, VeComponentOrientation{});

Obviously, the parameters for this call must match the list of components that the entity is composed of.
The result of creating an entity is a *handle*. A handle is an 8-bytes structure that uniquely identifies the new entity and you can use it later to access the entity again. A handle can be invalid, meaning that it does not point to any entity. You can test whether a handle is valid or not by calling

    handle.is_valid();

If the handle is valid, then it IDs an entity in VECS. However, the entity might have been erased from VECS previously. You can test whether the entity that the handle represents is still in VECS by calling either of these:

    handle.has_value();
    VecsRegistry{}.contains(handle); //any type
    VecsRegistry<VeEntityTypeNode>{}.contains(handle); //if certain that entity is of type VeEntityTypeNode

A part of a handle contains an ID for the entity type. You can get the type index by calculating

    handle.type();

Reading a component from a given entity can be done by any of these methods:

    VeComponentPosition pos1 = handle.component<VeComponentPosition>(handle);
    auto pos2 = VecsRegistry{}.component<VeComponentPosition>(handle);
    auto pos3 = VecsRegistry<VeEntityTypeNode>{}.component<VeComponentPosition>(handle); //if handle of type VeEntityTypeNode

Since entities are composed of a list of components, and components have a unique index in them, you can also retrieve a component by specifying its index like so:

    auto pos3 = VecsRegistry<VeEntityTypeNode>{}.component<1>(handle); //if handle of type VeEntityTypeNode

Again, all calls are finally handed to the last version, which then resolves the data. Only the last version is actually checked by the compiler at compile time, and the first two version thus could result in an empty component being returned. You can call *has_component<C()* to check whether an entity pointed represented by a handle does contain a specific component of type *C* using any of these methods:

    bool b1 = handle.has_component<VeComponentPosition>();
    bool b2 = VecsRegistry{}.has_component<VeComponentPosition>(handle);
    bool b3 = VecsRegistry<VeEntityTypeNode>{}.has_component<VeComponentPosition>();

The last call is only a wrapper for the concept *is_component_of<VeEntityTypeNode,VeComponentPosition>* which is evaluated at compile time.

You can update the value of one or many components through any of these update functions:

    handle.update(VeComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f}, VeComponentOrientation{} };
    VecsRegistry{}.update(handle, VeComponentPosition{ glm::vec3{-98.0f, -22.0f, -33.0f} });
    VecsRegistry<VeEntityTypeNode>{}.update(handle, VeComponentPosition{ glm::vec3{-97.0f, -22.0f, -33.0f } }, VeComponentOrientation{});

Again, all calls are finally forwarded to the last version, which should only be called if it is certain that the entity does contain the component.

Finally, you can erase entities from VECS using any of these calls:

    handle.erase();
    VecsRegistry{}.erase(handle);
    VecsRegistry<VeEntityTypeNode>{}.erase(handle); //if handle of type VeEntityTypeNode

When an entity is erased, for any of its components, the component *destructor* is called, if it has one and it is not trivially destructible. You can erase all entities by calling *clear()*:

    VecsRegistry{}.clear(); //erase all entities in VECS
    VecsRegistry<VeEntityTypeNode>{}.clear(); //erase all entities of type VeEntityTypeNode

If an entity is erased, the space in the component table is *not* removed, just invalidated. Thus, erasing entities produces gaps in the data and iterating through all entities gets increasingly less efficient. In order to compress the component table, you have to stop multithreaded access to VECS, and call either

    VecsRegistry{}.compress(); //compress all tables
    VecsRegistry<VeEntityTypeNode>{}.compress();  // compress only table for entity type VeEntityTypeNode

This will remove any gap in the component table(s) to speed up iterating through the entities in VECS. In a game, this can be done typically once per game loop iteration. Compressing may reshuffle rows, and if the ordering of entities is important, you may want to go through the entities once more and make sure that the ordering is ensured. An example for this is a scene graph, where nodes in a scene can have siblings and children, thus spanning up a tree that is stored in a flat table. When calculating the world matrices of the nodes, it is important to compute in the order from the tree root down to the leaves. Thus, when looping through the node entities, parent nodes must occur before child nodes. You can compare the positions in the component table using the function *index()*, use either of these:

    index_t first = handle.index();
    index_t second = VecsRegistry{}.index(handle);

If a child comes before a parent then you can swap the places of two entities in the component table using either

    VecsRegistry{}.swap(handle1, handle2); //if both handles of same type
    VecsRegistry<VeEntityTypeNode>{}.swap(handle1, handle2);  //if both handles of type VeEntityTypeNode

The entities are swapped only if they are of the same type. You can ask for the number of valid entities currently in VECS using

    VecsRegistry{}.size(); //return total number of entities in VECS
    VecsRegistry<VeEntityTypeNode>{}.size(); //return number of entities of type VeEntityTypeNode in VECS


### Entity Tuples

You can make a local copy of an entity by calling

    auto tuple_val = VecsRegistry<VeEntityTypeNode>{}.values(handle);

The result is a *std::tuple* holding a copy of all components of the entity. Of course this is only possible if all components are copyable. You can access the components by using the *std::get* function on the tuple, by either using the index of the component or its type (which should be unique):

    auto& comp1 = std::get<0>(tuple_val); //use index, or
    auto& comp2 = std::get<VeComponentPosition>(tuple_val); //use type as an index

The outcome is a reference that can be used for reading and writing the value in the tuple. Of course, writing a new value does not affect the entity in VECS. You can write the tuple back to VECS using

    VecsRegistry<VeEntityTypeNode>{}.update(handle, tuple_val);

On the other hand, if you want quick access without the need for updates, you can calls

    auto tuple_ptr = VecsRegistry<VeEntityTypeNode>{}.pointer(handle);

The result is a *std::tuple* that contains pointers to the components of the entity. Accessing them results in a direct read or writing of the original entity. In single thread operations, this is not a problem. However, in multithreaded operations, you should lock the entity first using either a read or write lock (see Parallel Operations).

  auto pos = std::get<VeComponentPosition*>(tuple_ptr)->m_position; //use * since it is a pointer!


## Iterators, Ranges and Loops

The basic use case of any ECS is to loop over all or some of the entities. VECS allows this in various ways. The basic mechanism is given by iterators and ranges. These can then be used to compose loops.

Iterators are generalized pointers, and are the main mechanism for looping over entities in VECS. Iterators are implemented in class *VecsIterator* and come in two basic forms, and can be used for two basic use cases.

The first form is a general iterator that can point to any entity and can be increased to jump ahead. The second one is an end-iterator, it is created by calling it with the boolean parameter *true*:

    VecsIterator<VeComponentName, VeUserComponentPosition> it;         //normal iterator
    VecsIterator<VeComponentName, VeUserComponentPosition> end(true);  //end iterator used as looping end point

Iterators have template parameters, which define their starting position and the entity categories they cover. If the template parameter is a list of component types, then the covered entities are all entity types that contain *all* specified component types. In the previous example, these are all entity types that contain the components *VeComponentName* and *VeUserComponentPosition*. Since the components are given explicitly, accessing the value with * yields a *std::tuple* that starts with a handle, and as rest contains direct references to the components in VECS.

    auto [handle, name, pos] = *it; //name and pos are references to the components in VECS

If on the other hand the template parameters are entity types, then these are the entity types the iterator covers, accessing an entity with the * operator yields the components that are found in *both* entities.

    VecsIterator<VeEntityTypeNode, VeEntityTypeDraw> it2;   //normal iterator
    auto [handle, name, pos] = *it2;                        //name and pos are references to the components

In this example, *it2* of course points to only one entity, but it can be of either type *VeEntityTypeNode* or *VeEntityTypeDraw*. Components *VeComponentName* and *VeComponentPosition* are found in both types, thus references to them are returned by the iterator. Iterators can exclusively either use lists of components or entities, but mixing is not allowed and results in a compilation error.

Starting with a begin entity, a *VecsIterator* can be increased to point to the next entity in the component table, by calling the operator ++, either as pre-increment (preferred!) or post-increment (expensive, results in a copy!) Furthermore, you can add a positive integer number to skip ahead more than one entity. Finally, you can compare two iterators for equality or inequality. Combining these operators, you can already implement a simple loop over a set of entities:

    VecsIterator<VeComponentName, VeUserComponentPosition> b;        //normal iterator
    VecsIterator<VeComponentName, VeUserComponentPosition> e(true);  //end iterator used as looping end point
    for (; b != e; ++b) {
        if (!b.has_value()) continue;   ///< Could be an invalidated entity, so test for it!
        auto [handle, name, pos] = *b;
        //...
    }

Note that this way of looping does not synchronize accesses in case or multithreaded operations. By changing to

    VecsIterator<VeComponentName, VeUserComponentPosition> b;        //normal iterator
    VecsIterator<VeComponentName, VeUserComponentPosition> e(true);  //end iterator used as looping end point
    for (; b != e; ++b) {
        VecsWriteLock lock( b.mutex() );	///< Write lock
			  if (!b.has_value()) continue;     ///< Could be an invalidated entity, so test for it!
        auto [handle, name, pos] = *b;
        //...
    }

you add synchronization and can safely read and write. If you only read, then you can change the lock to a read lock (see below for more on parallel operations). Also you should always check whether the entity does hold a value, since entities that are erased are not automatically removed from VECS, only after calling *compress()*!

A *VecsRange* is now a combination of two iterators, one points to a starting entity, one points to an end position. The end position can be an end iterator, or any entity following the starting entity, which is no more included into the range.

    VecsRange<VeEntityTypeNode, VeEntityTypeDraw> range_entities;
    VecsRange<VeComponentName, VeComponentPosition> range_components;

    VecsIterator<VeEntityTypeNode, VeEntityTypeDraw> b = range_entities.begin();  ///< Get begin iterator
    VecsIterator<VeEntityTypeNode, VeEntityTypeDraw> e = range_entities.end();    ///< Get end iterator

Again, ranges can use exclusively either lists of components or entities, but not both. For parallelization a range can be split into *N* non-overlapping subranges, yielding a *std::pmr::vector* holding the subranges:

    std::pmr::vector<VecsRange<VeEntityTypeNode, VeEntityTypeDraw>> subranges = range_entities.split(16);


The sole purpose of an ECS is to quickly loop over a subset of entities in a cache-optimal way. VECS allows for various modes of looping. An example for a simple manual loop has been already described in the previous section. Another way is to use *VecsRange* in a *range based for loop*:

    for (auto&& [handle, name, pos, orient] : VecsRange<VeEntityTypeNode, VeEntityTypeDraw>{}) {
        VecsWriteLock lock(handle.mutex()); ///< Write lock
        if (!handle.has_value()) continue;  ///< Could be an invalidated entity, so test for it!

        //....
    }

This is just a nicer way of writing a loop, and is essentially the same as in the previous example. As can be seen the range based loop does not provide synchronization, and thus you can decide yourself whether to lock the entity or not. However you should always check whether the entity is still in VECS by calling *has_value()*! As range you can specify any range, also a subrange that was derived by calling *split()*.

Finally, the safest way for looping is to use the *for_each* member function of *VecsRegistry*:

    VecsRegistry{}.for_each( VecsRange<VeEntityTypeNode, VeEntityTypeDraw>{}, [&](auto handle, auto& name) {
        //...
    });

    VecsRegistry{}.for_each<VeComponentName, VeComponentPosition>([&](auto handle, auto& name) {
        //...
    });

Both loop over all entities that have the components *VeComponentName* and *VeComponentPosition* and internally do all the synchronization and checking for you. In the first version you can provide any range or subrange, the second version loops over all suitable entities.


## Parallel Operations and Performance

In principle, VECS allows parallel operations if certain principles are upheld. First, only member functions of the class *VecsRegistry\<E\>* are actually internally synchronized. These synchronizations use *VecsWriteLock* or *VecsReadLock* on a per entity basis, and synchronization lasts only for the duration of the call. Such locks need the address a *std::atomic\<uint32_t\>*  (a VECS mutex) as input parameter, and every entity has its own such VECS mutex. You can get the VECS mutex of an entity by calling *mutex()* on a *handle* or an *iterator*. The call always returns a valid mutex, even if the entity has been erased and is no longer valid.

Note that calls to *VecsRegistry\<\>* and *VecsHandle* are eventually forwarded to their counterparts in class *VecsRegistry\<E\>* with the same name. Thus, these member functions are also implicitly internally synchronized.

The following list shows the calls that are internally or externally synchronized in class *VecsRegistry\<E\>*:

    template<typename E>
    class VecsRegistry : public VecsRegistryBaseClass {

      ...

      auto updateC(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool; ///< Dispatch from base class (externally synchronized)
  		auto componentE(VecsHandle handle, size_t compidx, void* ptr, size_t size) noexcept	-> bool; ///< Dispatch from base class (externally synchronized)
  		auto has_componentE(VecsHandle handle, size_t compidx) noexcept						-> bool; ///< Dispatch from base class (externally synchronized)
  		auto eraseE(index_t index) noexcept	-> void { m_component_table.erase(index); };			 ///< Dispatch from base class (externally synchronized)
  		auto compressE() noexcept	-> void { return m_component_table.compress(); };	///< Dispatch from base class (externally synchronized)
  		auto clearE() noexcept		-> size_t { return m_component_table.clear(); };	///< Dispatch from base class (externally synchronized)

    public:
      VecsRegistry(size_t r = 1 << c_max_size) noexcept : VecsRegistryBaseClass() { 	///< Constructor of class VecsRegistry<E>
        VecsComponentTable<E<Cs...>>{r};
      };
      VecsRegistry(std::nullopt_t u) noexcept : VecsRegistryBaseClass() { 			///< Constructor of class VecsRegistry<E>
        m_sizeE = 0;
      };

      template<typename... Cs>
      requires is_composed_of<E<Cs...>, Cs...> [[nodiscard]]
      auto insert(CCs&&... args) noexcept			-> VecsHandle;			///< Insert new entity of type E into VECS (internally synchronized)

      template<typename... Cs> [[nodiscard]]
  		auto transform(VecsHandle handle, Cs&&... args) noexcept	-> VecsHandle;	///< transform entity into new type (internally synchronized)

      auto values(VecsHandle handle) noexcept		-> vtll::to_tuple<E>;	///< Return a tuple with copies of the components (internally synchronized)
  		auto pointers(VecsHandle handle) noexcept	-> vtll::to_ptr_tuple<E>;	///< Return a tuple with pointers to the components (externally synchronized)

      template<typename C>
      requires is_component_type<C>
      auto has_component() noexcept				-> bool {	///< Return true if the entity type has a component C (internally synchronized)
        return is_component_of<E<Cs...>,C>;
      }

      template<size_t I, typename C = vtll::Nth_type<E, I>>
  		requires is_component_of<E, C>
  		auto component(VecsHandle handle) noexcept	-> C;		///< Get copy of a component given index of component (internally synchronized)

  		template<size_t I, typename C = vtll::Nth_type<E, I>>
  		requires is_component_of<E, C>
  		auto component_ptr(VecsHandle handle) noexcept	-> C*;	///< Get copy of a component given index of component (externally synchronized)

  		template<typename C>
  		requires is_component_of<E, C>
  		auto component(VecsHandle handle) noexcept	-> C;		///< Get copy of a component given type of component (internally synchronized)

  		template<typename C>
  		requires is_component_of<E, C>
  		auto component_ptr(VecsHandle handle) noexcept	-> C*;		///< Get copy of a component given type of component (externally synchronized)

      template<typename ET>
      requires is_tuple<ET, E<Cs...>>
      auto update(VecsHandle handle, ET&& ent) noexcept	-> bool;		///< Update a whole entity with the given tuple (internally synchronized)

      template<typename C>
      requires is_component_of<E<Cs...>, C>
      auto update(VecsHandle handle, C&& comp) noexcept	-> bool;		///< Update one component of an entity (internally synchronized)

      auto erase(VecsHandle handle) noexcept				-> bool;		///< Erase an entity from VECS (internally synchronized)
      auto clear() noexcept -> size_t { return clearE(); };				///< Clear entities of type E (externally synchronized)
      auto compress() noexcept -> void { compressE(); }					///< Remove erased rows from the component table (externally synchronized)
      auto max_capacity(size_t) noexcept					-> size_t;		///< Set max number of entities of this type (externally synchronized)
      auto size() noexcept -> size_t { return m_sizeE.load(); };	///< \returns the number of valid entities of type E (internally synchronized)
      auto swap(VecsHandle h1, VecsHandle h2) noexcept	-> bool;		///< Swap rows in component table (internally synchronized)
      auto contains(VecsHandle handle) noexcept			-> bool;		///< Test if an entity is still in VECS (externally synchronized)
    };

Note that *VecsRegistry<E>{}.pointer(...)* is actually externally synchronized. That means before calling this function with multiple threads you should first create a read or write lock, and keep it until you no longer need the tuple.

An example for looping over entities in parallel on several threads is given in example program parallel.cpp. For parallelization, the example uses the Vienna Game Job System (VGJS, https://github.com/hlavacs/ViennaGameJobSystem), a sibling project of VECS. VGJS is a header-only C++20 library enabling using function pointers, std::function, lambdas, or coroutines to be run in the job system.

    template<template<typename...> typename R, typename... Cs>
    void do_work(R<Cs...> range) {
        size_t i = 0;

        for (auto [handle, pos] : range) {
            if (!handle.is_valid()) continue;
            pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
            ++i;
        }

    	  /*VecsRegistry{}.for_each( std::move(range), [&](auto handle, auto& pos) {
    		    pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
    		    ++i;
    	  });*/
    }

    auto ranges = VecsRange<VeComponentPosition>{}.split(16); // split set of entities holding VeComponentPosition into 12 subranges

    std::pmr::vector<vgjs::Function> vec; //store functions that should be run in parallel

    for (int i = 0; i < ranges.size(); ++i) { //create an vector of functions, one function for each subrange
        vec.push_back(
            vgjs::Function([=]() { do_work( ranges[i] ); } //Function class can store meta information
            , vgjs::thread_index_t{}    //use any thread that is available
            , vgjs::thread_type_t{ 1 }  //log with type 1
            , vgjs::thread_id_t{ i })); //log with ID i
    }

    co_await vec;   //run the functions in parallel and wait for completion

    //...

Note that since the ranges do not overlap, there is actually no need for synchronization of no other thread accesses the entities. Thus there is no lock in the range based for loop.
If there are other threads accessing the entities, then you can either introduce locks, or switch to the *for_each* version. Not that this version will take longer time on average, due to locking every entity before accessing it.
