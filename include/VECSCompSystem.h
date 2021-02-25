#ifndef VECSCOMPONENT_H
#define VECSCOMPONENT_H

#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VTLL.h"


namespace vecs {

	//-------------------------------------------------------------------------
	//component types

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
	};

	struct VeComponentAnimation {
	};

	struct VeComponentCollisionShape {
	};

	struct VeComponentRigidBody {
	};

	using VeComponentTypeListSystem = vtll::type_list<
		VeComponentPosition
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


	//-------------------------------------------------------------------------
	//engine entity types

	using VeEntityTypeNode = VeEntityType<VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityTypeDraw = VeEntityType<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeAnimation = VeEntityType<VeComponentAnimation>;
	//...

	using VeEntityTypeListSystem = vtll::type_list<
		  VeEntityTypeNode
		, VeEntityTypeDraw
		, VeEntityTypeAnimation
		// ,... 
	>;

}

#endif

