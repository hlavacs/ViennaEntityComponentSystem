#ifndef BASICTEST_H
#define BASICTEST_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include "VECSUtil.h"

namespace vecs {

	//-------------------------------------------------------------------------
	//define user components here
	
	/// \brief Example for a user component
	struct MyComponentName {
		std::string m_name;
	};

	/// \brief Example for a user component
	struct MyComponentPosition {
		glm::vec3 m_position;
	};

	/// \brief Example for a user component
	struct MyComponentOrientation {
		glm::quat m_orientation;
	};

	/// \brief Example for a user component
	struct MyComponentTransform {
		glm::mat4 m_transform;
	};

	/// \brief Example for a user component
	struct MyComponentMaterial {
		int i;
	};

	/// \brief Example for a user component
	struct MyComponentGeometry {
		int i;
	};

	/// \brief Example for a user component
	struct MyComponentAnimation {
		int i;
	};

	/// \brief Example for a user component
	struct MyComponentCollisionShape {
		int i;
	};

	/// \brief Example for a user component
	struct MyComponentRigidBody {
		int i;
	};

	struct TAG1 {};
	struct TAG2 {};

	//-------------------------------------------------------------------------
	//define user entity types here

	using MyEntityTypeNode = vtll::type_list<MyComponentName, MyComponentPosition, MyComponentOrientation, MyComponentTransform>;
	using MyEntityTypeDraw = vtll::type_list<MyComponentName, MyComponentPosition, MyComponentOrientation, MyComponentTransform, MyComponentMaterial, MyComponentGeometry>;
	using MyEntityTypeAnimation = vtll::type_list<MyComponentName, MyComponentAnimation>;

	using MyEntityTypeList = vtll::type_list<
		  MyEntityTypeNode
		, MyEntityTypeDraw
		, MyEntityTypeAnimation
		// ,... 
	>;

	//-------------------------------------------------------------------------
	//tag maps

	using MyEntityTagMap = vtll::type_list<
		vtll::type_list< MyEntityTypeNode, vtll::type_list< TAG1, TAG2 > >
		//, ...
	>;

	//-------------------------------------------------------------------------
	//user size maps

	using VeTableSizeMap = vtll::type_list<
		  //vtll::type_list< MyEntityTypeNode, vtll::value_list< 1<<15 > >
		//, vtll::type_list< MyEntityTypeDraw, vtll::value_list< 1<<15 > >
		//, vtll::type_list< MyEntityTypeAnimation, vtll::value_list< 1<<15 > >
		//, ...
	>;

	//-------------------------------------------------------------------------
	//user table layouts

	using VeTableLayoutMap = vtll::type_list<
		  vtll::type_list< MyEntityTypeNode, VECS_LAYOUT_COLUMN >
		, vtll::type_list< MyEntityTypeDraw, VECS_LAYOUT_COLUMN >
		, vtll::type_list< MyEntityTypeAnimation, VECS_LAYOUT_COLUMN >
		//, ...
	>;

	VECS_DECLARE_PARTITION(, MyEntityTypeList, MyEntityTagMap, VeTableSizeMap, VeTableLayoutMap);


}

#endif

