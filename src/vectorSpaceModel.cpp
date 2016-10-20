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
#include "strus/base/fileio.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "vectorSpaceModelFiles.hpp"
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
	VectorSpaceModelInstance( const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_)
	{
		if (m_config.path.empty()) throw strus::runtime_error(_TXT("no 'path' configuration variable defined, cannot load model"));
		checkVersionFile( m_config.path + dirSeparator() + VERSIONFILE);
		m_config = readConfigurationFromFile( m_config.path + dirSeparator() + CONFIGFILE, m_errorhnd);
		m_lshmodel = readLshModelFromFile( m_config.path + dirSeparator() + MATRIXFILE);
		m_individuals = readSimHashVectorFromFile( m_config.path + dirSeparator() + RESVECFILE);
		m_sampleFeatureIndexMap = readSampleFeatureIndexMapFromFile( m_config.path + dirSeparator() + IDXFEATFILE);
		m_featureSampleIndexMap = readFeatureSampleIndexMapFromFile( m_config.path + dirSeparator() + FEATIDXFILE);
		m_sampleNames = readSampleNamesFromFile( m_config.path + dirSeparator() + VECNAMEFILE);
#ifdef STRUS_LOWLEVEL_DEBUG
		writeDumpToFile( tostring(), m_config.path + dirSeparator() + DUMPFILE);
#endif
	}

	VectorSpaceModelInstance( const VectorSpaceModelInstance& o)
		:m_errorhnd(o.m_errorhnd),m_config(o.m_config),m_lshmodel(o.m_lshmodel)
		,m_sampleFeatureIndexMap(o.m_sampleFeatureIndexMap)
		,m_featureSampleIndexMap(o.m_featureSampleIndexMap)
		,m_sampleNames(o.m_sampleNames){}

	virtual ~VectorSpaceModelInstance()
	{}

	virtual std::vector<unsigned int> mapVectorToFeatures( const std::vector<double>& vec) const
	{
		try
		{
			std::vector<unsigned int> rt;
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
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' mapping vector to features: %s"), MODULENAME, *m_errorhnd, std::vector<unsigned int>());
	}

	virtual std::vector<unsigned int> sampleFeatures( unsigned int index) const
	{
		try
		{
			std::vector<unsigned int> rt;
			std::vector<FeatureIndex> res = m_sampleFeatureIndexMap.getValues( index);
			rt.reserve( res.size());
			std::vector<FeatureIndex>::const_iterator ri = res.begin(), re =  res.end();
			for (; ri != re; ++ri) rt.push_back( *ri);
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting learnt features by sample index: %s"), MODULENAME, *m_errorhnd, std::vector<unsigned int>());
	}

	virtual std::vector<double> sampleVector( unsigned int index) const
	{
		try
		{
			std::vector<double> rt;
			std::vector<FeatureIndex> res = m_sampleFeatureIndexMap.getValues( index);
			rt.reserve( res.size());
			std::vector<FeatureIndex>::const_iterator ri = res.begin(), re =  res.end();
			for (; ri != re; ++ri) rt.push_back( *ri);
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting sample vector by index: %s"), MODULENAME, *m_errorhnd, std::vector<double>());
	}

	virtual std::vector<unsigned int> featureSamples( unsigned int feature) const
	{
		try
		{
			std::vector<unsigned int> rt;
			std::vector<FeatureIndex> res = m_featureSampleIndexMap.getValues( feature);
			rt.reserve( res.size());
			std::vector<SampleIndex>::const_iterator ri = res.begin(), re =  res.end();
			for (; ri != re; ++ri) rt.push_back( *ri);
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting associated sample indices by learnt feature: %s"), MODULENAME, *m_errorhnd, std::vector<unsigned int>());
	}

	virtual unsigned int nofFeatures() const
	{
		return m_individuals.size();
	}

	virtual std::string sampleName( unsigned int index) const
	{
		try
		{
			return m_sampleNames[ index];
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in instance of '%s' getting name of sample: %s"), MODULENAME, *m_errorhnd, std::string());
	}

	virtual unsigned int nofSamples() const
	{
		return m_sampleFeatureIndexMap.maxkey()+1;
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
	std::string tostring() const
	{
		std::ostringstream txtdump;
		txtdump << "config: " << m_config.tostring() << std::endl;
		txtdump << "lsh model: " << std::endl << m_lshmodel.tostring() << std::endl;
		txtdump << "individuals: " << std::endl;
		std::vector<SimHash>::const_iterator ii = m_individuals.begin(), ie = m_individuals.end();
		FeatureIndex fidx = 1;
		for (; ii != ie; ++ii,++fidx)
		{
			std::vector<SampleIndex> members = m_featureSampleIndexMap.getValues( fidx);
			txtdump << ii->tostring() << ": {";
			std::vector<SampleIndex>::const_iterator mi = members.begin(), me = members.end();
			for (std::size_t midx=0; mi != me; ++mi,++midx)
			{
				if (midx) txtdump << ", ";
				txtdump << *mi;
			}
			txtdump << "}" << std::endl;
		}
		txtdump << std::endl;
		return txtdump.str();
	}

private:
	ErrorBufferInterface* m_errorhnd;
	VectorSpaceModelConfig m_config;
	LshModel m_lshmodel;
	std::vector<SimHash> m_individuals;
	SampleFeatureIndexMap m_sampleFeatureIndexMap;
	FeatureSampleIndexMap m_featureSampleIndexMap;
	StringList m_sampleNames;
	DataRecordFile m_vectorfile;
};


class VectorSpaceModelBuilder
	:public VectorSpaceModelBuilderInterface
{
public:
	VectorSpaceModelBuilder( const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_)
		,m_simrelmap(),m_lshmodel(),m_genmodel()
		,m_samplear(),m_resultar()
		,m_sampleFeatureIndexMap()
		,m_featureSampleIndexMap()
		,m_sampleNames()
		,m_modelLoadedFromFile(false)
	{
		if (!m_config.prepath.empty())
		{
			// If 'prepath' is configured then all prepared data from a previous run is loaded from there
			checkVersionFile( m_config.prepath + dirSeparator() + VERSIONFILE);
			m_simrelmap = readSimRelationMapFromFile( m_config.prepath + dirSeparator() + SIMRELFILE);
			m_lshmodel = readLshModelFromFile( m_config.prepath + dirSeparator() + MATRIXFILE);
			m_genmodel = GenModel( m_config.threads, m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.isaf, m_config.with_singletons);
			m_samplear = readSimHashVectorFromFile( m_config.prepath + dirSeparator() + INPVECFILE);
			m_sampleNames = readSampleNamesFromFile( m_config.prepath + dirSeparator() + VECNAMEFILE);
			m_modelLoadedFromFile = true;

			std::string path = m_config.path;
			m_config = readConfigurationFromFile( m_config.prepath + dirSeparator() + CONFIGFILE, m_errorhnd);
			m_config.path = path;
		}
		else
		{
			m_lshmodel = LshModel( m_config.dim, m_config.bits, m_config.variations);
			m_genmodel = GenModel( m_config.threads, m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.isaf, m_config.with_singletons);
		}
	}
	virtual ~VectorSpaceModelBuilder()
	{}

	virtual void addSampleVector( const std::string& name, const std::vector<double>& vec)
	{
		try
		{
			if (0!=std::memchr( name.c_str(), '\0', name.size()))
			{
				throw strus::runtime_error(_TXT("name of vector contains null bytes"));
			}
			if (m_modelLoadedFromFile)
			{
				throw strus::runtime_error(_TXT("no more samples accepted, model was loaded from file"));
			}
			m_sampleNames.push_back( name);
			m_samplear.push_back( m_lshmodel.simHash( arma::normalise( arma::vec( vec))));
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error adding sample vector to '%s' builder: %s"), MODULENAME, *m_errorhnd);
	}

	virtual bool finalize()
	{
		try
		{
			const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
			if (!m_modelLoadedFromFile)
			{
				m_simrelmap = m_genmodel.getSimRelationMap( m_samplear, logfile);
			}
			m_resultar = m_genmodel.run(
					m_sampleFeatureIndexMap, m_featureSampleIndexMap,
					m_samplear, m_simrelmap, logfile);
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error finalizing '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

	virtual bool store()
	{
		try
		{
			if (!m_modelLoadedFromFile && m_simrelmap.nofSamples() == 0 && m_samplear.size() > 0)
			{
				// If we did not call finalize(), we build some structures like similarity relation map for future use to store.
				const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();
				m_simrelmap = m_genmodel.getSimRelationMap( m_samplear, logfile);
			}
			if (m_config.path.empty())
			{
				throw strus::runtime_error(_TXT("failed to store built instance (no file configured)"));
			}
			unsigned int ec = createDir( m_config.path, true);
			if (ec) throw strus::runtime_error(_TXT( "failed to create vector space model directory '%s' (system error %u: %s)"), m_config.path.c_str(), ec, ::strerror(ec));
#ifdef STRUS_LOWLEVEL_DEBUG
				writeDumpToFile( tostring(), path + dirSeparator() + DUMPFILE);
#endif
			writeVersionToFile( m_config.path + dirSeparator() + VERSIONFILE);
			writeConfigurationToFile( m_config, m_config.path + dirSeparator() + CONFIGFILE);
			writeLshModelToFile( m_lshmodel, m_config.path + dirSeparator() + MATRIXFILE);
			writeSimHashVectorToFile( m_resultar, std::string( m_config.path + dirSeparator() + RESVECFILE));
			if (!m_modelLoadedFromFile)
			{
				writeSimRelationMapToFile( m_simrelmap, m_config.path + dirSeparator() + SIMRELFILE);
				writeSimHashVectorToFile( m_samplear, m_config.path + dirSeparator() + INPVECFILE);
				writeSampleNamesToFile( m_sampleNames, m_config.path + dirSeparator() + VECNAMEFILE);
			}
			writeSampleFeatureIndexMapToFile( m_sampleFeatureIndexMap, m_config.path + dirSeparator() + IDXFEATFILE);
			writeFeatureSampleIndexMapToFile( m_featureSampleIndexMap, m_config.path + dirSeparator() + FEATIDXFILE);
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error storing '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

private:
	std::string tostring() const
	{
		std::ostringstream txtdump;
		std::vector<SimHash>::const_iterator si = m_samplear.begin(), se = m_samplear.end();
		for (unsigned int sidx=0; si != se; ++si,++sidx)
		{
			txtdump << "sample [" << sidx << "] " << si->tostring() << std::endl;
		}
		std::vector<SimHash>::const_iterator ri = m_resultar.begin(), re = m_resultar.end();
		for (unsigned int ridx=0; ri != re; ++ri,++ridx)
		{
			txtdump << "result [" << ridx << "] " << ri->tostring() << std::endl;
		}
		txtdump << "lsh model:" << std::endl << m_lshmodel.tostring() << std::endl;
		txtdump << "gen model:" << std::endl << m_genmodel.tostring() << std::endl;
		return txtdump.str();
	}

private:
	ErrorBufferInterface* m_errorhnd;
	VectorSpaceModelConfig m_config;
	SimRelationMap m_simrelmap;
	LshModel m_lshmodel;
	GenModel m_genmodel;
	std::vector<SimHash> m_samplear;
	std::vector<SimHash> m_resultar;
	SampleFeatureIndexMap m_sampleFeatureIndexMap;
	FeatureSampleIndexMap m_featureSampleIndexMap;
	StringList m_sampleNames;
	bool m_modelLoadedFromFile;
};


VectorSpaceModel::VectorSpaceModel( ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_){}


VectorSpaceModelInstanceInterface* VectorSpaceModel::createInstance( const std::string& config) const
{
	try
	{
		return new VectorSpaceModelInstance( config, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' instance: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorSpaceModelBuilderInterface* VectorSpaceModel::createBuilder( const std::string& config) const
{
	try
	{
		return new VectorSpaceModelBuilder( config, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' builder: %s"), MODULENAME, *m_errorhnd, 0);
}

bool VectorSpaceModel::destroyModel( const std::string& configstr) const
{
	try
	{
		VectorSpaceModelConfig config( configstr, m_errorhnd);
		if (config.path.empty()) throw strus::runtime_error(_TXT("no path defined, cannot remove model"));
		unsigned int ec;
		ec = removeFile( config.path + dirSeparator() + IDXFEATFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), IDXFEATFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + FEATIDXFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), FEATIDXFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + MATRIXFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), MATRIXFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + SIMRELFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), SIMRELFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + RESVECFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), RESVECFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + INPVECFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), INPVECFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + CONFIGFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), CONFIGFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + VERSIONFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), VERSIONFILE, ::strerror(ec), ec);
		ec = removeFile( config.path + dirSeparator() + DUMPFILE);
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), DUMPFILE, ::strerror(ec), ec);
		ec = removeDir( config.path);
		if (ec) throw strus::runtime_error(_TXT("failed to remove directory '%s': %s (%u)"), config.path.c_str(), ::strerror(ec), ec);
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error destroying a '%s' model: %s"), MODULENAME, *m_errorhnd, false);
}

