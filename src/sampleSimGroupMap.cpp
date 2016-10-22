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
#include "errorUtils.hpp"
#include <cstring>

using namespace strus;

void SampleSimGroupMap::init()
{
	std::size_t mm = m_nodearsize * std::max( (std::size_t)m_maxnodesize, sizeof(FeatureIndex));
	if (mm < m_nodearsize || mm < (std::size_t)m_maxnodesize)
	{
		throw std::bad_alloc();
	}
	m_nodear = (Node*)std::malloc( m_nodearsize * sizeof(Node));
	m_refs = (FeatureIndex*)std::calloc( m_nodearsize * m_maxnodesize, sizeof(FeatureIndex));
	if (!m_nodear || !m_refs)
	{
		if (m_nodear) std::free(m_nodear);
		if (m_refs) std::free(m_refs);
		throw std::bad_alloc();
	}
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
	std::memcpy( m_refs, o.m_refs, m_nodearsize * m_maxnodesize * sizeof(FeatureIndex));
}

SampleSimGroupMap::~SampleSimGroupMap()
{
	if (m_nodear) std::free(m_nodear);
	if (m_refs) std::free(m_refs);
}

bool SampleSimGroupMap::shares( const std::size_t& ndidx1, const std::size_t& ndidx2) const
{
	std::size_t i1=0, i2=0;
	const Node& nd1 = m_nodear[ ndidx1];
	const Node& nd2 = m_nodear[ ndidx2];
	while (i1<(std::size_t)m_maxnodesize && i2<(std::size_t)m_maxnodesize)
	{
		if (nd1.groupidx[i1] < nd2.groupidx[i2])
		{
			++i1;
		}
		else if (nd1.groupidx[i1] > nd2.groupidx[i2])
		{
			++i2;
		}
		else
		{
			return (nd1.groupidx[i1] != 0);
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

bool SampleSimGroupMap::Node::remove( const FeatureIndex& gidx)
{
	FeatureIndex ii=0;
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

bool SampleSimGroupMap::Node::insert( const FeatureIndex& gidx, std::size_t maxnodesize)
{
	if (size == (FeatureIndex)maxnodesize)
	{
		throw strus::runtime_error(_TXT("try to insert in full sampleSimGroupMap node"));
	}
	for (FeatureIndex ii=0; ii<size; ++ii)
	{
		if (groupidx[ii] >= gidx)
		{
			if (groupidx[ii] == gidx) return false;
			FeatureIndex kk = size++;
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

bool SampleSimGroupMap::Node::contains( const FeatureIndex& gidx) const
{
	FeatureIndex ii=0;
	for (; ii<size && groupidx[ii] < gidx; ++ii){}
	if (ii==size) return false;
	return (groupidx[ii] == gidx);
}

void SampleSimGroupMap::Node::check( FeatureIndex maxnodesize) const
{
	if (!size) return;
	FeatureIndex ii = 0;
	if (groupidx[ii++] == 0 || size > maxnodesize)
	{
		throw strus::runtime_error(_TXT("illegal SampleSimGroupMap::Node (order)"));
	}
	for (; ii<size; ++ii)
	{
		if (groupidx[ii] < groupidx[ii-1])
		{
			throw strus::runtime_error(_TXT("illegal SampleSimGroupMap::Node (order)"));
		}
	}
	for (; ii<(FeatureIndex)maxnodesize; ++ii)
	{
		if (groupidx[ii] != 0)
		{
			throw strus::runtime_error(_TXT("illegal SampleSimGroupMap::Node (eof)"));
		}
	}
}


