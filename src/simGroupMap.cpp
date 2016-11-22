/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Collection of similarity groups for thread safe access
#include "simGroupMap.hpp"
#include "cacheLineSize.hpp"

using namespace strus;
using namespace strus::utils;

ConceptIndex GlobalCountAllocator::get( unsigned int cnt)
{
	ConceptIndex rt = m_cnt.allocIncrement( cnt);
	if (rt > m_maxNofCount)
	{
		m_cnt.decrement( cnt);
		throw strus::runtime_error(_TXT("to many group ids allocated: %u"), rt);
	}
	return rt;
}

ConceptIndex SimGroupIdAllocator::alloc()
{
	ConceptIndex rt;
	if (m_freeList.empty())
	{
		ConceptIndex first = m_cnt->get( CacheLineSize);
		unsigned int ci = first + CacheLineSize-1, ce = first;
		for (; ci != ce; --ci)
		{
			m_freeList.push_back( ci);
		}
		rt = first;
	}
	else
	{
		rt = m_freeList.back();
		m_freeList.pop_back();
	}
	return rt;
}

void SimGroupIdAllocator::free( const ConceptIndex& idx)
{
	m_freeList.push_back( idx);
}

SimGroupMap::SimGroupMap()
	:m_cnt(0),m_armem(0),m_ar(0),m_arsize(0){}

SimGroupMap::SimGroupMap( GlobalCountAllocator* cnt_, std::size_t maxNofGroups)
	:m_cnt(cnt_),m_armem(0),m_ar(0),m_arsize(maxNofGroups)
{
	m_armem = utils::aligned_malloc( m_arsize * sizeof(SimGroupRef), 128);
	if (!m_armem) throw std::bad_alloc();
	m_ar = new (m_armem) SimGroupRef[ m_arsize];
}

SimGroupMap::~SimGroupMap()
{
	std::size_t ai = 0, ae = m_arsize;
	for (;ai != ae; ++ai)
	{
		m_ar[ai].~SimGroupRef();
	}
	utils::aligned_free( m_armem);
	m_armem = 0;
}

