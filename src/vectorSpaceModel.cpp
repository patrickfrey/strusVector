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
#include "strus/base/fileio.hpp"
#include "strus/base/configParser.hpp"
#include "strus/versionVector.hpp"
#include "strus/base/hton.hpp"
#include <armadillo>
#include <memory>

using namespace strus;
#define MODULENAME   "standard vector space model"
#define MATRIXFILE   "model.mat"
#define SIMRELFILE   "simrel.mat"
#define RESVECFILE   "result.lsh"
#define INPVECFILE   "input.lsh"
#define CONFIGFILE   "config.txt"
#define VERSIONFILE  "version.hdr"
#define DUMPFILE     "dump.txt"

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
		DefaultWithSingletons = 0
	};
	VectorSpaceModelConfig( const VectorSpaceModelConfig& o)
		:path(o.path),logfile(o.logfile),dim(o.dim),bits(o.bits),variations(o.variations)
		,simdist(o.simdist),raddist(o.raddist),eqdist(o.eqdist),mutations(o.mutations),votes(o.votes)
		,descendants(o.descendants),maxage(o.maxage),iterations(o.iterations)
		,assignments(o.assignments)
		,with_singletons(o.with_singletons)
		{}
	VectorSpaceModelConfig()
		:path(),logfile(),dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
		,simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
		,mutations(DefaultMutations),votes(DefaultMutationVotes)
		,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
		,assignments(DefaultAssignments)
		,with_singletons((bool)DefaultWithSingletons)
		{}
	VectorSpaceModelConfig( const std::string& config, ErrorBufferInterface* errorhnd)
		:path(),logfile(),dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
		,simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
		,mutations(DefaultMutations),votes(DefaultMutationVotes)
		,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
		,assignments(DefaultAssignments)
		,with_singletons((bool)DefaultWithSingletons)
	{
		std::string src = config;
		if (extractStringFromConfigString( path, src, "path", errorhnd)){}
		if (extractStringFromConfigString( logfile, src, "logfile", errorhnd)){}
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
		buf << "logfile=" << logfile << ";" << std::endl;
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
		buf << "singletons=" << (with_singletons?"yes":"no") << ";" << std::endl;
		return buf.str();
	}

	std::string path;
	std::string logfile;
	unsigned int dim;
	unsigned int bits;
	unsigned int variations;
	unsigned int simdist;
	unsigned int raddist;
	unsigned int eqdist;
	unsigned int mutations;
	unsigned int votes;
	unsigned int descendants;
	unsigned int maxage;
	unsigned int iterations;
	unsigned int assignments;
	bool with_singletons;
};

struct VectorSpaceModelData
{
	VectorSpaceModelData( const VectorSpaceModelConfig& config_)
		:config(config_){}

	VectorSpaceModelConfig config;
};

static void checkVectorSpaceModelVersion( const std::string& path)
{
	std::string versionfile( path + dirSeparator() + VERSIONFILE);
	std::string versionblob;
	unsigned int ec = readFile( versionfile, versionblob);
	if (ec) throw strus::runtime_error(_TXT("failed to load model version from file '%s' (errno %u): %s"), versionfile.c_str(), ec, ::strerror(ec));
	VectorSpaceModelHdr hdr;
	if (versionblob.size() != sizeof(hdr)) throw strus::runtime_error(_TXT("unknown file format"));
	std::memcpy( &hdr, versionblob.c_str(), sizeof(hdr));
	hdr.ntoh();
	hdr.check();
}

