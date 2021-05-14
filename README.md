# Vienna Entity Component System (VECS)

The Vienna Entity Component System (VECS) is a C++20 based ECS for game engines. An ECS is a data structure storing *entities* of different types.
Entity types consist of a number of *components*.
Different entity types can be composed of different components.

An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. Additionally, a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a physics system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system animation is only interested into position, orientation, and animation data.
Systems do not have to be specially defined, anything that specifies a number of components it is interested in and then loops over all entities that contain these components can be seen as a system.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* Make one or several partitions of your data.
* Compile time definition of components and entities. VECS creates a rigorous compile time frame around your data, minimizing runtime overhead.
* Can use tags to define subsets of entities dynamically.
* Designed to be used by multiple threads in parallel, low overhead locking.
* Easy to use, various ways to access the entities and their components.
* High performance by default, components are ordered sequentially in memory (row or column wise) for cache friendly  access.

VECS is currently in BETA.

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

In this example, *N = 1<<15*, and this is fixed at compile time. Note that *N* eventually must be a power of 2. If you specify any other number *N0*, VECS at compile time for *N* uses the smallest power of 2 that is larger or equal to *N0*. If nothing is specified, the default value for *N* is

    using VecsTableSizeDefault = vtll::value_list< 1<<10 >;

Another choice relates to the layout of segments. Layouts of segments can be row wise or column wise (default). In row wise, the components of each entity are stored next to each other. Thus when accessing all components at once, cache efficiency is best. In column wise, components are stored in separate arrays, and cache performance is optimal if only single components are accessed in for-loops. You can choose the layout for a specific entity type *E* like so:

    using MyTableLayoutMap = vtll::type_list<
      vtll::type_list< MyEntityTypeNode, VECS_LAYOUT_COLUMN >  // or VECS_LAYOUT_ROW
      //, ...
    >;

Now we have all necessary data to define a VECS partition, represented by *MyEntityTypeList*:

    VECS_DECLARE_PARTITION(, MyEntityTypeList, MyEntityTagMap, MyTableSizeMap, MyTableLayoutMap);

This defines the following classes (will be explained later):
* VecsHandle
* VecsRegistry<...>
* VecsIterator<...>
* VecsRange<...>

The first parameter in the macro VECS_DECLARE_PARTITION that was left out in the example is a partition name, which would be inserted between "Vecs" and the class type. By using names and different entity lists, you can declare different partitions for VECS. For instance, if you want to use VECS in your core game engine, for some other partition *MySystemEntityTypeList* you could set

    VECS_DECLARE_PARTITION(System, MySystemEntityTypeList, MySystemEntityTagMap, MySystemTableSizeMap, MySystemTableLayoutMap);

This defines the following classes by adding the partition name "System":
* VecsSystemHandle
* VecsSystemRegistry\<...\>
* VecsSystemIterator\<...\>
* VecsSystemRange\<...\>

However, the above classes are only aliases of the following basic templates:
* VecsHandleT\<P\>
* VecsRegistryT\<P, ...\>
* VecsIteratorT\<P, ...\>
* VecsRangeT\<P, ...\>

You can use the templates directly by simply adding your partition *P* as first template parameter. This way, you can use the same API names for different partitions, and differentiate between the partitions by using different *P* s. For example, you can use *VecsHandleT\<MyEntityTypeList\>* and *VecsHandleT\<MySystemEntityTypeList\>* instead of *VecsHandle* or *VecsSystemHandle* respectively.

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

    handle.is_valid(); //do this in a range based or for_each loop

If the handle is valid, then it IDs an entity in VECS. However, the entity might have been erased from VECS previously, e.g. by some other thread. You can test whether the entity that the handle represents is still in VECS by calling either of these:

    handle.has_value();               //true if VECS contains the entity
    VecsRegistry{}.has_value(handle); //true if VECS contains the entity
    VecsRegistry<MyEntityTypeNode>{}.has_value(handle); //true if VECS contains the entity AND entity is of type MyEntityTypeNode

When going through a range based or for_each loop, calling *is_valid()* should be preferred. A part of a handle contains a type ID for the entity type. You can get the type index by calling

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

The last call is only a wrapper for the concept *is_component_of<>* which is evaluated at compile time.

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
Thus, using the above example, the final list of all entity types will also contain the following types:

    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation >;
    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG1 >;
    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG2 >;
    vtll::type_list< MyComponentName, MyComponentPosition, MyComponentOrientation, TAG1, TAG2 >;

