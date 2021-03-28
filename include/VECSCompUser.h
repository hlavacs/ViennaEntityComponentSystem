/**
* \file VECSCompUser.h
* \brief Edit this file to define user entities and components for your game using a game engine that is based on VECS.
* 
* Alternatively, you can define your entities and components in an include file of your choice and define VECS_USER_DATA to
* prevent VECS from including this file. See beginning of VECS.h, and the provided example projects.
* Include your file before VECS.h.
* 
* VECS requires the following type lists:
* VeUserComponentTypeList, VeUserEntityTypeList, VeUserTableSizeMap, and VeUserTableLayoutMap

*/

#ifndef VECSCOMPONENTUSER_H
#define VECSCOMPONENTUSER_H


#include <limits>
#include <typeinfo>
#include <typeindex>

#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VECSUtil.h"

namespace vecs {

	//-------------------------------------------------------------------------
	//define user components here

	/** \brief Example for a user component*/
	//struct VeComponentPosition {
	//	glm::vec3 m_position;
	//};

	/** \brief Example for a user component*/
	//struct VeComponentOrientation {
	//	glm::quat m_orientation;
	//};


	using VeUserComponentTypeList = vtll::type_list<
		//VeComponentPosition
		//, VeComponentOrientation
		//, ...
	>;

	//-------------------------------------------------------------------------
	//define user entity types here

	//using VeEntityTypeNode = VeEntityType<VeComponentPosition, VeComponentOrientation>;

	using VeUserEntityTypeList = vtll::type_list<
		//VeEntityTypeNode
		// ,... 
	>;

	//-------------------------------------------------------------------------
	//user size maps

	using VeUserTableSizeMap = vtll::type_list<
		//vtll::type_list< VeEntityTypeNode, vtll::value_list< 15, 20 > >
		//, ...
	>;

	//-------------------------------------------------------------------------
	//user table layouts

	using VeUserTableLayoutMap = vtll::type_list<
		//vtll::type_list< VeEntityTypeNode, VECS_LAYOUT_COLUMN >
		//, ...
	>;


}


#endif

