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

enum {SerializeElementSize=(sizeof(SampleIndex)+sizeof(SampleIndex)+sizeof(unsigned short))};
struct SerializeElement
{
	SampleIndex row;
	SampleIndex col;
	unsigned short simdist;

	SerializeElement( SampleIndex row_, SampleIndex col_, unsigned short simdist_)
		:row(row_),col(col_),simdist(simdist_){}
	SerializeElement( const SerializeElement& o)
		:row(o.row),col(o.col),simdist(o.simdist){}

	SampleIndex row_ntoh() const
	{
		return ntohl( row);
	}

	void ntoh()
	{
		row = ByteOrder<SampleIndex>::ntoh( row);
		col = ByteOrder<SampleIndex>::ntoh( col);
		simdist = ByteOrder<unsigned short>::ntoh( simdist);
	}

	void hton()
	{
		row = ByteOrder<SampleIndex>::hton( row);
		col = ByteOrder<SampleIndex>::hton( col);
		simdist = ByteOrder<unsigned short>::hton( simdist);
	}
};

std::string SimRelationMap::serialization() const
{
	std::string rt;
	SampleIndex si=0, se=m_nofSamples;
	for (; si != se; ++si)
	{
		Row rw = row( si);
		Row::const_iterator ri = rw.begin(), re = rw.end();
		for (; ri != re; ++ri)
		{
			SerializeElement elem( si, ri->index, ri->simdist);
			elem.hton();
			rt.append( (const char*)&elem.row, sizeof(elem.row));
			rt.append( (const char*)&elem.col, sizeof(elem.col));
			rt.append( (const char*)&elem.simdist, sizeof(elem.simdist));
		}
	}
	return rt;
}

SimRelationMap SimRelationMap::fromSerialization( const std::string& blob)
{
	SimRelationMap rt;
	char const* si = blob.c_str();
	if (blob.size() % SerializeElementSize != 0)
	{
		throw strus::runtime_error(_TXT("incompatible data structure serialization"));
	}
	const char* se = si + blob.size();
	while (si < se)
	{
		SampleIndex* row = (SampleIndex*)si; si += sizeof(SampleIndex);
		SampleIndex* col = (SampleIndex*)si; si += sizeof(SampleIndex);
		unsigned short* simdist = (unsigned short*)si; si += sizeof(unsigned short);

		SampleIndex ridx_n = *row;
		std::vector<Element> elemlist;
		while (si != se && *row == ridx_n)
		{
			SerializeElement elem( *row, *col, *simdist);
			elem.ntoh();
			elemlist.push_back( Element( elem.col, elem.simdist));

			row = (SampleIndex*)si; si += sizeof(SampleIndex);
			col = (SampleIndex*)si; si += sizeof(SampleIndex);
			simdist = (unsigned short*)si; si += sizeof(unsigned short);
		}
		rt.addRow( ByteOrder<SampleIndex>::ntoh(ridx_n), elemlist);
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