Furthermore, all tags from the partition's tag map are also copied to the partition's tag list *VecsEntityTagList<P>*, which for the above example will - amongst others - also contain *TAG1* and *TAG2*. Tags are useful for grouping entity types, since any of the above tagged entity type lands in its own table. You can create tagged entities like any normal entity:

    auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
    auto h2 = VecsRegistry<MyEntityTypeNodeTagged<TAG1>>{}.insert(MyComponentName{ "Node T1" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});

In this example, two entities derived from the same basic entity type *MyEntityTypeNode* are created, one without a tag, and one with a tag. As you can see, you do not have to specify any value for tags, nor will tag references be included when using tuples, iterators or ranges.
You can always add or remove tags from existing entities by calling the *transform()* function on their handles:

    VecsRegistry<MyEntityTypeNode>{}.transform(h2);

The above call turns an entity of type *MyEntityTypeNodeTagged\<TAG1\>* into an entity of type *MyEntityTypeNode*, thus effectively removing the tag. Internally, all components are moved from the *MyEntityTypeNodeTagged\<TAG1\>* table to the *MyEntityTypeNode* table. Note that therefore references to the components for the previous type are no longer valid after this operation!

In the next section, iterators and ranges are described, which enable you to efficiently loop over groups of entities.


## Iterators, Ranges and Loops

The basic use case of any ECS is to loop over all or some of the entities. VECS allows this in various ways. The basic mechanism is given by iterators and ranges. These can then be used to compose loops.

Iterators are generalized pointers, and are the main mechanism for looping over entities in VECS. Iterators are implemented in class

    template<typename... Ts>
    class VecsIterator;

and come in two versions. The first form is a general iterator that can point to any entity and can be increased to jump ahead. The second one is an end-iterator, it is created by calling it with the boolean parameter *true*:

    VecsIterator<MyComponentName, VeUserComponentPosition> it;         //normal iterator
    VecsIterator<MyComponentName, VeUserComponentPosition> end(true);  //end iterator used as looping end point

