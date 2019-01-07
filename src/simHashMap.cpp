/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map for fast scan for similar SimHashes 
#include "simHashMap.hpp"
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


struct RankList
{
	struct Element
	{
		Index index;			///> index of the similar object in m_ar
		unsigned short simdist;		///> edit distance describing similarity

		/// \brief Default constructor
		Element()
			:index(0),simdist(0){}
		/// \brief Constructor
		Element( const Index& index_, unsigned short simdist_)
			:index(index_),simdist(simdist_){}
		/// \brief Copy constructor
		Element( const Element& o)
			:index(o.index),simdist(o.simdist){}

		/// \brief Order with ascending similarity (closest first)
		bool operator < (const Element& o) const
		{
			if (simdist == o.simdist) return index < o.index;
			return simdist < o.simdist;
		}
	};
	struct Result
	{
		Index index;			///> index of the similar object in m_ar
		double weight;			///> weight derived from similarity

		/// \brief Default constructor
		Result()
			:index(0),weight(0){}
		/// \brief Constructor
		Result( const Index& index_, double weight_)
			:index(index_),weight(weight_){}
		/// \brief Copy constructor
		Result( const Result& o)
			:index(o.index),weight(o.weight){}
	};
	RankList( int maxNofRanks_)
		:m_nofRanks(0),m_maxNofRanks(maxNofRanks_)
	{
		if (maxNofRanks_ == 0 || m_maxNofRanks > MaxIndexSize) throw strus::runtime_error( "%s",  _TXT( "illegal value for max number of ranks"));
		for (int ii=0; ii<m_maxNofRanks; ++ii) m_brute_index[ii] = (unsigned char)(unsigned int)ii;
	}

	void bruteInsert_at( int idx, const Element& elem)
	{
		int elemidx = m_brute_index[ m_maxNofRanks-1];
		std::memmove( m_brute_index+idx+1, m_brute_index+idx, m_maxNofRanks-idx-1);
		m_brute_index[ idx] = elemidx;
		m_brute_ar[ elemidx] = elem;
	}

	unsigned short lastdist() const
	{
		int last = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		return m_brute_ar[ m_brute_index[ last-1]].simdist;
	}

	void insert( const Element& elem)
	{
		if (!m_nofRanks)
		{
			m_brute_ar[ m_brute_index[ 0] = 0] = elem;
		}
		else
		{
			int first = 0;
			int last = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
			int mid = last-1;
	
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

	std::vector<Result> result( int width) const
	{
		std::vector<Result> rt;
		double width_f = width;
		int limit = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		for (int ridx=0; ridx<limit; ++ridx)
		{
			double weight = 1.0 - (double)m_brute_ar[ m_brute_index[ ridx]].simdist / width_f;
			rt.push_back( Result( m_brute_ar[ m_brute_index[ ridx]].index, weight));
		}
		return rt;
	}

	std::string tostring() const
	{
		std::ostringstream buf;
		int limit = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		for (int ridx=0; ridx<limit; ++ridx)
		{
			buf << "(" << m_brute_ar[ m_brute_index[ ridx]].simdist << "," << m_brute_ar[ m_brute_index[ ridx]].index << ") ";
		}
		return buf.str();
	}

private:
	enum {MaxIndexSize=256};
	int m_nofRanks;
	int m_maxNofRanks;
	unsigned char m_brute_index[ MaxIndexSize];
	Element m_brute_ar[ MaxIndexSize];
};

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


