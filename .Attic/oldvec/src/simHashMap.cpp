/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map for fast scan for similar SimHashes 
#include "simHashMap.hpp"
#include "simHashRankList.hpp"
#include "internationalization.hpp"
#include "strus/base/bitOperations.hpp"
#include "strus/base/malloc.hpp"
#include "strus/base/thread.hpp"
#include "strus/base/platform.hpp"
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>

using namespace strus;

typedef SimHashRankList RankList;

SimHashMap::~SimHashMap()
{
	if (m_selar1) strus::aligned_free( m_selar1);
	if (m_selar2) strus::aligned_free( m_selar2);
}

void SimHashMap::initBench()
{
	if (m_ar.empty()) return;
	int ar_size = m_ar[0].size();
	int mod = m_ar[0].arsize();
	if (mod > 1) mod -= 1;
	m_select1 = (m_seed+0) % mod;
	m_select2 = (m_seed+1) % mod;
	m_selar1 = (uint64_t*)strus::aligned_malloc( m_ar.size() * sizeof(uint64_t), strus::platform::CacheLineSize);
	m_selar2 = (uint64_t*)strus::aligned_malloc( m_ar.size() * sizeof(uint64_t), strus::platform::CacheLineSize);
	if (!m_selar1 || !m_selar2)
	{
		if (m_selar1) strus::aligned_free( m_selar1);
		if (m_selar2) strus::aligned_free( m_selar2);
		m_selar1 = 0;
		m_selar2 = 0;
		throw std::bad_alloc();
	}
	int si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (m_ar[si].size() != ar_size) throw std::runtime_error( _TXT("inconsistent dataset passed to sim hash map (sim hash element sizes differ)"));
		m_selar1[ si] = m_ar[ si].ar()[ m_select1];
		m_selar2[ si] = m_ar[ si].ar()[ m_select2];
	}
}



static std::vector<SimHashMap::QueryResult> queryResultFromRanklistElements( const std::vector<SimHash>& ar, const std::vector<RankList::Result>& elements)
{
	std::vector<SimHashMap::QueryResult> rt;
	std::vector<RankList::Result>::const_iterator ri = elements.begin(), re = elements.end();
	for (; ri != re; ++ri)
	{
		rt.push_back( SimHashMap::QueryResult( ar[ ri->index].id(), ri->weight));
	}
	return rt;
}

std::vector<SimHashMap::QueryResult> SimHashMap::findSimilar( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, int maxNofElements) const
{
	if (m_ar.empty()) return std::vector<SimHashMap::QueryResult>();
	RankList ranklist( maxNofElements);
	int ranklistSize = 0;
	double prob_simdist_factor = (double)prob_simdist / (double)simdist;
	int shdiff = ((int)prob_simdist * 64U) / m_vecdim;
	uint64_t needle1 = sh.ar()[ m_select1];
	uint64_t needle2 = sh.ar()[ m_select2];
	int si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if ((int)strus::BitOperations::bitCount( m_selar1[si] ^ needle1) <= shdiff)
		{
			if ((int)strus::BitOperations::bitCount( m_selar2[si] ^ needle2) <= shdiff)
			{
				if (m_ar[ si].near( sh, simdist))
				{
					unsigned short dist = m_ar[ si].dist( sh);
					ranklist.insert( RankList::Element( si, dist));
					ranklistSize++;
					if (ranklistSize > maxNofElements)
					{
						unsigned short lastdist = ranklist.lastdist();
						if (lastdist < dist)
						{
							simdist = lastdist;
							shdiff = (int)(prob_simdist_factor * simdist * 64U) / m_vecdim;
						}
					}
				}
			}
		}
	}
	return queryResultFromRanklistElements( m_ar, ranklist.result( sh.size()));
}

std::vector<SimHashMap::QueryResult> SimHashMap::findSimilar( const SimHash& sh, unsigned short simdist, int maxNofElements) const
{
	RankList ranklist( maxNofElements);
	int ranklistSize = 0;
	int si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (m_ar[ si].near( sh, simdist))
		{
			unsigned short dist = m_ar[ si].dist( sh);
			ranklist.insert( RankList::Element( si, dist));
			ranklistSize++;
			if (ranklistSize > maxNofElements)
			{
				unsigned short lastdist = ranklist.lastdist();
				if (lastdist < dist)
				{
					simdist = lastdist;
				}
			}
		}
	}
	return queryResultFromRanklistElements( m_ar, ranklist.result( sh.size()));
}


