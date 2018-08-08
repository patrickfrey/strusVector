/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage builder
#include "vectorStorageBuilder.hpp"
#include "simRelationReader.hpp"
#include "simRelationMapBuilder.hpp"
#include "logger.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/string_conv.hpp"
#include "strus/errorBufferInterface.hpp"

using namespace strus;
#define MODULENAME   "standard vector storage"
#define MAIN_CONCEPT_CLASSNAME ""

class SimRelationMapReader
	:public SimRelationReader
{
public:
	explicit SimRelationMapReader( const DatabaseAdapter* database_)
		:m_database(database_){}
	virtual ~SimRelationMapReader(){}
	virtual std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const
	{
		return m_database->readSimRelations( sidx);
	}
private:
	const DatabaseAdapter* m_database;
};

class SimRelationNeighbourReader
	:public SimRelationReader
{
public:
	SimRelationNeighbourReader( const DatabaseAdapter* database_, const std::vector<SimHash>* samplear_, unsigned int maxdist_)
		:m_database(database_),m_samplear(samplear_),m_maxdist(maxdist_){}
	virtual ~SimRelationNeighbourReader(){}

	virtual std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const
	{
		std::vector<SimRelationMap::Element> rt;
		std::vector<strus::Index> conlist = m_database->readSampleConceptIndices( MAIN_CONCEPT_CLASSNAME, sidx);
		std::vector<strus::Index>::const_iterator ci = conlist.begin(), ce = conlist.end();
		std::map<strus::Index,unsigned short> simmap;
		for (; ci != ce; ++ci)
		{
			std::vector<strus::Index> nblist = m_database->readConceptSampleIndices( MAIN_CONCEPT_CLASSNAME, *ci);
			std::vector<strus::Index>::const_iterator ni = nblist.begin(), ne = nblist.end();
			for (; ni != ne; ++ni)
			{
				if ((*m_samplear)[ sidx].near( (*m_samplear)[*ni], m_maxdist))
				{
					simmap[ *ni] = (*m_samplear)[ sidx].dist( (*m_samplear)[*ni]);
				}
			}
		}
		std::map<strus::Index,unsigned short>::const_iterator mi = simmap.begin(), me = simmap.end();
		for (; mi != me; ++mi)
		{
			rt.push_back( SimRelationMap::Element( mi->first, mi->second));
		}
		return rt;
	}
private:
	const DatabaseAdapter* m_database;
	const std::vector<SimHash>* m_samplear;
	unsigned int m_maxdist;
};


VectorStorageBuilder::VectorStorageBuilder( const VectorStorageConfig& config_, const std::string& configstr_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_)
	,m_dbi(database_)
	,m_database(database_,config_.databaseConfig,errorhnd_)
	,m_config(config_)
	,m_needToCalculateSimRelationMap(false),m_haveFeaturesAdded(false)
	,m_lshmodel(),m_genmodelmain(),m_genmodelmap(),m_samplear()
{
	m_transaction.reset( m_database.createTransaction());
	m_database.checkVersion();
	VectorStorageConfig cfg = m_database.readConfig();
	m_config = VectorStorageConfig( configstr_, errorhnd_, cfg);
	if (!m_config.isBuildCompatible( cfg))
	{
		throw std::runtime_error( _TXT("loading vector storage with incompatible configuration"));
	}
	if (m_config.maxdist > cfg.maxdist)
	{
		m_needToCalculateSimRelationMap = true;
	}
	m_lshmodel = m_database.readLshModel();
	m_genmodelmain = GenModel( m_config.threads, m_config.maxdist, m_config.gencfg.simdist, m_config.gencfg.raddist, m_config.gencfg.eqdist, m_config.gencfg.mutations, m_config.gencfg.votes, m_config.gencfg.descendants, m_config.gencfg.maxage, m_config.gencfg.iterations, m_config.gencfg.assignments, m_config.gencfg.greediness, m_config.gencfg.isaf, m_config.gencfg.baff, m_config.gencfg.fdf, m_config.gencfg.eqdiff, m_config.gencfg.with_singletons, m_errorhnd);
	GenModelConfigMap::const_iterator mi = m_config.altgenmap.begin(), me = m_config.altgenmap.end();
	for (; mi != me; ++mi)
	{
		m_genmodelmap[ mi->first] = GenModel( m_config.threads, m_config.maxdist, mi->second.simdist, mi->second.raddist, mi->second.eqdist, mi->second.mutations, mi->second.votes, mi->second.descendants, mi->second.maxage, mi->second.iterations, mi->second.assignments, mi->second.greediness, mi->second.isaf, mi->second.baff, mi->second.fdf, mi->second.eqdiff, mi->second.with_singletons, m_errorhnd);
	}
	m_samplear = m_database.readSampleSimhashVector( 0, std::numeric_limits<Index>::max());
}

