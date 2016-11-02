/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Functions for probabilistic evaluating similarity relations (incl. multithreaded)
#ifndef _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_BUILDER_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_BUILDER_HPP_INCLUDED
#include "simRelationMap.hpp"
#include "simHash.hpp"
#include "lshBench.hpp"
#include <vector>
#include <algorithm>

namespace strus {

class SimRelationMapBuilder
{
public:
	SimRelationMapBuilder( const std::vector<SimHash>& samplear, unsigned int maxdist_, unsigned int maxsimsam_, unsigned int threads_, unsigned int seed_);
	SimRelationMap getSimRelationMap( strus::Index idx) const;

	unsigned int size() const
	{
		return m_benchar.size();
	}

private:
	const SimHash* m_base;
	unsigned int m_maxdist;
	unsigned int m_maxsimsam;
	unsigned int m_threads;
	unsigned int m_seed;
	std::vector<LshBench> m_benchar;
};

} //namespace
#endif

