# Vienna Entity Component System (VECS)

The Vienna Entity Component System (VECS) is a C++20 based ECS for game engines. An ECS is a data structure storing *entities* of different types.
Entity types consist of a number of *components*.
Different entity types can be composed of different components.

An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. Additionally, a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a physics system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system animation is only interested into position, orientation, and animation data.
Systems do not have to be specially defined, anything that specifies a number of components it is interested in and then loops over all entities that contain these components can be seen as a system.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* Compile time definition of components and entities. VECS creates a rigorous compile time frame around your data, minimizing runtime overhead.
* Can use tags to define subsets of entities dynamically.
* Designed to be used by multiple threads in parallel, low overhead locking.
* Easy to use, various ways to access the entities and their components.
* High performance by default, components are ordered sequentially in memory (row or column wise) for cache friendly  access.


## The VECS Include Files

VECS is a header only library, consisting of the following header files:
* *IntType.h*: Template for strong integral types like *table_index_t* or *counter_t*. Such types enforce to use them explicitly as function parameters and prevent users from mixing them up with *size_t, int* or *unsigned long*. Also such a type can store a NULL value, and can be tested with *has_value()*.
* *VECSUtil.h*: Contains utility classes such as a class for implementing CRTP, mono state, and simple low-overhead read and write locks.
* *VECS.h*: The main include file containing most of VECS functionality.
* *VECSTable.h*: Defines a table data structure that can grow with minimal synchronization overhead, even when accessed by multiple threads.
* *VECSIterator.h*: Functionality for iterating over entities in VECS.
* *VECSCompSystem.h*: Examples for component types and entity types used in a game engine itself.
* *VECSCompUser.h*: Examples for components and entities that could be defined by the engine users to store their own entities.
* *VTLL.h*: The Vienna Type List Library is a collection of meta-algorithms for treating type lists and value lists.

VECS depends on two projects, the Vienna Type List Library (VTLL) and Vienna Game Job System (VGJS).

### Vienna Type List Library (VTLL)

As stated, VECS is a compile time ECS. Compositions of entities are defined in source code only. VECS thus heavily relies on its partner project Vienna Type List Library (VTLL), a header-only meta-programming library that helps creating entity types, type lists, compositions of components, accessing list elements, mapping data, etc. Especially, entities are defined as *vtll::type_list<...>* (or its shorter version *vtll::tl<...>*) and therefore are nothing more than a list of types. For more information goto https://github.com/hlavacs/ViennaTypeListLibrary.


### Vienna Game Job System (VGJS)

In the parallelization examples, VECS makes use of another partner project, the Vienna Game Job System (VGJS). VGJS is a header-only library for helping spawning threads and submitting jobs to them. VGJS is unique in the sense that you can submit C++ functions, lambdas, function pointers, *std::function*s, vectors containing them, and more notably co-routines enabling easy dependencies between successive jobs. For more information see https://github.com/hlavacs/ViennaGameJobSystem.
VGJS is not necessary to run VECS, and as stated it is only used as an example for multithreading.


## The VECS API

An *Entity Component System* is a data structure that stores data components in some flat way, meaning that data is stored linearly in memory as a vector or table. The stored data are called *components*. Components can in principle be any type that in this case is *movable* and *default constructible*.
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

    struct VeUserComponentResource {
      std::unique_ptr<my_resource_t> my_resource;
    };

As can be seen, these components consist of a std::string, floats and a unique pointer (which is movable but NOT copyable), and thus satisfy the conditions on components. Components are then used to define *entities*. Entities are composed of one or more components. This composition is defined using type lists, like

    using VeSystemEntityTypeNode
      = vtll::type_list<VeSystemComponentName, VeSystemComponentPosition>;

which defines the entity type *VeSystemEntityTypeNode* to be composed of the two components.

For defining components and entities, there are two major use cases. If you *create a game engine* yourself, you should define components and entities as belonging to the *system* in file *VECSCompSystem.h*.

If you are a *user of a VECS based engine*, then you can define your own types either in *VECSCompUser.h*, or in your own include files and by defining the macro

    #define VECS_USER_DATA

prevent *VECSCompUser.h* from being loaded (see e.g. *test.h*). Both cases are internally treated the same way and are discussed in the following.

