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
#include "strus/vectorStorageInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageSearchInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/wordVector.hpp"
#include "strus/vectorQueryResult.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/local_ptr.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/numstring.hpp"
#include "armadillo"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>

#define VEC_EPSILON 1e-6
bool g_verbose = false;

static void initRandomNumberGenerator()
{
	time_t nowtime;
	struct tm* now;

	::time( &nowtime);
	now = ::localtime( &nowtime);

	unsigned int seed = (now->tm_year+10000) + (now->tm_mon+100) + (now->tm_mday+1);
	std::srand( seed+2);
}

static strus::WordVector convertVectorStd( const arma::fvec& vec)
{
	strus::WordVector rt;
	arma::fvec::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		rt.push_back( *vi);
	}
	return rt;
}

static std::string getFeatureName( unsigned int idx)
{
	return strus::string_format( "F%u", idx);
}

static std::string getTypeName( unsigned int idx)
{
	return strus::string_format( "T%u", idx);
}

static strus::WordVector createSimilarVector( const strus::WordVector& vec_, double maxCosSim)
{
	arma::fvec vec( vec_);
	arma::fvec orig( vec);
	for (;;)
	{
		struct Change
		{
			int idx;
			float oldvalue;
		};
		enum {NofChanges=16};
		Change changes[ NofChanges];
		int ci;

		for (ci=0; ci < NofChanges; ++ci)
		{
			int idx = rand() % vec.size();
			float elem = vec[ idx];
			if ((rand() & 1) == 0)
			{
				elem -= elem / 10;
				if (elem < 0.0) continue;
			}
			else
			{
				elem += elem / 10;
			}
			changes[ ci].idx = idx;
			changes[ ci].oldvalue = vec[ idx];
			vec[ idx] = elem;
		}
		float cosSim = arma::norm_dot( vec, orig);
		if (maxCosSim > cosSim)
		{
			for (ci=0; ci < NofChanges; ++ci)
			{
				vec[ changes[ ci].idx] = changes[ ci].oldvalue;
			}
			break;
		}
		vec = arma::normalise( vec);
	}
	return convertVectorStd( vec);
}

strus::WordVector createRandomVector( unsigned int dim)
{
	return convertVectorStd( arma::normalise( arma::randu<arma::fvec>( dim))); // vector values between 0.0 and 1.0, with normalized vector length 1.0
}

