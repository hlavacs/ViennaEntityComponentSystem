/**
* \file VECSCompSystem.h
* \brief Edit this file to define system entities and components for your game engine.
*/

#ifndef VECSCOMPONENTSYSTEM_H
#define VECSCOMPONENTSYSTEM_H

#include "VTLL.h"


namespace vecs {

	//-------------------------------------------------------------------------
	//component types

	/** \brief Example for a system component*/
	//struct VeSystemComponentName {
	//	std::string m_name;
	//};

	/** \brief List of all system components*/
	using VeSystemComponentTypeList = vtll::type_list<
		//VeSystemComponentName
		//, ...
	>;

	//-------------------------------------------------------------------------
	//Table segment layout

	using VECS_LAYOUT_ROW = std::integral_constant<bool, true>;		///< Layout is row wise. Good if all components of an entity are needed at once.
	using VECS_LAYOUT_COLUMN = std::integral_constant<bool, false>;	///< Layout is column wise. Good is only single components of entities are needed.
	using VECS_LAYOUT_DEFAULT = VECS_LAYOUT_COLUMN;					///< Default is column wise

	//-------------------------------------------------------------------------
	//engine entity types

	/** \brief An example entity that is composed of one VeSystemComponentName */
	//using VeSystemEntityTypeNode = vtll::type_list<
		//VeSystemComponentName
		//, ...
	//>;
	//...

	/** \brief The list of all system entity types */
	using VeSystemEntityTypeList = vtll::type_list<
		//VeSystemEntityTypeNode
		// ,... 
	>;

	//-------------------------------------------------------------------------
	//engine size maps

	/** 
	* \brief A map defining table sizes for entity types. If no entry is given then a default value is chosen.
	* 
	* The entries A and B define the size of a table segment (2^A) and default maximum number of entities of this type possible (2^B).
	* The maximum number can later be changed to a larger value. 
	*/
	using VeSystemTableSizeMap = vtll::type_list<
  	    //vtll::type_list< VeSystemEntityTypeNode, vtll::value_list< 15, 20 > >
		//, ...
	>;

	//-------------------------------------------------------------------------
	//engine table layouts

	/**
	* \brief A map defining the layouts of table segments.
	* 
	* Layouts of segments can be row wise or column wise. In row wise, the components of each entity are stored 
	* next to each other. Thus when accessing all components at once, cache efficiency is best.
	* In column wise, components are stored in separate arrays, and cache performance is optimal if
	* only single components are accessed in for loops.
	*/
	using VeSystemTableLayoutMap = vtll::type_list<
		//vtll::type_list< VeSystemEntityTypeNode, VECS_LAYOUT_COLUMN >
		//, ...
	>;


}

#endif