### Option 1 - Using VECS in the Core of your Game engine

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

Once you have defined your component types, you can compose entities with them:

    using VeSystemEntityTypeNode = vtll::type_list<VeSystemComponentName, VeSystemComponentPosition>;

In this example, an entity type VeSystemEntityTypeNode is composed of the components VeSystemComponentName and VeSystemComponentPosition.
You can define arbitrarily many entities like this. However, component types must be *unique* within each entity type. Thus, an entity type is *not allowed* to contain the same component type twice! If you need the same data twice, pack it into different structs.

All system entity types must be put into this list:

    using VeSystemEntityTypeList = vtll::type_list<
      VeSystemEntityTypeNode
      // ,...
    >;

Components of entities are stored in tables, one table for each entity type *E*. Tables are organized in segments, each segment is able to store *N = 2^L* rows of component data. For each entity type you can define *N* in a map:

    using VeSystemTableSizeMap = vtll::type_list<
      vtll::type_list< VeSystemEntityTypeNode, vtll::value_list< 1<<15 > >
      //, ...
    >;

In this example, *N = 1<<15*, and this is fixed at compile time. Note that *N* eventually must be a power of 2. If you specify any other number *N0*, VECS at compile time for *N* uses the smallest power of 2 that is larger or equal to *N0*. If nothing is specified, the default value for *N* is

    using VeTableSizeDefault = vtll::value_list< 1<<10 >;

Another choice relates to the layout of segments. Layouts of segments can be row wise or column wise. In row wise, the components of each entity are stored next to each other. Thus when accessing all components at once, cache efficiency is best. In column wise, components are stored in separate arrays, and cache performance is optimal if only single components are accessed in for-loops. You can choose the layout for a specific entity type *E* like so:

    using VeSystemTableLayoutMap = vtll::type_list<
      vtll::type_list< VeSystemEntityTypeNode, VECS_LAYOUT_COLUMN >
      //, ...
    >;

Possible values are VECS_LAYOUT_COLUMN or VECS_LAYOUT_ROW, the default is VECS_LAYOUT_COLUMN.

### Option 2 - Being User of a Game Engine that is based on VECS

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

