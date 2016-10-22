/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for storing similarity relations
#include "simRelationMap.hpp"
#include "internationalization.hpp"
#include <limits>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>
#include "strus/base/hton.hpp"

#undef STRUS_LOWLEVEL_DEBUG

using namespace strus;

void SimRelationMap::addRow( const SampleIndex& index, std::vector<Element>::const_iterator ai, const std::vector<Element>::const_iterator& ae)
{
	if (index >= m_nofSamples) m_nofSamples = index+1;
	std::size_t aridx = m_ar.size();
	if (m_rowdescrmap.find( index) != m_rowdescrmap.end()) throw strus::runtime_error(_TXT("sim relation map row defined twice"));
	std::size_t asize = ae - ai;
	m_rowdescrmap[ index] = RowDescr( aridx, asize);
	m_ar.insert( m_ar.end(), ai, ae);
	std::sort( m_ar.begin() + aridx, m_ar.begin() + aridx + asize);
}

void SimRelationMap::addRow( const SampleIndex& index, const std::vector<Element>& ar)
{
	addRow( index, ar.begin(), ar.end());
}

std::string SimRelationMap::tostring() const
{
	std::ostringstream buf;
	RowDescrMap::const_iterator ri = m_rowdescrmap.begin(), re = m_rowdescrmap.end();
	for (; ri != re; ++ri)
	{
		const Element& elem = m_ar[ ri->second.aridx];
		buf << "(" << ri->first << "," << elem.index << ") = " << elem.simdist << std::endl;
	}
	return buf.str();
}

typedef std::pair<SampleIndex,SimRelationMap::Element> SimRelationCell;

SimRelationMap SimRelationMap::mirror() const
{
	SimRelationMap rt;
	std::set<SimRelationCell> simRelationMatrix;
	SampleIndex si = 0, se = nofSamples();
	for (; si != se; ++si)
	{
		Row row_ = row( si);
		Row::const_iterator ri = row_.begin(), re = row_.end();
		for (; ri != re; ++ri)
		{
			simRelationMatrix.insert( SimRelationCell( si, *ri));
			simRelationMatrix.insert( SimRelationCell( ri->index, SimRelationMap::Element( si, ri->simdist)));
		}
	}
	std::set<SimRelationCell>::const_iterator mi = simRelationMatrix.begin(), me = simRelationMatrix.end();
	while (mi != me)
	{
		std::vector<Element> elemlist;
		SampleIndex sidx = mi->first;
		for (; mi != me && mi->first == sidx; ++mi)
		{
			elemlist.push_back( mi->second);
		}
		rt.addRow( sidx, elemlist);
	}
	return rt;
}

void SimRelationMap::join( const SimRelationMap& o)
{
	RowDescrMap::const_iterator oi = o.m_rowdescrmap.begin(), oe = o.m_rowdescrmap.end();
	for (; oi != oe; ++oi)
	{
		addRow( oi->first, o.m_ar.begin() + oi->second.aridx, o.m_ar.begin() + oi->second.aridx + oi->second.size);
	}
}



