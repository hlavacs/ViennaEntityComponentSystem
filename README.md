# Vienna Entity Component System (VECS)

The Vienna Entity Component System (VECS) is a C++20 based ECS for game engines. An ECS is a data structure storing *entities* of different types.
Entity types consist of a number of *components*.
Different entity types can be composed of different components.

An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. Additionally, a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a physics system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system animation is only interested into position, orientation, and animation data.
Systems do not have to be specially defined, anything that specifies a number of components it is interested in and then loops over all entities that contain these components can be seen as a system.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* VECS is a template library. Make one or several disconnected partitions of your data through different instantiations.
* Compile time definition of components and entities. VECS creates a rigorous compile time frame around your data, minimizing runtime overhead.
* Can use tags to define subsets of entities dynamically.
* Designed to be used by multiple threads in parallel, low overhead locking.
* Easy to use, various ways to access the entities and their components through iterators and ranges.
* High performance by default, components are ordered sequentially in memory (row or column wise) for cache friendly access.
* Data structures can grow in memory (mostly) without locking. By choosing parameters carefully, you can avoid locking altogether, without preallocating all the memory (which you could if you want).


## The VECS Include Files

VECS is a header only library, consisting of the following header files:
* *IntType.h*: Template for strong integral types like *table_index_t* or *counter_t*. Such types enforce to use them explicitly as function parameters and prevent users from mixing them up with *size_t, int* or *unsigned long*. Also such a type can store a NULL value, and can be tested with *has_value()*.
* *VECSUtil.h*: Contains utility classes such as a class for implementing CRTP, mono state, and simple low-overhead read and write locks.
* *VECSTable.h*: Defines a table data structure that can grow with minimal synchronization overhead, even when accessed by multiple threads.
* *VECSIterator.h*: Functionality for iterating over entities in VECS.
* *VTLL.h*: The Vienna Type List Library is a collection of meta-algorithms for treating type lists and value lists.
* *VECS.h*: The main include file containing most of VECS functionality, loading all other include files. Include this file into your source code.

VECS depends on two projects, the Vienna Type List Library (VTLL) and Vienna Game Job System (VGJS).

### Vienna Type List Library (VTLL)

As stated, VECS is a compile time ECS. Compositions of entities are defined in source code only. VECS thus heavily relies on its partner project Vienna Type List Library (VTLL), a header-only meta-programming library that helps creating entity types, type lists, compositions of components, accessing list elements, mapping data, etc. Especially, entities are defined as *vtll::type_list<...>* (or its shorter version *vtll::tl<...>*) and therefore are nothing more than a list of types. For more information goto https://github.com/hlavacs/ViennaTypeListLibrary.


### Vienna Game Job System (VGJS)

In the parallelization examples, VECS makes use of another partner project, the Vienna Game Job System (VGJS). VGJS is a header-only library for helping spawning threads and submitting jobs to them. VGJS is unique in the sense that you can submit C++ functions, lambdas, function pointers, *std::function*s, vectors containing them, and more notably co-routines enabling easy dependencies between successive jobs. For more information see https://github.com/hlavacs/ViennaGameJobSystem.
VGJS is not necessary to run VECS, and as stated it is only used as an example for multithreading.


## The VECS API

An *Entity Component System* is a data structure that stores data components in some flat way, meaning that data is stored linearly in memory as a vector or table. The stored data are called *components*. Components can in principle be any type that being *movable* and *default constructible*.

**Note**: You *can* actually choose component types that are not movable, e.g., atomics, but obviously they cannot be moved, e.g., by calling swap, or compressing the table. So if you do either be aware that they can have any value afterwards, and must be reinitialized. Also make sure to enforce references when accessing them, e.g., by using auto& instead of auto, and avoid initializing or updating the whole component at once, since this cannot be done if any part of it cannot neither be copied nor moved.

Components can be plain old data types, structs, classes, (smart) pointers etc.
but not references since they must be bound to something when created. An example is given by

    #include "VECS.h"                 //include this file, then define components, entities, maps and partitions

    struct MyComponentName {
      std::string m_name;
    };

    struct MyComponentPosition {
      float x;
      float y;
      float z;
    };

    struct MyComponentResource {
      std::unique_ptr<my_resource_t> my_resource;
    };

