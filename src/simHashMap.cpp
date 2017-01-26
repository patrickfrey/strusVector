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
	if (m_selar1) utils::aligned_free( m_selar1);
	if (m_selar2) utils::aligned_free( m_selar2);
}

void SimHashMap::initBench()
{
	if (m_ar.empty()) return;
	m_vecsize = m_ar[0].size();
	unsigned int mod = m_ar[0].arsize();
	if (mod > 1) mod -= 1;
	m_select1 = (m_seed+0) % mod;
	m_select2 = (m_seed+1) % mod;
	m_selar1 = (uint64_t*)utils::aligned_malloc( m_ar.size() * sizeof(uint64_t), CacheLineSize);
	m_selar2 = (uint64_t*)utils::aligned_malloc( m_ar.size() * sizeof(uint64_t), CacheLineSize);
	if (!m_selar1 || !m_selar2)
	{
		if (m_selar1) utils::aligned_free( m_selar1);
		if (m_selar2) utils::aligned_free( m_selar2);
		throw std::bad_alloc();
	}
	std::size_t si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (m_ar[si].size() != m_vecsize) throw strus::runtime_error(_TXT("inconsistent dataset passed to sim hash map (sim hash element sizes differ)"));
		m_selar1[ si] = m_ar[ si].ar()[ m_select1];
		m_selar2[ si] = m_ar[ si].ar()[ m_select2];
	}
}


struct RankList
{
	RankList( unsigned int maxNofRanks_)
		:m_nofRanks(0),m_maxNofRanks(maxNofRanks_)
	{
		if (maxNofRanks_ == 0 || m_maxNofRanks > MaxIndexSize) throw strus::runtime_error( _TXT( "illegal value for max number of ranks"));
		for (unsigned int ii=0; ii<m_maxNofRanks; ++ii) m_brute_index[ii] = ii;
	}

	void bruteInsert_at( std::size_t idx, const SimRelationMap::Element& elem)
	{
		std::size_t elemidx = m_brute_index[ m_maxNofRanks-1];
		std::memmove( m_brute_index+idx+1, m_brute_index+idx, m_maxNofRanks-idx-1);
		m_brute_index[ idx] = elemidx;
		m_brute_ar[ elemidx] = elem;
	}

	unsigned short lastdist() const
	{
		std::size_t last = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		return m_brute_ar[ m_brute_index[ last-1]].simdist;
	}

	void insert( const SimRelationMap::Element& elem)
	{
		if (!m_nofRanks)
		{
			m_brute_ar[ m_brute_index[ 0] = 0] = elem;
		}
		else
		{
			std::size_t first = 0;
			std::size_t last = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
			std::size_t mid = last-1;
	
			while (last - first > 2)
			{
				if (elem < m_brute_ar[ m_brute_index[ mid]])
				{
					last = mid;
					mid = (first + mid) >> 1;
				}
				else if (m_brute_ar[ m_brute_index[ mid]] < elem)
				{
					first = mid+1;
					mid = (last + mid) >> 1;
				}
				else
				{
					bruteInsert_at( mid, elem);
					++m_nofRanks;
					return;
				}
			}
			for (; first < last; ++first)
			{
				if (elem < m_brute_ar[ m_brute_index[ first]])
				{
					bruteInsert_at( first, elem);
					break;
				}
			}
			if (first == last && last < m_maxNofRanks)
			{
				bruteInsert_at( last, elem);
			}
		}
		++m_nofRanks;
	}

	std::vector<SearchResultElement> result( unsigned int width, Index indexofs) const
	{
		std::vector<SearchResultElement> rt;
		double width_f = width;
		std::size_t limit = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		for (std::size_t ridx=0; ridx<limit; ++ridx)
		{
			double weight = 1.0 - (double)m_brute_ar[ m_brute_index[ ridx]].simdist / width_f;
			rt.push_back( SearchResultElement( m_brute_ar[ m_brute_index[ ridx]].index + indexofs, weight));
		}
		return rt;
	}

	std::string tostring() const
	{
		std::ostringstream buf;
		std::size_t limit = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		for (std::size_t ridx=0; ridx<limit; ++ridx)
		{
			buf << "(" << m_brute_ar[ m_brute_index[ ridx]].simdist << "," << m_brute_ar[ m_brute_index[ ridx]].index << ") ";
		}
		return buf.str();
	}

private:
	enum {MaxIndexSize=256};
	unsigned int m_nofRanks;
	unsigned int m_maxNofRanks;
	unsigned char m_brute_index[ MaxIndexSize];
	SimRelationMap::Element m_brute_ar[ MaxIndexSize];
};

std::vector<SearchResultElement> SimHashMap::findSimilar( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, unsigned int maxNofElements, const Index& indexofs) const
{
	RankList ranklist( maxNofElements);
	unsigned int ranklistSize = 0;
	double prob_simdist_factor = (double)prob_simdist / (double)simdist;
	unsigned int shdiff = ((unsigned int)prob_simdist * 64U) / m_vecsize;
	uint64_t needle1 = sh.ar()[ m_select1];
	uint64_t needle2 = sh.ar()[ m_select2];
	std::size_t si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (strus::BitOperations::bitCount( m_selar1[si] ^ needle1) <= shdiff)
		{
			if (strus::BitOperations::bitCount( m_selar2[si] ^ needle2) <= shdiff)
			{
				if (m_ar[ si].near( sh, simdist))
				{
					unsigned short dist = m_ar[ si].dist( sh);
					ranklist.insert( SimRelationMap::Element( si, dist));
					ranklistSize++;
					if (ranklistSize > maxNofElements)
					{
						unsigned short lastdist = ranklist.lastdist();
						if (lastdist < dist)
						{
							simdist = lastdist;
							shdiff = (unsigned int)(prob_simdist_factor * simdist * 64U) / m_vecsize;
						}
					}
				}
			}
		}
	}
	return ranklist.result( sh.size(), indexofs);
}

std::vector<SearchResultElement> SimHashMap::findSimilar( const SimHash& sh, unsigned short simdist, unsigned int maxNofElements, const Index& indexofs) const
{
	RankList ranklist( maxNofElements);
	unsigned int ranklistSize = 0;
	std::size_t si = 0, se = m_ar.size();
	for (; si != se; ++si)
	{
		if (m_ar[ si].near( sh, simdist))
		{
			unsigned short dist = m_ar[ si].dist( sh);
			ranklist.insert( SimRelationMap::Element( si, dist));
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
	return ranklist.result( sh.size(), indexofs);
}

std::vector<SearchResultElement> SimHashMap::findSimilarFromSelection( const std::vector<Index>& selection, const SimHash& sh, unsigned short simdist, unsigned int maxNofElements, const Index& indexofs) const
{
	RankList ranklist( maxNofElements);
	unsigned int ranklistSize = 0;
	std::vector<Index>::const_iterator si = selection.begin(), se = selection.end();
	for (; si != se; ++si)
	{
		if (*si < indexofs) continue;
		Index sidx = *si - indexofs;
		if (sidx >= (Index)m_ar.size()) continue;
		if (m_ar[ sidx].near( sh, simdist))
		{
			unsigned short dist = m_ar[ sidx].dist( sh);
			ranklist.insert( SimRelationMap::Element( sidx, dist));
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
	return ranklist.result( sh.size(), indexofs);
}

