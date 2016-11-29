/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map of similarity group identifiers for remapping them when cleaning up maps (eliminating leaks)
#ifndef _STRUS_VECTOR_SPACE_MODEL_SIM_GROUP_ID_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SIM_GROUP_ID_MAP_HPP_INCLUDED
#include "simGroup.hpp"
#include "internationalization.hpp"
#include <vector>

namespace strus {

class SimGroupIdMap
{
public:
	SimGroupIdMap()
		:m_ar(),m_cnt(0){}
	SimGroupIdMap( const SimGroupIdMap& o)
		:m_ar(o.m_ar),m_cnt(o.m_cnt){}

	ConceptIndex allocate( const ConceptIndex& cid)
	{
		if (!cid) throw strus::runtime_error(_TXT("concept id out of range (null)"));
		while ((std::size_t)cid > m_ar.size()) m_ar.push_back( 0);
		if (m_ar[ cid-1] != 0) throw strus::runtime_error(_TXT("duplicate allocation of concept in sim group id map"));
		m_ar[ cid-1] = ++m_cnt;
		return m_cnt;
	}

	ConceptIndex operator[]( const ConceptIndex& cid) const
	{
		if (cid == 0 || (std::size_t)cid > m_ar.size()) throw strus::runtime_error(_TXT("array bound read in sim group id map"));
		if (m_ar[ cid-1] == 0) throw strus::runtime_error(_TXT("access unknown concept id"));
		return m_ar[ cid-1];
	}

	ConceptIndex endIndex() const
	{
		return m_cnt+1;
	}

private:
	std::vector<ConceptIndex> m_ar;
	ConceptIndex m_cnt;
};

}//namespace
#endif

