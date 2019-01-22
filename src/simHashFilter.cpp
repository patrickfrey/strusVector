/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for filtering probable candidates for LSH comparison
#include "simHashFilter.hpp"
#include "internationalization.hpp"

using namespace strus;

void SimHashFilter::append( const SimHash* sar, std::size_t sarsize)
{
	if (sarsize == 0) return;

	if (!m_nofBenches)
	{
		m_elementArSize = sar[0].arsize();
		m_nofBenches = MaxNofBenches;
		while (m_elementArSize / 4 < m_nofBenches && m_nofBenches > 1) --m_nofBenches;
	}
	else if (m_elementArSize != sar[0].arsize())
	{
		throw strus::runtime_error(_TXT("mixing LSH values of different sizes in similarity hash filter: %d != %d"), m_elementArSize, (int)sar[0].arsize());
	}
	for (int ni=0; ni<m_nofBenches; ++ni)
	{
		m_benchar[ ni].append( sar, sarsize, ni);
	}
}


void SimHashFilter::search( std::vector<SimHashSelect>& resbuf, const SimHash& needle, int maxSimDist, int maxProbSimDist) const
{
	if (!m_nofBenches) return;
	resbuf.reserve( SimHashBench::Size);

	if (maxProbSimDist < maxSimDist)
	{
		throw strus::runtime_error(_TXT("invalid simdist=%d,probsimdist=%d arguments passed to LSH filter search"), maxSimDist, maxProbSimDist);
	}
	if (m_elementArSize != needle.arsize())
	{
		throw strus::runtime_error(_TXT("search of LSH value with different size than stored in similarity hash filter: %d != %d"), m_elementArSize, (int)needle.arsize());
	}
	double relProbSimDist = maxProbSimDist / m_elementArSize;
	double probSimDistSumLimitDecr = (double)(maxProbSimDist - maxSimDist) / (double)(m_elementArSize * 2);

	std::size_t si = 0, se = m_benchar[ 0].size();
	for (; si != se; ++si)
	{
		std::size_t residx = resbuf.size();
		std::size_t bi = 0, be = m_nofBenches;
		m_benchar[ bi][ si].search( resbuf, needle.ar()[ bi], relProbSimDist);
		for (++bi; bi != be; ++bi)
		{
			int relSumSimDist = (bi+1) * relProbSimDist - bi * probSimDistSumLimitDecr;
			m_benchar[ bi][ si].filter( resbuf, residx, needle.ar()[ bi], relProbSimDist, relSumSimDist);
		}
	}
}

int SimHashFilter::maxProbSumDist( int maxSimDist, int maxProbSimDist) const
{
	double relProbSimDist = (double)maxProbSimDist / (double)m_elementArSize;
	double probSimDistSumLimitDecr = (double)(maxProbSimDist - maxSimDist) / (double)(m_elementArSize * 2);
	return (m_nofBenches) * relProbSimDist - (m_nofBenches-1) * probSimDistSumLimitDecr;
}



