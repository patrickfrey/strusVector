/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function for evaluating similarity relations (incl. multithreaded)
#ifndef _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_HPP_INCLUDED
#include "simRelationMap.hpp"
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Get a sparse matrix of similarity relations defined by edit distance of the LSH values of an array of samples
/// \param[in] samplear array of samples to process
/// \param[in] maxdist maximum edit distance to put into the result matrix
/// \param[in] logfile file to log status
/// \param[in] threads number of threads to use, 0 for no threading at all
/// \param[in] commitsize number of rows to build in an iteration of a thread (multithreading only)
/// \return similarity relation map
SimRelationMap getSimRelationMap(
		const std::vector<SimHash>& samplear,
		unsigned int maxdist, const char* logfile, unsigned int threads, unsigned int commitsize);

} //namespace
#endif


