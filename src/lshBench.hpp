/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LSH Similarity model structure
#ifndef _STRUS_VECTOR_SPACE_MODEL_LSH_BENCH_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_LSH_BENCH_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "strus/index.hpp"
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Structure for probabilistic LSH similarity guesses
class LshBench
{
public:
	enum {Width=256,ReserveMemSize=2048};

	LshBench( const SimHash* base, const strus::Index& basesize_, unsigned int seed_, unsigned int seeksimdist);
	LshBench( const LshBench& o);
	strus::Index init( const strus::Index& ofs_);

	struct Candidate
	{
		strus::Index row;
		strus::Index col;

		Candidate( const strus::Index& row_, const strus::Index& col_)
			:row(row_),col(col_){}
		Candidate( const Candidate& o)
			:row(o.row),col(o.col){}
	};

	void findSimCandidates( std::vector<Candidate>& res, const LshBench& o) const;

private:
	uint64_t selmask( const SimHash& sh) const;

private:
	const SimHash* m_base;
	strus::Index m_basesize;
	strus::Index m_ofs;
	strus::Index m_size;
	unsigned int m_shsize;
	unsigned int m_seed;
	unsigned int m_maxdiff;
	uint64_t m_ar[ Width];
};


}//namespace
#endif

