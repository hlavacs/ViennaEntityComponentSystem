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

git submodule init
git submodule update

