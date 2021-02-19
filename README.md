# Vienna Entity Component System

The Vienna Entity Component System (VECS) is an C++20 based ECS for game engines. In a nutshell, all relevant game entities are assigned a number of data components. Different entity types can be composed of different components. An ECS is a generic storage for such entities and their components. Entities can be composed and created, retrieved, updated, or erased from the storage. Additionally, a so-called *system* can have access to one or several components it is interested in, and work on them. For example, a physics system may be interested into components position, velocity, orientation, mass and moment of inertia. Another system animation is only interested into position, orientation, and animation data.

Important features of VECS are:
* C++20
* Header only, simply include the headers to your project
* Generic compile time definition of entities
* Designed to be used by multiple threads in parallel
* Easy to use, various ways to access the entities and their components
* High performance, components are ordered sequentially for cache friendly update access

VECS is currently under development and will be finished soon.