static bool compareVector( const strus::WordVector& v1, const strus::WordVector& v2)
{
	strus::WordVector::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	strus::WordVector::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (unsigned int vidx=0; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2,++vidx)
	{
		float diff = (*vi1 > *vi2)?(*vi1 - *vi2):(*vi2 - *vi1);
		if (diff > VEC_EPSILON)
		{
			for (unsigned int vv=0; vv != vidx; ++vv)
			{
				if (g_verbose) std::cerr << strus::string_format("%u: %f == %f", vv, v1[vv], v2[vv]) << std::endl;
			}
			std::cerr << strus::string_format("diff of %u: %f and %f is bigger than %f", vidx, *vi1, *vi2, VEC_EPSILON) << std::endl;
			return false;
		}
	}
	if (vi1 == ve1 && vi2 == ve2)
	{
		return true;
	}
	else
	{
		if (g_verbose) std::cerr << strus::string_format( "vectors have different length %u != %u", (unsigned int)v1.size(), (unsigned int)v2.size()) << std::endl;
		return false;
	}
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

static std::string wordVectorToString( const strus::WordVector& vec, const char* separator, std::size_t maxNofElements)
{
	std::string rt;
	std::size_t ii = 0;
	for (; ii < vec.size() && ii < maxNofElements; ++ii)
	{
		if (ii) rt.append( separator);
		char buf[ 32];
		std::snprintf( buf, sizeof(buf), "%.5f", vec[ii]);
		rt.append( buf);
	}
	return rt;
}

static strus::ErrorBufferInterface* g_errorhnd = 0;

#define DEFAULT_CONFIG "path=vstorage"

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
		g_errorhnd = strus::createErrorBuffer_standard( 0, 1, NULL/*debug trace interface*/);
		if (!g_errorhnd) throw std::runtime_error("failed to create error buffer structure");

		initRandomNumberGenerator();

		std::string configstr( DEFAULT_CONFIG);
		strus::Index nofTypes = 2;
		strus::Index nofFeatures = 1000;
		unsigned int vecdim = 300/*strus::VectorStorage::DefaultDim*/;
		unsigned int threads = 0;
		std::string workdir = "./";
		bool printUsageAndExit = false;
		float sim_cos = 0.9;
		float result_sim_cos = 0.95;

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
		strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( workdir, g_errorhnd));
		strus::local_ptr<strus::VectorStorageInterface> sti( strus::createVectorStorage_std( workdir, g_errorhnd));
		if (!dbi.get() || !sti.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());

		// Remove traces of old test model before creating a new one:
		if (g_verbose) std::cerr << "create test repository ..." << std::endl;
		if (!dbi->destroyDatabase( configstr))
		{
			(void)g_errorhnd->fetchError();
		}
		// Creating repository for vector space model:
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
					if (std::rand() % 2 == 1)
					{
						featureTypeRelMap[ ni].push_back( ti);
						const char* gentype = "empty";

						if (std::rand() % 2 == 1)
						{
							if (std::rand() % modsim == 1 and !vec.empty())
							{
								float ff = (float)(std::rand() % 1000 + 1) / 1000.0;
								float randsim = sim_cos + (1.0 - sim_cos) * ff;
								vec = createSimilarVector( vec, randsim);
								gentype = "similar";
							}
							else
							{
								vec = createRandomVector( vecdim);
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
							if (g_verbose) std::cerr << "add feature (" << defs.back().type << "," << defs.back().feat << ") no vector" << std::endl;
						}
						else
						{
							if (g_verbose) std::cerr << "add feature (" << defs.back().type << "," << defs.back().feat << ") vector " << gentype << " (" << wordVectorToString( defs.back().vec, ", ", 5) << ", ...)" << std::endl;
						}
					}
				}
			}
			if (g_verbose) std::cerr << "build " << nofTypes << " types, " << nofFeatures << " features and " << nofVectors << " vectors." << std::endl;
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
				if (std::rand() % 1000 == 1)
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
			if (g_verbose) std::cerr << "inserted " << nofVectors << " vectors, " << nofFeatures << " features, " << nofTypes << " types, in " << nofCommits << " commits." << std::endl;
		}
		typedef std::pair<std::string,std::string> SimRel;
		typedef std::map<SimRel,float> SimMatrix;
		typedef std::map<std::string,SimMatrix> SimMatrixMap;
		SimMatrixMap simMatrixMap;
		int nofSimilarities = 0;
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
							if (sim > sim_cos)
							{
								if (di->type != oi->type || di->feat != oi->feat)
								{
									++nofSimilarities;
									char simstrbuf[ 32];
									std::snprintf( simstrbuf, sizeof(simstrbuf), "%.5f", sim);
									if (g_verbose) std::cerr << "type " << di->type << " insert similar vector of '" << di->feat << "': '" << oi->feat << "' similarity " << simstrbuf << std::endl;
								}
								simMatrix.insert( SimMatrix::value_type( SimRel( di->feat, oi->feat), sim));
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
			if (g_verbose) std::cerr << "got " << (nofSimilarities/2) << "*2 (reflexive) similarities without similarities to itself counted" << std::endl;
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
					if (!compareVector( di->vec, vec))
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
				std::vector<std::string> ftar = storage->getTypes();
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
					std::vector<std::string> types = storage->getFeatureTypes( feat);
					std::map<strus::Index,std::vector<strus::Index> >::const_iterator fi = featureTypeRelMap.find( ni);
					if (fi == featureTypeRelMap.end())
					{
						if (types.empty())
						{
							continue;
						}
						else
						{
							if (g_verbose) std::cerr << "number of types " << 0 << " does not match expected " << types.size() << std::endl;
							throw std::runtime_error("stored feature type relations do not match");
						}
					}
					if (types.size() != fi->second.size())
					{
						if (g_verbose) std::cerr << "number of types " << fi->second.size() << " does not match expected " << types.size() << std::endl;
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
							if (g_verbose) std::cerr << "type " << getTypeName( *ri) << " not in relations {" << typesbuf.str() << "}" << std::endl;
							throw std::runtime_error("stored feature type relations do not match");
						}
					}
				}
				if (g_verbose) std::cerr << "checked " << nofFeatures << " with total " << nofRelations << " relations" << std::endl;
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
					bool useRealWeights = (std::rand() % 2 == 1);

					if (g_verbose) std::cerr << "create searcher for type '" << type << "' " << (useRealWeights?"with real weights":"with approximative weights only") << std::endl;
					strus::local_ptr<strus::VectorStorageSearchInterface> searcher( storage->createSearcher( type, 0, 1, useRealWeights));
					if (!searcher.get())
					{
						throw std::runtime_error("failed to create searcher");
					}
					SimMatrixMap::const_iterator si = simMatrixMap.find( type);
					if (si == simMatrixMap.end()) throw std::runtime_error( "logic error: sim matrix not defined");
					const SimMatrix& simMatrix = si->second;

					std::vector<FeatureDef>::const_iterator di = defs.begin(), de = defs.end();
					for (; di != de; ++di)
					{
						if (!di->vec.empty() && di->type == type)
						{
							if (g_verbose) std::cerr << "find similar of '" << di->feat << "'" << std::endl;
							std::vector<strus::VectorQueryResult> simar = searcher->findSimilar( di->vec, 20/*maxNofResults*/, 0.9);
							std::vector<strus::VectorQueryResult> expect;
							SimRel qry( di->feat, "");
							std::map<SimRel,float>::const_iterator mi = simMatrix.upper_bound( qry);
							for (; mi != simMatrix.end() && mi->first.first == qry.first; ++mi)
							{
								expect.push_back( strus::VectorQueryResult( mi->first.second, mi->second));
							}
							if (!compareResult( simar, expect, result_sim_cos)
							||  !compareResult( expect, simar, result_sim_cos))
							{
								std::cerr << "result:" << std::endl;
								printResult( std::cerr, simar);
								std::cerr << "expected:" << std::endl;
								printResult( std::cerr, expect);
								throw std::runtime_error("searcher result does not match");
							}
							else if (simar.empty() || simar[0].weight() < 1.0 - VEC_EPSILON*10)
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
				if (g_verbose) std::cerr << "performed " << nofSearchesWithRealWeights << " searches with real weights and " << nofSearchesWithApproxWeights << " searches with LSH approximation only." << std::endl;
			}
		}
		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "uncaught exception");
		}
		if (g_verbose) std::cerr << "config:" << storage->config() << std::endl;
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
		return 1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
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
		return 3;
	}
}

