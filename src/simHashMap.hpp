/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map for fast scan for similar SimHashes 
#ifndef _STRUS_VECTOR_SIMHASH_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_MAP_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "strus/index.hpp"
#include "strus/vectorStorageSearchInterface.hpp"
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Map for fast scan for similar SimHashes 
class SimHashMap
{
public:
	SimHashMap( const SimHashMap& o)
		:m_ar(o.m_ar),m_selar1(0),m_selar2(0),m_select1(0),m_select2(0),m_vecsize(0),m_seed(o.m_seed){initBench();}
	SimHashMap( const std::vector<SimHash>& ar_, unsigned int seed_)
		:m_ar(ar_),m_selar1(0),m_selar2(0),m_select1(0),m_select2(0),m_vecsize(0),m_seed(seed_){initBench();}
	~SimHashMap();

	std::vector<VectorQueryResult> findSimilar( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, unsigned int maxNofElements, const Index& indexofs) const;
	std::vector<VectorQueryResult> findSimilar( const SimHash& sh, unsigned short simdist, unsigned int maxNofElements, const Index& indexofs) const;
	std::vector<VectorQueryResult> findSimilarFromSelection( const std::vector<Index>& selection, const SimHash& sh, unsigned short simdist, unsigned int maxNofElements, const Index& indexofs) const;

	const SimHash& operator[]( std::size_t idx) const	{return m_ar[idx];}

	typedef std::vector<SimHash>::const_iterator const_iterator;
	const_iterator begin() const				{return m_ar.begin();}
	const_iterator end() const				{return m_ar.end();}

	std::size_t size() const				{return m_ar.size();}

private:
	void initBench();

private:
	std::vector<SimHash> m_ar;
	uint64_t* m_selar1;
	uint64_t* m_selar2;
	unsigned int m_select1;
	unsigned int m_select2;
	unsigned int m_vecsize;
	unsigned int m_seed;
};

}//namespace
#endif