bool VectorStorageBuilder::run( const std::string& command)
{
	if (command.empty())
	{
		finalize();
		return true;
	}
	else if (strus::caseInsensitiveEquals( command, "rebase"))
	{
		rebase();
		return true;
	}
	else if (strus::caseInsensitiveEquals( command, "base"))
	{
		buildSimilarityRelationMap();
		return true;
	}
	else if (strus::caseInsensitiveEquals( command, "learn"))
	{
		if (m_samplear.empty())
		{
			throw std::runtime_error( _TXT("no features defined to learn concepts"));
		}
		learnConcepts();
		return true;
	}
	else if (strus::caseInsensitiveEquals( command, "clear"))
	{
		m_transaction->deleteData();
		m_transaction->commit();
		return true;
	}
	else
	{
		throw strus::runtime_error(_TXT("unknown command '%s'"), command.c_str());
	}
}

void VectorStorageBuilder::rebase()
{
	SimRelationNeighbourReader nbreader( &m_database, &m_samplear, m_config.maxdist);
	buildSimRelationMap( &nbreader);
}

void VectorStorageBuilder::buildSimilarityRelationMap()
{
	buildSimRelationMap( 0/*without SimRelationNeighbourReader*/);
}

void VectorStorageBuilder::learnConcepts()
{
	m_transaction->deleteSampleConceptIndexMaps();
	m_transaction->deleteConceptSampleIndexMaps();
	m_transaction->deleteConceptClassNames();
	m_transaction->commit();

	std::vector<std::string> clnames;
	run_genmodel( MAIN_CONCEPT_CLASSNAME, m_genmodelmain);
	GenModelMap::const_iterator mi = m_genmodelmap.begin(), me = m_genmodelmap.end();
	for (; mi != me; ++mi)
	{
		run_genmodel( mi->first, mi->second);
		clnames.push_back( mi->first);
	}
	m_transaction->writeConceptClassNames( clnames);
	m_transaction->commit();
}

void VectorStorageBuilder::finalize()
{
	if (!m_samplear.empty())
	{
		if (needToCalculateSimRelationMap())
		{
			buildSimRelationMap( 0/*without SimRelationNeighbourReader*/);
		}
		learnConcepts();
	}
}

bool VectorStorageBuilder::needToCalculateSimRelationMap()
{
	unsigned int nofSamples = m_database.readNofSamples();
	unsigned int simNofSamples = m_database.readNofSimRelations();
	if (m_needToCalculateSimRelationMap || m_config.with_forcesim || nofSamples > simNofSamples)
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		if (logfile)
		{
			Logger logout( logfile);
			logout << string_format( _TXT("need to calculate sim relation map (%s) forcesim=%s, nof samples %u / %u"), (m_needToCalculateSimRelationMap?"true":"false"), (m_config.with_forcesim?"true":"false"), nofSamples, simNofSamples);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void VectorStorageBuilder::buildSimRelationMap( const SimRelationReader* simmapreader)
{
	const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
	Logger logout( logfile);

	if (logout) logout << string_format( _TXT("calculate similarity relation matrix for %u features (selection %s)"), (unsigned int)m_samplear.size(), m_config.with_probsim?_TXT("probabilistic selection"):_TXT("try all"));
	uint64_t total_nof_similarities = 0;
	SimRelationMapBuilder simrelbuilder( m_samplear, m_config.maxdist, m_config.maxsimsam, m_config.rndsimsam, m_config.threads, m_config.with_probsim, logout, simmapreader);
	SimRelationMap simrelmap_part;
	while (simrelbuilder.getNextSimRelationMap( simrelmap_part))
	{
		m_transaction->writeSimRelationMap( simrelmap_part);
		m_transaction->commit();
		total_nof_similarities += simrelmap_part.nofRelationsDetected();
		if (logout) logout << string_format( _TXT("got total %u features with %uK similarities"), simrelmap_part.endIndex(), (unsigned int)(total_nof_similarities/1024));
	}
	m_transaction->writeNofSimRelations( m_samplear.size());
	m_transaction->commit();
	if (logout) logout << string_format( _TXT("calculated similarity relation matrix stored to database"));
}

void VectorStorageBuilder::run_genmodel( const std::string& clname, const GenModel& genmodel)
{
	const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
	SimRelationMapReader simrelmap( &m_database);
	std::vector<SampleIndex> singletons;
	SampleConceptIndexMap sampleConceptIndexMap;
	ConceptSampleIndexMap conceptSampleIndexMap;

	std::vector<SimHash> resultar = genmodel.run(
			singletons, sampleConceptIndexMap, conceptSampleIndexMap,
			clname, m_samplear, m_config.maxconcepts, simrelmap, m_config.threads, logfile);

	m_transaction->writeSampleConceptIndexMap( clname, sampleConceptIndexMap);
	m_transaction->writeConceptSampleIndexMap( clname, conceptSampleIndexMap);
	m_transaction->writeNofConcepts( clname, resultar.size());
}