As can be seen, these components consist of a std::string, floats and a unique pointer (which is movable but NOT copyable), and thus satisfy the conditions on components. Components are then used to define *entities*. Entities are composed of one or more components. This composition is defined using type lists, like this

    using MyEntityTypeNode = vtll::type_list<MyComponentName, MyComponentPosition>;

which defines the entity type *MyEntityTypeNode* to be composed of the two components. Likewise, you can define an arbitrary number of other entity types. A list of such entities is necessary for *declaring* a *VECS partition*, meaning that the particular partition can store all entities as given in this list, and none other. You can declare a list of entity types like so:

    using MyEntityTypeList = vtll::type_list<
      MyEntityTypeNode
      // ,...
    >;

In order to declare a VECS partition, besides the *entity list*, you need a *tag map*, a *size map*, and a *layout map*. All maps can be empty lists, in which case default values are used. Tags are simply empty structs (only their types are needed), and you can specify those entities that can be annotated by them with a tag map:

    struct TAG1{};
    struct TAG2{};

    using MyEntityTagMap = vtll::type_list<
      vtll::type_list< MyEntityTypeNode, vtll::type_list< TAG1, TAG2 > > //entities of type MyEntityTypeNode can have TAG1, TAG2, both or none as tags
      //, ...
    >;

Furthermore, components of entities are stored in tables, one table for each entity type (and tag combination). Tables are organized in segments, each segment is able to store *N = 2^L* rows of component data. For each entity type you can define *N* in a map:

    using MyTableSizeMap = vtll::type_list<
      vtll::type_list< MyEntityTypeNode, vtll::value_list< 1<<15 > >
      //, ...
    >;

In this example, *N = 1<<15*, and this is fixed at compile time. Note that *N* eventually must be a power of 2. If you specify any other number *N0*, VECS at compile time for *N* uses the smallest power of 2 that is larger or equal to *N0*. If nothing is specified, the default value for *N* is 1024.

Another choice relates to the layout of segments. Layouts of segments can be row wise or column wise (default). In row wise, the components of each entity are stored next to each other. Thus when accessing all components at once, cache efficiency is best. In column wise, components are stored in separate arrays, and cache performance is optimal if only single components are accessed in for-loops. You can choose the layout for a specific entity type *E* like so:

    using MyTableLayoutMap = vtll::type_list<
      vtll::type_list< MyEntityTypeNode, VECS_LAYOUT_COLUMN >  // or VECS_LAYOUT_ROW
      //, ...
    >;

Now we have all necessary data to define a *VECS partition* using this macro:

    namespace my_namespace {
      VECS_DECLARE_PARTITION(, MyEntityTypeList, MyEntityTagMap, MyTableSizeMap, MyTableLayoutMap);
    }

This defines the following types (will be explained later):
* my_namespace::PARTITION
* my_namespace::VecsHandle
* my_namespace::VecsRegistry<...>
* my_namespace::VecsIterator<...>
* my_namespace::VecsRange<...>

As can be seen, you can declare partitions represented by *my_namespace::PARTITION* in arbitrary namespaces. The macro simply does something like this:

    using PARTITION = vtll::type_list<MyEntityTypeList, MyEntityTagMap, MyTableSizeMap, MyTableLayoutMap>;

So a partition is uniquely identified by a type list that holds all given macro parameters, independent of the namespace it is declared in. Two identical *PARTITION* types thus result in the same partition. Two *PARTITION* types that differ in any way lead to two completely separated VECS partitions that do not share any data with each other.

The first parameter in the macro *VECS_DECLARE_PARTITION* that was left out in the example is a partition name, which would be inserted between "Vecs" and the class type.

By using different names and different entity lists, you can declare different partitions for VECS. For instance, if you want to use VECS in your core game engine, for some other partition *MySystemEntityTypeList* you could set

    namespace my_namespace {
      VECS_DECLARE_PARTITION(System, MySystemEntityTypeList, MySystemEntityTagMap, MySystemTableSizeMap, MySystemTableLayoutMap);
    }

