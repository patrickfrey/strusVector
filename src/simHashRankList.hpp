/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Temporary ranklist used by similarity hash search
#ifndef _STRUS_VECTOR_SIMHASH_RANKLIST_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_RANKLIST_HPP_INCLUDED
#include "strus/index.hpp"
#include "simHash.hpp"
#include "simHashQueryResult.hpp"
#include <utility>
#include <cstring>
#include <ostream>
#include <sstream>
#include <limits>

namespace strus {

struct SimHashRankList
{
public:
	enum {MaxSize=256};

public:
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
	explicit SimHashRankList( int maxNofRanks_)
		:m_nofRanks(0),m_maxNofRanks(maxNofRanks_)
	{
		if (maxNofRanks_ <= 0) throw strus::runtime_error( "%s",  _TXT( "illegal value for maximum number of ranks"));
		if (m_maxNofRanks > MaxSize) throw strus::runtime_error( "%s",  _TXT( "maximum number of ranks is out of range"));
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
		if (m_nofRanks == 0) return std::numeric_limits<unsigned short>::max();
		int last = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		return m_brute_ar[ m_brute_index[ last-1]].simdist;
	}

	bool complete() const
	{
		return m_nofRanks > m_maxNofRanks;
	}

	bool insert( const Element& elem)
	{
		if (m_nofRanks == 0)
		{
			m_brute_ar[ m_brute_index[ 0] = 0] = elem;
		}
		else if (elem.simdist >= lastdist())
		{
			if (m_nofRanks < m_maxNofRanks)
			{
				bruteInsert_at( m_nofRanks, elem);
			}
			else
			{
				return false;
			}
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
					return true;
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
		return true;
	}

	static double weightFromLshSimDist( int nofLshBits, int simDist)
	{
		double width_f = ((double)nofLshBits / 4) * 5;
		return 1.0 - (double)simDist / width_f;
	}

	static int lshSimDistFromWeight( int nofLshBits, double weight)
	{
		double width_f = ((double)nofLshBits / 4) * 5;
		return (1.0 - weight) * width_f;
	}

	std::vector<SimHashQueryResult> result( int nofLshBits) const
	{
		std::vector<SimHashQueryResult> rt;
		double width_f = ((double)nofLshBits / 4) * 5;
		int limit = m_nofRanks > m_maxNofRanks ? m_maxNofRanks:m_nofRanks;
		for (int ridx=0; ridx<limit; ++ridx)
		{
			double weight = 1.0 - (double)m_brute_ar[ m_brute_index[ ridx]].simdist / width_f;
			rt.push_back( SimHashQueryResult( m_brute_ar[ m_brute_index[ ridx]].index, weight));
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
	int m_nofRanks;
	int m_maxNofRanks;
	unsigned char m_brute_index[ MaxSize];
	Element m_brute_ar[ MaxSize];
};

}//namespace
#endif

