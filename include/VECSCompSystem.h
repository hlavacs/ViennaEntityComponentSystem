#ifndef VECSCOMPONENT_H
#define VECSCOMPONENT_H

#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VTLL.h"


namespace vecs {

	//-------------------------------------------------------------------------
	//component types

	/** \brief Example for a system component*/
	struct VeComponentName {
		std::string m_name;
	};

	/** \brief Example for a system component*/
	struct VeComponentPosition {
		glm::vec3 m_position;
	};

	/** \brief Example for a system component*/
	struct VeComponentOrientation {
		glm::quat m_orientation;
	};

	/** \brief Example for a system component*/
	struct VeComponentTransform {
		glm::mat4 m_transform;
	};

	/** \brief Example for a system component*/
	struct VeComponentMaterial {
		int i;
	};

	/** \brief Example for a system component*/
	struct VeComponentGeometry {
		int i;
	};

	/** \brief Example for a system component*/
	struct VeComponentAnimation {
		int i;
	};

	/** \brief Example for a system component*/
	struct VeComponentCollisionShape {
		int i;
	};

	/** \brief Example for a system component*/
	struct VeComponentRigidBody {
		int i;
	};

	using VeComponentTypeListSystem = vtll::type_list<
		VeComponentName
		, VeComponentPosition
		, VeComponentOrientation
		, VeComponentTransform
		, VeComponentMaterial
		, VeComponentGeometry
		, VeComponentAnimation
		, VeComponentCollisionShape
		, VeComponentRigidBody
		//, ...
	>;

	//-------------------------------------------------------------------------
	//entity types

	/**
	* \brief Base of all entity types
	*/
	template <typename... Ts>
	struct VeEntityType {};

	using VECS_LAYOUT_ROW = std::integral_constant<bool, true>;
	using VECS_LAYOUT_COLUMN = std::integral_constant<bool, false>;
	using VECS_LAYOUT_DEFAULT = VECS_LAYOUT_COLUMN;

	//-------------------------------------------------------------------------
	//engine entity types

	using VeEntityTypeNode = VeEntityType<VeComponentName, VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityTypeDraw = VeEntityType<VeComponentName, VeComponentPosition, VeComponentOrientation, VeComponentTransform, VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeAnimation = VeEntityType<VeComponentName, VeComponentAnimation>;
	//...

	using VeEntityTypeListSystem = vtll::type_list<
		  VeEntityTypeNode
		, VeEntityTypeDraw
		, VeEntityTypeAnimation
		// ,... 
	>;

	using VeTableSizeMapSystem = vtll::type_list<
		  vtll::type_list< VeEntityTypeNode,		vtll::value_list< 15, 20 > >
		, vtll::type_list< VeEntityTypeDraw,		vtll::value_list< 15, 20 > >
		, vtll::type_list< VeEntityTypeAnimation,	vtll::value_list< 15, 20 > >
		//, ...
	>;

	using VeTableLayoutMapSystem = vtll::type_list<
		  vtll::type_list< VeEntityTypeNode, VECS_LAYOUT_COLUMN >
		, vtll::type_list< VeEntityTypeDraw, VECS_LAYOUT_COLUMN >
		, vtll::type_list< VeEntityTypeAnimation, VECS_LAYOUT_COLUMN >
		//, ...
	>;


}

#endif

