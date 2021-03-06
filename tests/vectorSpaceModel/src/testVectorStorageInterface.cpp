/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program
#include "strus/lib/vector_std.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/lib/error.hpp"
#include "strus/lib/filelocator.hpp"
#include "strus/vectorStorageInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/fileLocatorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/storage/wordVector.hpp"
#include "strus/storage/vectorQueryResult.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/local_ptr.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/numstring.hpp"
#include "strus/base/pseudoRandom.hpp"
#include "strus/base/math.hpp"
#include "vectorUtils.hpp"
#include "armadillo"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <iomanip>
#include <cstdlib>
#include <limits>
#include <algorithm>

#define VEC_EPSILON 1e-5
static bool g_verbose = false;
static strus::PseudoRandom g_random;
static strus::ErrorBufferInterface* g_errorhnd = 0;
static strus::FileLocatorInterface* g_fileLocator = 0;

#define DEFAULT_CONFIG "path=vstorage"

static std::string getFeatureName( unsigned int idx)
{
	return strus::string_format( "F%u", idx);
}

static std::string getTypeName( unsigned int idx)
{
	return strus::string_format( "T%u", idx);
}

static bool compareResult( const std::vector<strus::VectorQueryResult>& result, const std::vector<strus::VectorQueryResult>& expect, float min_weight)
{
	std::vector<strus::VectorQueryResult>::const_iterator ri = result.begin(), re = result.end();
	for (; ri != re; ++ri)
	{
		if (ri->weight() > min_weight)
		{
			std::vector<strus::VectorQueryResult>::const_iterator ei = expect.begin(), ee = expect.end();
			for (; ei != ee; ++ei)
			{
				if (ei->value() == ri->value()) break;
			}
			if (ei == ee) return false;
		}
	}
	return true;
}

template <typename Element>
static void printArray( std::ostream& out, const std::vector<Element>& ar, const char* separator)
{
	typename std::vector<Element>::const_iterator ai = ar.begin(), ae = ar.end();
	for (int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (aidx) out << separator;
		out << *ai;
	}
}

static void printResult( std::ostream& out, const std::vector<strus::VectorQueryResult>& result)
{
	std::vector<strus::VectorQueryResult>::const_iterator ri = result.begin(), re = result.end();
	for (; ri != re; ++ri)
	{
		out << ri->value() << " " << ri->weight() << std::endl;
	}
}

struct FeatureDef
{
	std::string type;
	std::string feat;
	strus::WordVector vec;

	FeatureDef( const std::string& type_, const std::string& feat_, const strus::WordVector& vec_)
		:type(type_),feat(feat_),vec(vec_){}
	FeatureDef( const FeatureDef& o)
		:type(o.type),feat(o.feat),vec(o.vec){}
};

