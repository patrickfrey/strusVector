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

