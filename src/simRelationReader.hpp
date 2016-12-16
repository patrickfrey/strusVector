/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Interface to get the most similar elements of sidx in the ascending order of similarity
#ifndef _STRUS_VECTOR_SIMRELATION_READER_HPP_INCLUDED
#define _STRUS_VECTOR_SIMRELATION_READER_HPP_INCLUDED
#include "simRelationMap.hpp"
#include <vector>

namespace strus {

/// \brief Interface to get the most similar elements of sidx in the ascending order of similarity
class SimRelationReader
{
public:
	virtual ~SimRelationReader(){}
	virtual std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const=0;
};

} //namespace
#endif

