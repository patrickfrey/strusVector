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
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "indexListMap.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/configParser.hpp"
#include "strus/versionVector.hpp"
#include "strus/base/hton.hpp"
#include "armadillo"
#include <memory>

using namespace strus;
#define MODULENAME   "standard vector space model"

#define MATRIXFILE   "model.mat"		// Matrices for LSH calculation
#define SIMRELFILE   "simrel.mat"		// Sparse matrix of imilarity relation map
#define RESVECFILE   "result.lsh"		// Result LSH values
#define INPVECFILE   "input.lsh"		// Input sample LSH values
#define CONFIGFILE   "config.txt"		// Configuration as text
#define VERSIONFILE  "version.hdr"		// Version for checking compatibility
#define DUMPFILE     "dump.txt"			// Textdump output of structures if used with LOWLEVEL DEBUG defined
#define FEATIDXFILE  "featsampleindex.fsi"	// map feature number to sample indices [FeatureSampleIndexMap]
#define IDXFEATFILE  "samplefeatindex.sfi"	// map sample index to feature numbers [SampleFeatureIndexMap]


#undef STRUS_LOWLEVEL_DEBUG

struct VectorSpaceModelHdr
{
	enum {FILEID=0x3ff3};
	char name[58];
	unsigned short _id;
	unsigned short version_major;
	unsigned short version_minor;

	VectorSpaceModelHdr()
	{
		std::memcpy( name, "strus standard vector space model bin file\n\0", sizeof(name));
		_id = FILEID;
		version_major = STRUS_VECTOR_VERSION_MAJOR;
		version_minor = STRUS_VECTOR_VERSION_MINOR;
	}

	void check()
	{
		if (_id != FILEID) throw strus::runtime_error(_TXT("unknown file type, expected strus standard vector space model binary file"));
		if (version_major != STRUS_VECTOR_VERSION_MAJOR) throw strus::runtime_error(_TXT("major version (%u) of loaded strus standard vector space model binary file does not match"), version_major);
		if (version_minor > STRUS_VECTOR_VERSION_MINOR) throw strus::runtime_error(_TXT("minor version (%u) of loaded strus standard vector space model binary file does not match (newer than your version of strus)"), version_minor);
	}

	void hton()
	{
		_id = ByteOrder<unsigned short>::hton( _id);
		version_major = ByteOrder<unsigned short>::hton( version_major);
		version_minor = ByteOrder<unsigned short>::hton( version_minor);
	}

	void ntoh()
	{
		_id = ByteOrder<unsigned short>::ntoh( _id);
		version_major = ByteOrder<unsigned short>::ntoh( version_major);
		version_minor = ByteOrder<unsigned short>::ntoh( version_minor);
	}
};

