/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Collection of similarity groups for thread safe access
#ifndef _STRUS_VECTOR_SIM_GROUP_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SIM_GROUP_MAP_HPP_INCLUDED
#include "simGroup.hpp"
#include "internationalization.hpp"
#include "strus/base/atomic.hpp"
#include "strus/base/shared_ptr.hpp"
#include <vector>

namespace strus {

class GlobalCountAllocator
{
public:
	explicit GlobalCountAllocator( const ConceptIndex& maxNofCount_)
		:m_cnt(1),m_maxNofCount(maxNofCount_){}

	ConceptIndex get( unsigned int cnt);

	unsigned int nofGroupIdsAllocated() const
	{
		return m_cnt.value()-1;
	}

	void setCounter( const ConceptIndex& value);

private:
	strus::AtomicCounter<ConceptIndex> m_cnt;
	ConceptIndex m_maxNofCount;
};

class SimGroupIdAllocator
{
public:
	explicit SimGroupIdAllocator( GlobalCountAllocator* cnt_)
		:m_cnt(cnt_),m_freeList(){}
	SimGroupIdAllocator( const SimGroupIdAllocator& o)
		:m_cnt(o.m_cnt),m_freeList(o.m_freeList){}

	ConceptIndex alloc();
	void free( const ConceptIndex& idx);

private:
	GlobalCountAllocator* m_cnt;
	std::vector<ConceptIndex> m_freeList;
};

typedef strus::shared_ptr<SimGroup> SimGroupRef;

class SimGroupMap
{
public:
	SimGroupMap();
	explicit SimGroupMap( std::size_t maxNofGroups);
	~SimGroupMap();

	SimGroupRef get( const ConceptIndex& cidx) const;
	void setGroup( const ConceptIndex& cidx, const SimGroupRef& group);
	void resetGroup( const ConceptIndex& cidx);

private:
	SimGroupMap( const SimGroupMap&){};	//< non copyable
	void operator=( const SimGroupMap&){};	//< non copyable

private:
	void* m_armem;				//< aligned memory for array of sim groups
	SimGroupRef* m_ar;			//< array of sim groups
	std::size_t m_arsize;			//< size of array of sim groups (maximum number of groups -> not growing)
};

}//namespace
#endif

