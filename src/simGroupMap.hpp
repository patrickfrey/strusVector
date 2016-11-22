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
#include "internationalization.hpp"
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
	SimGroupMap();
	SimGroupMap( GlobalCountAllocator* cnt_, std::size_t maxNofGroups);
	~SimGroupMap();

	const SimGroupRef& get( const ConceptIndex& cidx) const
	{
		if (!cidx || (std::size_t)cidx > m_arsize)
		{
			throw strus::runtime_error(_TXT("array bound read in similarity group map"));
		}
		return m_ar[ cidx-1];
	}

	void setGroup( const ConceptIndex& cidx, const SimGroupRef& group)
	{
		if (!cidx || (std::size_t)cidx > m_arsize)
		{
			throw strus::runtime_error(_TXT("array bound write in similarity group map"));
		}
		if (cidx != group->id())
		{
			throw strus::runtime_error(_TXT("internal: wrong group assignment"));
		}
		m_ar[ cidx-1] = group;
	}

	void resetGroup( const ConceptIndex& cidx)
	{
		if (!cidx || (std::size_t)cidx > m_arsize)
		{
			throw strus::runtime_error(_TXT("array bound write in similarity group map"));
		}
		m_ar[ cidx-1].reset();
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
	SimGroupMap( const SimGroupMap&){};	//< non copyable
	void operator=( const SimGroupMap&){};	//< non copyable

private:
	GlobalCountAllocator* m_cnt;		//< global allocator of group ids
	void* m_armem;				//< aligned memory for array of groups
	SimGroupRef* m_ar;			//< array of groups
	std::size_t m_arsize;			//< size of array of groups (maximum number of groups -> not growing)
};

}//namespace
#endif

