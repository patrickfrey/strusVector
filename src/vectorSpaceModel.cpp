/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector space model interface
#include "vectorSpaceModel.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/vectorSpaceModelInstanceInterface.hpp"
#include "strus/vectorSpaceModelBuilderInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/base/fileio.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "databaseAdapter.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "stringList.hpp"
#include "armadillo"
#include <memory>

using namespace strus;
#define MODULENAME   "standard vector space model"

#undef STRUS_LOWLEVEL_DEBUG

class VectorSpaceModelInstance
	:public VectorSpaceModelInstanceInterface
{
public:
	VectorSpaceModelInstance( const DatabaseInterface* database_, const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_database(),m_config(config_,errorhnd_),m_lshmodel(),m_individuals()
	{
		m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
		m_database->checkVersion();
		m_config = m_database->readConfig();
		m_lshmodel = m_database->readLshModel();
		m_individuals = m_database->readResultSimhashVector();

#ifdef STRUS_LOWLEVEL_DEBUG
		std::cout << "config: " << m_config.tostring() << std::endl;
		std::cout << "lsh model: " << std::endl << m_lshmodel.tostring() << std::endl;
		std::cout << "individuals: " << std::endl;
		std::vector<SimHash>::const_iterator ii = m_individuals.begin(), ie = m_individuals.end();
		FeatureIndex fidx = 1;
		for (; ii != ie; ++ii,++fidx)
		{
			std::vector<SampleIndex> members = m_featureSampleIndexMap.getValues( fidx);
			std::cout << ii->tostring() << ": {";
			std::vector<SampleIndex>::const_iterator mi = members.begin(), me = members.end();
			for (std::size_t midx=0; mi != me; ++mi,++midx)
			{
				if (midx) std::cout << ", ";
				std::cout << *mi;
			}
			std::cout << "}" << std::endl;
		}
		std::cout << std::endl;
#endif
	}

	virtual ~VectorSpaceModelInstance()
	{}

	virtual std::vector<Index> mapVectorToConcepts( const std::vector<double>& vec) const
	{
		try
		{
			std::vector<Index> rt;
			SimHash hash( m_lshmodel.simHash( arma::normalise( arma::vec( vec))));
			std::vector<SimHash>::const_iterator ii = m_individuals.begin(), ie = m_individuals.end();
			for (std::size_t iidx=1; ii != ie; ++ii,++iidx)
			{
				if (ii->near( hash, m_config.simdist))
				{
					rt.push_back( iidx);
				}
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping vector to features: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual std::vector<Index> featureConcepts( const Index& index) const
	{
		try
		{
			return m_database->readSampleFeatureIndices( index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' retrieving associated features by sample index: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual std::vector<double> featureVector( const Index& index) const
	{
		try
		{
			return m_database->readSampleVector( index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting sample vector by index: %s"), MODULENAME, *m_errorhnd, std::vector<double>());
	}

	virtual std::string featureName( const Index& index) const
	{
		try
		{
			return m_database->readSampleName( index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting name of sample: %s"), MODULENAME, *m_errorhnd, std::string());
	}

	virtual std::vector<Index> conceptFeatures( const Index& conceptid) const
	{
		try
		{
			return m_database->readFeatureSampleIndices( conceptid);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting associated sample indices by learnt feature: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual unsigned int nofConcepts() const
	{
		return m_individuals.size();
	}

	virtual unsigned int nofFeatures() const
	{
		return m_database->readNofSamples();
	}

	virtual std::string config() const
	{
		try
		{
			return m_config.tostring();
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping configuration to string: %s"), MODULENAME, *m_errorhnd, std::string());
	}

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	VectorSpaceModelConfig m_config;
	LshModel m_lshmodel;
	std::vector<SimHash> m_individuals;
};


class VectorSpaceModelBuilder
	:public VectorSpaceModelBuilderInterface
{
public:
	VectorSpaceModelBuilder( const DatabaseInterface* database_, const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_)
		,m_simrelmap(),m_lshmodel(),m_genmodel(),m_samplear()
	{
		if (database_->exists( m_config.databaseConfig))
		{
			m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
			m_database->checkVersion();
			VectorSpaceModelConfig cfg = m_database->readConfig();
			if (!m_config.isBuildCompatible( cfg))
			{
				throw strus::runtime_error(_TXT("loading vector space model with incompatible configuration"));
			}
			m_lshmodel = m_database->readLshModel();
			m_genmodel = GenModel( m_config.threads, m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.isaf, m_config.with_singletons);

			m_simrelmap = m_database->readSimRelationMap();
			m_samplear = m_database->readSampleSimhashVector();
		}
		else if (database_->createDatabase( m_config.databaseConfig))
		{
			m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
			m_database->writeVersion();
			m_database->writeConfig( m_config);

			m_lshmodel = LshModel( m_config.dim, m_config.bits, m_config.variations);
			m_genmodel = GenModel( m_config.threads, m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.isaf, m_config.with_singletons);
			m_database->writeLshModel( m_lshmodel);
			m_database->commit();
		}
		else
		{
			throw strus::runtime_error(_TXT("failed to create non existing database for vector space model: %s"), m_errorhnd->fetchError());
		}
	}
	virtual ~VectorSpaceModelBuilder()
	{}

	virtual void addFeature( const std::string& name, const std::vector<double>& vec)
	{
		try
		{
			if (m_simrelmap.nofSamples() != 0)
			{
				m_database->deleteSimRelationMap();
				m_database->commit();
			}
			
			SimHash sh( m_lshmodel.simHash( arma::normalise( arma::vec( vec))));
			m_samplear.push_back( sh);
			m_database->writeSample( m_samplear.size(), name, vec, sh);
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error adding sample vector to '%s' builder: %s"), MODULENAME, *m_errorhnd);
	}

	virtual bool commit()
	{
		try
		{
			m_database->commit();
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in vector space model database commit of '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

	void deletePreviousRun()
	{
		m_database->deleteSampleFeatureIndexMap();
		m_database->deleteFeatureSampleIndexMap();
		m_database->deleteResultSimhashVector();
		m_database->commit();
	}

	void storeSimRelationMap()
	{
		m_database->writeVariables( m_samplear.size(), 0);
		if (m_simrelmap.nofSamples() == 0)
		{
			const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
			m_simrelmap = m_genmodel.getSimRelationMap( m_samplear, logfile);
			m_database->writeSimRelationMap( m_simrelmap);
		}
		m_database->commit();
	}

	virtual bool finalize()
	{
		try
		{
			const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
			storeSimRelationMap();
			deletePreviousRun();

			std::vector<SimHash> resultar;
			SampleFeatureIndexMap sampleFeatureIndexMap;
			FeatureSampleIndexMap featureSampleIndexMap;
			
			resultar = m_genmodel.run(
					sampleFeatureIndexMap, featureSampleIndexMap,
					m_samplear, m_simrelmap, logfile);
			m_database->writeResultSimhashVector( resultar);
			m_database->writeSampleFeatureIndexMap( sampleFeatureIndexMap);
			m_database->writeFeatureSampleIndexMap( featureSampleIndexMap);
			m_database->writeVariables( m_samplear.size(), resultar.size());
			m_database->commit();
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cout << "model built:" << std::endl;
			std::vector<SimHash>::const_iterator si = m_samplear.begin(), se = m_samplear.end();
			for (unsigned int sidx=0; si != se; ++si,++sidx)
			{
				std::cout << "sample [" << sidx << "] " << si->tostring() << std::endl;
			}
			std::vector<SimHash>::const_iterator ri = resultar.begin(), re = resultar.end();
			for (unsigned int ridx=0; ri != re; ++ri,++ridx)
			{
				std::cout << "result [" << ridx << "] " << ri->tostring() << std::endl;
			}
			std::cout << "lsh model:" << std::endl << m_lshmodel.tostring() << std::endl;
			std::cout << "gen model:" << std::endl << m_genmodel.tostring() << std::endl;
#endif
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error finalizing '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	VectorSpaceModelConfig m_config;
	SimRelationMap m_simrelmap;
	LshModel m_lshmodel;
	GenModel m_genmodel;
	std::vector<SimHash> m_samplear;
};


VectorSpaceModel::VectorSpaceModel( ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_){}


VectorSpaceModelInstanceInterface* VectorSpaceModel::createInstance( const DatabaseInterface* database, const std::string& config) const
{
	try
	{
		return new VectorSpaceModelInstance( database, config, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' instance: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorSpaceModelBuilderInterface* VectorSpaceModel::createBuilder( const DatabaseInterface* database, const std::string& config) const
{
	try
	{
		return new VectorSpaceModelBuilder( database, config, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' builder: %s"), MODULENAME, *m_errorhnd, 0);
}


