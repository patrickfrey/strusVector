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
#include <map>
#include <set>

namespace strus {

/// \brief Interface for building a repository of vectors with classifiers to map them to discrete features.
/// \remark This interface has the transaction context logically enclosed in the object, though use in a multithreaded context does not make much sense. Thread safety of the interface is guaranteed, but not the performance in a multithreaded context. It is thought as class that internally makes heavily use of multithreading, but is not thought to be fed by mutliple threads.
class VectorStorageTransaction
		:public VectorStorageTransactionInterface
{
public:
	virtual VectorStorageTransaction(){}
	virtual ~VectorStorageTransaction(){}

	virtual void addFeature( const std::string& name, const std::vector<double>& vec);

	virtual void defineFeatureConceptRelation( const std::string& relationTypeName, const Index& featidx, const Index& conidx);

	virtual bool commit();

	virtual void rollback();

private:
	typedef std::map<std::string,unsigned int> ConceptTypeMap;
	typedef std::pair<Index,Index> RelationDef;
	typedef std::set<RelationDef> Relation;

	ConceptTypeMap m_conceptTypeMap;
	std::vector<Relation> m_conceptFeatureRelationList;
	std::vector<Relation> m_featureConceptRelationList;
};

}//namespace
#endif