int main( int argc, const char** argv)
{
	try
	{
		int rt = 0;
		g_errorhnd = strus::createErrorBuffer_standard( 0, 0, NULL/*debug trace interface*/);
		if (!g_errorhnd) {std::cerr << "FAILED " << "strus::createErrorBuffer_standard" << std::endl; return -1;}
		g_fileLocator = strus::createFileLocator_std( g_errorhnd);
		if (!g_fileLocator) {std::cerr << "FAILED " << "strus::createFileLocator_std" << std::endl; return -1;}

		std::string configstr( DEFAULT_CONFIG);
		strus::Index nofTypes = 2;
		strus::Index nofFeatures = 1000;
		unsigned int vecdim = 300/*strus::VectorStorage::DefaultDim*/;
		unsigned int threads = 0;
		std::string workdir = "./";
		bool printUsageAndExit = false;
		float sim_cos = 0.9;
		float result_sim_cos = 0.95;
		double minSimilarity = 0.9;
		double speedRecallFactor = 0.88;

		// Parse parameters:
		int argidx = 1;
		bool finished_options = false;
		while (!finished_options && argc > argidx && argv[argidx][0] == '-')
		{
			if (0==std::strcmp( argv[argidx], "-h"))
			{
				printUsageAndExit = true;
			}
			else if (0==std::strcmp( argv[argidx], "-V"))
			{
				g_verbose = true;
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
		if (argc > argidx)
		{
			workdir = argv[ argidx++];
		}
		if (argc > argidx)
		{
			nofFeatures = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
		}
		if (argc > argidx)
		{
			nofTypes = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
		}
		if (argc > argidx)
		{
			std::cerr << "too many arguments (maximum 3 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}

		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<options>] [<workdir>] [<number of features>] [<number of types>]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h                     : print this usage" << std::endl;
			std::cerr << "-V                     : verbose output to stderr" << std::endl;
			std::cerr << "-s <CONFIG>            :specify test configuration string as <CONFIG>" << std::endl;
			std::cerr << "<workdir>              :working directory, default './'" << std::endl;
			std::cerr << "<number of features>   :number of features, default 1000" << std::endl;
			std::cerr << "<number of types>      :number of types, default 1" << std::endl;
			return rt;
		}
		if (g_errorhnd->hasError()) throw std::runtime_error("error in test configuration");

		if (g_verbose) std::cerr << "model config: " << configstr << std::endl;
		{
			std::string configsrc = configstr;
			(void)extractUIntFromConfigString( threads, configsrc, "threads", g_errorhnd);
			(void)extractUIntFromConfigString( vecdim, configsrc, "vecdim", g_errorhnd);
		}
		if (threads > 1)
		{
			delete g_errorhnd;
			g_errorhnd = strus::createErrorBuffer_standard( 0, threads+2, NULL/*debug trace interface*/);
			if (!g_errorhnd) throw std::runtime_error("failed to create error buffer structure");
		}
		// Build all objects:
		strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_fileLocator, g_errorhnd));
		strus::local_ptr<strus::VectorStorageInterface> sti( strus::createVectorStorage_std( g_fileLocator, g_errorhnd));
		if (!dbi.get() || !sti.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());

		// Remove traces of old test model before creating a new one:
		if (g_verbose) std::cerr << "create test repository ..." << std::endl;
		if (!dbi->destroyDatabase( configstr))
		{
			(void)g_errorhnd->fetchError();
		}
		// Creating the vector storage:
		if (!sti->createStorage( configstr, dbi.get()))
		{
			throw std::runtime_error( g_errorhnd->fetchError());
		}
		strus::local_ptr<strus::VectorStorageClientInterface> storage( sti->createClient( configstr, dbi.get()));
		if (!storage.get()) throw std::runtime_error( g_errorhnd->fetchError());

		std::map<strus::Index,std::vector<strus::Index> > featureTypeRelMap;

		std::vector<FeatureDef> defs;
		int modsim = std::min( 10, nofTypes * nofFeatures / 10 + 2);
		strus::Index nofVectors = 0;
		{
			if (g_verbose) std::cerr << "build test vectors ..." << std::endl;
			strus::WordVector vec;

			// Build the test vectors:
			strus::Index ti = 1, te = nofTypes;
			for (; ti <= te; ++ti)
			{
				strus::Index ni = 1, ne = nofFeatures;
				for (; ni <= ne; ++ni)
				{
					if (g_random.get( 0, 2) == 1)
					{
						featureTypeRelMap[ ni].push_back( ti);
						const char* gentype = "empty";

						if (g_random.get( 0, 2) == 1)
						{
							if (g_random.get( 0, modsim) == 1 && !vec.empty())
							{
								float ff = (float)(g_random.get( 1, 1001)) / 1000.0;
								float randsim = sim_cos + (1.0 - sim_cos) * ff;
								vec = strus::test::createSimilarVector( g_random, vec, randsim);
								gentype = "similar";
							}
							else
							{
								vec = strus::test::createRandomVector( g_random, vecdim);
								gentype = "random";
							}
							++nofVectors;
						}
						else
						{
							vec.clear();
						}
						defs.push_back( FeatureDef( getTypeName(ti), getFeatureName(ni), vec));
						if (vec.empty())
						{
							if (g_verbose) std::cerr << strus::string_format( "add feature %s '%s' no vector", defs.back().type.c_str(),defs.back().feat.c_str()) << std::endl;
						}
						else
						{
							std::string vecstr = defs.back().vec.tostring( ", ", 6);
							if (g_verbose) std::cerr << strus::string_format( "add feature %s '%s' vector %s (%s ...)", defs.back().type.c_str(),defs.back().feat.c_str(), gentype, vecstr.c_str()) << std::endl;
						}
					}
				}
			}
			if (g_verbose) std::cerr << strus::string_format( "build %d types, %d features and %d vectors.", nofTypes, nofFeatures, nofVectors) << std::endl;
		}{
			if (g_verbose) std::cerr << "insert test vectors ..." << std::endl;
			strus::local_ptr<strus::VectorStorageTransactionInterface> transaction( storage->createTransaction());
			if (!transaction.get()) throw std::runtime_error( g_errorhnd->fetchError());

			int nofCommits = 0;
			std::vector<FeatureDef>::const_iterator di = defs.begin(), de = defs.end();
			for (; di != de; ++di)
			{
				if (g_errorhnd->hasError())
				{
					throw std::runtime_error( "error inserting vectors");
				}
				if (di->vec.empty())
				{
					transaction->defineFeature( di->type, di->feat);
				}
				else
				{
					transaction->defineVector( di->type, di->feat, di->vec);
				}
				if (g_random.get( 0, 1000 == 1))
				{
					if (!transaction->commit())
					{
						throw std::runtime_error( "building example vector set failed");
					}
					transaction.reset( storage->createTransaction());
					if (!transaction.get()) throw std::runtime_error( g_errorhnd->fetchError());
					++nofCommits;
				}
			}
			if (!transaction->commit())
			{
				throw std::runtime_error( "building example vector set failed");
			}
			++nofCommits;
			if (g_verbose) std::cerr << strus::string_format( "inserted %d vectors, %d features, %d types, in %d commits.", nofVectors, nofFeatures, nofTypes, nofCommits) << std::endl;
		}
		typedef strus::VectorQueryResult SimFeat;
		typedef std::vector<strus::VectorQueryResult> SimFeatList;
		typedef std::map<std::string,SimFeatList> SimMatrix;
		typedef std::map<std::string,SimMatrix> SimMatrixMap;
		SimMatrixMap simMatrixMap;
		int nofSimilarities = 0;
		int nofSimilaritiesItself = 0;
		{
			// Create the real similarity matrix based on cosine distance of the sample vectors to verify the results:
			if (g_verbose) std::cerr << "create similarity matrix ..." << std::endl;
			{
				strus::Index ti = 1, te = nofTypes;
				for (; ti <= te; ++ti)
				{
					simMatrixMap[ getTypeName( ti)];
				}
			}
			std::vector<FeatureDef>::const_iterator di = defs.begin(), de = defs.end();
			for (; di != de; ++di)
			{
				if (!di->vec.empty())
				{
					SimMatrix& simMatrix = simMatrixMap[ di->type];
					std::vector<FeatureDef>::const_iterator oi = defs.begin(), oe = defs.end();
					for (; oi != oe; ++oi)
					{
						if (!oi->vec.empty() && di->type == oi->type)
						{
							double sim = arma::norm_dot( arma::fvec(di->vec), arma::fvec(oi->vec));
							if (di->feat == oi->feat && sim < 1.000 - VEC_EPSILON)
							{
								std::cerr << "vector 1: (" << di->vec.tostring( ", ", 20) << " ...)" << std::endl;
								std::cerr << "vector 2: (" << oi->vec.tostring( ", ", 20) << " ...)" << std::endl;
								char sbuf[ 32];
								std::snprintf( sbuf, sizeof(sbuf), "%.5f", (float)sim);
								std::cerr << "similarity: " << sbuf << std::endl;
								throw std::runtime_error("similarity to itself is not equal to 1");
							}
							if (sim > sim_cos)
							{
								if (di->feat != oi->feat)
								{
									++nofSimilarities;
									char simstrbuf[ 32];
									std::snprintf( simstrbuf, sizeof(simstrbuf), "%.5f", sim);
									if (g_verbose) std::cerr << strus::string_format( "type %s insert similar vector of '%s ': '%s' similarity %s", di->type.c_str(), di->feat.c_str(), oi->feat.c_str(), simstrbuf) << std::endl;
								}
								else
								{
									++nofSimilaritiesItself;
									if (di->feat != oi->feat) throw std::runtime_error( "impossible state in sim matrix build");
									if (g_verbose) std::cerr << strus::string_format( "type %s insert similarity to itself '%s'", di->type.c_str(), di->feat.c_str()) << std::endl; 
								}
								simMatrix[ di->feat].push_back( SimFeat( oi->feat, sim));
							}
							double vv = storage->vectorSimilarity( di->vec, oi->vec) - sim;
							if (std::fabs(vv) > VEC_EPSILON)
							{
								throw std::runtime_error("calculated vector similarity error beyond tolerable value");
							}
						}
					}
				}
			}
			if (g_verbose) std::cerr << strus::string_format( "got %d * 2 (reflexive) similarities without similarities to itself (%d) counted", (nofSimilarities/2), nofSimilaritiesItself) << std::endl;
		}{
			std::map<std::string,int> vectorcnt;
			{
				strus::Index ti = 1, te = nofTypes;
				for (; ti <= te; ++ti)
				{
					vectorcnt[ getTypeName(ti)] = 0;
				}
			}{
				std::vector<FeatureDef>::const_iterator di = defs.begin(), de = defs.end();
				for (; di != de; ++di)
				{
					strus::WordVector vec = storage->featureVector( di->type, di->feat);
					if (!strus::test::compareVector( di->vec, vec, VEC_EPSILON))
					{
						throw std::runtime_error("stored vector value does not match its image");
					}
					if (!di->vec.empty())
					{
						vectorcnt[ di->type] += 1;
					}
				}
			}{
				if (g_verbose) std::cerr << "test reading of vector counts ..." << std::endl;
				strus::Index ti = 1, te = nofTypes;
				for (; ti <= te; ++ti)
				{
					if (storage->nofVectors( getTypeName(ti)) != vectorcnt[ getTypeName(ti)])
					{
						throw std::runtime_error("stored vector count does not match");
					}
				}
			}{
				if (g_verbose) std::cerr << "test reading of types  ..." << std::endl;
				std::vector<std::string> ftar = storage->types();
				if ((int)ftar.size() != nofTypes)
				{
					throw std::runtime_error("stored number of feature types does not match");
				}
				std::vector<std::string>::const_iterator fi = ftar.begin(), fe = ftar.end();
				for (; fi != fe; ++fi)
				{
					if (vectorcnt.erase( *fi) != 1)
					{
						throw std::runtime_error("stored feature types do not match");
					}
				}
			}{
				if (g_verbose) std::cerr << "test reading of feature type relations ..." << std::endl;
				strus::Index ni = 1, ne = nofFeatures;
				int nofRelations = 0;
				for (; ni <= ne; ++ni)
				{
					std::string feat = getFeatureName( ni);
					std::vector<std::string> types = storage->featureTypes( feat);
					std::map<strus::Index,std::vector<strus::Index> >::const_iterator fi = featureTypeRelMap.find( ni);
					if (fi == featureTypeRelMap.end())
					{
						if (types.empty())
						{
							continue;
						}
						else
						{
							if (g_verbose) std::cerr << strus::string_format( "number of types %d does not match expected %d", 0, (int)types.size()) << std::endl;
							throw std::runtime_error("stored feature type relations do not match");
						}
					}
					if (types.size() != fi->second.size())
					{
						if (g_verbose) std::cerr << strus::string_format( "number of types %d does not match expected %d", (int)fi->second.size(), (int)types.size()) << std::endl;
						throw std::runtime_error("stored feature type relations do not match");
					}
					nofRelations += types.size();
					std::vector<strus::Index>::const_iterator ri = fi->second.begin(), re = fi->second.end();
					for (; ri != re; ++ri)
					{
						if (std::find( types.begin(), types.end(), getTypeName( *ri)) == types.end())
						{
							std::ostringstream typesbuf;
							printArray( typesbuf, types, ", ");
							std::string type = getTypeName( *ri);
							std::string typeliststr = typesbuf.str();
							if (g_verbose) std::cerr << strus::string_format( "type %s not in relations {%s}", type.c_str(), typeliststr.c_str()) << std::endl;
							throw std::runtime_error("stored feature type relations do not match");
						}
					}
				}
				if (g_verbose) std::cerr << strus::string_format( "checked %d with total %d relations", nofFeatures, nofRelations) << std::endl;
				if (g_errorhnd->hasError())
				{
					throw std::runtime_error( "error in test reading feature type relations");
				}
			}{
				if (g_verbose) std::cerr << "test similarity search ..." << std::endl;
				strus::Index ti = 1, te = nofTypes;
				int nofSearchesWithRealWeights = 0;
				int nofSearchesWithApproxWeights = 0;
				for (; ti <= te; ++ti)
				{
					std::string type = getTypeName( ti);
					bool useRealWeights = (g_random.get(0,2) == 1);
					if (g_random.get(0,2) == 1)
					{
						storage->prepareSearch( type);
					}
					if (g_verbose) std::cerr << strus::string_format( "search for type '%s' %s", type.c_str(), useRealWeights?"with real weights":"with approximative weights only") << std::endl;
					SimMatrixMap::const_iterator si = simMatrixMap.find( type);
					if (si == simMatrixMap.end()) throw std::runtime_error( "logic error: sim matrix not defined");
					const SimMatrix& simMatrix = si->second;

					std::vector<FeatureDef>::const_iterator di = defs.begin(), de = defs.end();
					for (; di != de; ++di)
					{
						if (!di->vec.empty() && di->type == type)
						{
							if (g_verbose) std::cerr << strus::string_format( "find similar of '%s'",di->feat.c_str()) << std::endl;
							std::vector<strus::VectorQueryResult> simar = storage->findSimilar( type, di->vec, 20/*maxNofResults*/, minSimilarity, speedRecallFactor, useRealWeights);
							std::vector<strus::VectorQueryResult> expect;

							SimMatrix::const_iterator mi = simMatrix.find( di->feat);
							if (mi != simMatrix.end())
							{
								expect = mi->second;
							}
							else
							{
								throw std::runtime_error( strus::string_format( "expected vector query result is empty for %s '%s'", di->type.c_str(), di->feat.c_str()));
							}
							if (!compareResult( simar, expect, result_sim_cos)
							||  !compareResult( expect, simar, result_sim_cos))
							{
								std::cerr << "result:" << std::endl;
								printResult( std::cerr, simar);
								std::cerr << "expected:" << std::endl;
								printResult( std::cerr, expect);
								throw std::runtime_error("search result does not match");
							}
							else if (simar.empty() || !strus::Math::isequal( simar[0].weight(), 1.0, VEC_EPSILON*40))
							{
								throw std::runtime_error( "similarity to itself not found");
							}
							else if (g_verbose)
							{
								std::vector<strus::VectorQueryResult>::const_iterator ri = simar.begin(), re = simar.end();
								std::ostringstream resbuf;
								for (int ridx=0; ri != re; ++ri,++ridx)
								{
									if (ridx) resbuf << ", ";
									char fbuf[32];
									std::snprintf( fbuf, sizeof(fbuf), "%.5f", ri->weight());
									resbuf << ri->value() << ":" << fbuf;
								}
								std::cerr << "found " << resbuf.str() << std::endl;
							}
							if (useRealWeights)
							{
								nofSearchesWithRealWeights += 1;
							}
							else
							{
								nofSearchesWithApproxWeights += 1;
							}
						}
					}
				}
				if (g_errorhnd->hasError())
				{
					throw std::runtime_error( "error in test similarity search");
				}
				if (g_verbose) std::cerr << strus::string_format( "performed %d searches with real weights and %d searches with LSH approximation only.", nofSearchesWithRealWeights, nofSearchesWithApproxWeights) << std::endl;
			}
		}
		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "uncaught exception");
		}
		if (g_verbose) std::cerr << "config:" << storage->config() << std::endl;
		std::cerr << "OK" << std::endl;
		delete g_fileLocator;
		delete g_errorhnd;
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
		delete g_fileLocator;
		delete g_errorhnd;
		return 1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		delete g_fileLocator;
		delete g_errorhnd;
		return 2;
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
		delete g_fileLocator;
		delete g_errorhnd;
		return 3;
	}
}

