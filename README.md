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



## Including VECS into your project


### Int Types


### Vienna Type List Library (VTLL)

As stated, VECS is a compile time ECS. Compositions of entities are defined in source code only. VECS thus heavily relies on its partner project Vienna Type List Library (VTLL), a header-only meta-programming library that helps creating entity types, lists, compositions of components, accessing list elements, mapping data, etc. For more information goto https://github.com/hlavacs/ViennaTypeListLibrary.


### Vienna Game Job System (VGJS)

In the parallelization examples, VECS makes use of another partner project, the Vienna Game Job System (VGJS). VGJS is a header-only library for helping spawning threads and submitting jobs to them. VGJS is unique in the sense that you can submit C++ functions, lambdas, function pointers, std::functions, vectors containing them, and more notably co-routines enabling easy dependencies between successive jobs. For more information see https://github.com/hlavacs/ViennaGameJobSystem.


## The VECS API


### Components and Entity Types


### The VECS Registry


### Handles


### Entity Proxy



### Iterators


## Parallel Operations



## Looping


### Range Based For loop



### for_each




## Performance




## Examples




## Implementation Details