struct VectorSpaceModelConfig
{
	enum Defaults {
		DefaultThreads = 0,
		DefaultDim = 300,
		DefaultBits = 64,
		DefaultVariations = 32,
		DefaultSimDist = 340,	//< 340 out of 2K (32*64) ~ cosine dist 0.9
		DefaultRadDist = 320,	//< 340 out of 2K (32*64) ~ cosine dist 0.9
		DefaultEqDist = 60,
		DefaultMutations = 50,
		DefaultMutationVotes = 13,
		DefaultDescendants = 10,
		DefaultMaxAge = 20,
		DefaultIterations = 20,
		DefaultAssignments = 7,
		DefaultIsaf = 60,
		DefaultWithSingletons = 0
	};
	VectorSpaceModelConfig( const VectorSpaceModelConfig& o)
		:path(o.path),prepath(o.prepath),logfile(o.logfile),threads(o.threads)
		,dim(o.dim),bits(o.bits),variations(o.variations)
		,simdist(o.simdist),raddist(o.raddist),eqdist(o.eqdist),mutations(o.mutations),votes(o.votes)
		,descendants(o.descendants),maxage(o.maxage),iterations(o.iterations)
		,assignments(o.assignments)
		,isaf(o.isaf)
		,with_singletons(o.with_singletons)
		{}
	VectorSpaceModelConfig()
		:path(),prepath(),logfile(),threads(DefaultThreads)
		,dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
		,simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
		,mutations(DefaultMutations),votes(DefaultMutationVotes)
		,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
		,assignments(DefaultAssignments)
		,isaf((float)DefaultIsaf / 100)
		,with_singletons((bool)DefaultWithSingletons)
		{}
	VectorSpaceModelConfig( const std::string& config, ErrorBufferInterface* errorhnd)
		:path(),prepath(),logfile(),threads(DefaultThreads)
		,dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
		,simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
		,mutations(DefaultMutations),votes(DefaultMutationVotes)
		,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
		,assignments(DefaultAssignments)
		,isaf((float)DefaultIsaf / 100)
		,with_singletons((bool)DefaultWithSingletons)
	{
		std::string src = config;
		if (extractStringFromConfigString( path, src, "path", errorhnd)){}
		if (extractStringFromConfigString( prepath, src, "prepath", errorhnd)){}
		if (extractStringFromConfigString( logfile, src, "logfile", errorhnd)){}
		if (extractUIntFromConfigString( threads, src, "threads", errorhnd)){}
		if (extractUIntFromConfigString( dim, src, "dim", errorhnd)){}
		if (extractUIntFromConfigString( bits, src, "bit", errorhnd)){}
		if (extractUIntFromConfigString( variations, src, "var", errorhnd)){}
		if (extractUIntFromConfigString( simdist, src, "simdist", errorhnd))
		{
			raddist = simdist;
			eqdist = simdist / 6;
		}
		if (extractUIntFromConfigString( raddist, src, "raddist", errorhnd)){}
		if (raddist > simdist)
		{
			throw strus::runtime_error(_TXT("the 'raddist' configuration parameter must not be bigger than 'simdist'"));
		}
		if (extractUIntFromConfigString( eqdist, src, "eqdist", errorhnd)){}
		if (eqdist > simdist)
		{
			throw strus::runtime_error(_TXT("the 'eqdist' configuration parameter must not be bigger than 'simdist'"));
		}
		if (extractUIntFromConfigString( mutations, src, "mutations", errorhnd)){}
		if (extractUIntFromConfigString( votes, src, "votes", errorhnd)){}
		if (extractUIntFromConfigString( descendants, src, "descendants", errorhnd)){}
		if (extractUIntFromConfigString( iterations, src, "iterations", errorhnd))
		{
			maxage = iterations;
		}
		if (extractUIntFromConfigString( maxage, src, "maxage", errorhnd)){}
		if (extractUIntFromConfigString( assignments, src, "assignments", errorhnd)){}
		double val;
		if (extractFloatFromConfigString( val, src, "isaf", errorhnd)){isaf=(float)val;}
		if (extractBooleanFromConfigString( with_singletons, src, "singletons", errorhnd)){}

		if (dim == 0 || bits == 0 || variations == 0 || mutations == 0 || descendants == 0 || maxage == 0 || iterations == 0)
		{
			throw strus::runtime_error(_TXT("error in vector space model configuration: dim, bits, var, mutations, descendants, maxage or iterations values must not be zero"));
		}
		std::string::const_iterator si = src.begin(), se = src.end();
		for (; si != se && (unsigned char)*si <= 32; ++si){}
		if (si != se)
		{
			throw strus::runtime_error(_TXT("unknown configuration parameter: %s"), src.c_str());
		}
		if (errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("error loading vector space model configuration: %s"), errorhnd->fetchError());
		}
	}

	std::string tostring() const
	{
		std::ostringstream buf;
		buf << "path=" << path << ";" << std::endl;
		buf << "prepath=" << prepath << ";" << std::endl;
		buf << "logfile=" << logfile << ";" << std::endl;
		buf << "threads=" << threads << ";" << std::endl;
		buf << "dim=" << dim << ";" << std::endl;
		buf << "bit=" << bits << ";" << std::endl;
		buf << "var=" << variations << ";" << std::endl;
		buf << "simdist=" << simdist << ";" << std::endl;
		buf << "raddist=" << raddist << ";" << std::endl;
		buf << "eqdist=" << eqdist << ";" << std::endl;
		buf << "mutations=" << mutations << ";" << std::endl;
		buf << "votes=" << votes << ";" << std::endl;
		buf << "descendants=" << descendants << ";" << std::endl;
		buf << "maxage=" << maxage << ";" << std::endl;
		buf << "iterations=" << iterations << ";" << std::endl;
		buf << "assignments=" << assignments << ";" << std::endl;
		buf << "isaf=" << isaf << ";" << std::endl;
		buf << "singletons=" << (with_singletons?"yes":"no") << ";" << std::endl;
		return buf.str();
	}

	std::string path;		///< path of model
	std::string prepath;		///< for builder: path with preprocessed model (similarity relation map and input LSH values are precalculated)
	std::string logfile;		///< file where to log some status data
	unsigned int threads;		///< maximum number of threads to use (0 = no threading)
	unsigned int dim;		///< input vector dimension
	unsigned int bits;		///< number of bits to calculate for an LSH per variation
	unsigned int variations;	///< number of variations
	unsigned int simdist;		///< maximum LSH edit distance considered as similarity
	unsigned int raddist;		///< centroid radius distance (smaller than simdist)
	unsigned int eqdist;		///< maximum LSH edit distance considered as equal
	unsigned int mutations;		///< number of mutations when a genetic code is changed
	unsigned int votes;		///< number of votes to decide in which direction a mutation should go
	unsigned int descendants;	///< number of descendants to evaluate the fitest of when trying to change an individual
	unsigned int maxage;		///< a factor used to slow down mutation rate
	unsigned int iterations;	///< number of iterations in the loop
	unsigned int assignments;	///< maximum number of group assignments for each input vector
	float isaf;			///< fraction of elements of a superset that has to be in a subset for declaring the subset as dependent (is a) of the superset
	bool with_singletons;		///< true, if singleton vectors thould also get into the result
};

