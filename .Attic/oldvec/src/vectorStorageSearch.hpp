/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage search interface
#ifndef _STRUS_VECTOR_STORAGE_SEARCH_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_SEARCH_IMPLEMENTATION_HPP_INCLUDED
#include "strus/vectorStorageSearchInterface.hpp"
#include "databaseAdapter.hpp"
#include "simHashMap.hpp"

#error DEPRECATED

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DatabaseInterface;


class VectorStorageSearch
	:public VectorStorageSearchInterface
{
public:
	VectorStorageSearch( const Reference<DatabaseAdapter>& database, const LshModel& model_, const std::string& type, int indexPart, int nofParts, ErrorBufferInterface* errorhnd_);

	virtual ~VectorStorageSearch();

	virtual std::vector<VectorQueryResult> findSimilar( const WordVector& vec, int maxNofResults, double minSimilarity, bool realVecWeights) const;

	virtual void close();

private:
	ErrorBufferInterface* m_errorhnd;
	LshModel m_lshmodel;
	SimHashMap m_simhashar;
	Index m_typeno;
	Reference<DatabaseAdapter> m_database;
};

}//namespace
#endif

