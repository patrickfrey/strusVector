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
#include "strus/wordVector.hpp"
#include "strus/reference.hpp"
#include "databaseAdapter.hpp"
#include "lshModel.hpp"
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
	VectorStorageClient( const DatabaseInterface* database_, const std::string& configstring_, ErrorBufferInterface* errorhnd_);

	virtual ~VectorStorageClient(){}

	virtual VectorStorageSearchInterface* createSearcher( const std::string& type, int indexPart, int nofParts) const;

	virtual VectorStorageTransactionInterface* createTransaction();

	virtual std::vector<std::string> types() const;

	virtual ValueIteratorInterface* createFeatureValueIterator() const;

	virtual std::vector<std::string> featureTypes( const std::string& featureValue) const;

	virtual int nofVectors( const std::string& type) const;

	virtual WordVector featureVector( const std::string& type, const std::string& featureValue) const;

	virtual double vectorSimilarity( const WordVector& v1, const WordVector& v2) const;

	virtual WordVector normalize( const WordVector& vec) const;

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
	const LshModel model() const
	{
		return m_model;
	}

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	LshModel m_model;
	bool m_with_realVecWeights;
	strus::mutex m_transaction_mutex;	///< mutual exclusion in the critical part of a transaction
};

}//namespace
#endif