Alternatively, you can define this in your own include file, and define the macro *VECS_USER_DATA* in order to prevent later on using the definitions from *VECSCompUser.h*. The example projects use this approach, for example in *test.h*:

    #ifndef TEST_H
    #define TEST_H

    #include <limits>
    #include <typeinfo>
    #include <typeindex>
    #include "VECSUtil.h"

    #include "VECSCompSystem.h" //get basic type list

    #define VECS_USER_DATA      //define macro to prevent loading VECSCompUser.h

    namespace vecs {

      //-------------------------------------------------------------------------
    	//define user components here

      /// \brief Example for a user component
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

    #include "basic_test.h" //your own definitions, also #define VECS_USER_DATA

    #include "VECS.h"       //VECS does not load VECSCompUser.h since you defined VECS_USER_DATA

    using namespace vecs;

    int main() {
        //...
    }


### The VECS Registry

Entities are stored in the *VecsRegistry*. This data structure uses the *mono state* pattern, so if you want to use it you simply instantiate it

    VecsRegistry reg{};

of create a temp object and directly use it

    std::cout << VecsRegistry{}.size() << "\n"; //number of entities currently in VECS

There is only one registry state, and you can instantiate it any number of times, the result is always the same.

Using *VecsRegistry* is not bound to a specific entity type *E*, but commands eventually need this information. However, all calls are then passed on to *VecsRegistry<E>*, where *E* is an entity type and must be registered in either entity list *VeSystemEntityTypeList* or *VeUserEntityTypeList*. Note that VECS concatenates these two lists into the list *VecsEntityTypeListWOTags*, and later on creates the overall entity list *VecsEntityTypeList*. So *E* can be any member of *VecsEntityTypeList*.

*VecsRegistry<E>* is a specialized version of the registry made only for entity type *E*. It is recommended to always use this specialized version, if possible. For instance, if you want to create an entity of type *E*, you have at some point to specify the entity type. In the following, we define an *example* entity type *VeEntityTypeNode* like so:

    using VeEntityTypeNode = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation >;
    using VeEntityTypeDraw = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentMaterial, VeComponentGeometry>;

We assume that is has been registered as described in the previous section. Of course, any other similarly defined type can be used for the following examples as well. You can create an entity of type *VeEntityTypeNode* using:

    VecsHandle handle = VecsRegistry<VeEntityTypeNode>{}.insert("Node2", VeComponentPosition{}, VeComponentOrientation{});

Note that the parameters for this call must match the list of components that the entity is composed of. Move-semantics automatically apply. The result of creating an entity is a *handle*. A handle is an 8-bytes structure that uniquely identifies the new entity and you can use it later to access the entity again. A handle can be invalid, meaning that it does not point to any entity. You can test whether a handle is valid or not by calling

    handle.is_valid();

If the handle is valid, then it IDs an entity in VECS. However, the entity might have been erased from VECS previously, e.g. by some other thread. You can test whether the entity that the handle represents is still in VECS by calling either of these:

    handle.has_value();
    VecsRegistry{}.contains(handle); //any type
    VecsRegistry<VeEntityTypeNode>{}.contains(handle); //call only if entity is of type VeEntityTypeNode

A part of a handle contains an ID for the entity type. You can get the type index by calling

    handle.type();               //call this
    VecsRegistry{}.type(handle); //or this

Reading a component from a given entity can be done by any of these methods:

    VeComponentPosition pos1 = handle.component<VeComponentPosition>();  //copy
    VeComponentPosition& pos2 = handle.component<VeComponentPosition>(); //reference
    VeComponentPosition* pos3 = handle.component_ptr<VeComponentPosition>(); //pointer

    auto  pos4 = VecsRegistry{}.component<VeComponentPosition>(handle); //copy
    auto& pos5 = VecsRegistry{}.component<VeComponentPosition>(handle); //reference
    auto* pos6 = VecsRegistry{}.component_ptr<VeComponentPosition>(handle); //pointer

    auto pos7 = VecsRegistry<VeEntityTypeNode>{}.component<VeComponentPosition>(handle); //if handle of type VeEntityTypeNode
    auto& pos8 = VecsRegistry<VeEntityTypeNode>{}.component<VeComponentPosition>(handle); //if handle of type VeEntityTypeNode
    auto* pos9 = VecsRegistry<VeEntityTypeNode>{}.component_ptr<VeComponentPosition>(handle); //if handle of type VeEntityTypeNode

Again, all calls are finally handed to the last version, which then resolves the data. Only the last version is actually checked by the compiler at compile time, and the first two version thus could result in an empty component being returned. You can call *has_component<C()* to check whether an entity pointed represented by a handle does contain a specific component of type *C* using any of these methods:

    bool b1 = handle.has_component<VeComponentPosition>();
    bool b2 = VecsRegistry{}.has_component<VeComponentPosition>(handle);
    bool b3 = VecsRegistry<VeEntityTypeNode>{}.has_component<VeComponentPosition>();

The last call is only a wrapper for the concept *is_component_of<VeEntityTypeNode,VeComponentPosition>* which is evaluated at compile time.

Additionally, you can obtain all components at once packed into a tuple:

    auto tuple_ref = VecsRegistry<VeEntityTypeNode>{}.tuple(handle); //tuple with references to the components
    auto tuple_ptr = VecsRegistry<VeEntityTypeNode>{}.tuple_ptr(handle); //tuple with pointers to the components

You can access the components either by speciyfing the index, or by specifying the type (which is unique for each entity type):

    auto  comp1 = std::get<VeComponentPosition&>(tuple_ref); //copy
    auto& comp2 = std::get<VeComponentPosition&>(tuple_ref); //reference
    auto* comp3 = std::get<VeComponentPosition*>(tuple_ptr); //pointer

Since the above calls may yield references or addresses of components in VECS, you can directly read and modify the component directly. You can also utilize *std::move()* on any of them:

    handle.component<VeComponentPosition>() = VeComponentPosition{};

**NOTE:** references and pointers as obtained above may become eventually invalid. Do not store them for later use, only use them immediately and if you are sure that the entity is still valid. Store the handles instead, and test whether the handle is valid. When using multithreading, use read or write locks (see below) to ensure safe accesses.

You can also update several components with one *update* call (again, move semantics apply):

    handle.update(VeComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f}, VeComponentOrientation{} }; //move semantics due to rvalue references

    VeComponentPosition pos{ glm::vec3{-98.0f, -22.0f, -33.0f} };
    VecsRegistry{}.update(handle, pos); //copy semantics due to value reference

    VecsRegistry<VeEntityTypeNode>{}.update(handle, pos, VeComponentOrientation{}); //copy for first, move for the second

