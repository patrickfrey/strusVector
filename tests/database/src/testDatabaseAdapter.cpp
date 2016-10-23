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
				config = argv[++argidx];
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
		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<options>] [<number of examples>]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h           : print this usage" << std::endl;
			std::cerr << "-s <CONFIG>  : specify test configuration string as <CONFIG>" << std::endl;
			return rt;
		}
		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "uncaught exception");
		}
#ifdef STRUS_LOWLEVEL_DEBUG
#endif
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

