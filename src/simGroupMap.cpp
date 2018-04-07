/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Collection of similarity groups for thread safe access
#include "simGroupMap.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/malloc.hpp"
#include "strus/base/platform.hpp"

using namespace strus;

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

void GlobalCountAllocator::setCounter( const ConceptIndex& value)
{
	m_cnt.set( ((value-1 + strus::platform::CacheLineSize-1) / strus::platform::CacheLineSize) * strus::platform::CacheLineSize + 1);
}

ConceptIndex SimGroupIdAllocator::alloc()
{
	ConceptIndex rt;
	if (m_freeList.empty())
	{
		ConceptIndex first = m_cnt->get( strus::platform::CacheLineSize);
		unsigned int ci = first + strus::platform::CacheLineSize-1, ce = first;
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
	:m_armem(0),m_ar(0),m_arsize(0){}

SimGroupMap::SimGroupMap( std::size_t maxNofGroups)
	:m_armem(0),m_ar(0),m_arsize(((maxNofGroups + strus::platform::CacheLineSize - 1) / strus::platform::CacheLineSize) * strus::platform::CacheLineSize)
{
	m_armem = strus::aligned_malloc( m_arsize * sizeof(SimGroupRef), strus::platform::CacheLineSize);
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
	strus::aligned_free( m_armem);
	m_armem = 0;
}

SimGroupRef SimGroupMap::get( const ConceptIndex& cidx) const
{
	if (cidx == 0 || (std::size_t)cidx > m_arsize)
	{
		throw std::runtime_error( _TXT("array bound read in similarity group map"));
	}
	return m_ar[ cidx-1];
}

void SimGroupMap::setGroup( const ConceptIndex& cidx, const SimGroupRef& group)
{
	if (cidx == 0 || (std::size_t)cidx > m_arsize)
	{
		throw std::runtime_error( _TXT("array bound write in similarity group map"));
	}
	m_ar[ cidx-1] = group;
}

void SimGroupMap::resetGroup( const ConceptIndex& cidx)
{
	if (cidx == 0 || (std::size_t)cidx > m_arsize)
	{
		throw std::runtime_error( _TXT("array bound write in similarity group map"));
	}
	m_ar[ cidx-1].reset();
}