Iterators have template parameters which determine which entity types their iterate over, and which components they can yield in a for-loop. This is implemented by deriving them from a common base class depending on the partition *P* and two template parameters *ETL* and *CTL*. *ETL* is a type list of entity types the iterator should iterate over, and *CTL* is a type list of components. The iterator offers a dereferencing operator*(), yielding a tuple that holds a handle and references to the components:

    template<typename P, typename ETL, typename CTL>
    class VecsIteratorBaseClass {

        //...

      public:
        using value_type		= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleT<P>>, CTL > >;
        using reference			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleT<P>&>, vtll::to_ref<CTL> > >;
        using pointer			= vtll::to_tuple< vtll::cat< vtll::tl<VecsHandleT<P>*>, vtll::to_ptr<CTL> > >;
        using iterator_category = std::forward_iterator_tag;
        using difference_type	= size_t;
        using last_type			= vtll::back<ETL>;	///< last type for end iterator

        VecsIteratorBaseClass(bool is_end = false) noexcept;							///< Constructor that should be called always from outside
        VecsIteratorBaseClass(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept;	///< Copy constructor

        auto operator=(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> VecsIteratorBaseClass<P, ETL,CTL>;	///< Copy
        auto operator+=(size_t N) noexcept										-> VecsIteratorBaseClass<P, ETL,CTL>;	///< Increase and set
        auto operator+(size_t N) noexcept										-> VecsIteratorBaseClass<P, ETL,CTL>;	///< Increase
        auto operator!=(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> bool;	///< Unqequal
        auto operator==(const VecsIteratorBaseClass<P, ETL, CTL>& v) noexcept	-> bool;	///< Equal

        inline auto handle() noexcept		-> VecsHandleT<P>&;			///< Return handle ref of the current entity
        inline auto mutex_ptr() noexcept	-> std::atomic<uint32_t>*;	///< Return pointer to the mutex of this entity
        inline auto is_valid() noexcept		-> bool;					///< Is currently pointing to a valid entity

        inline auto operator*() noexcept		-> reference;							///< Access the data
        inline auto operator++() noexcept		-> VecsIteratorBaseClass<P, ETL,CTL>&;	///< Increase by 1
        inline auto operator++(int) noexcept	-> VecsIteratorBaseClass<P, ETL,CTL>&;	///< Increase by 1
        inline auto size() noexcept				-> size_t;								///< Number of valid entities
        inline auto sizeE() noexcept			-> size_t;
    };

When specifying *VecsIterator<Ts...>* the lists *ETL* and *CTL* are created depending on the *Ts*, and the base class is determined. In the following the various ways to turn the parameters *Ts* into the base class *ETL* and *CTL* are explained.


### Iterating over Specific Entity types

You can specify a list *ETL* of entity types directly, which may also include entity types with tags. Then the iterator will loop over exactly this list of entity types. For instance

    VecsIterator< vtll::type_list<MyEntityTypeNode, MyEntityTypeNodeTagged<TAG2>> > it;
    VecsIterator< vtll::type_list<MyEntityTypeNode, MyEntityTypeNodeTagged<TAG2>> > end(true);

    while( it!= end) {
      auto [handle, pos, orient, trans] = *it; //get access to the first entity, might be invalid so you better check
      //VecsWriteLock lock(handle.mutex()); //use this if you are using multithreading
      if( handle.is_valid() ) {
        //...
      }
      ++it; //use this operator if possible, the alternative it++ creates a costly copy!
    }

lets you access the first possible entity in either tables for types *MyEntityTypeNode* and *MyEntityTypeNodeTagged<TAG2>*. Accessing the operator\*() yields a handle (by reference), and references to those components that all the given entity types contain (their intersection). References to tags are never yielded.
The first definition is the *CTL* for the case for a specific list of entity types, which is the intersection of all given types, and removing the tag types. Then the specific *VecsIteratorTemplate* is defined, and the base class as well as the constructor for it are defined. Finally, *VecsIterator* is then derived from *VecsIteratorTemplate*.

### Iterating over Basic Entity Types and All their Tagged Versions

Another use case is iterating over basic types and all their tagged version. For instance, iterating over all *MyEntityTypeNode* entities, but also over its tagged versions *MyEntityTypeNodeTagged<TAG1>*, *MyEntityTypeNodeTagged<TAG2>*, and *MyEntityTypeNodeTagged<TAG1, TAG2>* you can simply use them in as (variadic) template parameters directly:

    VecsIterator<MyEntityTypeNode> it;

This can be done for any list of basic entity types. If you specify any type for which no tags are specified in the tag map, then this type is used as is. Again, the components are those contained in all types, minus the tags. The implementation of this is similarly simple (see *VECSIterator.h*), using the metafunction *expand_tags<...>* (defined in *VECS.h*), that takes a list of entities and expands it with the tagged versions if there are any.


### Iterating over Entity Types That Contain given Tags

If you want to loop over all tagged versions of a basic type *E* (you can only use one entity type) that have given tags, then you can specify e.g.

    VecsIterator<MyEntityTypeNode, TAG1> it;

This iterator loops over all entities of the types MyEntityTypeNodeTagged\<TAG1\> and MyEntityTypeNodeTagged\<TAG1,TAG2\>. It is essentially the same as

    VecsIterator< vtll::tl< MyEntityTypeNodeTagged<TAG1>, MyEntityTypeNodeTagged<TAG1, TAG2> > > it;

which would specify the types explicitly. Again, expansion is done only if the entity type *E* does have tags specified in the tag map. The components are the components of the entity types minus the tags, so basically the components of the untagged basic entity type *E*.

### Iterating over All Entity Types that Contain given Component Types

The next method allows you to specify a number of (variadic) component types. The iterator would then loop over all entity types that contain these types. The components yielded are exactly the components specified, e.g.

    VecsIterator<MyComponentName, MyComponentPosition> it;

loops over all entity types having these components!

### Iterating over All Entity Types

Finally, you can loop over all entity types in VECS, simply by specifying nothing:

    VecsIterator<> it;

The yielded components are again those contained in all entity types (this might be e.g. a name, a GUID, etc.), or more likely, an empty list. In any case, the iterator operator\*() will always yield the entity handle.

### Ranges and Range Based Loops

In C++, a range is any class that offers *begin()* and *end()* functions yielding respective iterators. VECS offers a special range class *VecsRange<...>* for this. The meanings of the template parameters are exactly the same as for iterators, and ranges are simply two iterators put together. A *VecsRange* can be used directly in a *C++ range based loop*:

    for (auto [handle, name] : VecsRange<MyComponentName, MyComponentPosition>{}) {
      if( handle.is_valid() ) {
        //....
      }
    }

This version is the fastest version, but when obtaining the references does not lock the entities. Thus, when using multithreading, you should use this version only if you can make sure that no two threads write to the same entities (or components) at the same time. One way to achieve this is to run so called systems (range based loops) together that do not access the same components. Additionally, VECS ranges offer a thread save range based loop called *for_each*:

    VecsRange<MyEntityTypeNode, TAG1>{}.for_each([&](VecsHandle& handle, auto& name, auto& pos, auto& orient, auto& transf) {
      //...
    });

This loop guarantees that any loop iteration contains only valid entities, which are also automatically write locked, depending on the parameter sync. Due to the write lock, this loop is slightly slower than the C++ range based version, but you do not have to worry about locking or using the references. The implementation looks like this:

    template<typename P, typename C>
    struct Functor;

    template<typename P, template<typename...> typename Seq, typename... Cs>
    struct Functor<P, Seq<Cs...>> {
      using type = void(VecsHandleTemplate<P>&, Cs&...);	///< Arguments for the functor
    };

    inline auto for_each(std::function<typename Functor<P, CTL>::type> f, bool sync = true) -> void {
      auto b = begin();
      auto e = end();
      for (; b != e; ++b) {
        if( sync ) VecsWriteLock lock(b.mutex_ptr());		///< Write lock
        if (!b.is_valid()) continue;
        std::apply(f, *b);					///< Run the function on the references
      }
    }

You see that the loop is actually quite simple, and you can easily come up with your own versions, which e.g. only read lock entities, etc.

## Parallel Operations and Performance

In principle, VECS allows parallel operations on multiple threads if certain principles are upheld.
First, VECS only locks some internal data structures, but it does not lock single entities (the only exception being the VecsRangeBaseClass::for_each loop). So any time you change the state of an entity by writing on it (through a reference of calling *update()*), erasing it, swapping it with another entity of the same type, transforming it, etc., you should create a *VecsWriteLock* for this entity.

    {
      VecsIterator<MyComponentName> it;
      VecsWriteLock lock(it.mutex());		///< Write lock using an iterator
      //... now the entity is completely locked
    }

A write lock excludes any other lock to the entity, be it writing or reading.
If you only read components, then a read lock may be faster, since read locks do not prevent other read locks, but they prevent write locks.

    for (auto [handle, name] : VecsRange<MyComponentName>{}) {
      VecsReadLock handle.mutex() );
      if( handle.is_valid() ) {
        //....
      }
    }

An example for looping over entities in parallel on several threads is given in example program parallel.cpp. For parallelization, the example uses the Vienna Game Job System (VGJS, https://github.com/hlavacs/ViennaGameJobSystem), a sibling project of VECS. VGJS is a header-only C++20 library enabling using function pointers, std::function, lambdas, or coroutines to be run in the job system.

    template<template<typename...> typename R, typename... Cs>
    void do_work(R<Cs...> range) {
        size_t i = 0;

        for (auto [handle, pos] : range) {
            if (!handle.is_valid()) continue;
            pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
            ++i;
        }

        /*bool sync = true;
      	range.for_each([&](auto handle, auto& pos) {
      		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
      		++i;
      	}, sync);*/
    }

    vgjs::Coro<> start( size_t num ) {
        std::cout << "Start \n";

    	co_await [&]() { init(num); };

    	int thr = 12;
    	std::pmr::vector<vgjs::Function> vec;

    	auto ranges = VecsRange<MyComponentPosition>{}.split(thr);
    	for (int i = 0; i < ranges.size(); ++i) {
    		vec.push_back(vgjs::Function([=]() { do_work(ranges[i]); }, vgjs::thread_index_t{}, vgjs::thread_type_t{ 1 }, vgjs::thread_id_t{ i }));
    	}

    	auto lin =
    		vgjs::Function([&]() {
    				do_work(VecsRange<MyComponentPosition>{});
    			}
    			, vgjs::thread_index_t{}, vgjs::thread_type_t{1}, vgjs::thread_id_t{100});

    	auto t0 = high_resolution_clock::now();

    	co_await lin;

    	auto t1 = high_resolution_clock::now();

    	co_await vec;

    	auto t2 = high_resolution_clock::now();

    	auto d1 = duration_cast<nanoseconds>(t1 - t0);
    	auto d2 = duration_cast<nanoseconds>(t2 - t1);

    	double dt1 = d1.count() / 1.0;
    	double dt2 = d2.count() / 1.0;

    	size_t size = VecsRegistry{}.size();

    	std::cout << "Num " << 2 * num << " Size " << size << "\n";
    	std::cout << "Linear " << dt1        << " ns Parallel 1 " << dt2        << " ns Speedup " << dt1 / dt2 << "\n\n";
    	std::cout << "Linear " << dt1 / size << " ns Parallel 1 " << dt2 / size << " ns \n";

      vgjs::terminate();
      co_return;
    }

Note that since the ranges do not overlap, there is actually no need for synchronization of no other thread accesses the entities. Thus there is no lock in the range based for loop.
If there are other threads accessing the entities, then you can either introduce locks, or switch to the *for_each* version. Not that this version will take longer time on average, due to locking every entity before accessing it.
