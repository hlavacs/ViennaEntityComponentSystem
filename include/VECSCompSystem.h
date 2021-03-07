#ifndef VECSCOMPONENT_H
#define VECSCOMPONENT_H

#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VTLL.h"


namespace vecs {

	//-------------------------------------------------------------------------
	//component types

	struct VeComponentName {
		std::string m_name;
	};

	struct VeComponentPosition {
		glm::vec3 m_position;
	};

	struct VeComponentOrientation {
		glm::quat m_orientation;
	};

	struct VeComponentTransform {
		glm::mat4 m_transform;
	};

	struct VeComponentMaterial {
		int i;
	};

	struct VeComponentGeometry {
		int i;
	};

	struct VeComponentAnimation {
		int i;
	};

	struct VeComponentCollisionShape {
		int i;
	};

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

	template <typename... Ts>
	struct VeEntityType {};

	using VECS_LAYOUT_ROW = std::integral_constant<bool, true>;
	using VECS_LAYOUT_COLUMN = std::integral_constant<bool, false>;
	using VECS_LAYOUT_DEFAULT = VECS_LAYOUT_COLUMN;

	//-------------------------------------------------------------------------
	//engine entity types

	using VeEntityTypeNode = VeEntityType<VeComponentName, VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityTypeDraw = VeEntityType<VeComponentName, VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeAnimation = VeEntityType<VeComponentName, VeComponentAnimation>;
	//...

	using VeEntityTypeListSystem = vtll::type_list<
		  VeEntityTypeNode
		, VeEntityTypeDraw
		, VeEntityTypeAnimation
		// ,... 
	>;

	using VeTableSizeDefault = vtll::value_list< 10, 16 >;

	using VeTableSizeMapSystem = vtll::type_list<
		  vtll::type_list< VeEntityTypeNode,		vtll::value_list< 12, 18 > >
		, vtll::type_list< VeEntityTypeDraw,		vtll::value_list< 12, 18 > >
		, vtll::type_list< VeEntityTypeAnimation,	vtll::value_list< 8, 10 > >
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

