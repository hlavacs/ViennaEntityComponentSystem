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
* *IntType.h*: Template for strong integral types like *index_t*. Such types enforce to use them explicitly as function parameters and prevent users from mixing them up with *int* or *unsigned long*. Also such a type can store a NULL value, and can be tested with *is_valid()*.
* *VECSUtil.h*: Contains utility classes such as a class for implementing CRTP, mono state, and simple low-overhead read and write locks.
* *VECS.h*: The main include file containing most of VECS functionality.
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

    #include "VECS.h"       //VECS

    using namespace vecs;

    int main() {
        std::atomic_flag flag;

        //...
    }


### The VECS Registry

Entities are stored in the *VecsRegistry*. This data structure uses the *mono state* pattern, so if you want to use it you simply instantiate it:

    VecsRegistry reg{};

There is only one version, and you can instantiate it any number of times, the result is always the same version. In the first instantiation you can pass a parameter specifying the maximum number of entities to be stored in the ECS. If no parameter is given, then VecsTableMaxSize::value is used, which is the smallest power of 2 larger than the sum of the number of entities of any type E.
If you just want to initialize the registry you can do it like this:

    VecsRegistry{ 1 << 20 };

Using *VecsRegistry* is not bound to a specific entity type, but commands evenmtually need this information. However, all calls are eventually passed on to *VecsRegistry<E>*, where *E* is an entity type. This is a specialized version of the registry made only for entity type *E*. It is recommended to always use this specialized version, if possible. For instance, if you want to create an entity of type *E*, you have at some point to specify the entity type. You can create an entity of type

    using VeEntityTypeNode = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation >;

like so

    auto handle = VecsRegistry{}.insert<VeEntityTypeNode>("Node1", {}, {});

or like so:

    VecsHandle handle = VecsRegistry<VeEntityTypeNode>{}.insert("Node1", {}, {});

In fact, the first call simply calls the second call internally. The result of creating an entity is a *handle*. A handle is an 8-bytes structure that identifies the new entity and you can use it later to access the entity again. For instance, reading a component from a given entity goes like this:

    std::optional<VeComponentPosition> pos = handle.component<VeComponentPosition>(handle);

In this case, a *std::optional* is returned, since it could happen that the component is actually not part of the entity. Another way of accessing the component is by

    auto pos = VecsRegistry{}.component<VeComponentPosition>(handle);

or

    std::optional<VeComponentPosition> pos = VecsRegistry<VeEntityTypeNode>{}.component<VeComponentPosition>(handle);

Again, all calls are finally handed to the latter version, which then resolves the data.
You can update the value of a component through the update function:

    handle.update(VeComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f} };

or

    handle.update<VeComponentPosition>(VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} });

or

    VecsRegistry{}.update<VeComponentPosition>(handle, VeComponentPosition{ glm::vec3{-98.0f, -22.0f, -33.0f} });

or

    VecsRegistry<VeEntityTypeNode>{}.update<VeComponentPosition>(handle, VeComponentPosition{ glm::vec3{-97.0f, -22.0f, -33.0f} });

Again, all calls are finally handed to the latter version. 





### Entity Proxy



### Iterators


## Parallel Operations



## Looping


### Range Based For loop



### for_each




## Performance




## Examples




## Implementation Details