This defines the following classes by adding the partition name "System":
* my_namespace::PARTITION
* my_namespace::VecsSystemHandle
* my_namespace::VecsSystemRegistry\<...\>
* my_namespace::VecsSystemIterator\<...\>
* my_namespace::VecsSystemRange\<...\>

Internally, the above names are just alias-declarations for the partial template specializations
* vecs::VecsHandleT\<my_namespace::PARTITION\>
* vecs::VecsRegistryT\<my_namespace::PARTITION, ...\>
* vecs::VecsIteratorT\<my_namespace::PARTITION, ...\>
* vecs::VecsRangeT\<my_namespace::PARTITION, ...\>

which you can also use, if you want to.

In the following examples it is assumed that there was no name given. If you use partition names, then change the examples accordingly by inserting the partition names into the VECS type names.


### The VECS Registry

Entities are stored in the *VecsRegistry*. This data structure uses the *mono state* pattern, so if you want to use it you simply instantiate it

    VecsRegistry reg{};

of create a temp object and directly use it

    std::cout << VecsRegistry{}.size() << "\n"; //number of entities currently in the VECS partition

There is only one registry state, and you can instantiate it any number of times, the result is always the same.

Using *VecsRegistry* is not bound to a specific entity type *E*, but commands eventually need this information. All calls are then passed on to *VecsRegistry\<E\>*, where *E* is an entity type and must be registered in the partition's entity type list.

*VecsRegistry\<E\>* is a specialized version of the registry made only for entity type *E*. It is recommended to always use this specialized version, if possible. For instance, if you want to create an entity of type *E*, you have at some point to specify the entity type. In the following, we define example entity types

    using MyEntityTypeNode = vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation >;
    using MyEntityTypeDraw = vtll::type_list< MyComponentName, MyComponentPosition, MyComponentMaterial, MyComponentGeometry>;

We assume that is has been registered as described in the previous section. Of course, any other similarly defined type can be used for the following examples as well. You can create an entity of type *MyEntityTypeNode* using:

    VecsHandle handle = VecsRegistry<MyEntityTypeNode>{}.insert("Node2", MyComponentPosition{}, MyComponentOrientation{});

Note that the parameters for this call must match the list of components that the entity is composed of. Move-semantics automatically apply. The result of creating an entity is a *handle*. A handle is an 8-bytes structure that uniquely identifies the new entity and you can use it later to access the entity again. A handle can be invalid, meaning that it does not point to any entity. You can test whether a handle is valid or not by calling

    handle.is_valid(); //do this in a range based loop

If the handle is valid, then it IDs an entity in VECS. However, the entity might have been erased from VECS previously, e.g. by some other thread. You can test whether the entity that the handle represents is still in VECS by calling either of these:

    handle.has_value();               //true if VECS contains the entity
    VecsRegistry{}.has_value(handle); //true if VECS contains the entity
    VecsRegistry<MyEntityTypeNode>{}.has_value(handle); //true if VECS contains the entity AND entity is of type MyEntityTypeNode

You can get the type index of an entity by calling

    handle.type();               //call this
    VecsRegistry{}.type(handle); //or this

The type ID is the index of the entity type in the entity type partition list, starting with 0. Reading a component from a given entity can be done by any of these methods:

    MyComponentPosition pos1 = handle.component<MyComponentPosition>();  //copy - use only if the component is copyable
    MyComponentPosition& pos2 = handle.component<MyComponentPosition>(); //reference
    MyComponentPosition* pos3 = handle.component_ptr<MyComponentPosition>(); //pointer

    auto  pos4 = VecsRegistry{}.component<MyComponentPosition>(handle); //copy - use only if the component is copyable
    auto& pos5 = VecsRegistry{}.component<MyComponentPosition>(handle); //reference
    auto* pos6 = VecsRegistry{}.component_ptr<MyComponentPosition>(handle); //pointer

    auto pos7 = VecsRegistry<MyEntityTypeNode>{}.component<MyComponentPosition>(handle); //if handle of type MyEntityTypeNode
    auto& pos8 = VecsRegistry<MyEntityTypeNode>{}.component<MyComponentPosition>(handle); //if handle of type MyEntityTypeNode
    auto* pos9 = VecsRegistry<MyEntityTypeNode>{}.component_ptr<MyComponentPosition>(handle); //if handle of type MyEntityTypeNode

