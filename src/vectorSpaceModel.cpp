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
#include "simRelationReader.hpp"
#include "simRelationMapBuilder.hpp"
#include "getSimhashValues.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "stringList.hpp"
#include "armadillo"
#include <memory>

using namespace strus;
#define MODULENAME   "standard vector space model"

#undef STRUS_LOWLEVEL_DEBUG

#define MAIN_CONCEPT_CLASSNAME ""

class VectorSpaceModelInstance
	:public VectorSpaceModelInstanceInterface
{
public:
	typedef Reference<std::vector<SimHash> > SimHashVectorReference;

	VectorSpaceModelInstance( const std::string& config_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_database(),m_config(config_,errorhnd_),m_lshmodel(),m_individuals()
	{
		m_conceptClassMap[ MAIN_CONCEPT_CLASSNAME] = m_individuals.size();
		m_individuals.push_back( SimHashVectorReference());
		m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
		m_database->checkVersion();
		if (m_database->isempty()) throw strus::runtime_error(_TXT("try to open a vector space model that is empty, need to be built first"));
		m_config = m_database->readConfig();

		std::vector<std::string> clnames = m_database->readConceptClassNames();
		std::vector<std::string>::const_iterator ci = clnames.begin(), ce = clnames.end();
		for (; ci != ce; ++ci)
		{
			m_conceptClassMap[ *ci] = m_individuals.size();
			m_individuals.push_back( SimHashVectorReference());
		}
	}

	virtual ~VectorSpaceModelInstance()
	{}

	void loaddata()
	{
		if (!m_lshmodel.get())
		{
			m_lshmodel.reset( new LshModel( m_database->readLshModel()));
		}
		conceptClassMap::const_iterator ci = m_conceptClassMap.begin(), ce = m_conceptClassMap.end();
		for (; ci != ce; ++ci)
		{
			SimHashVectorReference& ivec = m_individuals[ci->second];
			if (!ivec.get())
			{
				ivec.reset( new std::vector<SimHash>( m_database->readConceptSimhashVector( ci->first)));
			}
		}
	}

	const SimHashVectorReference& loadSimHashVectorRef( const std::string& conceptClass) const
	{
		if (!m_lshmodel.get())
		{
			m_lshmodel.reset( new LshModel( m_database->readLshModel()));
		}
		conceptClassMap::const_iterator ci = m_conceptClassMap.find( conceptClass);
		if (ci == m_conceptClassMap.end()) throw strus::runtime_error(_TXT("concept class identifier '%s' unknown"), conceptClass.c_str());
		SimHashVectorReference& ivec = m_individuals[ ci->second];
		if (!ivec.get())
		{
			ivec.reset( new std::vector<SimHash>( m_database->readConceptSimhashVector( ci->first)));
		}
		return ivec;
	}

	virtual void preload()
	{
		try
		{
			loaddata();
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error in instance of '%s' when pre-loading data: %s"), MODULENAME, *m_errorhnd);
	}

	virtual std::vector<std::string> conceptClassNames() const
	{
		try
		{
			std::vector<std::string> rt;
			rt.push_back( MAIN_CONCEPT_CLASSNAME);
			std::vector<std::string> dbclnames = m_database->readConceptClassNames();
			rt.insert( rt.end(), dbclnames.begin(), dbclnames.end());
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting the list of implemented concept class names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<Index> mapVectorToConcepts( const std::string& conceptClass, const std::vector<double>& vec) const
	{
		try
		{
			const SimHashVectorReference& ivec = loadSimHashVectorRef( conceptClass);
			const GenModelConfig& gencfg = m_config.genModelConfig( conceptClass);
			std::vector<Index> rt;
			SimHash hash( m_lshmodel->simHash( arma::normalise( arma::vec( vec))));
			std::vector<SimHash>::const_iterator ii = ivec->begin(), ie = ivec->end();
			for (std::size_t iidx=1; ii != ie; ++ii,++iidx)
			{
				if (ii->near( hash, gencfg.simdist))
				{
					rt.push_back( iidx);
				}
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping vector to concepts: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual std::vector<Index> featureConcepts( const std::string& conceptClass, const Index& index) const
	{
		try
		{
			return m_database->readSampleConceptIndices( conceptClass, index);
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
			if (utils::caseInsensitiveEquals( name, "featlsh"))
			{
				if (index < 0) throw strus::runtime_error(_TXT("feature index out of range"));

				SimHash simhash = m_database->readSampleSimhash( index);
				rt.push_back( simhash.tostring());
			}
			else if (utils::caseInsensitiveStartsWith( name, "conlsh_"))
			{
				std::string clname( name.c_str() + 7/*strlen("conlsh_")*/);
				conceptClassMap::const_iterator ci = m_conceptClassMap.find( clname);
				if (ci == m_conceptClassMap.end()) return rt;
				SimHashVectorReference& ivec = m_individuals[ci->second];
				if (!ivec.get())
				{
					ivec.reset( new std::vector<SimHash>( m_database->readConceptSimhashVector( ci->first)));
				}
				if (index <= 0 || (std::size_t)index > ivec->size()) throw strus::runtime_error(_TXT("concept index out of range"));
				rt.push_back( (*ivec)[ index-1].tostring());
			}
			else if (utils::caseInsensitiveStartsWith( name, "singletons_"))
			{
				std::string clname( name.c_str() + 11/*strlen("singletons_")*/);
				std::vector<SampleIndex> res = m_database->readConceptSingletons( clname);
				std::vector<SampleIndex>::const_iterator si = res.begin(), se = res.end();
				for (; si != se; ++si)
				{
					std::ostringstream elemstr;
					elemstr << *si << " " << m_database->readSampleName( *si);
					rt.push_back( elemstr.str());
				}
			}
			else if (utils::caseInsensitiveEquals( name, "simrel"))
			{
				if (index >= 0)
				{
					std::vector<SimRelationMap::Element> elems = m_database->readSimRelations( index);
					std::vector<SimRelationMap::Element>::const_iterator ei = elems.begin(), ee = elems.end();
					for (; ei != ee; ++ei)
					{
						std::ostringstream elemstr;
						elemstr << ei->index << " " << ei->simdist;
						rt.push_back( elemstr.str());
					}
				}
				else
				{
					SampleIndex si = 0, se = m_database->readNofSamples();
					for (; si != se; ++si)
					{
						std::vector<SimRelationMap::Element> elems = m_database->readSimRelations( si);
						std::vector<SimRelationMap::Element>::const_iterator ei = elems.begin(), ee = elems.end();
						for (; ei != ee; ++ei)
						{
							std::ostringstream elemstr;
							elemstr << si << " " << ei->index << " " << ei->simdist;
							rt.push_back( elemstr.str());
						}
					}
				}
			}
			else if (utils::caseInsensitiveEquals( name, "simsingletons"))
			{
				std::vector<SampleIndex> res = m_database->readSimSingletons();
				std::vector<SampleIndex>::const_iterator si = res.begin(), se = res.end();
				for (; si != se; ++si)
				{
					std::ostringstream elemstr;
					elemstr << *si << " " << m_database->readSampleName( *si);
					rt.push_back( elemstr.str());
				}
			}
			else if (utils::caseInsensitiveEquals( name, "nofsimrel"))
			{
				std::ostringstream elemstr;
				SampleIndex sidx = m_database->readLastSimRelationIndex();
				SampleIndex nofSamples = m_database->readNofSimRelations();
				elemstr << sidx << " " << nofSamples;
				rt.push_back( elemstr.str());
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
						elemstr << vi->first << " " << vi->second;
						rt.push_back( elemstr.str());
					}
				}
			}
			else if (utils::caseInsensitiveEquals( name, "state"))
			{
				std::ostringstream elemstr;
				elemstr << m_database->readState();
				rt.push_back( elemstr.str());
			}
			else
			{
				throw strus::runtime_error(_TXT("unknonwn feature attribute name '%s'"), name.c_str());
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting attributes: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<std::string> attributeNames() const
	{
		try
		{
			std::vector<std::string> rt;
			rt.push_back( "featlsh");
			conceptClassMap::const_iterator ci = m_conceptClassMap.begin(), ce = m_conceptClassMap.end();
			for (; ci != ce; ++ci)
			{
				rt.push_back( std::string("conlsh_") + ci->first);
				rt.push_back( std::string("singletons_") + ci->first);
			}
			rt.push_back( "simrel");
			rt.push_back( "simsingletons");
			rt.push_back( "nofsimrel");
			rt.push_back( "variable");
			rt.push_back( "state");
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting feature attribute names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<Index> conceptFeatures( const std::string& conceptClass, const Index& conceptid) const
	{
		try
		{
			return m_database->readConceptSampleIndices( conceptClass, conceptid);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting associated feature indices by learnt concept: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual unsigned int nofConcepts( const std::string& conceptClass) const
	{
		try
		{
			conceptClassMap::const_iterator ci = m_conceptClassMap.find( conceptClass);
			if (ci == m_conceptClassMap.end()) return 0;
			if (!m_individuals[ ci->second].get())
			{
				return m_database->readNofConcepts( conceptClass);
			}
			else
			{
				return m_individuals[ ci->second]->size();
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
			return m_config.tostring();
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping configuration to string: %s"), MODULENAME, *m_errorhnd, std::string());
	}

private:
	ErrorBufferInterface* m_errorhnd;
	Reference<DatabaseAdapter> m_database;
	VectorSpaceModelConfig m_config;
	mutable Reference<LshModel> m_lshmodel;				//< cached model
	mutable std::vector<SimHashVectorReference> m_individuals;	//< cached concept vectors
	typedef std::map<std::string,std::size_t> conceptClassMap;
	conceptClassMap m_conceptClassMap;				//< maps names of concept classes to the associated individuals
};

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


class VectorSpaceModelBuilder
	:public VectorSpaceModelBuilderInterface
{
public:
	VectorSpaceModelBuilder( const std::string& config_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_dbi(database_),m_config(config_,errorhnd_)
		,m_state(0),m_needToCalculateSimRelationMap(false),m_haveFeaturesAdded(false)
		,m_lshmodel(),m_genmodelmain(),m_genmodelmap(),m_samplear()
	{
		m_database.reset( new DatabaseAdapter( database_, m_config.databaseConfig, m_errorhnd));
		m_database->checkVersion();
		m_state = m_database->readState();
		if (m_database->isempty() || m_state < 1)
		{
			m_database->writeConfig( m_config);
			m_lshmodel = LshModel( m_config.dim, m_config.bits, m_config.variations);
			m_genmodelmain = GenModel( m_config.threads, m_config.maxdist, m_config.gencfg.simdist, m_config.gencfg.raddist, m_config.gencfg.eqdist, m_config.gencfg.mutations, m_config.gencfg.votes, m_config.gencfg.descendants, m_config.gencfg.maxage, m_config.gencfg.iterations, m_config.gencfg.assignments, m_config.gencfg.isaf, m_config.gencfg.eqdiff, m_config.gencfg.with_singletons, m_errorhnd);
			GenModelConfigMap::const_iterator mi = m_config.altgenmap.begin(), me = m_config.altgenmap.end();
			for (; mi != me; ++mi)
			{
				m_genmodelmap[ mi->first] = GenModel( m_config.threads, m_config.maxdist, mi->second.simdist, mi->second.raddist, mi->second.eqdist, mi->second.mutations, mi->second.votes, mi->second.descendants, mi->second.maxage, mi->second.iterations, mi->second.assignments, mi->second.isaf, mi->second.eqdiff, mi->second.with_singletons, m_errorhnd);
			}
			m_database->writeLshModel( m_lshmodel);
			m_database->writeState( m_state = 1);
			m_database->commit();
		}
		else
		{
			VectorSpaceModelConfig cfg = m_database->readConfig();
			m_config = VectorSpaceModelConfig( config_, errorhnd_, cfg);
			if (!m_config.isBuildCompatible( cfg))
			{
				throw strus::runtime_error(_TXT("loading vector space model with incompatible configuration"));
			}
			if (m_config.maxdist > cfg.maxdist)
			{
				m_needToCalculateSimRelationMap = true;
			}
			m_lshmodel = m_database->readLshModel();
			m_genmodelmain = GenModel( m_config.threads, m_config.maxdist, m_config.gencfg.simdist, m_config.gencfg.raddist, m_config.gencfg.eqdist, m_config.gencfg.mutations, m_config.gencfg.votes, m_config.gencfg.descendants, m_config.gencfg.maxage, m_config.gencfg.iterations, m_config.gencfg.assignments, m_config.gencfg.isaf, m_config.gencfg.eqdiff, m_config.gencfg.with_singletons, m_errorhnd);
			GenModelConfigMap::const_iterator mi = m_config.altgenmap.begin(), me = m_config.altgenmap.end();
			for (; mi != me; ++mi)
			{
				m_genmodelmap[ mi->first] = GenModel( m_config.threads, m_config.maxdist, mi->second.simdist, mi->second.raddist, mi->second.eqdist, mi->second.mutations, mi->second.votes, mi->second.descendants, mi->second.maxage, mi->second.iterations, mi->second.assignments, mi->second.isaf, mi->second.eqdiff, mi->second.with_singletons, m_errorhnd);
			}
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
				if (m_vecar.size() < m_config.maxfeatures)
				{
					m_vecar.push_back( vec);
					m_namear.push_back( name);
					nofFeaturesAdded = m_vecar.size();
				}
				else
				{
					return;
				}
			}
			if (m_config.commitsize && nofFeaturesAdded >= m_config.commitsize)
			{
				if (!done())
				{
					throw strus::runtime_error(_TXT("autocommit failed after %u operations"), m_config.commitsize);
				}
			}
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error adding sample vector to '%s' builder: %s"), MODULENAME, *m_errorhnd);
	}

	virtual bool done()
	{
		try
		{
			utils::ScopedLock lock( m_mutex);
			std::vector<SimHash> shar = strus::getSimhashValues( m_lshmodel, m_vecar, m_config.threads, m_errorhnd);
			std::size_t si = 0, se = m_vecar.size();
			for (; si != se; ++si)
			{
				m_database->writeSample( m_samplear.size() + si, m_namear[si], m_vecar[si], shar[si]);
			}
			m_samplear.insert( m_samplear.end(), shar.begin(), shar.end());
			m_database->writeNofSamples( m_samplear.size());
			m_database->commit();

			m_haveFeaturesAdded |= (m_vecar.size() != 0);
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

	virtual bool run( const std::string& command)
	{
		try
		{
			if (command.empty() || utils::caseInsensitiveEquals( command, "finalize"))
			{
				finalize();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "rebase"))
			{
				rebase();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "base"))
			{
				buildSimilarityRelationMap();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "learn"))
			{
				learnConcepts();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "condep"))
			{
				buildConceptDependencies();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "clear"))
			{
				m_database->clear();
				m_database->commit();
				return true;
			}
			else
			{
				throw strus::runtime_error(_TXT("unknown command '%s'"), command.c_str());
			}
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error executing command of '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

private:
	void rebase()
	{
		utils::ScopedLock lock( m_mutex);
		m_database->commit();
		SimRelationNeighbourReader nbreader( m_database.get(), &m_samplear, m_config.maxdist);
		buildSimRelationMap( &nbreader);
	}

	void buildSimilarityRelationMap()
	{
		utils::ScopedLock lock( m_mutex);
		m_database->commit();
		buildSimRelationMap( 0/*without SimRelationNeighbourReader*/);
	}

	void learnConcepts()
	{
		utils::ScopedLock lock( m_mutex);
		m_database->commit();

		m_database->deleteSampleConceptIndexMaps();
		m_database->deleteConceptSampleIndexMaps();
		m_database->deleteConceptDependencies();
		m_database->deleteConceptSimhashVectors();
		m_database->deleteConceptClassNames();
		m_database->commit();

		std::vector<std::string> clnames;
		run_genmodel( MAIN_CONCEPT_CLASSNAME, m_genmodelmain);
		GenModelMap::const_iterator mi = m_genmodelmap.begin(), me = m_genmodelmap.end();
		for (; mi != me; ++mi)
		{
			run_genmodel( mi->first, mi->second);
			clnames.push_back( mi->first);
		}
		m_database->writeNofSamples( m_samplear.size());
		m_database->writeConceptClassNames( clnames);
		m_database->writeState( 3);
		m_database->commit();
		buildConceptDependencies();
		m_database->writeState( 4);
		m_database->commit();
	}

	void finalize()
	{
		{
			utils::ScopedLock lock( m_mutex);
			m_database->commit();
			if (needToCalculateSimRelationMap())
			{
				buildSimRelationMap( 0/*without SimRelationNeighbourReader*/);
			}
		}
		learnConcepts();
	}

	void buildConceptDependencies()
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		Logger logout( logfile);
		if (logout) logout << _TXT("writing concept class dependencies");
		std::vector<VectorSpaceModelConfig::ConceptClassDependency>::const_iterator
			ci = m_config.conceptClassDependecies.begin(), ce = m_config.conceptClassDependecies.end();
		for (; ci != ce; ++ci)
		{
			writeDependencyLinks( ci->first, ci->second);
			m_database->commit();
		}
	}

	bool needToCalculateSimRelationMap()
	{
		unsigned int nofSamples = m_database->readNofSamples();
		unsigned int simNofSamples = m_database->readNofSimRelations();
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

	void buildSimRelationMap( const SimRelationReader* simmapreader)
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		Logger logout( logfile);

		m_database->writeState( 1);
		m_database->commit();

		if (logout) logout << string_format( _TXT("calculate similarity relation matrix for %u features (selection %s)"), m_samplear.size(), m_config.with_probsim?_TXT("probabilistic selection"):_TXT("try all"));
		uint64_t total_nof_similarities = 0;
		SampleIndex startsampleidx = m_haveFeaturesAdded?0:m_database->readLastSimRelationIndex();
		SimRelationMapBuilder simrelbuilder( m_samplear, startsampleidx, m_config.maxdist, m_config.maxsimsam, m_config.rndsimsam, m_config.threads, m_config.with_probsim, logout, simmapreader);
		SimRelationMap simrelmap_part;
		while (simrelbuilder.getNextSimRelationMap( simrelmap_part))
		{
			m_database->writeSimRelationMap( simrelmap_part);
			m_database->commit();
			total_nof_similarities += simrelmap_part.nofRelationsDetected();
			if (logout) logout << string_format( _TXT("got total %u features with %uK similarities"), simrelmap_part.endIndex(), (unsigned int)(total_nof_similarities/1024));
		}
		m_database->writeNofSimRelations( m_samplear.size());
		m_database->writeState( 2);
		m_database->commit();
		if (logout) logout << string_format( _TXT("calculated similarity relation matrix stored to database"));
	}

	void run_genmodel( const std::string& clname, const GenModel& genmodel)
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		SimRelationMapReader simrelmap( m_database.get());
		std::vector<SampleIndex> singletons;
		SampleConceptIndexMap sampleConceptIndexMap;
		ConceptSampleIndexMap conceptSampleIndexMap;

		std::vector<SimHash> resultar = genmodel.run(
				singletons, sampleConceptIndexMap, conceptSampleIndexMap,
				clname, m_samplear, m_config.maxconcepts, simrelmap, m_config.threads, logfile);

		m_database->writeConceptSimhashVector( clname, resultar);
		m_database->writeSampleConceptIndexMap( clname, sampleConceptIndexMap);
		m_database->writeConceptSampleIndexMap( clname, conceptSampleIndexMap);
		m_database->writeNofConcepts( clname, resultar.size());
	}

	std::vector<ConceptIndex> translateConcepts( const std::string& from_clname, ConceptIndex from_index, const std::string& to_clname)
	{
		std::set<ConceptIndex> to_conceptset;
		std::vector<SampleIndex> from_members = m_database->readConceptSampleIndices( from_clname, from_index);
		std::vector<SampleIndex>::const_iterator mi = from_members.begin(), me = from_members.end();
		for (; mi != me; ++mi)
		{
			std::vector<ConceptIndex> to_concepts = m_database->readSampleConceptIndices( to_clname, *mi);
			std::vector<ConceptIndex>::const_iterator
				vi = to_concepts.begin(), ve = to_concepts.end();
			for (; vi != ve; ++vi)
			{
				to_conceptset.insert( *vi);
			}
		}
		return std::vector<ConceptIndex>( to_conceptset.begin(), to_conceptset.end());
	}

	void writeDependencyLinks( const std::string& from_clname, const std::string& to_clname)
	{
		unsigned int ci = 0, ce = m_database->readNofConcepts( from_clname);
		for (unsigned int cidx=0; ci != ce; ++ci,++cidx)
		{
			m_database->writeConceptDependencies( from_clname, ci, to_clname, translateConcepts( from_clname, ci, to_clname));
			if (cidx && cidx % m_config.commitsize == 0)
			{
				m_database->commit();
			}
		}
		m_database->commit();
	}

private:
	typedef std::map<std::string,GenModel> GenModelMap;

private:
	ErrorBufferInterface* m_errorhnd;
	const DatabaseInterface* m_dbi;
	Reference<DatabaseAdapter> m_database;
	VectorSpaceModelConfig m_config;
	unsigned int m_state;
	bool m_needToCalculateSimRelationMap;
	bool m_haveFeaturesAdded;
	LshModel m_lshmodel;
	GenModel m_genmodelmain;
	GenModelMap m_genmodelmap;
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

std::vector<std::string> VectorSpaceModel::builderCommands() const
{
	try
	{
		std::vector<std::string> rt;
		rt.push_back( "base");
		rt.push_back( "rebase");
		rt.push_back( "learn");
		rt.push_back( "finalize");
		rt.push_back( "clear");
		rt.push_back( "condep");
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error getting list of commands of '%s' builder: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
}

std::string VectorSpaceModel::builderCommandDescription( const std::string& command) const
{
	try
	{
		if (command.empty() || utils::caseInsensitiveEquals( command, "finalize"))
		{
			return _TXT("Calculate sparse similarity relation matrix if needed (base) and do learn concepts.");
		}
		else if (utils::caseInsensitiveEquals( command, "learn"))
		{
			return _TXT("Learn concepts.");
		}
		else if (utils::caseInsensitiveEquals( command, "base"))
		{
			return _TXT("Calculate sparse similarity relation matrix that is the base datastructure for concept learning.");
		}
		else if (utils::caseInsensitiveEquals( command, "rebase"))
		{
			return _TXT("Recalculate sparse similarity relation matrix that is the base datastructure for concept learning taking data created during the learning step into account. Makes only sense if the similarity relation matrix was calculated with heuristics (configuration probsim=yes).");
		}
		else if (utils::caseInsensitiveEquals( command, "condep"))
		{
			return _TXT("Recalculate concept dependencies.");
		}
		else if (utils::caseInsensitiveEquals( command, "clear"))
		{
			return _TXT("Reset the database, delete all vectors inserted and data accumulated by the builder.");
		}
		else
		{
			throw strus::runtime_error(_TXT("unknown command '%s'"), command.c_str());
		}
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error getting description of command of '%s' builder: %s"), MODULENAME, *m_errorhnd, std::string());
}

