/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program
#include "strus/lib/vectorspace_std.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/lib/error.hpp"
#include "strus/vectorSpaceModelInterface.hpp"
#include "strus/vectorSpaceModelInstanceInterface.hpp"
#include "strus/vectorSpaceModelBuilderInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "indexListMap.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "random.hpp"
#include "sampleSimGroupMap.hpp"
#include "simGroup.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "sparseDim2Field.hpp"
#include "armadillo"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <limits>

#undef STRUS_LOWLEVEL_DEBUG

static void initRandomNumberGenerator()
{
	time_t nowtime;
	struct tm* now;

	::time( &nowtime);
	now = ::localtime( &nowtime);

	unsigned int seed = (now->tm_year+10000) + (now->tm_mon+100) + (now->tm_mday+1);
	std::srand( seed+2);
}

static std::vector<double> convertVectorStd( const arma::vec& vec)
{
	std::vector<double> rt;
	arma::vec::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		rt.push_back( *vi);
	}
	return rt;
}

std::vector<double> getRandomVector( unsigned int dim)
{
	return convertVectorStd( (arma::randu<arma::vec>( dim) - 0.5) * 2.0); // values between -1.0 and 1.0
}

bool parseUint( unsigned int& res, const std::string& numstr)
{
	res = 0;
	std::string::const_iterator ni = numstr.begin(), ne = numstr.end();
	for (; ni != ne; ++ni)
	{
		if (*ni >= '0' && *ni <= '9') res = res * 10 + (*ni - '0'); else break;
	}
	return (ni != ne);
}

static strus::ErrorBufferInterface* g_errorhnd = 0;

static unsigned int getNofFeatures( unsigned int nofSamples)
{
	return nofSamples / 3 + nofSamples / 2;
}

static std::string getSampleName( unsigned int sidx)
{
	return strus::string_format( "S%u", sidx);
}

static strus::SimRelationMap getSimRelationMap( unsigned int nofSamples, unsigned short maxsimdist)
{
	strus::Random rnd;
	strus::SimRelationMap rt;
	unsigned int si = 0, se = nofSamples;
	for (; si != se; ++si)
	{
		if (rnd.get( 1, 3) < 2) continue; 
		std::vector<strus::SimRelationMap::Element> elems;
		unsigned int oi = 0, oe = nofSamples, step = rnd.get( 1, nofSamples/2); 
		for (; oi < oe; oi += step)
		{
			unsigned short simdist = rnd.get( 1, maxsimdist);
			elems.push_back( strus::SimRelationMap::Element( oi, simdist));
		}
		rt.addRow( si, elems);
	}
	return rt;
}

strus::SampleFeatureIndexMap getSampleFeatureIndexMap( unsigned int nofSamples)
{
	strus::Random rnd;
	unsigned int nofFeatures = getNofFeatures( nofSamples);
	strus::IndexListMap<strus::Index,strus::Index> rt;
	unsigned int si = 0, se = nofSamples;
	for (; si != se; ++si)
	{
		if (rnd.get( 1, 3) < 2) continue;
		std::vector<strus::Index> elems;
		unsigned int fi = 1, fe = nofFeatures+1, step = 1+rnd.get( 0, nofFeatures/2); 
		for (; fi < fe; fi += step)
		{
			if (rnd.get( 0, 4) == 0) continue;
			elems.push_back( fi);
		}
		rt.add( si, elems);
	}
	return rt;
}

strus::FeatureSampleIndexMap getFeatureSampleIndexMap( unsigned int nofSamples)
{
	strus::IndexListMap<strus::Index,strus::Index> rt;
	unsigned int nofFeatures = getNofFeatures( nofSamples);
	strus::IndexListMap<strus::Index,strus::Index> rt;
	unsigned int fi = 1, fe = nofFeatures+1;
	for (; fi < fe; ++fi)
	{
		if (rnd.get( 1, 3) < 2) continue;
		std::vector<strus::Index> elems;
		unsigned int si = 0, se = nofSamples, step = 1+rnd.get( 0, nofSamples/2); 
		for (; si != se; si += step)
		{
			elems.push_back( si);
		}
		rt.add( fi, elems);
	}
	return rt;
}

