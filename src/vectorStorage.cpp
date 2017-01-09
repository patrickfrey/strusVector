/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage interface
#include "vectorStorage.hpp"
#include "vectorStorageDump.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageBuilderInterface.hpp"
#include "strus/vectorStorageSearchInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include "vectorStorageConfig.hpp"
#include "databaseAdapter.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "simHash.hpp"
#include "simHashMap.hpp"
#include "simRelationMap.hpp"
#include "simRelationReader.hpp"
#include "simRelationMapBuilder.hpp"
#include "getSimhashValues.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "stringList.hpp"
#include "armadillo"
#include <memory>
#include <limits>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using namespace strus;
#define MODULENAME   "standard vector storage"

#undef STRUS_LOWLEVEL_DEBUG

#define MAIN_CONCEPT_CLASSNAME ""


class VectorStorageSearch
	:public VectorStorageSearchInterface
{
public:
	VectorStorageSearch( const DatabaseAdapter& database, const VectorStorageConfig& config_, const Index& range_from_, const Index& range_to_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_)
		,m_config(config_)
		,m_lshmodel(database.readLshModel())
		,m_samplear(database.readSampleSimhashVector(range_from_,range_to_),1/*prob select random seed*/)
		,m_range_from(range_from_)
		,m_range_to(range_to_)
	{
		if (m_config.with_realvecweights)
		{
			m_database = boost::make_shared<DatabaseAdapter>( database);
		}
	}

	virtual ~VectorStorageSearch(){}

	virtual std::vector<Result> findSimilar( const std::vector<double>& vec, unsigned int maxNofResults) const
	{
		try
		{
			if (m_config.with_realvecweights && m_database.get())
			{
				std::vector<Result> rt;
				arma::vec vv = arma::vec( vec);
				SimHash hash( m_lshmodel.simHash( arma::normalise( vv)));
				if (m_config.gencfg.probdist)
				{
					rt = m_samplear.findSimilar( hash, m_config.maxdist, m_config.maxdist * m_config.gencfg.probdist / m_config.gencfg.simdist, maxNofResults * 2 + 10, m_range_from);
				}
				else
				{
					rt = m_samplear.findSimilar( hash, m_config.maxdist, maxNofResults * 2 + 10, m_range_from);
				}
				std::vector<Result>::iterator ri = rt.begin(), re = rt.end();
				for (; ri != re; ++ri)
				{
					arma::vec resvv( m_database->readSampleVector( ri->featidx()));
					ri->setWeight( arma::norm_dot( vv, resvv));
				}
				std::sort( rt.begin(), rt.end(), std::greater<strus::VectorStorageSearchInterface::Result>());
				rt.resize( maxNofResults);
				return rt;
			}
			else
			{
				SimHash hash( m_lshmodel.simHash( arma::normalise( arma::vec( vec))));
				if (m_config.gencfg.probdist)
				{
					return m_samplear.findSimilar( hash, m_config.maxdist, m_config.maxdist * m_config.gencfg.probdist / m_config.gencfg.simdist, maxNofResults, m_range_from);
				}
				else
				{
					return m_samplear.findSimilar( hash, m_config.maxdist, maxNofResults, m_range_from);
				}
			}
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in similar vector search of '%s': %s"), MODULENAME, *m_errorhnd, std::vector<Result>());
	}

private:
	ErrorBufferInterface* m_errorhnd;
	VectorStorageConfig m_config;
	LshModel m_lshmodel;
	SimHashMap m_samplear;
	boost::shared_ptr<DatabaseAdapter> m_database;
	Index m_range_from;
	Index m_range_to;
};


class VectorStorageClient
	:public VectorStorageClientInterface
{
public:
	VectorStorageClient( const VectorStorageConfig& config_, const std::string& configstr_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_database(database_,config_.databaseConfig,errorhnd_),m_config(config_)
	{
		m_database.checkVersion();
		if (m_database.isempty()) throw strus::runtime_error(_TXT("try to open a vector storage that is empty, need to be built first"));
		VectorStorageConfig cfg = m_database.readConfig();
		m_config = VectorStorageConfig( configstr_, errorhnd_, cfg);
	}

	virtual ~VectorStorageClient()
	{}

	virtual VectorStorageSearchInterface* createSearcher( const Index& range_from, const Index& range_to) const
	{
		try
		{
			return new VectorStorageSearch( m_database, m_config, range_from, range_to, m_errorhnd);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' creating searcher: %s"), MODULENAME, *m_errorhnd, 0);
	}

	virtual std::vector<std::string> conceptClassNames() const
	{
		try
		{
			std::vector<std::string> rt;
			rt.push_back( MAIN_CONCEPT_CLASSNAME);
			std::vector<std::string> dbclnames = m_database.readConceptClassNames();
			rt.insert( rt.end(), dbclnames.begin(), dbclnames.end());
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the list of implemented concept class names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}


	virtual std::vector<Index> featureConcepts( const std::string& conceptClass, const Index& index) const
	{
		try
		{
			return m_database.readSampleConceptIndices( conceptClass, index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' retrieving associated concepts by sample index: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual std::vector<double> featureVector( const Index& index) const
	{
		try
		{
			return m_database.readSampleVector( index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting sample vector by index: %s"), MODULENAME, *m_errorhnd, std::vector<double>());
	}

	virtual std::string featureName( const Index& index) const
	{
		try
		{
			return m_database.readSampleName( index);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting name of sample: %s"), MODULENAME, *m_errorhnd, std::string());
	}

	virtual Index featureIndex( const std::string& name) const
	{
		try
		{
			return m_database.readSampleIndex( name);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting index of feature by its name: %s"), MODULENAME, *m_errorhnd, -1);
	}

	virtual std::vector<std::string> featureAttributes( const std::string& name, const Index& index) const
	{
		try
		{
			std::vector<std::string> rt;
			if (utils::caseInsensitiveEquals( name, "lsh"))
			{
				if (index < 0) throw strus::runtime_error(_TXT("feature index out of range"));

				SimHash simhash = m_database.readSampleSimhash( index);
				rt.push_back( simhash.tostring());
			}
			else if (utils::caseInsensitiveEquals( name, "simrel"))
			{
				if (index >= 0)
				{
					std::vector<SimRelationMap::Element> elems = m_database.readSimRelations( index);
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
					SampleIndex si = 0, se = m_database.readNofSamples();
					for (; si != se; ++si)
					{
						std::vector<SimRelationMap::Element> elems = m_database.readSimRelations( si);
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
				std::vector<SampleIndex> res = m_database.readSimSingletons();
				std::vector<SampleIndex>::const_iterator si = res.begin(), se = res.end();
				for (; si != se; ++si)
				{
					std::ostringstream elemstr;
					elemstr << *si << " " << m_database.readSampleName( *si);
					rt.push_back( elemstr.str());
				}
			}
			else if (utils::caseInsensitiveEquals( name, "nofsimrel"))
			{
				std::ostringstream elemstr;
				SampleIndex sidx = m_database.readLastSimRelationIndex();
				SampleIndex nofSamples = m_database.readNofSimRelations();
				elemstr << sidx << " " << nofSamples;
				rt.push_back( elemstr.str());
			}
			else
			{
				throw strus::runtime_error(_TXT("unknonwn feature attribute name '%s'"), name.c_str());
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting feature attribute: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<std::string> featureAttributeNames() const
	{
		try
		{
			std::vector<std::string> rt;
			rt.push_back( "lsh");
			rt.push_back( "simrel");
			rt.push_back( "simsingletons");
			rt.push_back( "nofsimrel");
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting feature attribute names: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

	virtual std::vector<Index> conceptFeatures( const std::string& conceptClass, const Index& conceptid) const
	{
		try
		{
			return m_database.readConceptSampleIndices( conceptClass, conceptid);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting associated feature indices by learnt concept: %s"), MODULENAME, *m_errorhnd, std::vector<Index>());
	}

	virtual unsigned int nofConcepts( const std::string& conceptClass) const
	{
		try
		{
			return m_database.readNofConcepts( conceptClass);
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting number of concepts learnt: %s"), MODULENAME, *m_errorhnd, 0);
	}

	virtual unsigned int nofFeatures() const
	{
		return m_database.readNofSamples();
	}

	virtual std::string config() const
	{
		try
		{
			return m_config.tostring();
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' mapping configuration to string: %s"), MODULENAME, *m_errorhnd, std::string());
	}

private:
	ErrorBufferInterface* m_errorhnd;
	DatabaseAdapter m_database;
	VectorStorageConfig m_config;
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


class VectorStorageBuilder
	:public VectorStorageBuilderInterface
{
public:
	VectorStorageBuilder( const VectorStorageConfig& config_, const std::string& configstr_, const DatabaseInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_)
		,m_dbi(database_)
		,m_database(database_,config_.databaseConfig,errorhnd_)
		,m_config(config_)
		,m_state(0),m_needToCalculateSimRelationMap(false),m_haveFeaturesAdded(false)
		,m_lshmodel(),m_genmodelmain(),m_genmodelmap(),m_samplear()
	{
		m_database.checkVersion();
		m_state = m_database.readState();
		if (m_database.isempty() || m_state < 1)
		{
			m_database.writeConfig( m_config);
			m_lshmodel = LshModel( m_config.dim, m_config.bits, m_config.variations);
			m_genmodelmain = GenModel( m_config.threads, m_config.maxdist, m_config.gencfg.simdist, m_config.gencfg.raddist, m_config.gencfg.eqdist, m_config.gencfg.mutations, m_config.gencfg.votes, m_config.gencfg.descendants, m_config.gencfg.maxage, m_config.gencfg.iterations, m_config.gencfg.assignments, m_config.gencfg.greediness, m_config.gencfg.isaf, m_config.gencfg.baff, m_config.gencfg.fdf, m_config.gencfg.eqdiff, m_config.gencfg.with_singletons, m_errorhnd);
			GenModelConfigMap::const_iterator mi = m_config.altgenmap.begin(), me = m_config.altgenmap.end();
			for (; mi != me; ++mi)
			{
				m_genmodelmap[ mi->first] = GenModel( m_config.threads, m_config.maxdist, mi->second.simdist, mi->second.raddist, mi->second.eqdist, mi->second.mutations, mi->second.votes, mi->second.descendants, mi->second.maxage, mi->second.iterations, mi->second.assignments, mi->second.greediness, mi->second.isaf, mi->second.baff, mi->second.fdf, mi->second.eqdiff, mi->second.with_singletons, m_errorhnd);
			}
			m_database.writeLshModel( m_lshmodel);
			m_database.writeState( m_state = 1);
			m_database.commit();
		}
		else
		{
			VectorStorageConfig cfg = m_database.readConfig();
			m_config = VectorStorageConfig( configstr_, errorhnd_, cfg);
			if (!m_config.isBuildCompatible( cfg))
			{
				throw strus::runtime_error(_TXT("loading vector storage with incompatible configuration"));
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
	}

	virtual ~VectorStorageBuilder()
	{}

	static std::string utf8clean( const std::string& name)
	{
		std::string rt;
		std::size_t si = 0, se = name.size();
		while (si < se)
		{
			unsigned char asciichr = name[si];
			if (asciichr && asciichr < 128)
			{
				rt.push_back( asciichr);
				++si;
			}
			else
			{
				std::size_t chrlen = strus::utf8charlen( asciichr);
				uint32_t chr = strus::utf8decode( name.c_str() + si, chrlen);
				if (chr)
				{
					char buf[ 16];
					chrlen = strus::utf8encode( buf, chr);
					rt.append( buf, chrlen);
				}
				si += chrlen;
			}
		}
		return rt;
	}

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
					m_namear.push_back( utf8clean( name));
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
				m_database.writeSample( m_samplear.size() + si, m_namear[si], m_vecar[si], shar[si]);
			}
			m_samplear.insert( m_samplear.end(), shar.begin(), shar.end());
			m_database.writeNofSamples( m_samplear.size());
			m_database.commit();

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
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in vector storage database commit of '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

	virtual bool run( const std::string& command)
	{
		try
		{
			if (command.empty())
			{
				done();
				finalize();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "rebase"))
			{
				done();
				rebase();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "base"))
			{
				done();
				buildSimilarityRelationMap();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "learn"))
			{
				done();
				learnConcepts();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "condep"))
			{
				done();
				buildConceptDependencies();
				return true;
			}
			else if (utils::caseInsensitiveEquals( command, "clear"))
			{
				m_database.clear();
				m_database.commit();
				m_vecar.clear();
				m_namear.clear();
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
		m_database.commit();
		SimRelationNeighbourReader nbreader( &m_database, &m_samplear, m_config.maxdist);
		buildSimRelationMap( &nbreader);
	}

	void buildSimilarityRelationMap()
	{
		utils::ScopedLock lock( m_mutex);
		m_database.commit();
		buildSimRelationMap( 0/*without SimRelationNeighbourReader*/);
	}

	void learnConcepts()
	{
		utils::ScopedLock lock( m_mutex);
		m_database.commit();

		m_database.deleteSampleConceptIndexMaps();
		m_database.deleteConceptSampleIndexMaps();
		m_database.deleteConceptDependencies();
		m_database.deleteConceptClassNames();
		m_database.commit();

		std::vector<std::string> clnames;
		run_genmodel( MAIN_CONCEPT_CLASSNAME, m_genmodelmain);
		GenModelMap::const_iterator mi = m_genmodelmap.begin(), me = m_genmodelmap.end();
		for (; mi != me; ++mi)
		{
			run_genmodel( mi->first, mi->second);
			clnames.push_back( mi->first);
		}
		m_database.writeNofSamples( m_samplear.size());
		m_database.writeConceptClassNames( clnames);
		m_database.writeState( 3);
		m_database.commit();
		buildConceptDependencies();
		m_database.writeState( 4);
		m_database.commit();
	}

	void finalize()
	{
		{
			utils::ScopedLock lock( m_mutex);
			m_database.commit();
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
		std::vector<VectorStorageConfig::ConceptClassDependency>::const_iterator
			ci = m_config.conceptClassDependecies.begin(), ce = m_config.conceptClassDependecies.end();
		for (; ci != ce; ++ci)
		{
			writeDependencyLinks( ci->first, ci->second);
			m_database.commit();
		}
	}

	bool needToCalculateSimRelationMap()
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

	void buildSimRelationMap( const SimRelationReader* simmapreader)
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		Logger logout( logfile);

		m_database.writeState( 1);
		m_database.commit();

		if (logout) logout << string_format( _TXT("calculate similarity relation matrix for %u features (selection %s)"), m_samplear.size(), m_config.with_probsim?_TXT("probabilistic selection"):_TXT("try all"));
		uint64_t total_nof_similarities = 0;
		SampleIndex startsampleidx = m_haveFeaturesAdded?0:(m_database.readLastSimRelationIndex()+1);
		SimRelationMapBuilder simrelbuilder( m_samplear, startsampleidx, m_config.maxdist, m_config.maxsimsam, m_config.rndsimsam, m_config.threads, m_config.with_probsim, logout, simmapreader);
		SimRelationMap simrelmap_part;
		while (simrelbuilder.getNextSimRelationMap( simrelmap_part))
		{
			m_database.writeSimRelationMap( simrelmap_part);
			m_database.commit();
			total_nof_similarities += simrelmap_part.nofRelationsDetected();
			if (logout) logout << string_format( _TXT("got total %u features with %uK similarities"), simrelmap_part.endIndex(), (unsigned int)(total_nof_similarities/1024));
		}
		m_database.writeNofSimRelations( m_samplear.size());
		m_database.writeState( 2);
		m_database.commit();
		if (logout) logout << string_format( _TXT("calculated similarity relation matrix stored to database"));
	}

	void run_genmodel( const std::string& clname, const GenModel& genmodel)
	{
		const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
		SimRelationMapReader simrelmap( &m_database);
		std::vector<SampleIndex> singletons;
		SampleConceptIndexMap sampleConceptIndexMap;
		ConceptSampleIndexMap conceptSampleIndexMap;

		std::vector<SimHash> resultar = genmodel.run(
				singletons, sampleConceptIndexMap, conceptSampleIndexMap,
				clname, m_samplear, m_config.maxconcepts, simrelmap, m_config.threads, logfile);

		m_database.writeSampleConceptIndexMap( clname, sampleConceptIndexMap);
		m_database.writeConceptSampleIndexMap( clname, conceptSampleIndexMap);
		m_database.writeNofConcepts( clname, resultar.size());
	}

	std::vector<ConceptIndex> translateConcepts( const std::string& from_clname, ConceptIndex from_index, const std::string& to_clname)
	{
		std::set<ConceptIndex> to_conceptset;
		std::vector<SampleIndex> from_members = m_database.readConceptSampleIndices( from_clname, from_index);
		std::vector<SampleIndex>::const_iterator mi = from_members.begin(), me = from_members.end();
		for (; mi != me; ++mi)
		{
			std::vector<ConceptIndex> to_concepts = m_database.readSampleConceptIndices( to_clname, *mi);
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
		unsigned int ci = 1, ce = m_database.readNofConcepts( from_clname)+1;
		for (; ci != ce; ++ci)
		{
			m_database.writeConceptDependencies( from_clname, ci, to_clname, translateConcepts( from_clname, ci, to_clname));
			if (ci % m_config.commitsize == 0)
			{
				m_database.commit();
			}
		}
		m_database.commit();
	}

private:
	typedef std::map<std::string,GenModel> GenModelMap;

private:
	ErrorBufferInterface* m_errorhnd;
	const DatabaseInterface* m_dbi;
	DatabaseAdapter m_database;
	VectorStorageConfig m_config;
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


VectorStorage::VectorStorage( ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_){}


bool VectorStorage::createStorage( const std::string& configsource, const DatabaseInterface* dbi) const
{
	try
	{
		VectorStorageConfig config( configsource, m_errorhnd);
		if (dbi->createDatabase( config.databaseConfig))
		{
			DatabaseAdapter database( dbi, config.databaseConfig, m_errorhnd);
			database.writeVersion();
			database.commit();
		}
		else
		{
			throw strus::runtime_error(_TXT("failed to create repository for vector storage: %s"), m_errorhnd->fetchError());
		}
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' repository: %s"), MODULENAME, *m_errorhnd, false);
}

bool VectorStorage::resetStorage( const std::string& configsrc, const DatabaseInterface* dbi) const
{
	try
	{
		VectorStorageConfig config( configsrc, m_errorhnd);
		DatabaseAdapter database( dbi, config.databaseConfig, m_errorhnd);
		database.clear();
		database.commit();
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("failed to reset '%s' repository: %s"), MODULENAME, *m_errorhnd, false);
}


VectorStorageClientInterface* VectorStorage::createClient( const std::string& configsrc, const DatabaseInterface* database) const
{
	try
	{
		return new VectorStorageClient( VectorStorageConfig(configsrc,m_errorhnd), configsrc, database, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' client interface: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorStorageBuilderInterface* VectorStorage::createBuilder( const std::string& configsrc, const DatabaseInterface* database) const
{
	try
	{
		return new VectorStorageBuilder( VectorStorageConfig(configsrc,m_errorhnd), configsrc, database, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' builder: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorStorageDumpInterface* VectorStorage::createDump( const std::string& configsource, const DatabaseInterface* database, const std::string& keyprefix) const
{
	try
	{
		return new VectorStorageDump( database, configsource, keyprefix, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' builder: %s"), MODULENAME, *m_errorhnd, 0);
}

std::vector<std::string> VectorStorage::builderCommands() const
{
	try
	{
		std::vector<std::string> rt;
		rt.push_back( "base");
		rt.push_back( "rebase");
		rt.push_back( "learn");
		rt.push_back( "clear");
		rt.push_back( "condep");
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error getting list of commands of '%s' builder: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
}

std::string VectorStorage::builderCommandDescription( const std::string& command) const
{
	try
	{
		if (command.empty())
		{
			return _TXT("Short cut for subsequent call of 'base' and 'learn'. Calculate sparse similarity relation matrix if needed (=base) and do learn concepts (=learn).");
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

