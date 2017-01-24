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
#include "vectorStorageConfig.hpp"
#include "databaseAdapter.hpp"
#include "simHashMap.hpp"
#include <boost/shared_ptr.hpp>


namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DatabaseInterface;


class VectorStorageSearch
	:public VectorStorageSearchInterface
{
public:
	VectorStorageSearch( const Reference<DatabaseAdapter>& database, const VectorStorageConfig& config_, const Index& range_from_, const Index& range_to_, ErrorBufferInterface* errorhnd_);

	virtual ~VectorStorageSearch(){}

	virtual std::vector<Result> findSimilar( const std::vector<double>& vec, unsigned int maxNofResults) const;

	virtual void close();

private:
	ErrorBufferInterface* m_errorhnd;
	VectorStorageConfig m_config;
	LshModel m_lshmodel;
	SimHashMap m_samplear;
	Reference<DatabaseAdapter> m_database;
	Index m_range_from;
	Index m_range_to;
};

}//namespace
#endif

