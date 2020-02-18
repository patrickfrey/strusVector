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
#include "strus/storage/index.hpp"
#include "simHash.hpp"
#include "simHashQueryResult.hpp"
#include "rankList.hpp"
#include <utility>
#include <cstring>
#include <ostream>
#include <sstream>
#include <limits>

namespace strus {

struct SimHashRank
{
	Index index;			///> index of the similar object in m_ar
	unsigned short simdist;		///> edit distance describing similarity

	/// \brief Default constructor
	SimHashRank()
		:index(0),simdist(0){}
	/// \brief Constructor
	SimHashRank( const Index& index_, unsigned short simdist_)
		:index(index_),simdist(simdist_){}
	/// \brief Copy constructor
	SimHashRank( const SimHashRank& o)
		:index(o.index),simdist(o.simdist){}

	/// \brief Order with ascending similarity (closest first)
	bool operator < (const SimHashRank& o) const
	{
		if (simdist == o.simdist) return index < o.index;
		return simdist < o.simdist;
	}
};

class SimHashRankList
	:public RankList<SimHashRank>
{
public:
	explicit SimHashRankList( int maxNofRanks_)
		:RankList<SimHashRank>( maxNofRanks_){}

	unsigned short lastdist() const
	{
		return back().simdist;
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
		const_iterator ri = begin(), re = end();
		for (; ri != re; ++ri)
		{
			const SimHashRank& elem = *ri;
			double weight = 1.0 - (double)elem.simdist / width_f;
			rt.push_back( SimHashQueryResult( elem.index, elem.simdist, weight));
		}
		return rt;
	}

	std::string tostring() const
	{
		std::ostringstream buf;
		const_iterator ri = begin(), re = end();
		for (; ri != re; ++ri)
		{
			const SimHashRank& elem = *ri;
			buf << "(" << elem.index << "->" << elem.simdist << ") ";
		}
		return buf.str();
	}
};

}//namespace
#endif

