/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for filtering probable candidates for LSH comparison
#ifndef _STRUS_VECTOR_SIMHASH_FILTER_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_FILTER_HPP_INCLUDED
#include "simHashBench.hpp"
#include <utility>
#include <cstring>
#include <vector>

namespace strus {

class SimHashFilter
{
public:
	enum {MaxNofBenches=4};
	struct Stats
	{
		int nofBenches;
		int nofCandidates[ MaxNofBenches];

		Stats()
			:nofBenches(0) {std::memset( nofCandidates, 0, sizeof(nofCandidates));}
		Stats( const Stats& o)
			:nofBenches(o.nofBenches) {std::memcpy( nofCandidates, o.nofCandidates, sizeof(nofCandidates));}
	};

public:
	SimHashFilter()
		:m_benchar(),m_nofBenches(0),m_elementArSize(0){}
	SimHashFilter( const SimHashFilter& o)
		:m_nofBenches(o.m_nofBenches),m_elementArSize(o.m_elementArSize) {for (int i=0; i<MaxNofBenches; ++i) m_benchar[i] = o.m_benchar[i];}
	~SimHashFilter(){}
#if __cplusplus >= 201103L
	SimHashFilter( SimHashFilter&& o)
		:m_nofBenches(o.m_nofBenches),m_elementArSize(o.m_elementArSize) {for (int i=0; i<MaxNofBenches; ++i) {m_benchar[i] = std::move( o.m_benchar[i]);}}
	SimHashFilter& operator =( SimHashFilter&& o)
		{m_nofBenches = o.m_nofBenches; m_elementArSize = o.m_elementArSize; for (int i=0; i<MaxNofBenches; ++i) {m_benchar[i] = std::move( o.m_benchar[i]);} return *this;}
#endif
	SimHashFilter& operator =( const SimHashFilter& o)
		{m_nofBenches = o.m_nofBenches; m_elementArSize = o.m_elementArSize; for (int i=0; i<MaxNofBenches; ++i) {m_benchar[i] = o.m_benchar[i];} return *this;}

	void append( const SimHash* sar, std::size_t sarsize);

	/// \param[out] resbuf buffer where to append result to
	void search( std::vector<SimHashSelect>& resbuf, const SimHash& needle, int maxSimDist, int maxProbSimDist) const;

	void searchWithStats( Stats& stats, std::vector<SimHashSelect>& resbuf, const SimHash& needle, int maxSimDist, int maxProbSimDist) const;

	int maxProbSumDist( int maxSimDist, int maxProbSimDist) const;

private:
	SimHashBenchArray m_benchar[ MaxNofBenches];
	int m_nofBenches;
	int m_elementArSize;
};

}//namespace
#endif


