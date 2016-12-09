/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map for fast scan for similar SimHashes 
#include "simHashMap.hpp"
#include "simRelationMap.hpp"
#include "internationalization.hpp"
#include "cacheLineSize.hpp"
#include "utils.hpp"
#include "strus/base/bitOperations.hpp"
#include <vector>
#include <string>
#include <stdexcept>

using namespace strus;

SimHashMap::~SimHashMap()
{
	if (m_selar) utils::aligned_free( m_selar);
}

void SimHashMap::initBench()
{
	if (m_ar.empty()) return;
	m_vecsize = m_ar[0].size();
	unsigned int mod = m_ar[0].arsize();
	if (mod > 1) mod -= 1;
	m_select = m_seed % mod;
	m_selar = (uint64_t*)utils::aligned_malloc( m_ar.size() * sizeof(uint64_t), CacheLineSize);
	if (!m_selar) throw std::bad_alloc();
	std::size_t si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (m_ar[si].size() != m_vecsize) throw strus::runtime_error(_TXT("inconsistent dataset passed to sim hash map (sim hash element sizes differ)"));
		m_selar[ si] = m_ar[ si].ar()[ m_select];
	}
}


std::vector<Index> SimHashMap::findSimilar( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, unsigned int maxNofElements) const
{
	std::set<SimRelationMap::Element> ranklist;
	unsigned int ranklistSize = 0;
	double prob_simdist_factor = (double)prob_simdist / (double)simdist;
	unsigned int shdiff = ((unsigned int)prob_simdist * 64U) / m_vecsize;
	uint64_t needle = sh.ar()[ m_select];
	std::size_t si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (strus::BitOperations::bitCount( m_selar[si] ^ needle) <= shdiff)
		{
			if (m_ar[ si].near( sh, simdist))
			{
				unsigned short dist = m_ar[ si].dist( sh);
				if (ranklistSize < maxNofElements)
				{
					ranklist.insert( SimRelationMap::Element( si, dist));
					++ranklistSize;
				}
				else
				{
					std::set<SimRelationMap::Element>::iterator it = ranklist.end();
					--it;
					ranklist.erase(it);
					ranklist.insert( SimRelationMap::Element( si, dist));
					simdist = ranklist.rbegin()->simdist;
					shdiff = (unsigned int)(prob_simdist_factor * simdist);
				}
			}
		}
	}
	std::vector<Index> rt;
	std::set<SimRelationMap::Element>::const_iterator ri = ranklist.begin(), re = ranklist.end();
	for (; ri != re; ++ri)
	{
		rt.push_back( ri->index);
	}
	return rt;
}

std::vector<Index> SimHashMap::findSimilar( const SimHash& sh, unsigned short simdist, unsigned int maxNofElements) const
{
	std::set<SimRelationMap::Element> ranklist;
	unsigned int ranklistSize = 0;
	std::size_t si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (m_ar[ si].near( sh, simdist))
		{
			unsigned short dist = m_ar[ si].dist( sh);
			if (ranklistSize < maxNofElements)
			{
				ranklist.insert( SimRelationMap::Element( si, dist));
				++ranklistSize;
			}
			else
			{
				std::set<SimRelationMap::Element>::iterator it = ranklist.end();
				--it;
				ranklist.erase(it);
				ranklist.insert( SimRelationMap::Element( si, dist));
				simdist = ranklist.rbegin()->simdist;
			}
		}
	}
	std::vector<Index> rt;
	std::set<SimRelationMap::Element>::const_iterator ri = ranklist.begin(), re = ranklist.end();
	for (; ri != re; ++ri)
	{
		rt.push_back( ri->index);
	}
	return rt;
}



