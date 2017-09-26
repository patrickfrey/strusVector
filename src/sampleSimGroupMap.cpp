/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for a map of sample indices to similarity groups they are members of
#include "sampleSimGroupMap.hpp"
#include "internationalization.hpp"
#include "cacheLineSize.hpp"
#include "errorUtils.hpp"
#include "utils.hpp"
#include <cstring>

using namespace strus;

void SampleSimGroupMap::init()
{
	std::size_t mm = m_nodearsize * std::max( (std::size_t)m_maxnodesize, sizeof(ConceptIndex));
	if (mm < m_nodearsize || mm < (std::size_t)m_maxnodesize)
	{
		throw std::bad_alloc();
	}
	m_nodear = (Node*)utils::aligned_malloc( m_nodearsize * sizeof(Node), CacheLineSize);
	m_refs = (ConceptIndex*)utils::aligned_malloc( m_nodearsize * m_maxnodesize * sizeof(ConceptIndex), CacheLineSize);
	if (!m_nodear || !m_refs)
	{
		if (m_nodear) utils::aligned_free(m_nodear);
		if (m_refs) utils::aligned_free(m_refs);
		throw std::bad_alloc();
	}
	std::memset( m_refs, 0, m_nodearsize * m_maxnodesize * sizeof(ConceptIndex));
	std::size_t ni = 0, ne = m_nodearsize;
	std::size_t refidx = 0;
	for (; ni != ne; ++ni,refidx += m_maxnodesize)
	{
		m_nodear[ ni].init( m_refs + refidx);
	}
}

SampleSimGroupMap::SampleSimGroupMap( std::size_t nofNodes_, std::size_t maxNodeSize_)
	:m_nodear(0),m_refs(0),m_nodearsize(nofNodes_),m_maxnodesize(maxNodeSize_)
{
	init();
}

SampleSimGroupMap::SampleSimGroupMap( const SampleSimGroupMap& o)
	:m_nodear(0),m_refs(0),m_nodearsize(o.m_nodearsize),m_maxnodesize(o.m_maxnodesize)
{
	init();
	std::memcpy( m_refs, o.m_refs, m_nodearsize * m_maxnodesize * sizeof(ConceptIndex));
}

SampleSimGroupMap::~SampleSimGroupMap()
{
	if (m_nodear) utils::aligned_free( m_nodear);
	if (m_refs) utils::aligned_free( m_refs);
}

bool SampleSimGroupMap::shares( const std::size_t& ndidx, const ConceptIndex* car, std::size_t carsize) const
{
	std::size_t i1=0, i2=0;
	const Node& nd = m_nodear[ ndidx];
	while (i1<(std::size_t)nd.size && i2<carsize)
	{
		if (nd.groupidx[i1] < car[i2])
		{
			++i1;
		}
		else if (nd.groupidx[i1] > car[i2])
		{
			++i2;
		}
		else
		{
			return true;
		}
	}
	return false;
}

void SampleSimGroupMap::check() const
{
	std::size_t ni = 0, ne = m_nodearsize;
	for (; ni != ne; ++ni)
	{
		m_nodear[ni].check( m_maxnodesize);
	}
}

bool SampleSimGroupMap::Node::remove( const ConceptIndex& gidx)
{
	ConceptIndex ii=0;
	for (; ii<size && groupidx[ii] < gidx; ++ii){}
	if (ii==size) return false;
	if (groupidx[ii] == gidx)
	{
		--size;
		for (; ii<size; ++ii)
		{
			groupidx[ii] = groupidx[ii+1];
		}
		groupidx[ii] = 0;
		return true;
	}
	return false;
}

bool SampleSimGroupMap::Node::insert( const ConceptIndex& gidx, std::size_t maxnodesize)
{
	if (size == (ConceptIndex)maxnodesize)
	{
		return false;
	}
	for (ConceptIndex ii=0; ii<size; ++ii)
	{
		if (groupidx[ii] >= gidx)
		{
			if (groupidx[ii] == gidx) return false;
			ConceptIndex kk = size++;
			for (; kk>ii; --kk)
			{
				groupidx[kk] = groupidx[kk-1];
			}
			groupidx[ii] = gidx;
			return true;
		}
	}
	groupidx[ size++] = gidx;
	return true;
}

bool SampleSimGroupMap::Node::contains( const ConceptIndex& gidx) const
{
	ConceptIndex ii=0;
	for (; ii<size && groupidx[ii] < gidx; ++ii){}
	if (ii==size) return false;
	return (groupidx[ii] == gidx);
}

void SampleSimGroupMap::Node::check( ConceptIndex maxnodesize) const
{
	if (!size) return;
	ConceptIndex ii = 0;
	if (groupidx[ii++] == 0 || size > maxnodesize)
	{
		throw strus::runtime_error( "%s", _TXT("illegal SampleSimGroupMap::Node (order)"));
	}
	for (; ii<size; ++ii)
	{
		if (groupidx[ii] < groupidx[ii-1])
		{
			throw strus::runtime_error( "%s", _TXT("illegal SampleSimGroupMap::Node (order)"));
		}
	}
	for (; ii<(ConceptIndex)maxnodesize; ++ii)
	{
		if (groupidx[ii] != 0)
		{
			throw strus::runtime_error( "%s", _TXT("illegal SampleSimGroupMap::Node (eof)"));
		}
	}
}

void SampleSimGroupMap::Node::rewrite( const SimGroupIdMap& groupIdMap)
{
	ConceptIndex ii = 0;
	for (; ii<size; ++ii)
	{
		groupidx[ii] = groupIdMap[ groupidx[ii]];
	}
}

void SampleSimGroupMap::rewrite( const SimGroupIdMap& groupIdMap)
{
	std::size_t ni = 0, ne = m_nodearsize;
	for (; ni != ne; ++ni)
	{
		m_nodear[ni].rewrite( groupIdMap);
	}
}

void SharedSampleSimGroupMap::check() const
{
	std::size_t ni = 0, ne = nofNodes();
	for (; ni != ne; ++ni)
	{
		Lock nd( this, ni);
		Parent::checkNode( ni);
	}
}