struct TestDataset
{
	TestDataset( const strus::VectorSpaceModelConfig& config, unsigned int nofSamples_)
		:nofFeatures(getNofFeatures( nofSamples_))
		,nofSamples(nofSamples_)
		,lshmodel( config.dim, config.bits, config.variations)
		,simrelmap(getSimRelationMap( nofSamples, config.simdist))
		,sfmap(getSampleFeatureIndexMap( nofSamples))
		,fsmap(getFeatureSampleIndexMap( nofSamples))
		,sampleNames(),sampleVectors(),sampleSimHashs()
	{
		unsigned int si = 0, se = nofSamples;
		for (; si != se; ++si)
		{
			std::vector<double> vec = getRandomVector( config.dim);
			sampleNames.push_back( getSampleName(si));
			sampleVectors.push_back( vec);
			sampleSimHashs.push_back( lshmodel.simHash( vec));
		}
	}

	unsigned int nofFeatures;
	unsigned int nofSamples;
	strus::LshModel lshmodel;
	strus::SimRelationMap simrelmap;
	strus::SampleFeatureIndexMap sfmap;
	strus::FeatureSampleIndexMap fsmap;
	std::vector< std::string> sampleNames;
	std::vector< std::vector<double> > sampleVectors;
	std::vector< strus::SimHash> sampleSimHashs;
};


static void writeDatabase( const strus::VectorSpaceModelConfig& config, const TestDataset& dataset)
{
	std::auto_ptr<strus::DatabaseInterface> dbi( createDatabase_leveldb( g_errorhnd));
	strus::DatabaseAdapter database( dbi.get(), config.databaseConfig, g_errorhnd);

	database.writeVersion();
	database.writeVariables( dataset.nofSamples, dataset.nofFeatures);
	unsigned int si = 0, se = nofSamples;
	for (; si != se; ++si)
	{
		writeSample( si, dataset.sampleNames[si], dataset.sampleVectors[si], dataset.sampleSimHashs[si]);
	}
	database.writeResultSimhashVector( dataset.sampleSimHashs);
	database.writeSimRelationMap( dataset.simrelmap);
	database.writeSampleFeatureIndexMap( dataset.sfmap);
	database.writeFeatureSampleIndexMap( dataset.fsmap);
	database.writeConfig( config.tostring());
	database.writeLshModel( dataset.lshmodel);
}

static bool compare( const std::vector<double>& v1, const std::vector<double>& v2)
{
	std::vector<double>::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	std::vector<double>::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2)
	{
		diff = (vi1 > vi2)?(vi1 - vi2):(vi2 - vi1);
		if (diff > std::numeric_limits<double>::epsilon()) return false;
	}
	return vi1 == ve1 && vi2 == ve2;
}

static bool compare( const std::vector<strus::SimHash>& v1, const std::vector<strus::SimHash>& v2)
{
	std::vector<double>::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	std::vector<double>::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2)
	{
		if (*vi1 != *vi2) return false;
	}
	return vi1 == ve1 && vi2 == ve2;
}

static bool compare( const strus::SimRelationMap& m1, const strus::SimRelationMap& m2)
{
	if (m1.nofSamples() != m2.nofSamples()) return false;
	unsigned int si = 0, s2 = m1.nofSamples();
	for (; si != se; ++si)
	{
		strus::SimRelationMap::Row r1 = m1.row( si);
		strus::SimRelationMap::Row r2 = m2.row( si);
		strus::SimRelationMap::Row::const_iterator
				ri1 = r1.begin(), re1 = r1.end(), 
				ri2 = r2.begin(), re2 = r2.end();
		for (; ri1 != ri2 && ri2 != re2; ++ri1,++ri2)
		{
			if (ri1->index != ri2->index || ri1->simdist != ri2->simdist) return false;
		}
		if (ri1 != re1 || ri2 != re2) return false;
	}
	return true;
}

static bool compare( const strus::IndexListMap<strus::Index>& m1, const strus::IndexListMap<strus::Index>& m2)
{
	unsigned int si = 0, s2 = m1.maxkey()+1;
	for (; si != se; ++si)
	{
		std::vector<strus::Index> v1 = m1.getValues( si);
		std::vector<strus::Index> v2 = m2.getValues( si);
		std::vector<strus::Index>::const_iterator
				vi1 = v1.begin(), ve1 = v1.end(), 
				vi2 = v2.begin(), ve2 = v2.end();
		for (; vi1 != vi2 && vi2 != ve2; ++vi1,++vi2)
		{
			if (*vi1 != *vi2) return false;
		}
		if (vi1 != ve1 || vi2 != ve2) return false;
	}
	return true;
}

static bool compare( const strus::LshModel& m1, const strus::LshModel& m2)
{
	return m1.isequal( m2);
}

