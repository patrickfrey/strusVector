/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Class for probabilistic evaluating similarity relations (incl. multithreaded)
#ifndef _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_BUILDER_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_BUILDER_HPP_INCLUDED
#include "simRelationMap.hpp"
#include "simHash.hpp"
#include "lshBench.hpp"
#include "random.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace strus {

/// \brief Forward declaration
class DatabaseInterface;
/// \brief Forward declaration
class ErrorBufferInterface;

/// \brief Class for building a similarity relations matrix fast (probabilistic with possible misses) (incl. multithreaded)
class SimRelationMapBuilder
{
public:
	SimRelationMapBuilder( const DatabaseInterface* database_, const std::string& databaseConfig_, const std::vector<SimHash>& samplear, unsigned int maxdist_, unsigned int maxsimsam_, unsigned int rndsimsam_, unsigned int threads_, ErrorBufferInterface* errorhnd_);
	bool getNextSimRelationMap( SimRelationMap& res);
	SimRelationMap getSimRelationMap( strus::Index idx) const;

private:
	ErrorBufferInterface* m_errorhnd;
	const DatabaseInterface* m_database;
	std::string m_databaseConfig;
	const SimHash* m_base;
	unsigned int m_maxdist;
	unsigned int m_maxsimsam;
	unsigned int m_rndsimsam;
	unsigned int m_threads;
	unsigned int m_index;
	std::vector<LshBench> m_benchar;
	Random m_rnd;
	unsigned int m_selectseed;
};

} //namespace
#endif

