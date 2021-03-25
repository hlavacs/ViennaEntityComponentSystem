#ifndef VECSCOMPONENTUSER_H
#define VECSCOMPONENTUSER_H


#include <limits>
#include <typeinfo>
#include <typeindex>
#include "VECSUtil.h"


#ifndef VECS_USER_DATA

namespace vecs {

	//-------------------------------------------------------------------------
	//define user components here

	/** \brief Example for a user component*/
	struct VeComponentUser1 {
		//..
	};
	//...

	//using VeComponentTypeListUser = vttl::type_list<VeComponentUser1>; //include all user components into this list
	
	using VeComponentTypeListUser = vtll::type_list<>; //default is no user define components


	//-------------------------------------------------------------------------
	//define user entity types here

	using VeEntityUser1 = VeEntityType<VeComponentPosition, VeComponentUser1>; //can be any mix of component types

	using VeEntityTypeListUser = vtll::type_list<
		//VeEntityeUser1
		//, ...  
	>;

	using VeTableSizeMapUser = vtll::type_list<
		  vtll::type_list< VeEntityUser1,	vtll::value_list< 10, 16 > >
		//, ...
	>;

	using VeTableLayoutMapUser = vtll::type_list<
		vtll::type_list< VeEntityUser1, VECS_LAYOUT_COLUMN >
		//, ...
	>;


}

#endif

#endif

