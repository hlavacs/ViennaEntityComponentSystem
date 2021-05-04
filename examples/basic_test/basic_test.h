#ifndef BASICTEST_H
#define BASICTEST_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include "VECSUtil.h"

#include "VECSCompSystem.h"

#define VECS_USER_DATA

namespace vecs {

	//-------------------------------------------------------------------------
	//define user components here

	/** \brief Example for a user component*/
	struct VeComponentName {
		std::string m_name;
	};

	/** \brief Example for a user component*/
	struct VeComponentPosition {
		glm::vec3 m_position;
	};

	/** \brief Example for a user component*/
	struct VeComponentOrientation {
		glm::quat m_orientation;
	};

	/** \brief Example for a user component*/
	struct VeComponentTransform {
		glm::mat4 m_transform;
	};

	/** \brief Example for a user component*/
	struct VeComponentMaterial {
		int i;
	};

	/** \brief Example for a user component*/
	struct VeComponentGeometry {
		int i;
	};

	/** \brief Example for a user component*/
	struct VeComponentAnimation {
		int i;
	};

	/** \brief Example for a user component*/
	struct VeComponentCollisionShape {
		int i;
	};

	/** \brief Example for a user component*/
	struct VeComponentRigidBody {
		int i;
	};

	struct TAG1 {};
	struct TAG2 {};

	//-------------------------------------------------------------------------
	//define user entity types here

	using VeEntityTypeNode = vtll::type_list<VeComponentName, VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityTypeDraw = vtll::type_list<VeComponentName, VeComponentPosition, VeComponentOrientation, VeComponentTransform, VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeAnimation = vtll::type_list<VeComponentName, VeComponentAnimation>;

	using VeUserEntityTypeList = vtll::type_list<
		  VeEntityTypeNode
		, VeEntityTypeDraw
		, VeEntityTypeAnimation
		// ,... 
	>;

	//-------------------------------------------------------------------------
	//tag maps

	using VeUserEntityTagMap = vtll::type_list<
		vtll::type_list< VeEntityTypeNode, vtll::type_list< TAG1, TAG2 > >
		//, ...
	>;

	//-------------------------------------------------------------------------
	//user size maps

	using VeUserTableSizeMap = vtll::type_list<
		  //vtll::type_list< VeEntityTypeNode, vtll::value_list< 1<<15 > >
		//, vtll::type_list< VeEntityTypeDraw, vtll::value_list< 1<<15 > >
		//, vtll::type_list< VeEntityTypeAnimation, vtll::value_list< 1<<15 > >
		//, ...
	>;

	//-------------------------------------------------------------------------
	//user table layouts

	using VeUserTableLayoutMap = vtll::type_list<
		  vtll::type_list< VeEntityTypeNode, VECS_LAYOUT_COLUMN >
		, vtll::type_list< VeEntityTypeDraw, VECS_LAYOUT_COLUMN >
		, vtll::type_list< VeEntityTypeAnimation, VECS_LAYOUT_COLUMN >
		//, ...
	>;

}

#endif