static void readAndCheckDatabase( const strus::VectorSpaceModelConfig& config, const TestDataset& dataset)
{
	std::auto_ptr<strus::DatabaseInterface> dbi( createDatabase_leveldb( g_errorhnd));
	strus::DatabaseAdapter database( dbi.get(), config.databaseConfig, g_errorhnd);

	database.checkVersion();
	if (dataset.nofSamples != database.readNofSamples()) throw std::runtime_error("number of samples does not match");
	if (dataset.nofFeatures != database.readNofFeatures()) throw std::runtime_error("number of features does not match");

	unsigned int si = 0, se = dataset.nofSamples;
	for (; si != se; ++si)
	{
		if (!compare( readSampleVector( si), dataset.sampleVectors[ si])) throw std::runtime_error("sample vectors do not match");
		if (readSampleName( si) != dataset.sampleNames[si]) throw std::runtime_error("sample names do not match");
		if (readSampleIndex( dataset.sampleNames[si]) != si) throw std::runtime_error("sample indices got by name do not match");
	}
	if (!compare( dataset.sampleSimHashs, database.readSampleSimhashVector())) throw std::runtime_error("sample sim hash values do not match");
	if (!compare( dataset.sampleSimHashs, database.readResultSimhashVector())) throw std::runtime_error("result sim hash values do not match");

	if (!compare( dataset.simrelmap, database.readSimRelationMap())) throw std::runtime_error("result sim relation map does not match");
	if (!compare( dataset.sfmap, database.readSampleFeatureIndexMap())) throw std::runtime_error("sample feature index map does not match");
	if (!compare( dataset.fsmap, database.readFeatureSampleIndexMap())) throw std::runtime_error("feature sample index map does not match");
	if (config.tostring() != database.readConfig()) throw std::runtime_error("configuration does not match");
	if (!compare( dataset.lshmodel, database.readLshModel())) throw std::runtime_error("LSH model does not match");
}


#define DEFAULT_CONFIG \
	"path=vsmodel;"\
	"logfile=-;"\
	"dim=300;"\
	"bit=64;"\
	"var=32;"\
	"simdist=340;"\
	"raddist=250;"\
	"eqdist=80;"\
	"mutations=20;"\
	"votes=5;"\
	"descendants=5;"\
	"maxage=20;"\
	"iterations=20;"\
	"assignments=7;"\
	"singletons=no"

int main( int argc, const char** argv)
{
	try
	{
		int rt = 0;
		g_errorhnd = strus::createErrorBuffer_standard( 0, 0);
		if (!g_errorhnd) throw std::runtime_error("failed to create error buffer structure");

		initRandomNumberGenerator();
		std::string configstr( DEFAULT_CONFIG);
		unsigned int nofSamples = 1000;
		bool printUsageAndExit = false;

		// Parse parameters:
		int argidx = 1;
		bool finished_options = false;
		while (!finished_options && argc > argidx && argv[argidx][0] == '-')
		{
			if (0==std::strcmp( argv[argidx], "-h"))
			{
				printUsageAndExit = true;
			}
			else if (0==std::strcmp( argv[argidx], "-s"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -s needs argument (configuration string)" << std::endl;
					printUsageAndExit = true;
				}
				configstr = argv[++argidx];
			}
			else if (0==std::strcmp( argv[argidx], "--"))
			{
				finished_options = true;
			}
			++argidx;
		}
		if (argc > argidx + 1)
		{
			std::cerr << "too many arguments (maximum 1 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}
		else
		{
			if (!parseUint( nofSamples, argv[ argidx]))
			{
				std::cerr << "non negative number (number of examples) expected as first argument" << std::endl;
				rt = 1;
				printUsageAndExit = true;
			}
		}
		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<options>] [<number of examples>]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h           : print this usage" << std::endl;
			std::cerr << "-s <CONFIG>  : specify test configuration string as <CONFIG>" << std::endl;
			return rt;
		}
		strus::VectorSpaceModelConfig config( configstr, g_errorhnd);
		if (g_errorhnd->hasError()) throw std::runtime_error("error in test configuration");

		TestDataset dataset( lshmodel, const strus::VectorSpaceModelConfig& config, unsigned int nofSamples)
		writeDatabase( lshmodel, config, nofSamples);
		readAndCheckDatabase( lshmodel, config, nofSamples);

		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "uncaught exception");
		}
		std::cerr << "done" << std::endl;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		return -2;
	}
	catch (const std::logic_error& err)
	{
		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;
		return -3;
	}
}