Only the last version is actually checked by the compiler at compile time, and the first two version thus could result in an empty component being returned. You can call *has_component\<C\>()* to check whether an entity represented by a handle does contain a specific component of type *C* using any of these methods:

    bool b1 = handle.has_component<MyComponentPosition>();
    bool b2 = VecsRegistry{}.has_component<MyComponentPosition>(handle);
    bool b3 = VecsRegistry<MyEntityTypeNode>{}.has_component<MyComponentPosition>();

The last call is only a wrapper for the concept *is_component_of\<\>* which is evaluated at compile time.

Additionally, you can obtain all components at once packed into a tuple:

    auto tuple_ref = VecsRegistry<MyEntityTypeNode>{}.tuple(handle); //tuple with references to the components
    auto tuple_ptr = VecsRegistry<MyEntityTypeNode>{}.tuple_ptr(handle); //tuple with pointers to the components

Like with any C++ tuple, you can access the components either by specifying the index, or by specifying the type (which is unique for each entity type):

    auto  comp1 = std::get<MyComponentPosition&>(tuple_ref); //copy -  use only if the component is copyable
    auto& comp2 = std::get<MyComponentPosition&>(tuple_ref); //reference
    auto * comp3 = std::get<MyComponentPosition*>(tuple_ptr); //pointer

Since the above calls may yield references or addresses of components in VECS, you can read and modify the component directly. You can also utilize *std::move()* on any of them:

    handle.component<MyComponentPosition>() = MyComponentPosition{};

**NOTE:** references and pointers as obtained above may become eventually invalid. Do not store them for later use, only use them immediately and if you are sure that the entity is still valid. Store the handles instead, and test whether the handle is valid before using it. When using multithreading, use read or write locks (see below) to ensure safe accesses.

You can also update several components with one *update()* call (again, move semantics apply):

    handle.update( MyComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f}, MyComponentOrientation{} ); //move semantics due to rvalue references

    MyComponentPosition pos{ glm::vec3{-98.0f, -22.0f, -33.0f} };
    VecsRegistry{}.update(handle, pos); //copy semantics due to value reference

    VecsRegistry<MyEntityTypeNode>{}.update(handle, pos, MyComponentOrientation{}); //copy for first, move for the second

Finally, you can erase entities from VECS using any of these calls:

    handle.erase();
    VecsRegistry{}.erase(handle);
    VecsRegistry<MyEntityTypeNode>{}.erase(handle); //if handle of type MyEntityTypeNode

When an entity is erased, for any of its components, each component's *destructor* is called, if it has one and it is not trivially destructible. You can erase all entities by calling *clear()*:

    VecsRegistry{}.clear();                   //erase all entities in VECS
    VecsRegistry<MyEntityTypeNode>{}.clear(); //erase all entities of type MyEntityTypeNode, does NOT affect the tagged versions!

If an entity is erased, the space in the component table is *not* removed, just invalidated. Thus, erasing entities produces gaps in the data and iterating through all entities gets increasingly less efficient. In order to compress the component table, you have to stop multithreaded access to VECS, and call either

    VecsRegistry{}.compress();                    //compress all tables
    VecsRegistry<MyEntityTypeNode>{}.compress();  // compress only table for entity type MyEntityTypeNode