class VectorSpaceModelInstance
	:public VectorSpaceModelInstanceInterface
{
public:
	VectorSpaceModelInstance( const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_),m_lshmodel(0)
	{
		loadModelFromFile( m_config.path);
	}

	virtual ~VectorSpaceModelInstance()
	{
		if (m_lshmodel) delete( m_lshmodel);
	}

	virtual std::vector<unsigned int> mapVectorToFeatures( const std::vector<double>& vec) const
	{
		try
		{
			std::vector<unsigned int> rt;
			SimHash hash( m_lshmodel->simHash( arma::vec( vec)));
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

	virtual unsigned int nofFeatures() const
	{
		return m_individuals.size();
	}

	virtual std::string config() const
	{
		return m_config.tostring();
	}

private:
	void loadModelFromFile( const std::string& path);

#ifdef STRUS_LOWLEVEL_DEBUG
	std::string tostring() const;
#endif

private:
	ErrorBufferInterface* m_errorhnd;
	VectorSpaceModelConfig m_config;
	LshModel* m_lshmodel;
	std::vector<SimHash> m_individuals;
};

void VectorSpaceModelInstance::loadModelFromFile( const std::string& path)
{
	if (path.empty()) throw strus::runtime_error(_TXT("no 'path' configuration variable defined, cannot load model"));
	// Read and check version:
	checkVectorSpaceModelVersion( path);
	unsigned int ec;

	// Read configuration:
	std::string configfile( path + dirSeparator() + CONFIGFILE);
	std::string configblob;
	ec = readFile( configfile, configblob);
	if (ec) throw strus::runtime_error(_TXT("failed to load config from file '%s' (errno %u): %s"), configfile.c_str(), ec, ::strerror(ec));
	try
	{
		m_config = VectorSpaceModelConfig( configblob, m_errorhnd);
	}
	catch (const std::runtime_error& err)
	{
		throw strus::runtime_error(_TXT("vector space model is corrupt or has different version (%s)"), err.what());
	}
	m_config.path = path;

	// Read LSH model matrices:
	std::string lshmodelfile( path + dirSeparator() + MATRIXFILE);
	std::string lshmodelblob;
	ec = readFile( lshmodelfile, lshmodelblob);
	if (ec) throw strus::runtime_error(_TXT("failed to load model from file '%s' (errno %u): %s"), lshmodelfile.c_str(), ec, ::strerror(ec));
	std::auto_ptr<LshModel> lshmodel( LshModel::createFromSerialization( lshmodelblob));

	// Read learnt centroid vectors:
	std::string veclshfile( path + dirSeparator() + RESVECFILE);
	std::string veclshblob;
	ec = readFile( veclshfile, veclshblob);
	if (ec) throw strus::runtime_error(_TXT("failed to load lsh values from file '%s' (errno %u): %s"), veclshfile.c_str(), ec, ::strerror(ec));
	m_individuals = SimHash::createFromSerialization( veclshblob);

#ifdef STRUS_LOWLEVEL_DEBUG
	std::string txtdumpfile( path + dirSeparator() + DUMPFILE);
	ec = writeFile( txtdumpfile, tostring());
	if (ec) throw strus::runtime_error(_TXT("failed to store debug text dump of instance loaded to file '%s' (system error %u: %s)"), txtdumpfile.c_str(), ec, ::strerror(ec));
#endif
	m_lshmodel = lshmodel.release();
}

#ifdef STRUS_LOWLEVEL_DEBUG
std::string VectorSpaceModelInstance::tostring() const
{
	std::ostringstream txtdump;
	txtdump << "LSH:" << std::endl << m_lshmodel->tostring() << std::endl;
	txtdump << "GEN:" << std::endl;
	std::vector<SimHash>::const_iterator ii = m_individuals.begin(), ie = m_individuals.end();
	for (; ii != ie; ++ii)
	{
		txtdump << ii->tostring() << std::endl;
	}
	txtdump << std::endl;
	return txtdump.str();
}
#endif

class VectorSpaceModelBuilder
	:public VectorSpaceModelBuilderInterface
{
public:
	VectorSpaceModelBuilder( const std::string& config_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_config(config_,errorhnd_),m_lshmodel(0),m_genmodel(0)
	{
		try
		{
			m_lshmodel = new LshModel( m_config.dim, m_config.bits, m_config.variations);
			m_genmodel = new GenModel( m_config.simdist, m_config.raddist, m_config.eqdist, m_config.mutations, m_config.votes, m_config.descendants, m_config.maxage, m_config.iterations, m_config.assignments, m_config.with_singletons);
		}
		catch (const std::exception& err)
		{
			if (m_lshmodel)
			{
				delete m_lshmodel;
				m_lshmodel = 0;
			}
			if (m_genmodel)
			{
				delete m_genmodel;
				m_genmodel = 0;
			}
			throw strus::runtime_error( err.what());
		}
	}
	virtual ~VectorSpaceModelBuilder()
	{
		if (m_lshmodel) delete m_lshmodel;
		if (m_genmodel) delete m_genmodel;
	}

	virtual void addSampleVector( const std::vector<double>& vec)
	{
		try
		{
#ifdef STRUS_LOWLEVEL_DEBUG
			m_samplevecar.push_back( vec);
#endif
			m_samplear.push_back( m_lshmodel->simHash( arma::vec( vec)));
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error adding sample vector to '%s' builder: %s"), MODULENAME, *m_errorhnd);
	}

	virtual bool finalize()
	{
		try
		{
			const char* logfile = m_config.logfile.empty()?0:m_config.logfile.c_str();

			SimRelationMap simrelmap;
			std::string simrelfile( m_config.path + dirSeparator() + SIMRELFILE);
			if (isFile( simrelfile))
			{
				checkVectorSpaceModelVersion( m_config.path);
				std::string simrelblob;
				unsigned int ec = readFile( simrelfile, simrelblob);
				if (ec) throw strus::runtime_error(_TXT("failed to load precalculated similarity relation matrix from file '%s' (errno %u): %s"), simrelfile.c_str(), ec, ::strerror(ec));
				m_simrelmap = SimRelationMap::fromSerialization( simrelblob);
			}
			else
			{
				m_simrelmap = m_genmodel->getSimRelationMap( m_samplear, logfile);
			}
			m_resultar = m_genmodel->run( m_samplear, m_simrelmap, logfile);
			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error finalizing '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

	virtual bool store()
	{
		try
		{
			unsigned int ec;
			// Create directory for model files:
			if (m_config.path.empty()) throw strus::runtime_error(_TXT("failed to store built instance (no file configured)"));
			ec = createDir( m_config.path, true);
			if (ec) throw strus::runtime_error(_TXT( "failed to create directory '%s' to store model built (system error %u: %s)"), m_config.path.c_str(), ec, ::strerror(ec));
#ifdef STRUS_LOWLEVEL_DEBUG
			std::string txtdumpfile( path + dirSeparator() + DUMPFILE);
			ec = writeFile( txtdumpfile, tostring());
			if (ec)
			{
				throw strus::runtime_error(_TXT("failed to store debug text dump of instance built (system error %u: %s)"), ec, ::strerror(ec));
			}
#endif
			// Write version:
			std::string versionfile( m_config.path + dirSeparator() + VERSIONFILE);
			std::string versionblob;
			VectorSpaceModelHdr hdr;
			hdr.hton();
			versionblob.append( (const char*)&hdr, sizeof(hdr));
			ec = writeFile( versionfile, versionblob);
			if (ec) throw strus::runtime_error(_TXT("failed to write model version to file '%s' (errno %u): %s"), versionfile.c_str(), ec, ::strerror(ec));

			// Write configuration:
			std::string configfile( m_config.path + dirSeparator() + CONFIGFILE);
			ec = writeFile( configfile, m_config.tostring());
			if (ec) throw strus::runtime_error(_TXT("failed to write config to file '%s' (errno %u): %s"), configfile.c_str(), ec, ::strerror(ec));

			// Write similarity relation file if not yet written:
			std::string simrelfile( m_config.path + dirSeparator() + SIMRELFILE);
			if (!isFile( simrelfile))
			{
				ec = writeFile( simrelfile, m_simrelmap.serialization());
				if (ec) throw strus::runtime_error(_TXT("failed to write similarity relation matrix to file '%s' (errno %u): %s"), simrelfile.c_str(), ec, ::strerror(ec));
			}

			// Write LSH model matrices:
			std::string lshmodelfile( m_config.path + dirSeparator() + MATRIXFILE);
			ec = writeFile( lshmodelfile, m_lshmodel->serialization());
			if (ec) throw strus::runtime_error(_TXT("failed to store lhs model to file '%s' (errno %u): %s"), lshmodelfile.c_str(), ec, ::strerror(ec));

			// Write learnt centroid vectors:
			std::string veclshfile( m_config.path + dirSeparator() + RESVECFILE);
			ec = writeFile( veclshfile, SimHash::serialization( m_resultar));
			if (ec) throw strus::runtime_error(_TXT("failed to write learnt lsh values to file '%s' (errno %u): %s"), veclshfile.c_str(), ec, ::strerror(ec));

			// Write input sample vectors:
			std::string samplefile( m_config.path + dirSeparator() + INPVECFILE);
			ec = writeFile( samplefile, SimHash::serialization( m_samplear));
			if (ec) throw strus::runtime_error(_TXT("failed to write sample lsh values to file '%s' (errno %u): %s"), samplefile.c_str(), ec, ::strerror(ec));

			return true;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error storing '%s' builder: %s"), MODULENAME, *m_errorhnd, false);
	}

private:
#ifdef STRUS_LOWLEVEL_DEBUG
	std::string tostring() const
	{
		std::ostringstream txtdump;
		txtdump << "DATA:" << std::endl;
		std::vector<std::vector<double> >::const_iterator vi = m_samplevecar.begin(), ve = m_samplevecar.end();
		for (unsigned int vidx=0; vi != ve; ++vi,++vidx)
		{
			txtdump << "sample [" << vidx << "] ";
			std::vector<double>::const_iterator xi = vi->begin(), xe = vi->end();
			for (unsigned int xidx=0; xi != xe; ++xi,++xidx)
			{
				if (xidx) txtdump << ", ";
				txtdump << *xi;
			}
			txtdump << std::endl;
		}
		std::vector<SimHash>::const_iterator si = m_samplear.begin(), se = m_samplear.end();
		for (unsigned int sidx=0; si != se; ++si,++sidx)
		{
			txtdump << "hash [" << sidx << "] " << si->tostring() << std::endl;
		}
		std::vector<SimHash>::const_iterator ri = m_resultar.begin(), re = m_resultar.end();
		for (unsigned int ridx=0; ri != re; ++ri,++ridx)
		{
			txtdump << "result [" << ridx << "] " << ri->tostring() << std::endl;
		}
		txtdump << "LSH:" << std::endl << m_lshmodel->tostring() << std::endl;
		txtdump << "GEN:" << std::endl << m_genmodel->tostring() << std::endl;
		return txtdump.str();
	}
#endif

private:
	ErrorBufferInterface* m_errorhnd;
	VectorSpaceModelConfig m_config;
	SimRelationMap m_simrelmap;
	LshModel* m_lshmodel;
	GenModel* m_genmodel;
#ifdef STRUS_LOWLEVEL_DEBUG
	std::vector<std::vector<double> > m_samplevecar;
#endif
	std::vector<SimHash> m_samplear;
	std::vector<SimHash> m_resultar;
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
		ec = removeFile( std::string( config.path + dirSeparator() + MATRIXFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), MATRIXFILE, ::strerror(ec), ec);
		ec = removeFile( std::string( config.path + dirSeparator() + SIMRELFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), SIMRELFILE, ::strerror(ec), ec);
		ec = removeFile( std::string( config.path + dirSeparator() + RESVECFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), RESVECFILE, ::strerror(ec), ec);
		ec = removeFile( std::string( config.path + dirSeparator() + INPVECFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), INPVECFILE, ::strerror(ec), ec);
		ec = removeFile( std::string( config.path + dirSeparator() + CONFIGFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), CONFIGFILE, ::strerror(ec), ec);
		ec = removeFile( std::string( config.path + dirSeparator() + VERSIONFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), VERSIONFILE, ::strerror(ec), ec);
		ec = removeFile( std::string( config.path + dirSeparator() + DUMPFILE));
		if (ec) throw strus::runtime_error(_TXT("failed to remove file '%s': %s (%u)"), DUMPFILE, ::strerror(ec), ec);
		ec = removeDir( config.path);
		if (ec) throw strus::runtime_error(_TXT("failed to remove directory '%s': %s (%u)"), config.path.c_str(), ::strerror(ec), ec);
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error destroying a '%s' model: %s"), MODULENAME, *m_errorhnd, false);
}

