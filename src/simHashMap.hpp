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
	class QueryResult
	{
	public:
		/// \brief Default constructor
		QueryResult()
			:m_featno(0),m_weight(0.0){}
		/// \brief Copy constructor
		QueryResult( const QueryResult& o)
			:m_featno(o.m_featno),m_weight(o.m_weight){}
		/// \brief Constructor
		QueryResult( const Index& featno_, double weight_)
			:m_featno(featno_),m_weight(weight_){}
	
		Index featno() const			{return m_featno;}
		double weight() const			{return m_weight;}
	
		void setWeight( double weight_)		{m_weight = weight_;}
	
		bool operator < ( const QueryResult& o) const
		{
			if (m_weight + std::numeric_limits<double>::epsilon() < o.m_weight) return true;
			if (m_weight - std::numeric_limits<double>::epsilon() > o.m_weight) return false;
			return m_featno < o.m_featno;
		}
		bool operator > ( const QueryResult& o) const
		{
			if (m_weight + std::numeric_limits<double>::epsilon() < o.m_weight) return false;
			if (m_weight - std::numeric_limits<double>::epsilon() > o.m_weight) return true;
			return m_featno > o.m_featno;
		}
	
	private:
		Index m_featno;
		double m_weight;
	};

	SimHashMap()
		:m_ar(),m_selar1(0),m_selar2(0),m_select1(0),m_select2(0),m_vecsize(0),m_seed(0){}
	SimHashMap( const SimHashMap& o)
		:m_ar(o.m_ar),m_selar1(0),m_selar2(0),m_select1(0),m_select2(0),m_vecsize(o.m_vecsize),m_seed(o.m_seed){initBench();}
	explicit SimHashMap( const std::vector<SimHash>& ar_, int seed_=0)
		:m_ar(ar_),m_selar1(0),m_selar2(0),m_select1(0),m_select2(0),m_vecsize(0),m_seed(seed_){initBench();}
	~SimHashMap();

	SimHashMap& operator=( const SimHashMap& o)
	{
		m_ar = o.m_ar; m_selar1 = 0; m_selar2 = 0; m_select1 = 0; m_select2 = 0; m_vecsize = o.m_vecsize; m_seed = o.m_seed;
		initBench();
		return *this;
	}

	std::vector<QueryResult> findSimilar( const SimHash& sh, unsigned short simdist, unsigned short prob_simdist, int maxNofElements) const;
	std::vector<QueryResult> findSimilar( const SimHash& sh, unsigned short simdist, int maxNofElements) const;

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
	int m_select1;
	int m_select2;
	int m_vecsize;
	int m_seed;
};

}//namespace
#endif

