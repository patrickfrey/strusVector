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
#include "strus/storage/wordVector.hpp"
#include "strus/reference.hpp"
#include "databaseAdapter.hpp"
#include "simHashQueryResult.hpp"
#include "sentenceLexerConfig.hpp"
#include "lshModel.hpp"
#include "simHashMap.hpp"
#include "strus/base/thread.hpp"
#include <vector>
#include <string>
#include <map>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DebugTraceContextInterface;
/// \brief Forward declaration
class DatabaseInterface;
/// \brief Forward declaration
class VectorStorage;

class VectorStorageClient
	:public VectorStorageClientInterface
{
public:
	VectorStorageClient( const DatabaseInterface* database_, const std::string& configstring_, ErrorBufferInterface* errorhnd_);

	virtual ~VectorStorageClient();

	virtual void prepareSearch( const std::string& type);

	virtual std::vector<VectorQueryResult> findSimilar( const std::string& type, const WordVector& vec, int maxNofResults, double minSimilarity, double speedRecallFactor, bool realVecWeights) const;

	virtual VectorStorageTransactionInterface* createTransaction();

	virtual std::vector<std::string> types() const;

	virtual ValueIteratorInterface* createFeatureValueIterator() const;

	virtual std::vector<std::string> featureTypes( const std::string& featureValue) const;

	virtual int nofTypes() const;

	virtual int nofFeatures() const;

	virtual int nofVectors( const std::string& type) const;

	virtual WordVector featureVector( const std::string& type, const std::string& featureValue) const;

	virtual double vectorSimilarity( const WordVector& v1, const WordVector& v2) const;

	virtual WordVector normalize( const WordVector& vec) const;

	virtual SentenceLexerInstanceInterface* createSentenceLexer() const;
	
	virtual std::string config() const;

	virtual void close();

	virtual void compaction();
	
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

	void resetSimHashMapTypes( const std::vector<std::string>& types_);

public:/*SentenceLexerContext*/
	std::vector<std::string> getTypeNames( const strus::Index& featno) const;
	std::vector<strus::Index> getRelatedTypes( const strus::Index& featno) const;
	strus::Index getFeatNo( const std::string& featname) const;
	strus::Index getTypeNo( const std::string& typname) const;
	WordVector getVector( const strus::Index& typeno, const strus::Index& featno) const;
	std::string getTypeNameFromIndex( const Index& typeno) const;
	std::string getFeatNameFromIndex( const Index& featno) const;

private:
	strus::Reference<SimHashMap> getOrCreateTypeSimHashMap( const std::string& type) const;
	strus::Reference<SimHashMap> getSimHashMap( const std::string& type) const;
	std::vector<VectorQueryResult> simHashToVectorQueryResults( const std::vector<SimHashQueryResult>& res, int maxNofResults, double minSimilarity) const;

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	Reference<DatabaseAdapter> m_database;
	LshModel m_model;
	typedef strus::Reference<SimHashMap> SimHashMapRef;
	typedef std::map<std::string,SimHashMapRef> SimHashMapMap;
	typedef strus::Reference<SimHashMapMap> SimHashMapMapRef;
	mutable strus::Reference<SimHashMapMap> m_simHashMapMap;
	std::vector<std::string> m_inMemoryTypes;			///< cached types
	SentenceLexerConfig m_lexerConfig;				///< sentence lexer configuration
	strus::mutex m_transaction_mutex;				///< mutual exclusion in the critical part of a transaction
};

}//namespace
#endif

