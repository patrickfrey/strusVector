/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LSH Similarity model structure
#include "lshBench.hpp"
#include "strus/base/bitOperations.hpp"
#include "internationalization.hpp"
#include <cstring>

using namespace strus;

uint64_t LshBench::selmask( const SimHash& sh) const
{
	return sh.ar()[ m_seed % sh.arsize()];
}

LshBench::LshBench( const SimHash* base_, const strus::Index& basesize_, unsigned int seed_, unsigned int seeksimdist)
	:m_base(base_),m_basesize(basesize_),m_ofs(0),m_size(0),m_shsize(0),m_seed(seed_),m_maxdiff(0)
{
	std::memset( m_ar, 0, sizeof(*m_ar)*Width);
	if (m_basesize <= 0)
	{
		if (m_basesize < 0) throw strus::runtime_error(_TXT("illegal range passed to LshBench"));
		return;
	}
	m_shsize = m_base[0].size();
	if (m_shsize == 0) throw strus::runtime_error(_TXT("empty simhash values passed to LshBench"));
	m_maxdiff = seeksimdist * 64 / m_shsize;
}

LshBench::LshBench( const LshBench& o)
{
	std::memcpy( this, &o, sizeof(*this));
}

strus::Index LshBench::init( const strus::Index& ofs_)
{
	// Initializes m_size,m_ofs and m_ar:
	if (ofs_ >= m_basesize) throw strus::runtime_error(_TXT("illegal range passed to LshBench::init"));
	m_ofs = ofs_;
	strus::Index restsize = m_basesize - m_ofs;
	m_size = restsize>Width?Width:restsize;

	strus::Index ai = 0, ae = m_size;
	for (; ai != ae; ++ai)
	{
		if (m_base[m_ofs+ai].size() != m_shsize) throw strus::runtime_error(_TXT("empty simhash values passed to LshBench::init"));
		m_ar[ai] = selmask( m_base[m_ofs+ai]);
	}
	return m_size;
}

void LshBench::findSimCandidates( std::vector<Candidate>& res, const LshBench& o) const
{
	strus::Index oi = 0, oe = o.m_size;
	for (; oi != oe; ++oi)
	{
		res.reserve( (res.size() / ReserveMemSize + 2) * ReserveMemSize);
		uint64_t ndsel = o.m_ar[ oi];

		strus::Index ai = 0, ae = m_size;
		for (; ai != ae; ++ai)
		{
			if (strus::BitOperations::bitCount( m_ar[ai] ^ ndsel) <= m_maxdiff)
			{
				if (m_ofs+ai != o.m_ofs+oi)
				{
					res.push_back( Candidate( m_ofs+ai, o.m_ofs+oi));
				}
			}
		}
	}
}




