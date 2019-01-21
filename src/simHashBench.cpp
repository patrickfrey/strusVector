/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Map for fast scan for similar SimHashes 
#include "simHashBench.hpp"
#include "strus/base/bitOperations.hpp"
#include "strus/base/malloc.hpp"
#include "strus/base/platform.hpp"
#include "internationalization.hpp"
#include <stdexcept>
#include <cstring>
#include <new>

using namespace strus;

SimHashBench::SimHashBench()
	:m_ar((uint64_t*)strus::aligned_malloc( Size * sizeof(uint64_t), strus::platform::CacheLineSize)),m_arsize(0),m_startIdx(0)
{
	if (!m_ar) throw std::bad_alloc();
	std::memset( m_ar, 0, Size * sizeof(uint64_t));
}

SimHashBench::SimHashBench( const SimHashBench& o)
	:m_ar((uint64_t*)strus::aligned_malloc( Size * sizeof(uint64_t), strus::platform::CacheLineSize)),m_arsize(o.m_arsize),m_startIdx(o.m_startIdx)
{
	if (!m_ar) throw std::bad_alloc();
	std::memcpy( m_ar, o.m_ar, Size * sizeof(uint64_t));
}

SimHashBench& SimHashBench::operator=( const SimHashBench& o)
{
	uint64_t* newar = (uint64_t*)strus::aligned_malloc( Size * sizeof(uint64_t), strus::platform::CacheLineSize);
	if (!newar) throw std::bad_alloc();
	strus::aligned_free( m_ar);
	m_ar = newar;
	std::memcpy( m_ar, o.m_ar, Size * sizeof(uint64_t));
	m_arsize = o.m_arsize;
	m_startIdx = o.m_startIdx;
	return *this;
}

SimHashBench::~SimHashBench()
{
	strus::aligned_free( m_ar);
}

void SimHashBench::append( const SimHash* sar, std::size_t sarSize, int simHashIdx)
{
	if (sarSize == 0) return;
	int simHashSize = sar[ 0].arsize();
	if (simHashIdx >= simHashSize) throw strus::runtime_error(_TXT("simhash index out of range: %d >= %d"), simHashIdx, simHashSize);

	std::size_t ai = 0, ae = Size;
	if (m_arsize + sarSize < Size)
	{
		ae = sarSize;
	}
	else if (m_arsize + sarSize > Size)
	{
		throw strus::runtime_error( _TXT("number of elements %d written exceeds size of structure %d"), (int)(m_arsize + sarSize), (int)Size);
	}
	for (; ai != ae; ++ai)
	{
		m_ar[m_arsize+ai] = sar[ ai].ar()[ simHashIdx];
	}
	m_arsize += sarSize;
}

void SimHashBench::fill( const SimHash* sar, std::size_t sarSize, int simHashIdx, int startIdx_)
{
	if (m_arsize > sarSize)
	{
		std::memset( m_ar, 0, Size * sizeof(uint64_t));
	}
	m_arsize = 0;
	m_startIdx = startIdx_;

	append( sar, sarSize, simHashIdx);
}


void SimHashBench::search( std::vector<SimHashSelect>& resbuf, uint64_t needle, int simHashIdx, int maxSimDist) const
{
	std::size_t ai = 0, ae = m_arsize;
	for (; ai != ae; ++ai)
	{
		int simDist = strus::BitOperations::bitCount( m_ar[ simHashIdx] ^ needle);
		if (simDist <= maxSimDist)
		{
			resbuf.push_back( SimHashSelect( m_startIdx + ai, simDist));
		}
	}
}

void SimHashBench::filter( std::vector<SimHashSelect>& resbuf, std::size_t residx, uint64_t needle, int simHashIdx, int maxSimDist, int maxSumSimDist) const
{
	std::size_t destidx = residx;
	std::size_t srcidx = residx;
	for (; srcidx < resbuf.size(); ++srcidx)
	{
		SimHashSelect& sel = resbuf[ srcidx];
		std::size_t aridx = (std::size_t)sel.idx - m_startIdx;

		if (sel.idx < m_startIdx || aridx >= m_arsize) throw std::runtime_error(_TXT("array bound read in SimHashBench::filter"));

		int simDist = strus::BitOperations::bitCount( m_ar[ simHashIdx] ^ needle);
		int sumSimDist = simDist + sel.shdiff;

		if (simDist <= maxSimDist && sumSimDist <= maxSumSimDist)
		{
			//... match
			resbuf[ destidx++] = SimHashSelect( sel.idx, sumSimDist);
		}
	}
	resbuf.resize( destidx);
}

void SimHashBenchArray::append( const SimHash* sar, std::size_t sarsize, int simHashIdx)
{
	SimHash const* sptr = sar;
	if (!m_ar.empty() && !m_ar.back().full())
	{
		std::size_t elementsLeft = SimHashBench::Size - m_ar.back().size();
		std::size_t elementsInsert = (sarsize < elementsLeft) ? sarsize : elementsLeft;

		m_ar.back().append( sptr, elementsInsert, simHashIdx);
		sptr += elementsInsert;
		sarsize -= elementsInsert;
	}
	while (sarsize)
	{
		int startIdx = m_ar.size() * SimHashBench::Size;
		std::size_t elementsInsert = sarsize < SimHashBench::Size ? sarsize : SimHashBench::Size;

		m_ar.push_back( SimHashBench());
		m_ar.back().fill( sptr, elementsInsert, simHashIdx, startIdx);
		sptr += elementsInsert;
		sarsize -= elementsInsert;
	}
}