Finally, you can erase entities from VECS using any of these calls:

    handle.erase();
    VecsRegistry{}.erase(handle);
    VecsRegistry<VeEntityTypeNode>{}.erase(handle); //if handle of type VeEntityTypeNode

When an entity is erased, for any of its components, each component's *destructor* is called, if it has one and it is not trivially destructible. You can erase all entities by calling *clear()*:

    VecsRegistry{}.clear();                   //erase all entities in VECS
    VecsRegistry<VeEntityTypeNode>{}.clear(); //erase all entities of type VeEntityTypeNode

If an entity is erased, the space in the component table is *not* removed, just invalidated. Thus, erasing entities produces gaps in the data and iterating through all entities gets increasingly less efficient. In order to compress the component table, you have to stop multithreaded access to VECS, and call either

    VecsRegistry{}.compress();                    //compress all tables
    VecsRegistry<VeEntityTypeNode>{}.compress();  // compress only table for entity type VeEntityTypeNode

This will remove any gap in the component table(s) to speed up iterating through the entities in VECS. In a game, this can be done typically once per game loop iteration. Compressing may reshuffle rows, and if the ordering of entities is important, you may want to go through the entities once more and make sure that the ordering is ensured. An example for this is a scene graph, where nodes in a scene can have siblings and children, thus spanning up a tree that is stored in a flat table. When calculating the world matrices of the nodes, it is important to compute in the order from the tree root down to the leaves. Thus, when looping through the node entities, parent nodes must occur before child nodes. You can compare the positions in the component table using the function *table_index()*, use either of these:

    table_index_t first = handle.table_index();
    table_index_t second = VecsRegistry{}.table_index(handle);

If a child comes before a parent then you can swap the places of two entities in the component table using either

    VecsRegistry{}.swap(handle1, handle2); //if both handles of same type
    VecsRegistry<VeEntityTypeNode>{}.swap(handle1, handle2);  //if both handles of type VeEntityTypeNode

The entities are swapped only if they are of the same type. You can ask for the number of valid entities currently in VECS using

    VecsRegistry{}.size(); //return total number of entities in VECS
    VecsRegistry<VeEntityTypeNode>{}.size(); //return number of entities of type VeEntityTypeNode in VECS


## Tags

Any unique combination of components results in a new entity type. Tags are empty components that can be appended to entity types, in any combination. Consider for example the Tags

    struct TAG1{};
    struct TAG2{};

Also consider the definition of *VeEntityTypeNode*

    using VeEntityTypeNode = vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation >;

By defining this helper, we can easily create variants for *VeEntityTypeNode*

    template<typename... Ts>
    using VeEntityTypeNodeTagged = vtll::app< VeEntityTypeNode, Ts... >;

by using the VTLL function *vtll::app<>* (append). For example, *VeEntityTypeNodeTagged\<TAG1\>* is the same as

    vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation, TAG1 >

Note that since VECS does not know the component types you plan to add, it is your task to define a similar helper for each entity type, if you want to use them for tags.
VECS supports creating tagged variants by offering a tag map, e.g.

    using VeUserEntityTagMap = vtll::type_list<
      vtll::type_list< VeEntityTypeNode, vtll::type_list< TAG1, TAG2 > >
      //, ...
    >;

For each entity type, you can note the tags that this type can have. In the above example, the
type *VeEntityTypeNode* can have the tags TAG1 and TAG2. There are again two tag maps, one for the system, and one for the user space. At compile time, VECS takes all entity types from the list *VecsEntityTypeListWOTags* and expands all types with all possible tag combinations they can have. Thus, using the above example, the final list *VecsEntityTypeList* containing all entity types will also contain the following types:

    vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation >;
    vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation, TAG1 >;
    vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation, TAG2 >;
    vtll::type_list< VeComponentName, VeComponentPosition, VeComponentOrientation, TAG1, TAG2 >;