This will remove any gap in the component table(s) to speed up iterating through the entities in VECS. In a game, this can be done typically once per game loop iteration. Compressing may reshuffle rows, and if the ordering of entities is important, you may want to go through the entities once more and make sure that the ordering is ensured. An example for this is a scene graph, where nodes in a scene can have siblings and children, thus spanning up a tree that is stored in a flat table. When calculating the world matrices of the nodes, it is important to compute in the order from the tree root down to the leaves. Thus, when looping through the node entities, parent nodes must occur before child nodes. You can compare the positions in the component table using the function *table_index()*, use either of these:

    table_index_t first = handle.table_index();
    table_index_t second = VecsRegistry{}.table_index(handle);

If a child comes before a parent then you can swap the places of two entities in the component table using either

    VecsRegistry{}.swap(handle1, handle2); //if both handles of same type
    VecsRegistry<MyEntityTypeNode>{}.swap(handle1, handle2);  //if both handles of type MyEntityTypeNode

The entities are swapped only if they are of the same type. Note that this does not work between different entity types, especially also for the same entity type having different tags, since these are stored in different tables. You can ask for the number of valid entities currently in VECS using

    VecsRegistry{}.size(); //return total number of entities in VECS
    VecsRegistry<MyEntityTypeNode>{}.size(); //return number of entities of type MyEntityTypeNode in VECS


## Tags

Any unique combination of components results in a new entity type. Tags are empty components that can be appended to entity types, in any combination. Consider for example the Tags

    struct TAG1{};
    struct TAG2{};

Also consider the definition of *MyEntityTypeNode*

    using MyEntityTypeNode = vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation >;

By defining this helper, we can easily create variants for *MyEntityTypeNode*

    template<typename... Ts>
    using MyEntityTypeNodeTagged = vtll::app< MyEntityTypeNode, Ts... >;

by using the VTLL function *vtll::app<>* (append). For example, *MyEntityTypeNodeTagged\<TAG1\>* is the same as

    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG1 >

Note that since VECS does not know the component types you plan to add, it is your task to define a similar helper for each entity type, if you want to use them for tags.
VECS supports creating tagged variants by offering a tag map, e.g.

    using MyEntityTagMap = vtll::type_list<
      vtll::type_list< MyEntityTypeNode, vtll::type_list< TAG1, TAG2 > >
      //, ...
    >;

For each entity type, you can note the tags that this type can have. In the above example, the basic type *MyEntityTypeNode* can have the tags TAG1 and TAG2.
Internally VECS creates entity types with the basic type (in the example *MyEntityTypeNode*) and all possible tag combinations.
Thus, using the above example, the final list of all entity types *my_namespace::VecsRegistry\<\>::entity_type_list* also contains the following types:

    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation >;
    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG1 >;
    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG2 >;
    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG1, TAG2 >;

Also, the type list *my_namespace::VecsRegistry\<\>::component_type_list* contains all components of the partition, and the partition's tag list *my_namespace::VecsRegistry\<\>::entity_tag_list* contains all tag types, which for the above example will - amongst others - also contain *TAG1* and *TAG2*. The lists you can use are thus

    my_namespace::VecsRegistry\<\>::entity_type_list    //list of all entity types in the partition)
    my_namespace::VecsRegistry\<\>::component_type_list //list of all components and tags in the partition)
    my_namespace::VecsRegistry\<\>::entity_tag_list     //list of all tags in the partition)

Tags are useful for grouping entity types, since any of the above tagged entity type lands in its own table. You can create tagged entities like any normal entity:

    auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
    auto h2 = VecsRegistry<MyEntityTypeNodeTagged<TAG1>>{}.insert(MyComponentName{ "Node T1" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});

In this example, two entities derived from the same basic entity type *MyEntityTypeNode* are created, one without a tag, and one with a tag. As you can see, you do not have to specify any value for tags, nor will tag references be included when using tuples, iterators or ranges.
You can always add or remove tags from existing entities by calling the *transform()* function on their handles:

    VecsRegistry<MyEntityTypeNode>{}.transform(h2);

The above call turns an entity of type *MyEntityTypeNodeTagged\<TAG1\>* into an entity of type *MyEntityTypeNode*, thus effectively removing the tag. Internally, all components are moved from the *MyEntityTypeNodeTagged\<TAG1\>* table to the *MyEntityTypeNode* table. Note that therefore references to the components for the previous type are no longer valid after this operation!

