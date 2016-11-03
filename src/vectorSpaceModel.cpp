/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector space model interface
#include "vectorSpaceModel.hpp"
#include "vectorSpaceModelDump.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/vectorSpaceModelInstanceInterface.hpp"
#include "strus/vectorSpaceModelBuilderInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "databaseAdapter.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "simRelationMapBuilder.hpp"
#include "getSimRelationMap.hpp"
#include "getSimhashValues.hpp"
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
	VectorSpaceModelInstance( const std::string& config_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_database(),m_config(config_,errorhnd_),m_lshmodel(),m_individuals()
	{
		m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
		m_database->checkVersion();
		if (m_database->isempty()) throw strus::runtime_error(_TXT("try to open a vector space model that is empty, need to be built first"));
		m_config = m_database->readConfig();
	}

	virtual ~VectorSpaceModelInstance()
	{}

	virtual void preload()
	{
		if (!m_lshmodel.get())
		{
			m_lshmodel.reset( new LshModel( m_database->readLshModel()));
		}
		if (!m_individuals.get())
		{
			m_individuals.reset( new std::vector<SimHash>( m_database->readResultSimhashVector()));
		}
	}

	virtual std::vector<Index> mapVectorToConcepts( const std::vector<double>& vec) const
	{
		try
		{
			if (!m_lshmodel.get())
			{
				m_lshmodel.reset( new LshModel( m_database->readLshModel()));
			}
			if (!m_individuals.get())
			{
				m_individuals.reset( new std::vector<SimHash>( m_database->readResultSimhashVector()));
			}
			std::vector<Index> rt;
			SimHash hash( m_lshmodel->simHash( arma::normalise( arma::vec( vec))));
			std::vector<SimHash>::const_iterator ii = m_individuals->begin(), ie = m_individuals->end();
			for (std::size_t iidx=1; ii != ie; ++ii,++iidx)
			{
				if (ii->near( hash, m_config.simdist))
				{
					rt.push_back( iidx);
				}
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping vector to concepts: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual std::vector<Index> featureConcepts( const Index& index) const
	{
		try
		{
			return m_database->readSampleConceptIndices( index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' retrieving associated concepts by sample index: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
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

	virtual Index featureIndex( const std::string& name) const
	{
		try
		{
			return m_database->readSampleIndex( name);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting index of feature by its name: %s"), MODULENAME, *m_errorhnd, -1);
	}

	virtual std::vector<std::string> attributes( const std::string& name, const Index& index) const
	{
		try
		{
			std::vector<std::string> rt;
			if (utils::caseInsensitiveEquals( name, "featurelsh"))
			{
				if (index < 0) throw strus::runtime_error(_TXT("feature index out of range"));

				SimHash simhash = m_database->readSampleSimhash( index);
				rt.push_back( simhash.tostring());
			}
			else if (utils::caseInsensitiveEquals( name, "conceptlsh"))
			{
				if (!m_individuals.get())
				{
					m_individuals.reset( new std::vector<SimHash>( m_database->readResultSimhashVector()));
				}
				if (index <= 0 || (std::size_t)index > m_individuals->size()) throw strus::runtime_error(_TXT("concept index out of range"));
				rt.push_back( (*m_individuals)[ index-1].tostring());
			}
			else if (utils::caseInsensitiveEquals( name, "simrel"))
			{
				if (index < 0) throw strus::runtime_error(_TXT("feature index out of range"));

				std::vector<SimRelationMap::Element> elems = m_database->readSimRelations( index);
				std::vector<SimRelationMap::Element>::const_iterator ei = elems.begin(), ee = elems.end();
				for (; ei != ee; ++ei)
				{
					std::ostringstream elemstr;
					elemstr << ei->index << " " << ei->simdist;
					rt.push_back( elemstr.str());
				}
			}
			else if (utils::caseInsensitiveEquals( name, "variable"))
			{
				std::vector<std::pair<std::string,uint64_t> > vardefs = m_database->readVariables();
				if (index >= 0)
				{
					if ((std::size_t)index >= vardefs.size()) throw strus::runtime_error(_TXT("variable index out of range"));
					std::ostringstream elemstr;
					elemstr << vardefs[index].first << " " << vardefs[index].second;
					rt.push_back( elemstr.str());
				}
				else
				{
					std::vector<std::pair<std::string,uint64_t> >::const_iterator vi = vardefs.begin(), ve = vardefs.end();
					for (; vi != ve; ++vi)
					{
						std::ostringstream elemstr;
						elemstr << vardefs[index].first << " " << vardefs[index].second;
						rt.push_back( elemstr.str());
					}
				}
			}
			else
			{
				throw strus::runtime_error(_TXT("unknonwn feature attribute name '%s'"), name.c_str());
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting feature attribute names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<std::string> attributeNames() const
	{
		try
		{
			std::vector<std::string> rt;
			rt.push_back( "featurelsh");
			rt.push_back( "conceptlsh");
			rt.push_back( "simrel");
			rt.push_back( "variable");
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting feature attribute names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<Index> conceptFeatures( const Index& conceptid) const
	{
		try
		{
			return m_database->readConceptSampleIndices( conceptid);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting associated feature indices by learnt concept: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual unsigned int nofConcepts() const
	{
		try
		{
			if (!m_individuals.get())
			{
				return m_database->readNofConcepts();
			}
			else
			{
				return m_individuals->size();
			}
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting number of concepts learnt: %s"), MODULENAME, *m_errorhnd, 0);
	}

	virtual unsigned int nofFeatures() const
	{
		return m_database->readNofSamples();
	}

	virtual std::string config() const
	{
		try
		{
			return m_database->config() + ";" + m_config.tostring();
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping configuration to string: %s"), MODULENAME, *m_errorhnd, std::string());
	}

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	VectorSpaceModelConfig m_config;
	mutable Reference<LshModel> m_lshmodel;				//< cached model
	mutable Reference<std::vector<SimHash> > m_individuals;		//< cached concepts
};

class SimRelationImpl
	:public GenModel::SimRelationI
{
public:
	explicit SimRelationImpl( const DatabaseAdapter* database_)
		:database(database_){}
	virtual ~SimRelationImpl(){}
	virtual std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const
	{
		return database->readSimRelations( sidx);
	}
private:
	const DatabaseAdapter* database;
};


class VectorSpaceModelBuilder
	:public VectorSpaceModelBuilderInterface
{
public:
	VectorSpaceModelBuilder( const std::string& config_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_)
		,m_state(0),m_lshmodel(),m_genmodel(),m_samplear()
	{
		m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
		m_database->checkVersion();
		m_state = m_database->readState();
		if (m_database->isempty() || m_state < 1)
		{
			m_database->writeConfig( m_config);
			m_lshmodel = LshModel( m_config.dim, m_config.bits, m_config.variations);
			m_genmodel = GenModel( m_config.threads, m_config.maxdist, m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.isaf, m_config.with_singletons);
			m_database->writeLshModel( m_lshmodel);
			m_database->writeState( m_state = 1);
			m_database->commit();
		}
		else
		{
			VectorSpaceModelConfig cfg = m_database->readConfig();
			if (!m_config.isBuildCompatible( cfg))
			{
				throw strus::runtime_error(_TXT("loading vector space model with incompatible configuration"));
			}
			m_lshmodel = m_database->readLshModel();
			m_genmodel = GenModel( m_config.threads, m_config.maxdist, m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.isaf, m_config.with_singletons);
			m_samplear = m_database->readSampleSimhashVector();
		}
	}
	
	virtual ~VectorSpaceModelBuilder()
	{}

	virtual void addFeature( const std::string& name, const std::vector<double>& vec)
	{
		try
		{
			unsigned int nofFeaturesAdded = 0;
			{
				utils::ScopedLock lock( m_mutex);
				resetSimRelationMap();
				m_vecar.push_back( vec);
				m_namear.push_back( name);
				nofFeaturesAdded = m_vecar.size();
			}
			if (m_config.commitsize && nofFeaturesAdded >= m_config.commitsize)
			{
				if (!commit())
				{
					throw strus::runtime_error(_TXT("autocommit failed after %u operations"), m_config.commitsize);
				}
			}
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error adding sample vector to '%s' builder: %s"), MODULENAME, *m_errorhnd);
	}

	virtual bool commit()
	{
		try
		{
			utils::ScopedLock lock( m_mutex);
			std::vector<SimHash> shar = strus::getSimhashValues( m_lshmodel, m_vecar, m_config.threads);
			std::size_t si = 0, se = m_vecar.size();
			for (; si != se; ++si)
			{
				m_database->writeSample( m_samplear.size() + si, m_namear[si], m_vecar[si], shar[si]);
			}
			m_samplear.insert( m_samplear.end(), shar.begin(), shar.end());
			m_database->writeNofSamples( m_samplear.size());
			m_database->commit();

			m_vecar.clear();
			m_namear.clear();
			if (!m_config.logfile.empty())
			{
				Logger logout( m_config.logfile.c_str());
				logout << string_format( _TXT("inserted %u features"), m_samplear.size());
			}
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in vector space model database commit of '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

	bool needToCalculateSimRelationMap()
	{
		return (m_state < 2);
	}

	void resetSimRelationMap()
	{
		if (m_state >= 2)
		{
			m_state = 1;
			m_database->writeState( m_state);
			m_database->deleteSimRelationMap();
			m_database->commit();
		}
	}

	void buildSimRelationMap()
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		Logger logout( logfile);

		m_database->deleteSimRelationMap();
		m_database->writeState( 1);
		m_database->commit();

		if (logout) logout << string_format( _TXT("calculate similarity relation matrix for %u features"), m_samplear.size());
		if (m_config.with_probsim)
		{
			if (logout) logout << string_format( _TXT("use probabilistic prediction of similarity as prefilter"));
			uint64_t total_nof_similarities = 0;
			SimRelationMapBuilder simrelbuilder( m_samplear, m_config.maxdist, m_config.maxsimsam, m_config.rndsimsam, m_config.threads);
			SimRelationMap simrelmap_part;
			while (simrelbuilder.getNextSimRelationMap( simrelmap_part))
			{
				m_database->writeSimRelationMap( simrelmap_part);
				m_database->commit();
				total_nof_similarities += simrelmap_part.nofRelationsDetected();
				if (logout) logout << string_format( _TXT("got total %u features with %uK similarities"), simrelmap_part.endIndex(), (unsigned int)(total_nof_similarities/1024));
			}
		}
		else
		{
			std::size_t si = 0, se = m_samplear.size();
			uint64_t total_nof_similarities = 0;
			for (; si < se; si += m_config.commitsize)
			{
				std::size_t chunkend = si + m_config.commitsize;
				if (chunkend > se) chunkend = se;
				SimRelationMap simrelmap_part = strus::getSimRelationMap( m_samplear, si,  chunkend, m_config.maxdist, m_config.maxsimsam, m_config.rndsimsam, m_config.threads);
				m_database->writeSimRelationMap( simrelmap_part);
				m_database->commit();
				total_nof_similarities += simrelmap_part.nofRelationsDetected();
				if (logout) logout << string_format( _TXT("got total %u features with %uK similarities"), chunkend, (unsigned int)(total_nof_similarities/1024));
			}
		}
		m_database->writeState( 2);
		m_database->commit();
		if (logout) logout << string_format( _TXT("calculated similarity relation matrix stored to database"));
	}

	virtual bool finalize()
	{
		try
		{
			utils::ScopedLock lock( m_mutex);
			m_database->commit();
			if (needToCalculateSimRelationMap())
			{
				buildSimRelationMap();
			}
			m_database->deleteSampleConceptIndexMap();
			m_database->deleteConceptSampleIndexMap();
			m_database->deleteResultSimhashVector();
			m_database->commit();

			std::vector<SimHash> resultar;
			SampleConceptIndexMap sampleConceptIndexMap;
			ConceptSampleIndexMap conceptSampleIndexMap;

			const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
			SimRelationImpl simrelmap( m_database.get());
			resultar = m_genmodel.run(
					sampleConceptIndexMap, conceptSampleIndexMap,
					m_samplear, simrelmap, logfile);
			m_database->writeResultSimhashVector( resultar);
			m_database->writeSampleConceptIndexMap( sampleConceptIndexMap);
			m_database->writeConceptSampleIndexMap( conceptSampleIndexMap);
			m_database->writeNofSamples( m_samplear.size());
			m_database->writeNofConcepts( resultar.size());
			m_database->writeState( 3);
			m_database->commit();
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error finalizing '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	VectorSpaceModelConfig m_config;
	unsigned int m_state;
	LshModel m_lshmodel;
	GenModel m_genmodel;
	utils::Mutex m_mutex;
	std::vector<SimHash> m_samplear;
	std::vector<std::vector<double> > m_vecar;
	std::vector<std::string> m_namear;
};


VectorSpaceModel::VectorSpaceModel( ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_){}


bool VectorSpaceModel::createRepository( const std::string& configsource, const DatabaseInterface* dbi) const
{
	try
	{
		VectorSpaceModelConfig config( configsource, m_errorhnd);
		if (dbi->createDatabase( config.databaseConfig))
		{
			DatabaseAdapter database( dbi, config.databaseConfig, m_errorhnd);
			database.writeVersion();
			database.commit();
		}
		else
		{
			throw strus::runtime_error(_TXT("failed to create repository for vector space model: %s"), m_errorhnd->fetchError());
		}
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' repository: %s"), MODULENAME, *m_errorhnd, false);
}

bool VectorSpaceModel::resetRepository( const std::string& configsrc, const DatabaseInterface* dbi) const
{
	try
	{
		VectorSpaceModelConfig config( configsrc, m_errorhnd);
		DatabaseAdapter database( dbi, config.databaseConfig, m_errorhnd);
		database.clear();
		database.commit();
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("failed to reset '%s' repository: %s"), MODULENAME, *m_errorhnd, false);
}


VectorSpaceModelInstanceInterface* VectorSpaceModel::createInstance( const std::string& configsrc, const DatabaseInterface* database) const
{
	try
	{
		return new VectorSpaceModelInstance( configsrc, database, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' instance: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorSpaceModelBuilderInterface* VectorSpaceModel::createBuilder( const std::string& configsrc, const DatabaseInterface* database) const
{
	try
	{
		return new VectorSpaceModelBuilder( configsrc, database, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' builder: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorSpaceModelDumpInterface* VectorSpaceModel::createDump( const std::string& configsource, const DatabaseInterface* database, const std::string& keyprefix) const
{
	try
	{
		return new VectorSpaceModelDump( database, configsource, keyprefix, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' builder: %s"), MODULENAME, *m_errorhnd, 0);
}

