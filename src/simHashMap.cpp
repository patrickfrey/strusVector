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

	std::vector<strus::Index> result() const
	{
		std::vector<strus::Index> rt;
		std::size_t limit = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		for (std::size_t ridx=0; ridx<limit; ++ridx)
		{
			rt.push_back( m_brute_ar[ m_brute_index[ ridx]].index);
		}
		return rt;
	}
	
private:
	enum {MaxIndexSize=256};
	unsigned int m_nofRanks;
	unsigned int m_maxNofRanks;
	unsigned char m_brute_index[ MaxIndexSize];
	SimRelationMap::Element m_brute_ar[ MaxIndexSize];
};

std::vector<Index> SimHashMap::findSimilar2( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, unsigned int maxNofElements) const
{
	RankList ranklist( maxNofElements);
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
				ranklist.insert( SimRelationMap::Element( si, dist));
				ranklistSize++;
				if (ranklistSize > maxNofElements)
				{
					unsigned short lastdist = ranklist.lastdist();
					if (lastdist < dist)
					{
						simdist = lastdist;
						shdiff = (unsigned int)(prob_simdist_factor * simdist);
					}
				}
			}
		}
	}
	return ranklist.result();
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