Furthermore, all tags from the tag maps are also copied to the global tag list *VecsEntityTagList*, which for the above example will also contain *TAG1* and *TAG2*.
Tags are useful for grouping entity types, since any of the above tagged entity type lands in its own table. You can create tagged entities like any normal entity:

    auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
    auto h2 = VecsRegistry<VeEntityTypeNodeTagged<TAG1>>{}.insert(VeComponentName{ "Node T1" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});

In this example, two entities derived from the same basic entity type *VeEntityTypeNode* are created, one without a tag, and one with a tag. As you can see, you do not have to specify any value for tags, nor will tag references be included when using iterators or ranges (this is true for any member of the list *VecsEntityTagList*).
You can always add or remove tags from existing entities by calling the *transform()* function on their handles:

    VecsRegistry<VeEntityTypeNode>{}.transform(h2);

The above call turns an entity of type *VeEntityTypeNodeTagged\<TAG1\>* into an entity of type *VeEntityTypeNode*, thus effectively removing the tag. Internally, all components are moved from the *VeEntityTypeNodeTagged\<TAG1\>* table to the *VeEntityTypeNode* table. Note that therefore references to the components for the previous type are no longer valid after this operation!

In the next section, iterators and ranges are described, which enable you to efficiently loop over groups of entities.


## Iterators, Ranges and Loops

The basic use case of any ECS is to loop over all or some of the entities. VECS allows this in various ways. The basic mechanism is given by iterators and ranges. These can then be used to compose loops.

Iterators are generalized pointers, and are the main mechanism for looping over entities in VECS. Iterators are implemented in class

    template<typename... Ts>
    class VecsIterator;

and come in two versions. The first form is a general iterator that can point to any entity and can be increased to jump ahead. The second one is an end-iterator, it is created by calling it with the boolean parameter *true*:

    VecsIterator<VeComponentName, VeUserComponentPosition> it;         //normal iterator
    VecsIterator<VeComponentName, VeUserComponentPosition> end(true);  //end iterator used as looping end point

