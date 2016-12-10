/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map for fast scan for similar SimHashes 
#ifndef _STRUS_VECTOR_SPACE_MODEL_SIMHASH_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_SIMHASH_MAP_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "strus/index.hpp"
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Map for fast scan for similar SimHashes 
class SimHashMap
{
public:
	SimHashMap( const SimHashMap& o)
		:m_ar(o.m_ar),m_selar(0),m_select(0),m_vecsize(0),m_seed(o.m_seed){initBench();}
	SimHashMap( const std::vector<SimHash>& ar_, unsigned int seed_)
		:m_ar(ar_),m_selar(0),m_select(0),m_vecsize(0),m_seed(seed_){initBench();}
	~SimHashMap();

	std::vector<Index> findSimilar( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, unsigned int maxNofElements) const;
	std::vector<Index> findSimilar( const SimHash& sh, unsigned short simdist, unsigned int maxNofElements) const;

	const SimHash& operator[]( std::size_t idx) const	{return m_ar[idx];}

	typedef std::vector<SimHash>::const_iterator const_iterator;
	const_iterator begin() const				{return m_ar.begin();}
	const_iterator end() const				{return m_ar.end();}

	std::size_t size() const				{return m_ar.size();}

private:
	void initBench();

private:
	std::vector<SimHash> m_ar;
	uint64_t* m_selar;
	unsigned int m_select;
	unsigned int m_vecsize;
	unsigned int m_seed;
};

}//namespace
#endif