struct VectorSpaceModelData
{
	VectorSpaceModelData( const VectorSpaceModelConfig& config_)
		:config(config_){}

	VectorSpaceModelConfig config;
};

static void checkVersionFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load model version from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	VectorSpaceModelHdr hdr;
	if (content.size() != sizeof(hdr)) throw strus::runtime_error(_TXT("unknown file format"));
	std::memcpy( &hdr, content.c_str(), sizeof(hdr));
	hdr.ntoh();
	hdr.check();
}

static void writeVersionToFile( const std::string& filename)
{
	std::string content;
	VectorSpaceModelHdr hdr;
	hdr.hton();
	content.append( (const char*)&hdr, sizeof(hdr));
	unsigned int ec = writeFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to write model version to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

#ifdef STRUS_LOWLEVEL_DEBUG
static void writeDumpToFile( const std::string& content, const std::string& filename)
{
	unsigned int ec = writeFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to write text dump to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}
#endif

static std::vector<SimHash> readSimHashVectorFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load lsh values from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return SimHash::createFromSerialization( content);
}

static void writeSimHashVectorToFile( const std::vector<SimHash>& ar, const std::string& filename)
{
	unsigned int ec = writeFile( filename, SimHash::serialization( ar));
	if (ec) throw strus::runtime_error(_TXT("failed to write lsh values to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

static VectorSpaceModelConfig readConfigurationFromFile( const std::string& filename, ErrorBufferInterface* errorhnd)
{
	VectorSpaceModelConfig rt;
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load config from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	try
	{
		rt = VectorSpaceModelConfig( content, errorhnd);
	}
	catch (const std::runtime_error& err)
	{
		throw strus::runtime_error(_TXT("error in vector space model configuration: %s"), err.what());
	}
	return rt;
}

static void writeConfigurationToFile( const VectorSpaceModelConfig& config, const std::string& filename)
{
	unsigned int ec = writeFile( filename, config.tostring());
	if (ec) throw strus::runtime_error(_TXT("failed to configuration to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

static LshModel readLshModelFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load model from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return LshModel::fromSerialization( content);
}

static void writeLshModelToFile( const LshModel& model, const std::string& filename)
{
	unsigned int ec = writeFile( filename, model.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write lsh values to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

static SimRelationMap readSimRelationMapFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to similarity relation map from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return SimRelationMap::fromSerialization( content);
}

static void writeSimRelationMapToFile( const SimRelationMap& simrelmap, const std::string& filename)
{
	unsigned int ec = writeFile( filename, simrelmap.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write similarity relation map to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

static SampleFeatureIndexMap readSampleFeatureIndexMapFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to read sample feature index map from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return SampleFeatureIndexMap::fromSerialization( content);
}

static void writeSampleFeatureIndexMapToFile( const SampleFeatureIndexMap& map, const std::string& filename)
{
	unsigned int ec = writeFile( filename, map.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write sample feature index map to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

static FeatureSampleIndexMap readFeatureSampleIndexMapFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to read sample feature index map from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return FeatureSampleIndexMap::fromSerialization( content);
}

static void writeFeatureSampleIndexMapToFile( const FeatureSampleIndexMap& map, const std::string& filename)
{
	unsigned int ec = writeFile( filename, map.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write sample feature index map to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}


class VectorSpaceModelInstance
	:public VectorSpaceModelInstanceInterface
{
public:
	VectorSpaceModelInstance( const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_),m_lshmodel()
	{
		if (m_config.path.empty()) throw strus::runtime_error(_TXT("no 'path' configuration variable defined, cannot load model"));
		checkVersionFile( m_config.path + dirSeparator() + VERSIONFILE);
		m_config = readConfigurationFromFile( m_config.path + dirSeparator() + CONFIGFILE, m_errorhnd);
		m_lshmodel = readLshModelFromFile( m_config.path + dirSeparator() + MATRIXFILE);
		m_individuals = readSimHashVectorFromFile( m_config.path + dirSeparator() + RESVECFILE);
		m_sampleFeatureIndexMap = readSampleFeatureIndexMapFromFile( m_config.path + dirSeparator() + IDXFEATFILE);
		m_featureSampleIndexMap = readFeatureSampleIndexMapFromFile( m_config.path + dirSeparator() + FEATIDXFILE);
#ifdef STRUS_LOWLEVEL_DEBUG
		writeDumpToFile( tostring(), m_config.path + dirSeparator() + DUMPFILE);
#endif
	}

	VectorSpaceModelInstance( const VectorSpaceModelInstance& o)
		:m_errorhnd(o.m_errorhnd),m_config(o.m_config),m_lshmodel(o.m_lshmodel),m_sampleFeatureIndexMap(o.m_sampleFeatureIndexMap),m_featureSampleIndexMap(o.m_featureSampleIndexMap){}

	virtual ~VectorSpaceModelInstance()
	{}

	virtual std::vector<unsigned int> mapVectorToFeatures( const std::vector<double>& vec) const
	{
		try
		{
			std::vector<unsigned int> rt;
			SimHash hash( m_lshmodel.simHash( arma::vec( vec)));
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

	virtual std::vector<unsigned int> mapIndexToFeatures( unsigned int index) const
	{
		std::vector<unsigned int> rt;
		std::vector<FeatureIndex> res = m_sampleFeatureIndexMap.getValues( index);
		rt.reserve( res.size());
		std::vector<FeatureIndex>::const_iterator ri = res.begin(), re =  res.end();
		for (; ri != re; ++ri) rt.push_back( *ri);
		return rt;
	}

	virtual std::vector<unsigned int> mapFeatureToIndices( unsigned int feature) const
	{
		std::vector<unsigned int> rt;
		std::vector<FeatureIndex> res = m_featureSampleIndexMap.getValues( feature);
		rt.reserve( res.size());
		std::vector<SampleIndex>::const_iterator ri = res.begin(), re =  res.end();
		for (; ri != re; ++ri) rt.push_back( *ri);
		return rt;
	}

	virtual unsigned int nofFeatures() const
	{
		return m_individuals.size();
	}

	virtual unsigned int nofSamples() const
	{
		return m_sampleFeatureIndexMap.maxkey()+1;
	}

	virtual std::string config() const
	{
		return m_config.tostring();
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

	virtual void addVector( const std::vector<double>& vec)
	{
		try
		{
			if (m_modelLoadedFromFile)
			{
				throw strus::runtime_error(_TXT("no more samples accepted, model was loaded from file"));
			}
			m_samplear.push_back( m_lshmodel.simHash( arma::vec( vec)));
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

