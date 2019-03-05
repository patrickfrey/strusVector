/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Vector repository transaction implementation
#ifndef _STRUS_VECTOR_STORGE_TRANSACTION_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_STORGE_TRANSACTION_IMPLEMENTATION_HPP_INCLUDED
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseTransactionInterface.hpp"
#include "strus/reference.hpp"
#include "strus/base/symbolTable.hpp"
#include "databaseAdapter.hpp"
#include "vectorDef.hpp"
#include <map>
#include <set>

namespace strus {

/// \brief Forward declaration
class VectorStorageClient;
/// \brief Forward declaration
class DebugTraceContextInterface;

/// \brief Interface for building a repository of vectors with classifiers to map them to discrete features.
/// \remark This interface has the transaction context logically enclosed in the object, though use in a multithreaded context does not make much sense. Thread safety of the interface is guaranteed, but not the performance in a multithreaded context. It is thought as class that internally makes heavily use of multithreading, but is not thought to be fed by mutliple threads.
class VectorStorageTransaction
		:public VectorStorageTransactionInterface
{
public:
	VectorStorageTransaction( VectorStorageClient* storage_, Reference<DatabaseAdapter>& database_, ErrorBufferInterface* errorhnd_);

	virtual ~VectorStorageTransaction();

	virtual void defineVector( const std::string& type, const std::string& name, const WordVector& vec);

	virtual void defineFeature( const std::string& type, const std::string& name);

	virtual void clear();

	virtual bool commit();

	virtual void rollback();

private:
	void defineElement( const std::string& type, const std::string& name, const WordVector& vec);
	void reset();

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	VectorStorageClient* m_storage;

	Reference<DatabaseAdapter> m_database;
	Reference<DatabaseAdapter::Transaction> m_transaction;

	std::vector<std::vector<VectorDef> > m_vecar;
	SymbolTable m_typetab;
	SymbolTable m_nametab;

	struct FeatureTypeRelation
	{
		int featno;
		int typeno;

		FeatureTypeRelation( int featno_, int typeno_)		:featno(featno_),typeno(typeno_){}
		FeatureTypeRelation( const FeatureTypeRelation& o)	:featno(o.featno),typeno(o.typeno){}
		FeatureTypeRelation()					:featno(0),typeno(0){}

		bool operator < (const FeatureTypeRelation& o) const
		{
			return (featno == o.featno) ? (typeno < o.typeno):(featno < o.featno);
		}
	};

	std::set<FeatureTypeRelation> m_featTypeRelations;
};

}//namespace
#endif


