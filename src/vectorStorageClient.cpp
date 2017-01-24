/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage client interface
#include "vectorStorageClient.hpp"
#include "vectorStorageSearch.hpp"
#include "vectorStorageTransaction.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"

#define MODULENAME   "standard vector storage"
#define MAIN_CONCEPT_CLASSNAME ""

#undef STRUS_LOWLEVEL_DEBUG

using namespace strus;

VectorStorageClient::VectorStorageClient( const VectorStorageConfig& config_, const std::string& configstr_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_database(),m_config(config_)
{
	m_database.reset( new DatabaseAdapter( database_,config_.databaseConfig,errorhnd_));
	m_database->checkVersion();
	if (m_database->isempty()) throw strus::runtime_error(_TXT("try to open a vector storage that is empty, need to be built first"));
	VectorStorageConfig cfg = m_database->readConfig();
	m_config = VectorStorageConfig( configstr_, errorhnd_, cfg);
}

VectorStorageSearchInterface* VectorStorageClient::createSearcher( const Index& range_from, const Index& range_to) const
{
	try
	{
		return new VectorStorageSearch( m_database, m_config, range_from, range_to, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' creating searcher: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorStorageTransactionInterface* VectorStorageClient::createTransaction()
{
	try
	{
		return new VectorStorageTransaction( this, m_database, m_config, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' creating transaction: %s"), MODULENAME, *m_errorhnd, 0);
}

std::vector<std::string> VectorStorageClient::conceptClassNames() const
{
	try
	{
		std::vector<std::string> rt;
		rt.push_back( MAIN_CONCEPT_CLASSNAME);
		std::vector<std::string> dbclnames = m_database->readConceptClassNames();
		rt.insert( rt.end(), dbclnames.begin(), dbclnames.end());
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the list of implemented concept class names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
}


std::vector<Index> VectorStorageClient::featureConcepts( const std::string& conceptClass, const Index& index) const
{
	try
	{
		return m_database->readSampleConceptIndices( conceptClass, index);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' retrieving associated concepts by sample index: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
}

std::vector<double> VectorStorageClient::featureVector( const Index& index) const
{
	try
	{
		return m_database->readSampleVector( index);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting sample vector by index: %s"), MODULENAME, *m_errorhnd, std::vector<double>());
}

std::string VectorStorageClient::featureName( const Index& index) const
{
	try
	{
		return m_database->readSampleName( index);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting name of sample: %s"), MODULENAME, *m_errorhnd, std::string());
}

Index VectorStorageClient::featureIndex( const std::string& name) const
{
	try
	{
		return m_database->readSampleIndex( name);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting index of feature by its name: %s"), MODULENAME, *m_errorhnd, -1);
}

std::vector<Index> VectorStorageClient::conceptFeatures( const std::string& conceptClass, const Index& conceptid) const
{
	try
	{
		return m_database->readConceptSampleIndices( conceptClass, conceptid);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting associated feature indices by learnt concept: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
}

unsigned int VectorStorageClient::nofConcepts( const std::string& conceptClass) const
{
	try
	{
		return m_database->readNofConcepts( conceptClass);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting number of concepts learnt: %s"), MODULENAME, *m_errorhnd, 0);
}

unsigned int VectorStorageClient::nofFeatures() const
{
	try
	{
		return m_database->readNofSamples();
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting number of features defined: %s"), MODULENAME, *m_errorhnd, 0);
}

std::string VectorStorageClient::config() const
{
	try
	{
		return m_config.tostring();
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' mapping configuration to string: %s"), MODULENAME, *m_errorhnd, std::string());
}