In the next section, iterators and ranges are described, which enable you to efficiently loop over groups of entities.


## Iterators, Ranges and Loops

The basic use case of any ECS is to loop over all or some of the entities. VECS allows this in various ways. The basic mechanism is given by iterators and ranges. These can then be used to compose loops.

Iterators are generalized pointers, and are the main mechanism for looping over entities in VECS. Iterators are implemented in class *VecsIterator\<...\>*.
Iterators have a template parameter *CTL* determining which components they can yield *by operator *()* in a for-loop.
Upon being called, this dereferencing *operator *()* yields a tuple that holds *references* to a mutex, a handle and to the components listed in *CTL*. So through this tuple you can directly read and write values for the pointed entity.

However, you will not create iterators yourself, but rather use a range to get them.

### Ranges and Range Based Loops

In C++, a range is any class that offers *begin()* and *end()* functions yielding respective iterators. A *VecsRegistry<E>* is such a range, offering iterators to iterate over all its entities of type *E*. VECS also offers a special range class *VecsRange\<...\>* for iterating over combinations of entity types. The meanings of the template parameters are exactly the same as for iterators, and ranges are simply two iterators put together. A *VecsRange\<...\>* can be used directly in a *C++ range based loop*:

    for (auto [mutex, handle, name, pos] : VecsRange<MyComponentName, MyComponentPosition>{}) { //can use the mutex to lock the entity
      //VecsWriteLock(mutex);    //use read or write lock if access is multithreaded
      if( handle.is_valid() ) {
        //....
      }
    }

This version is the fastest version, but it does not check whether the handle retrieved is valid, so you have to do this explicitly. Additionally, when using multithreaded access, it is advisable to first lock the entity before checking its validity.

Additionally, VECS ranges offer a thread safe range based loop called *for_each*:

    bool sync = true; //if true -> lock the entities before entering the loop
    VecsRange<MyEntityTypeNode>{}.for_each([&](auto& mutex, auto& handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    }, sync);

This loop guarantees that any loop iteration contains only valid entities, which are also automatically write locked (if parameter *sync* is true). When using locking, this loop type is significantly slower than the C++ range based version, but you do not have to worry about locking or using the references. If you do not use locking, then the performance is about the same as the C++ range based version.

It must be noted that VECS ranges are compatible with the C++20 ranges specification, and are thus composable. Iterating over entities and their components can be done in one of the following ways.


### Iterating over Basic Entity Types and All their Tagged Versions

You can specify a type list *ETL* of entity types directly. Then the iterator will loop over this list of entity types AND try to expand the types using the tag map. For instance

    VecsRange< vtll::tl<MyEntityTypeNode> > range{};
    range.for_each([&](auto& mutex, auto& handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    }, false);

lets you access entities for types *MyEntityTypeNode*, but also over its tagged versions *MyEntityTypeNodeTagged\<TAG1\>*, *MyEntityTypeNodeTagged\<TAG2\>*, and *MyEntityTypeNodeTagged\<TAG1, TAG2\>*. Accessing the *operator\*()* yields a mutex and handle (by reference), and references to those components that all the given entity types contain (their intersection). **References to tags are never yielded**.

### Iterating over Specific Entity types

Another use case is iterating over given types only. For instance, for iterating over *MyEntityTypeNode* and *MyEntityTypeNodeTagged\<TAG1\>*, you can simply use them in as (variadic) template parameters directly:

    VecsRange< MyEntityTypeNode, MyEntityTypeNodeTagged\<TAG1\> > range;
    range.for_each([&](auto& mutex, auto& handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    }, false);

This can be done for any list of basic entity types. Again, the components are those contained in all types, minus the tags. If you want to iterate over only one type of entities, you can also use the registry directly:

    VecsRegistry<MyEntityTypeNode> range;
    range.for_each([&](auto& mutex, auto& handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    }, false);


### Iterating over Entity Types That Contain given Tags

