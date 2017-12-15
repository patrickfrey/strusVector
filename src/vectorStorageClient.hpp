/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage client interface
#ifndef _STRUS_VECTOR_STORAGE_CLIENT_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_CLIENT_IMPLEMENTATION_HPP_INCLUDED
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/reference.hpp"
#include "vectorStorageConfig.hpp"
#include "databaseAdapter.hpp"
#include "strus/base/thread.hpp"
#include <vector>
#include <string>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DatabaseInterface;

class VectorStorageClient
	:public VectorStorageClientInterface
{
public:
	VectorStorageClient( const VectorStorageConfig& config_, const std::string& configstr_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_);

	virtual ~VectorStorageClient(){}

	virtual VectorStorageSearchInterface* createSearcher( const Index& range_from, const Index& range_to) const;

	virtual VectorStorageTransactionInterface* createTransaction();

	virtual std::vector<std::string> conceptClassNames() const;

	virtual std::vector<Index> featureConcepts( const std::string& conceptClass, const Index& index) const;

	virtual std::vector<double> featureVector( const Index& index) const;

	virtual std::string featureName( const Index& index) const;

	virtual Index featureIndex( const std::string& name) const;

	virtual double vectorSimilarity( const std::vector<double>& v1, const std::vector<double>& v2) const;
	
	virtual std::vector<Index> conceptFeatures( const std::string& conceptClass, const Index& conceptid) const;

	virtual unsigned int nofConcepts( const std::string& conceptClass) const;

	virtual unsigned int nofFeatures() const;

	virtual std::string config() const;

	virtual void close();

public:/*VectorStorageTransaction*/
	friend class TransactionLock;
	class TransactionLock
	{
	public:
		TransactionLock( VectorStorageClient* storage_)
			:m_mutex(&storage_->m_transaction_mutex)
		{
			m_mutex->lock();
		}
		~TransactionLock()
		{
			m_mutex->unlock();
		}

	private:
		strus::mutex* m_mutex;
	};

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	VectorStorageConfig m_config;
	strus::mutex m_transaction_mutex;	///< mutual exclusion in the critical part of a transaction
};

}//namespace
#endif

