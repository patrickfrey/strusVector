/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Functions for probabilistic evaluating similarity relations (incl. multithreaded)
#include "simRelationMapBuilder.hpp"
#include "simHash.hpp"
#include "lshBench.hpp"
#include <vector>
#include <algorithm>

using namespace strus;

#define WEIGHTFACTOR(dd) (dd + dd / 3)

// HIER WEITER MULTITHREADING

SimRelationMapBuilder::SimRelationMapBuilder( const std::vector<SimHash>& samplear, unsigned int maxdist_, unsigned int maxsimsam_, unsigned int threads_, unsigned int seed_)
	:m_base(samplear.data()),m_maxdist(maxdist_),m_maxsimsam(maxsimsam_),m_threads(threads_),m_seed(seed_)
{
	std::size_t ofs = 0;
	while (ofs < samplear.size())
	{
		m_benchar.push_back( LshBench( m_base, samplear.size(), m_seed, WEIGHTFACTOR(m_maxdist)));
		ofs += m_benchar.back().init( ofs);
	}
}

SimRelationMap SimRelationMapBuilder::getSimRelationMap( strus::Index idx) const
{
	std::vector<LshBench::Candidate> res;
	std::size_t bi = 0, be = m_benchar.size();
	for (; bi != be; ++bi)
	{
		m_benchar[ idx].findSimCandidates( res, m_benchar[ bi]);
	}
	std::vector<LshBench::Candidate>::const_iterator ri = res.begin(), re = res.end();
	typedef std::map<strus::Index,std::vector<SimRelationMap::Element> > RowMap;
	RowMap rowmap;
	for (; ri != re; ++ri)
	{
		if (m_base[ ri->row].near( m_base[ri->col], m_maxdist))
		{
			unsigned short simdist = m_base[ri->row].dist( m_base[ri->col]);
			rowmap[ ri->row].push_back( SimRelationMap::Element( ri->row, simdist));
		}
	}
	SimRelationMap rt;
	RowMap::iterator mi = rowmap.begin(), me = rowmap.end();
	for (; mi != me; ++mi)
	{
		std::vector<SimRelationMap::Element>& elems = mi->second;
		std::sort( elems.begin(), elems.end());
		if (elems.size() > m_maxsimsam)
		{
			elems.resize( m_maxsimsam);
		}
		rt.addRow( mi->first, elems);
	}
	return rt;
}

