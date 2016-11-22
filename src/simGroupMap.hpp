/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Collection of similarity groups for thread safe access
#ifndef _STRUS_VECTOR_SPACE_MODEL_SIM_GROUP_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SIM_GROUP_MAP_HPP_INCLUDED
#include "simGroup.hpp"
#include "utils.hpp"
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

private:
	utils::AtomicCounter<ConceptIndex> m_cnt;
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

typedef utils::SharedPtr<SimGroup> SimGroupRef;

class SimGroupMap
{
public:
	SimGroupMap()
		:m_cnt(0),m_armem(0),m_ar(0),m_arsize(0){}

	SimGroupMap( GlobalCountAllocator* cnt_, std::size_t maxNofGroups)
		:m_cnt(cnt_),m_armem(0),m_ar(0),m_arsize(maxNofGroups)
	{
		m_armem = (SimGroup**)utils::aligned_malloc( m_arsize * sizeof(SimGroupRef), 128);
		if (!m_armem) throw std::bad_alloc();
		m_ar = new (m_armem) SimGroupRef[ m_arsize];
	}
	~SimGroupMap()
	{
		delete m_ar;
		utils::aligned_free( m_armem);
	}

	const SimGroupRef& get( const ConceptIndex& cidx) const
	{
		return m_ar[ cidx];
	}

	void setGroup( const ConceptIndex& cidx, const SimGroupRef& group)
	{
		m_ar[ cidx-1] = group;
	}

	ConceptIndex nofGroupIdsAllocated() const
	{
		return m_cnt->nofGroupIdsAllocated();
	}

	GlobalCountAllocator* idAllocator()
	{
		return m_cnt;
	}

private:
	GlobalCountAllocator* m_cnt;		//< global allocator of group ids
	void* m_armem;				//< aligned memory for array of groups
	SimGroupRef* m_ar;			//< array of groups
	std::size_t m_arsize;			//< size of array of groups (maximum number of groups -> not growing)
};

}//namespace
#endif