Iterators have template parameters which determine which entity types their iterate over, and which components they can yield in a for-loop. This is implemented by deriving them from a common base class depending on exactly two template parameters ETL and CTL. ETL is a type list of entity types the iterator should iterate over, and CTL is a type list of components. The iterator offers a dereferencing operator*(), yielding a tuple that holds a handle and references to the components:

    template<typename ETL, typename CTL>
    class VecsIteratorBaseClass {

      //...

      public:
        using value_type = vtll::to_tuple< vtll::cat< vtll::tl<VecsHandle>, CTL > >;
        using reference = vtll::to_tuple< vtll::cat< vtll::tl<VecsHandle>, vtll::to_ref<CTL> > >;
        using pointer = vtll::to_tuple< vtll::cat< vtll::tl<VecsHandle>, vtll::to_ptr<CTL> > >;
        using iterator_category = std::forward_iterator_tag;
        using difference_type	= size_t;
        using last_type = vtll::back<ETL>;	///< last type for end iterator

        VecsIteratorBaseClass(bool is_end = false) noexcept;				///< Constructor that should be called always from outside
        VecsIteratorBaseClass(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept;		///< Copy constructor

        auto operator=(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> VecsIteratorBaseClass<ETL,CTL>;	///< Copy
        auto operator+=(size_t N) noexcept		-> VecsIteratorBaseClass<ETL,CTL>;	///< Increase and set
        auto operator+(size_t N) noexcept			-> VecsIteratorBaseClass<ETL,CTL>;	///< Increase
        auto operator!=(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> bool;	///< Unqequal
        auto operator==(const VecsIteratorBaseClass<ETL,CTL>& v) noexcept	-> bool;	///< Equal

        auto handle() noexcept			-> VecsHandle;				///< Return handle of the current entity
        auto mutex_ptr() noexcept		-> std::atomic<uint32_t>*;	///< Return poiter to the mutex of this entity
        auto has_value() noexcept		-> bool;					///< Is currently pointint to a valid entity

        auto operator*() noexcept		-> reference;						///< Access the data
        auto operator++() noexcept		-> VecsIteratorBaseClass<ETL,CTL>&;	///< Increase by 1
        auto operator++(int) noexcept	-> VecsIteratorBaseClass<ETL,CTL>&;	///< Increase by 1
        auto size() noexcept			-> size_t;							///< Number of valid entities
    };

When specifying *VecsIterator<Ts...>* the lists *ETL* and *CTL* are created depending on the *Ts*, and the base class is determined. In the following the various ways to turn the parameters *Ts* into the base class *ETL* and *CTL* are explained.


### Iterating over Specific Entity types

You can specify a list *ETL* of entity types directly, which may also include entity types with tags. Then the iterator will loop over exactly this list of entity types. For instance

    VecsIterator< vtll::type_list<VeEntityTypeNode, VeEntityTypeNodeTagged<TAG2>> > it;
    VecsIterator< vtll::type_list<VeEntityTypeNode, VeEntityTypeNodeTagged<TAG2>> > end(true);

    while( it!= end) {
      auto [handle, pos, orient, trans] = *it; //get access to the first entity, might be invalid so you better check
      //VecsWriteLock lock(handle.mutex()); //use this if you are using multi
      if( handle.has_value() ) {
        //...
      }
      ++it; //use this operator if possible, the alternative it++ creates a costly copy!
    }

lets you access the first possible entity in either tables for types *VeEntityTypeNode* and *VeEntityTypeNodeTagged<TAG2>*. Accessing the operator\*() yields a handle (by value), and references to those components that all the given entity types contain (their intersection). References to tags are never yielded. The whole definition for the specific *VecsIterator* is given as follows:

    template<typename ETL>
    using it_CTL_entity_list = vtll::remove_types< vtll::intersection< ETL >, VecsEntityTagList >;

    template<typename ETL>
    requires (is_entity_type_list<ETL>::value)
    class VecsIterator<ETL> : public VecsIteratorBaseClass< ETL, it_CTL_entity_list<ETL> > {
    public:
      using component_types = it_CTL_entity_list<ETL> ;
      VecsIterator(bool end = false) noexcept : VecsIteratorBaseClass< ETL, it_CTL_entity_list<ETL> >(end) {};
    };

The first definition is the *CTL* for the case for a specific list of entity types, which is the intersection of all given types, and removing the tag types. Then the specfic *VecsIterator<ETL>* is defined, and the base class as well as the constructor for it are defined. This is all there is to this.

### Iterating over Basic Entity Types and All their Tagged Versions

Another use case is iterating over basic types and all their tagged version. For instance, iterating over all *VeEntityTypeNode* entities, but also over its tagged versions *VeEntityTypeNodeTagged<TAG1>*, *VeEntityTypeNodeTagged<TAG2>*, and *VeEntityTypeNodeTagged<TAG1, TAG2>* you can simply use them in as template parameters directly:

    VecsIterator<VeEntityTypeNode> it;

This can be done for any list of basic entity types. If you specify any type for which no tags are specified in the tag map, then this type is used as is. Again, the components are those contained in all types, minus the tags. The implementation of this is similarly simple (see *VECSIterator.h*), using the metafunction *expand_tags<...>* (defined in *VECS.h*), that takes a list of entities and expands it with the tagged versions if there are any.


### Iterating over Entity Types That Contain given Tags

If you want to loop over all tagged versions of a basic type *E* (you can only use one entity type) that have a number of tags, then you can specify e.g.

    VecsIterator<VeEntityTypeNode, TAG1> it;

This iterator loops over all entities of the types VeEntityTypeNodeTagged\<TAG1\> and VeEntityTypeNodeTagged\<TAG1,TAG2\>. It is essentially the same as

    VecsIterator< vtll::tl< VeEntityTypeNodeTagged<TAG1>, VeEntityTypeNodeTagged<TAG1, TAG2> > > it;

which would specify the types explicitly. Again, expansion is done only if the entity type *E* does have tags specified in the tag map. The components are the components of the entity types minus the tags, so basically the components of the untagged basic entity type *E*.

### Iterating over All Entity Types that Contain given Component Types

The next method allows you to specify a number of component types. The iterator would then loop over all entity types that contain these types. The components yielded are exactly the components specified, e.g.

    VecsIterator<VeComponentName> it;

loops over all entity types having this component!

### Iterating over All Entity Types

Finally, you can loop over all entity types in VECS, simply by specifying nothing:

    VecsIterator<> it;

The yielded components are again those contained in all entity types (this might be e.g. a name, a GUID, etc.), or more likely, an empty list. In any case, the iterator operator\*() will always yield the entity handle.

### Ranges and Range Based Loops

In C++, a range is any class that offers *begin()* and *end()* functions yielding respective iterators. VECS offers a special range class

    template<typename... Ts>
    class VecsRange;

which is derived from a base class

    template<typename ETL, typename CTL>
    class VecsRangeBaseClass {
      VecsIteratorBaseClass<ETL,CTL> m_begin;
      VecsIteratorBaseClass<ETL,CTL> m_end;

      public:
        VecsRangeBaseClass() noexcept : m_begin{ false }, m_end{ true } {

      //...
    };

The meanings of the template parameters are exactly the same as for iterators, and so ranges are simply two iterators put together. A *VecsRange* can be used directly in a *C++ range based loop*:

    for (auto [handle, name] : VecsRange<VeComponentName>{}) {
      if( handle.has_value() ) {
        //....
      }
    }

This version is the fastest version, but when obtaining the references does not lock the entities. Thus, when using multithreading, you should consider avoiding this version.
Additionally, VECS ranges offer a thread save range based loop called *for_each*:

    VecsRange<VeEntityTypeNode, TAG1>{}.for_each([&](VecsHandle handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    });

This loop guarantees that any loop iteration contains only valid entities, which are also automatically write locked. Due to the write lock, this loop is slightly slower than the C++ version, but you do not have to worry about locking or using the references. The implementation looks like this:

    template<typename C>
    struct Functor;

    template<template<typename...> typename Seq, typename... Cs>
    struct Functor<Seq<Cs...>> {
      using type = void(VecsHandle, Cs&...);	///< Arguments for the functor
    };

    template<typename ETL, typename CTL>
    inline auto class VecsRangeBaseClass<ETL,CTL>::for_each(std::function<typename Functor<CTL>::type> f) -> void {
      auto b = begin();
      auto e = end();
      for (; b != e; ++b) {
        VecsWriteLock lock(b.mutex());		///< Write lock
        if (!b.has_value()) continue;
        std::apply(f, *b);					 ///< Run the function on the references
      }
    }

You see that the loop is actually quite simple, and you can easily come up with your own versions, which e.g. only read lock entities, etc.

## Parallel Operations and Performance

In principle, VECS allows parallel operations on multiple threads if certain principles are upheld.
First, VECS only locks some internal data structures, but it does not lock single entities (the only exception being the VecsRangeBaseClass::for_each loop). So any time you change the state of an entity by writing on it (through a reference of calling *update()*), erasing it, swapping it with another entity of the same type, transforming it, etc., you should create a *VecsWriteLock* for this entity.

    {
      VecsIterator<VeComponentName> it;
      VecsWriteLock lock(it.mutex());		///< Write lock using an iterator
      //... now the entity is completely locked
    }

A write lock excludes any other lock to the entity, be it writing or reading.
If you only read components, then a read lock may be faster, since read locks do not prevent other read locks, but they prevent write locks.

    for (auto [handle, name] : VecsRange<VeComponentName>{}) {
      VecsReadLock handle.mutex() );
      if( handle.has_value() ) {
        //....
      }
    }




are actually internally synchronized. These synchronizations use *VecsWriteLock* or *VecsReadLock* on a per entity basis, and synchronization lasts only for the duration of the call. Such locks need the address a *std::atomic\<uint32_t\>*  (a VECS mutex) as input parameter, and every entity has its own such VECS mutex. You can get the VECS mutex of an entity by calling *mutex()* on a *handle* or an *iterator*. The call always returns a valid mutex, even if the entity has been erased and is no longer valid.

Note that calls to *VecsRegistry\<\>* and *VecsHandle* are eventually forwarded to their counterparts in class *VecsRegistry\<E\>* with the same name. Thus, these member functions are also implicitly internally synchronized.

The following list shows the calls that are internally or externally synchronized in class *VecsRegistry\<E\>*:




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