If you want to loop over all tagged versions of a basic type *E* (you can only use one entity type) that *have given tags*, then you can specify e.g.

    VecsRange<MyEntityTypeNode, TAG1> range;

This iterator loops over all entities of the types *MyEntityTypeNodeTagged\<TAG1\>* and *MyEntityTypeNodeTagged\<TAG1,TAG2\>*. It is essentially the same as

    VecsRange< MyEntityTypeNodeTagged<TAG1>, MyEntityTypeNodeTagged<TAG1, TAG2> > range;

which would specify the types explicitly. Again, expansion is done only if the entity type *E* does have tags specified in the tag map. The components are the components of the entity types minus the tags, so basically the components of the untagged basic entity type *E*.

### Iterating over All Entity Types that Contain given Component Types

The next method allows you to specify a number of (variadic) component types. The iterator would then loop over all entity types that contain these types. The components yielded are exactly the components specified, e.g.

    VecsRange<MyComponentName, MyComponentPosition> range;

loops over all entity types having these components!

### Iterating over All Entity Types

Finally, you can loop over all entity types in VECS, simply by specifying nothing:

    VecsRange<> range;

The yielded components are again those contained in all entity types (this might be e.g. a name, a GUID, etc.), or more likely, an empty list. In any case, the iterator operator\*() will always yield the entity handle.


## Parallel Operations

In principle, VECS allows parallel operations on multiple threads if certain principles are upheld.
First, VECS only locks some internal data structures, but it does not lock single entities (the only exception being the VecsRangeBaseClass::for_each loop if you select syncing). So any time you change the state of an entity by writing on it (through a reference of calling *update()*), erasing it, swapping it with another entity of the same type, transforming it, etc., you should create a *VecsWriteLock* for this entity, or make sure that there is no other access to this entity at this time.

    {
      VecsHandle handle;
      VecsWriteLock lock(handle.mutex_ptr());		///< Write lock using a handle
      //... now the entity is completely locked
    }

A write lock excludes any other lock to the entity, be it writing or reading.
If you only read components, then a read lock may be faster, since read locks do not prevent other read locks, but they prevent write locks.

    for (auto [mutex, handle, name] : VecsRange<MyComponentName>{}) {
      VecsReadLock lock(mutex);
      if( handle.is_valid() ) {
        //....
      }
    }

Using *for_each* further simplifies the code:

    VecsRegistry<MyEntityTypeNode> range;
    range.for_each([&](auto& mutex, auto& handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    }, true);

Note that locking involves atomics, and severely degrades the performance of your program. Avoid locking if possible! Also locking several entities at once involves the possibility of a deadlock due to a cycle of locks. E.g. thread 1 locks entities A and B, thread 2 locks B and C, and thread 3 locks C and A.

### External Synchronization

An example for looping over entities in parallel on several threads is given in example program *parallel.cpp*. For parallelization, the example uses the Vienna Game Job System (VGJS, https://github.com/hlavacs/ViennaGameJobSystem), a sibling project of VECS. VGJS is a header-only C++20 library enabling using function pointers, std::function, lambdas, or coroutines to be run in the job system.

The example uses the function *split(size_t N)* of *VecsRange* to cut a range of entities into a set of *N* smaller, non-overlapping ranges. These ranges then can be scheduled to be worked on in parallel. Note that since the ranges do not overlap, there is actually no need for synchronization since no other thread accesses the entities. Thus there is no need for a lock in a range based for-loop. So instead of using locks, you can start threads in parallel if they do not interfere with each other. Two threads interfere with each other, if at least one is writing on a range of components (including erasing entities), and the other is reading or writing to the same range of components.
In order to avoid interference, only start threads in parallel that either only read the same component ranges, or do not share components that they write to.

If you use locks in the components, be aware that two threads easily can deadlock each other by locking the same entity pair in reverse order. VecsXLocks are kept simple and do not account for such an event. In order to avoid deadlocks by locking pairs, you have to ensure that the sequence the entities are locked is always the same. For instance, if you want to lock a pair, get them into some total ordering, e.g. by using their handle's map index, and always lock the smaller one first.
