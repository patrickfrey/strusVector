/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure holding one part of an array of LSH values to filter probable candidates for LSH comparison
#ifndef _STRUS_VECTOR_SIMHASH_BENCH_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_BENCH_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "simHash.hpp"
#include <utility>
#include <vector>

namespace strus {

/// \brief Reference to probable similarity candidate 
struct SimHashSelect
{
	int idx;	//< index of element in array
	int shdiff;	//< accumulated diff over all benches

	SimHashSelect()
		:idx(0),shdiff(0){}
	SimHashSelect( int idx_, int shdiff_)
		:idx(idx_),shdiff(shdiff_){}
	SimHashSelect( const SimHashSelect& o)
		:idx(o.idx),shdiff(o.shdiff){}

	bool operator < ( const SimHashSelect& o) const
	{
		return shdiff == o.shdiff ? idx < o.idx : shdiff < o.shdiff;
	}
};


/// \brief Structure holding one part of an array of LSH values to filter probable candidates for LSH comparison
class SimHashBench
{
public:
	enum {Size=32768};

public:
	SimHashBench();
	SimHashBench( const SimHashBench& o);
	~SimHashBench();
#if __cplusplus >= 201103L
	SimHashBench( SimHashBench&& o)
		:m_ar(o.m_ar),m_arsize(o.m_arsize),m_startIdx(o.m_startIdx) {o.m_ar=0;o.m_arsize=0;o.m_startIdx=0;}
	SimHashBench& operator=(SimHashBench&& o)
		{if (m_ar) std::free(m_ar); m_ar=o.m_ar;m_arsize=o.m_arsize;m_startIdx=o.m_startIdx;o.m_ar=0;o.m_arsize=0;o.m_startIdx=0;return *this;}
#endif
	SimHashBench& operator=( const SimHashBench& o);

	void fill( const SimHash* sar, std::size_t sarsize, int simHashIdx, int startIdx);
	void append( const SimHash* sar, std::size_t sarsize, int simHashIdx);

	/// \param[out] resbuf buffer where to append result to
	void search( std::vector<SimHashSelect>& resbuf, uint64_t needle, int maxSimDist) const;

	/// \param[in,out] resbuf buffer with result to filter
	void filter( std::vector<SimHashSelect>& resbuf, std::size_t residx, uint64_t needle, int maxSimDist, int maxSumSimDist) const;

	std::size_t size() const
	{
		return m_arsize;
	}
	bool full() const
	{
		return m_arsize == Size;
	}

private:
	uint64_t* m_ar;
	std::size_t m_arsize;
	int m_startIdx;
};


/// \brief Structure holding one part of an array of LSH values to filter probable candidates for LSH comparison
class SimHashBenchArray
{
public:
	SimHashBenchArray()
		:m_ar(){}
	SimHashBenchArray( const SimHashBenchArray& o)
		:m_ar(o.m_ar){}
#if __cplusplus >= 201103L
	SimHashBenchArray( SimHashBenchArray&& o)
		:m_ar(std::move(o.m_ar)) {}
	SimHashBenchArray& operator=(SimHashBenchArray&& o)
		{m_ar=std::move(o.m_ar); return *this;}
#endif
	SimHashBenchArray& operator=( const SimHashBenchArray& o)
		{m_ar=o.m_ar; return *this;}

	void append( const SimHash* sar, std::size_t sarsize, int simHashIdx);

	typedef std::vector<SimHashBench>::const_iterator const_iterator;
	const_iterator begin() const		{return m_ar.begin();}
	const_iterator end() const		{return m_ar.end();}

private:
	std::vector<SimHashBench> m_ar;
};

}//namespace
#endif


