/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage builder
#ifndef _STRUS_VECTOR_STORAGE_BUILDER_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_BUILDER_IMPLEMENTATION_HPP_INCLUDED
#include "vectorStorageConfig.hpp"
#include "databaseAdapter.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "simHash.hpp"
#include "utils.hpp"
#include <vector>
#include <string>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DatabaseInterface;

class VectorStorageBuilder
{
public:
	VectorStorageBuilder( const VectorStorageConfig& config_, const std::string& configstr_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_);

	~VectorStorageBuilder(){}

	bool run( const std::string& command);

private:
	void rebase();

	void buildSimilarityRelationMap();

	void learnConcepts();

	void finalize();

	bool needToCalculateSimRelationMap();

	void buildSimRelationMap( const SimRelationReader* simmapreader);

	void run_genmodel( const std::string& clname, const GenModel& genmodel);

private:
	typedef std::map<std::string,GenModel> GenModelMap;

private:
	ErrorBufferInterface* m_errorhnd;
	const DatabaseInterface* m_dbi;
	DatabaseAdapter m_database;
	Reference<DatabaseAdapter::Transaction> m_transaction;
	VectorStorageConfig m_config;
	bool m_needToCalculateSimRelationMap;
	bool m_haveFeaturesAdded;
	LshModel m_lshmodel;
	GenModel m_genmodelmain;
	GenModelMap m_genmodelmap;
	std::vector<SimHash> m_samplear;
};


}//namespace
#endif

