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

#define MAIN_CONCEPTNAME ""

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

static std::vector<double> createSimilarVector( const std::vector<double>& vec_, double maxCosSim)
{
	arma::vec vec( vec_);
	arma::vec orig( vec);
	for (;;)
	{
		unsigned int idx = rand() % vec.size();
		double elem = vec[ idx];
		if ((rand() & 1) == 0)
		{
			elem -= elem / 10;
			if (elem < 0.0) continue;
		}
		else
		{
			elem += elem / 10;
		}
		double oldelem = vec[ idx];
		vec[ idx] = elem;
		double cosSim = arma::norm_dot( vec, orig);
		if (maxCosSim > cosSim)
		{
			vec[ idx] = oldelem;
			break;
		}
	}
	return convertVectorStd( vec);
}

std::vector<double> createRandomVector( unsigned int dim)
{
	return convertVectorStd( (arma::randu<arma::vec>( dim) - 0.5) * 2.0); // values between -1.0 and 1.0
}

static strus::ErrorBufferInterface* g_errorhnd = 0;

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
		std::string config( DEFAULT_CONFIG);
		unsigned int nofFeatures = 1000;
		unsigned int dim = 0;
		bool use_model_built = false;
		bool use_group_assign = false;
		bool printUsageAndExit = false;

		// Parse parameters:
		int argidx = 1;
		bool finished_options = false;
		while (!finished_options && argc > argidx && argv[argidx][0] == '-')
		{
			if (0==std::strcmp( argv[argidx], "-g"))
			{
				use_group_assign = true;
			}
			else if (0==std::strcmp( argv[argidx], "-b"))
			{
				use_model_built = true;
			}
			else if (0==std::strcmp( argv[argidx], "-h"))
			{
				printUsageAndExit = true;
			}
			else if (0==std::strcmp( argv[argidx], "--"))
			{
				finished_options = true;
			}
			++argidx;
		}
		if (argc > argidx + 2)
		{
			std::cerr << "too many arguments (maximum 1 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}
		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<config>] [<number of concepts added>]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h     : print this usage" << std::endl;
			std::cerr << "-g     : use concept assignment instead of similarity to verify results" << std::endl;
			std::cerr << "-b     : do not recreate model, use the one built" << std::endl;
			return rt;
		}
		if (argc >= argidx + 1)
		{
			config = argv[argidx+0];
		}
		if (argc >= argidx + 2)
		{
			bool error = false;
			if (argv[argidx+1][0] >= '0' && argv[argidx+1][0] <= '9')
			{
				nofFeatures = std::atoi( argv[argidx+1]);
			}
			else
			{
				error = true;
			}
			if (error) throw std::runtime_error( "number (non negative integer) expected as 2nd argument");
		}
		std::cerr << "model config: " << config << std::endl;
		std::string configsrc = config;
		if (!extractUIntFromConfigString( dim, configsrc, "dim", g_errorhnd)) throw std::runtime_error("configuration parameter 'dim' is not specified");

		// Build all objects:
		std::auto_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_errorhnd));
		std::auto_ptr<strus::VectorSpaceModelInterface> vmodel( createVectorSpaceModel_std( g_errorhnd));
		if (!dbi.get() || !vmodel.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());

		// Remove traces of old test model before creating a new one:
		if (!use_model_built && !dbi->destroyDatabase( config))
		{
			(void)g_errorhnd->fetchError();
		}

		// Build the test vectors:
		std::vector<std::vector<double> > samplear;
		if (use_model_built)
		{
			std::auto_ptr<strus::VectorSpaceModelInstanceInterface> instance( vmodel->createInstance( config, dbi.get()));
			if (!instance.get()) throw std::runtime_error( g_errorhnd->fetchError());
			strus::Index si = 0, se = instance->nofFeatures();
			for (; si != se; ++si)
			{
				samplear.push_back( instance->featureVector( si));
			}
		}
		std::auto_ptr<strus::VectorSpaceModelBuilderInterface> builder( vmodel->createBuilder( config, dbi.get()));
		if (!builder.get()) throw std::runtime_error( g_errorhnd->fetchError());

		if (!use_model_built)
		{
			std::cerr << "create " << nofFeatures << " sample vectors" << std::endl;
			for (std::size_t sidx = 0; sidx != nofFeatures; ++sidx)
			{
				std::vector<double> vec;
				if (!sidx || rand() % 3 < 2)
				{
					vec = createRandomVector( dim);
				}
				else
				{
					std::size_t idx = rand() % sidx;
					double sim = 0.90 + (rand() % 120) * 0.001;
					vec = createSimilarVector( samplear[ idx], sim);
				}
				samplear.push_back( vec);
				char nam[ 64];
				snprintf( nam, sizeof(nam), "_%u", (unsigned int)sidx);
				builder->addFeature( nam, vec);
			}
			if (!builder->commit())
			{
				throw std::runtime_error( "building example vector set for VSM failed");
			}
		}

		// Create the real similarity matrix based on cosine distance of the sample vectors to verify the results:
		std::cerr << "create similarity matrix" << std::endl;
		unsigned int nofSimilarities = 0;
		strus::SparseDim2Field<unsigned char> expSimMatrix;
		strus::SparseDim2Field<double> simMatrix;
		{
			std::vector<arma::vec> samplevecar;

			for (std::size_t sidx = 0; sidx != nofFeatures; ++sidx)
			{
				samplevecar.push_back( arma::vec( samplear[ sidx]));

				for (std::size_t oidx=0; oidx != sidx; ++oidx)
				{
					double sim = arma::norm_dot( samplevecar.back(), samplevecar[ oidx]);
					if (sim > 0.9)
					{
						nofSimilarities += 1;
						std::cerr << "sample " << sidx << " has " << sim << " similarity to " << oidx << std::endl;
						expSimMatrix( sidx, oidx) = 1;
						expSimMatrix( oidx, sidx) = 1;
					}
					if (sim > 0.8)
					{
						simMatrix( sidx, oidx) = sim;
						simMatrix( oidx, sidx) = sim;
					}
				}
			}
		}
		// Build the model:
		std::cerr << "building model" << std::endl;
		if (!builder->finalize())
		{
			throw std::runtime_error( "error in finalize VSM");
		}
		builder.reset();
		std::cerr << "builder closed" << std::endl;

		// Categorize the input vectors and build some maps out of the assignments of concepts:
		std::cerr << "load model to categorize vectors" << std::endl;
		std::auto_ptr<strus::VectorSpaceModelInstanceInterface> categorizer( vmodel->createInstance( config, dbi.get()));
		if (!categorizer.get())
		{
			throw std::runtime_error( "failed to create VSM instance from model stored");
		}
		std::cerr << "loaded trained model with " << categorizer->nofConcepts( MAIN_CONCEPTNAME) << " concepts" << std::endl;
		typedef strus::SparseDim2Field<unsigned char> ConceptMatrix;
		typedef std::multimap<strus::Index,strus::Index> ClassesMap;
		typedef std::pair<strus::Index,strus::Index> ClassesElem;
		ConceptMatrix conceptMatrix;
		ConceptMatrix conceptInvMatrix;
		ClassesMap classesmap;
		unsigned int groupAssozMisses = 0;
		unsigned int groupAssozFalses = 0;

		std::vector<std::vector<double> >::const_iterator si = samplear.begin(), se = samplear.end();
		for (std::size_t sidx=0; si != se; ++si,++sidx)
		{
			std::vector<strus::Index> ctgar( categorizer->mapVectorToConcepts( MAIN_CONCEPTNAME, *si));
			std::vector<strus::Index> ctgar2( categorizer->featureConcepts( MAIN_CONCEPTNAME, sidx));
			std::vector<strus::Index>::const_iterator ci,ce,ca,zi,ze,za;
			if (use_group_assign)
			{
				za = zi = ctgar.begin(), ze = ctgar.end();
				ca = ci = ctgar2.begin(), ce = ctgar2.end();
			}
			else
			{
				ca = ci = ctgar.begin(), ce = ctgar.end();
				za = zi = ctgar2.begin(), ze = ctgar2.end();
			}
			std::cout << "[" << sidx << "] => ";
			for (std::size_t cidx=0; ci != ce; ++ci,++cidx)
			{
				while (zi != ze && *zi < *ci)
				{
					++groupAssozFalses;
					++zi;
				}
				if (zi != ze && *zi == *ci)
				{
					++zi;
				}
				else if (zi == ze || *zi > *ci)
				{
					++groupAssozMisses;
				}
				conceptMatrix( sidx, *ci-1) = 1;
				conceptInvMatrix( *ci-1, sidx) = 1;
				if (cidx) std::cout << ", ";
				std::cout << *ci;
				classesmap.insert( ClassesElem( *ci, sidx));
			}
			while (zi < ze)
			{
				++groupAssozFalses;
				++zi;
			}
			std::cout << std::endl;
			std::cout << " " << sidx << "  => ";
			std::size_t zidx = 0;
			for (zi = za; zi != ze; ++zi,++zidx)
			{
				if (zidx) std::cout << ", ";
				std::cout << *zi;
			}
			std::cout << std::endl;
		}
		// Output classes:
		unsigned int memberAssozMisses = 0;
		unsigned int memberAssozFalses = 0;
		ClassesMap::const_iterator ci = classesmap.begin(), ce = classesmap.end();
		while (ci != ce)
		{
			strus::Index key = ci->first;
			std::vector<strus::Index> members( categorizer->conceptFeatures( MAIN_CONCEPTNAME, ci->first));
			std::vector<strus::Index>::const_iterator mi = members.begin(), me = members.end();
			std::cout << "(" << key << ") <= ";
			for (; ci != ce && ci->first == key; ++ci)
			{
				while (mi != me && *mi < ci->second)
				{
					++memberAssozFalses;
					++mi;
				}
				if (mi != me && *mi == ci->second)
				{
					++mi;
				}
				else if (mi == me || *mi > ci->second)
				{
					++memberAssozMisses;
				}
				std::cout << ' ' << ci->second;
			}
			while (mi < me)
			{
				++memberAssozFalses;
				++mi;
			}
			std::cout << std::endl;
			std::cout << " " << key << "  <= ";
			for (mi = members.begin(); mi != me; ++mi)
			{
				std::cout << ' ' << *mi;
			}
			std::cout << std::endl;
		}
		// Build a similarity matrix defined by concepts shared between input vectors:
		std::cerr << "build sample to sample concept relation matrix:" << std::endl;
		strus::SparseDim2Field<unsigned char> outSimMatrix;
		unsigned int fi=0, fe=categorizer->nofConcepts( MAIN_CONCEPTNAME);
		for (; fi != fe; ++fi)
		{
			std::vector<unsigned int> conceptMembers;
			ConceptMatrix::const_row_iterator
				ri = conceptInvMatrix.begin_row( fi),
				re = conceptInvMatrix.end_row( fi);
			for (; ri != re; ++ri)
			{
				std::vector<unsigned int>::const_iterator
					mi = conceptMembers.begin(), me = conceptMembers.end();
				for (; mi != me; ++mi)
				{
					outSimMatrix( *mi, ri.col()) = 1;
					outSimMatrix( ri.col(), *mi) = 1;
				}
				conceptMembers.push_back( ri.col());
			}
		}
		// Compare the similarity matrix defined by concept assignments and the one defined by the cosine measure of the sample vectors and accumulate the results:
		std::cerr << "test calculated concept assignments:" << std::endl;
		unsigned int nofMisses = 0;
		unsigned int nofFalsePositives = 0;
		unsigned int nofBadFalsePositives = 0;
		for (std::size_t sidx=0; sidx != nofFeatures; ++sidx)
		{
			for (std::size_t oidx=0; oidx != nofFeatures; ++oidx)
			{
				double sim = simMatrix.get( sidx, oidx);
				unsigned char val = outSimMatrix.get( sidx, oidx);
				if (sidx != oidx)
				{
					if (sim >= 0.9)
					{
						if (val == 0)
						{
							nofMisses += 1;
#ifdef STRUS_LOWLEVEL_DEBUG
							std::cerr << "missed relation (" << sidx << "," << oidx << ") sim = " << sim << std::endl;
#endif
						}
					}
					else
					{
						if (val != 0)
						{
							nofFalsePositives += 1;
							if (sim < 0.85) nofBadFalsePositives += 1;
#ifdef STRUS_LOWLEVEL_DEBUG
							std::cerr << "false positive (" << sidx << "," << oidx << ") sim = " << sim << std::endl;
#endif
						}
					}
				}
			}
		}
		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "uncaught exception");
		}
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << "output:" << std::endl << outSimMatrix.tostring() << std::endl;
		std::cerr << "expected:" << std::endl << expSimMatrix.tostring() << std::endl;
#endif
		std::cerr << "group assoz miss = " << groupAssozMisses << std::endl;
		std::cerr << "group assoz false = " << groupAssozFalses << std::endl;
		std::cerr << "member assoz miss = " << memberAssozMisses << std::endl;
		std::cerr << "member assoz false = " << memberAssozFalses << std::endl;
		std::cerr << "number of similarities = " << nofSimilarities << " reflexive = " << (nofSimilarities*2) << std::endl;
		std::cerr << "number of misses = " << nofMisses << std::endl;
		std::cerr << "false positives = " << nofFalsePositives << std::endl;
		std::cerr << "bad false positives = " << nofBadFalsePositives << std::endl;
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

