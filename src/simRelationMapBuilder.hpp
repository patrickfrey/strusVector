/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Class for probabilistic evaluating similarity relations (incl. multithreaded)
#ifndef _STRUS_VECTOR_GET_SIMILARITY_RELATION_MAP_BUILDER_HPP_INCLUDED
#define _STRUS_VECTOR_GET_SIMILARITY_RELATION_MAP_BUILDER_HPP_INCLUDED
#include "simRelationMap.hpp"
#include "simRelationReader.hpp"
#include "simHash.hpp"
#include "lshBench.hpp"
#include "random.hpp"
#include "logger.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace strus {

/// \brief Class for building a similarity relations matrix fast (probabilistic with possible misses) (incl. multithreaded)
class SimRelationMapBuilder
{
public:
	SimRelationMapBuilder( const std::vector<SimHash>& samplear, unsigned int maxdist_, unsigned int maxsimsam_, unsigned int rndsimsam_, unsigned int threads_, bool probabilistic_, Logger& logout, const SimRelationReader* simmapreader_);
	bool getNextSimRelationMap( SimRelationMap& res);

public:/*Internal for thread context*/
	SimRelationMap getSimRelationMap( strus::Index idx) const;

private:
	const SimHash* m_base;
	bool m_probabilistic;
	unsigned int m_maxdist;
	unsigned int m_maxsimsam;
	unsigned int m_rndsimsam;
	unsigned int m_threads;
	unsigned int m_index;
	std::vector<LshBench> m_benchar;
	Random m_rnd;
	unsigned int m_selectseed;
	const SimRelationReader* m_simmapreader;
};

} //namespace
#endif

