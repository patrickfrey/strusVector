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
#include "strus/base/pseudoRandom.hpp"

#undef STRUS_LOWLEVEL_DEBUG

using namespace strus;

std::vector<SimRelationMap::Element> SimRelationMap::mergeElementLists( const std::vector<Element>& elemlist1, const std::vector<Element>& elemlist2)
{
	std::vector<Element> e1sorted = elemlist1;
	std::sort( e1sorted.begin(), e1sorted.end());
	std::vector<Element> e2sorted = elemlist2;
	std::sort( e2sorted.begin(), e2sorted.end());
	std::vector<Element> rt;
	std::vector<Element>::const_iterator ii1 = e1sorted.begin(), ie1 = e1sorted.end();
	std::vector<Element>::const_iterator ii2 = e2sorted.begin(), ie2 = e2sorted.end();
	while (ii1 != ie1 && ii2 != ie2)
	{
		if (ii1->index < ii2->index)
		{
			rt.push_back( *ii1++);
		}
		else if (ii1->index > ii2->index)
		{
			rt.push_back( *ii2++);
		}
		else
		{
			rt.push_back( *ii2++);
			++ii1;
		}
	}
	for (; ii1 != ie1; ++ii1)
	{
		rt.push_back( *ii1);
	}
	for (; ii2 != ie2; ++ii2)
	{
		rt.push_back( *ii2);
	}
	return rt;
}

std::vector<SimRelationMap::Element> SimRelationMap::selectElementSubset( const std::vector<SimRelationMap::Element>& elemlist, unsigned int maxsimsam, unsigned int rndsimsam, unsigned int rndseed)
{
	if (elemlist.size() > maxsimsam + rndsimsam)
	{
		std::vector<SimRelationMap::Element> elemlist_copy( elemlist);
		std::sort( elemlist_copy.begin(), elemlist_copy.end());

		PseudoRandom rnd( rndseed);
		std::vector<SimRelationMap::Element> subset_elemlist;

		// First part of the result are the 'maxsimsam' best elements:
		while (elemlist_copy.size() - maxsimsam < rndsimsam + (rndsimsam >> 1))
		{
			// ... reduce the number of random picks to at least 2/3 of the rest to reduce random choice collisions (imagine picking N-1 random elements out of N)
			maxsimsam += (rndsimsam >> 1);
			rndsimsam -= (rndsimsam >> 1);
		}
		subset_elemlist.insert( subset_elemlist.end(), elemlist_copy.begin(), elemlist_copy.begin() + maxsimsam);

		// Second part of the result are 'rndsimsam' randomly chosen elements from the rest:
		std::set<std::size_t> rndchoiceset;
		std::size_t ri = 0, re = rndsimsam;
		for (; ri != re; ++ri)
		{
			std::size_t choice = rnd.get( maxsimsam, elemlist_copy.size());
			while (rndchoiceset.find( choice) != rndchoiceset.end())
			{
				// Find element not chosen yet:
				choice = choice + 1;
				if (choice >= elemlist_copy.size()) choice = 0;
			}
			rndchoiceset.insert( choice);
			subset_elemlist.push_back( elemlist_copy[ choice]);
		}
		return subset_elemlist;
	}
	else
	{
		return elemlist;
	}
}

void SimRelationMap::addRow( const SampleIndex& index, std::vector<Element>::const_iterator ai, const std::vector<Element>::const_iterator& ae)
{
	if (index >= m_endIndex) m_endIndex = index+1;
	if (index < m_startIndex) m_startIndex = index;
	std::size_t aridx = m_ar.size();
	if (m_rowdescrmap.find( index) != m_rowdescrmap.end()) throw std::runtime_error( _TXT("sim relation map row defined twice"));
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
	SampleIndex si = startIndex(), se = endIndex();
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



