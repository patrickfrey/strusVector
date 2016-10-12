/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program
#include "strus/lib/vectorspace_std.hpp"
#include "strus/lib/error.hpp"
#include "strus/vectorSpaceModelInterface.hpp"
#include "strus/vectorSpaceModelInstanceInterface.hpp"
#include "strus/vectorSpaceModelBuilderInterface.hpp"
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
#define SAMPLESFILE "samples.bin"

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
		g_errorhnd = strus::createErrorBuffer_standard( 0, 0);
		if (!g_errorhnd) throw std::runtime_error("failed to create error buffer structure");

		initRandomNumberGenerator();
		std::string config( DEFAULT_CONFIG);
		unsigned int nofSamples = 1000;
		unsigned int dim = 0;
		std::string path;
		std::string prepath;
		bool use_prepared_model = false;

		if (argc > 3)
		{
			std::cerr << "Usage: " << argv[0] << " [<config>] [<number of sample vectors>]" << std::endl;
			throw std::runtime_error( "too many arguments (maximum 1 expected)");
		}
		if (argc >= 2)
		{
			config = argv[1];
		}
		if (argc >= 3)
		{
			bool error = false;
			if (argv[2][0] >= '0' && argv[2][0] <= '9')
			{
				nofSamples = std::atoi( argv[2]);
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
		if (!extractStringFromConfigString( path, configsrc, "path", g_errorhnd)) throw std::runtime_error("configuration parameter 'path' is not specified");
		if (extractStringFromConfigString( prepath, configsrc, "prepath", g_errorhnd))
		{
			use_prepared_model = true;
		}
		std::auto_ptr<strus::VectorSpaceModelInterface> vmodel( createVectorSpaceModel_std( g_errorhnd));
		if (!vmodel.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());
		if (!vmodel->destroyModel( config))
		{
			(void)g_errorhnd->fetchError();
		}
		std::auto_ptr<strus::VectorSpaceModelBuilderInterface> builder( vmodel->createBuilder( config));
		if (!builder.get()) throw std::runtime_error( g_errorhnd->fetchError());

		std::vector<std::vector<double> > samplear;
		if (use_prepared_model)
		{
			std::string content;
			unsigned int ec = strus::readFile( prepath + strus::dirSeparator() + SAMPLESFILE, content);
			if (ec) throw std::runtime_error("could not load prepared sample vectors");
			double const* vi = (const double*)content.c_str();
			const double* ve = vi + (content.size() / sizeof(double));
			while (vi < ve)
			{
				std::vector<double> elem;
				unsigned int di = 0, de = dim;
				for (; di != de && vi != ve; ++di)
				{
					elem.push_back( *vi++);
				}
				if (di != de) throw std::runtime_error("file with sample vectors does not match in size");
				samplear.push_back( elem);
			}
			if (vi != ve) throw std::runtime_error("file with sample vectors does not match in size");
		}
		else
		{
			std::cerr << "create " << nofSamples << " sample vectors" << std::endl;
			for (std::size_t sidx = 0; sidx != nofSamples; ++sidx)
			{
				std::vector<double> vec;
				if (!sidx || rand() % 3 < 2)
				{
					vec = createRandomVector( dim);
				}
				else
				{
					std::size_t idx = rand() % sidx;
					double sim = 0.90 + (rand() % 100) * 0.001;
					vec = createSimilarVector( samplear[ idx], sim);
				}
				samplear.push_back( vec);
				builder->addSampleVector( vec);
			}
		}
		std::cerr << "create similarity matrix" << std::endl;
		unsigned int nofSimilarities = 0;
		strus::SparseDim2Field<unsigned char> expSimMatrix;
		strus::SparseDim2Field<double> simMatrix;
		{
			std::vector<arma::vec> samplevecar;

			for (std::size_t sidx = 0; sidx != nofSamples; ++sidx)
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
		std::cerr << "building model" << std::endl;
		if (!builder->finalize())
		{
			throw std::runtime_error( "error in finalize");
		}
		std::cerr << "store model" << std::endl;
		if (!builder->store())
		{
			throw std::runtime_error( "error storing the model learnt");
		}
		std::cerr << "write samples to file" << std::endl;
		std::string content;
		{
			std::vector<std::vector<double> >::const_iterator si = samplear.begin(), se = samplear.end();
			for (; si != se; ++si)
			{
				content.append( (const char*)si->data(), si->size() * sizeof(double));
			}
			unsigned int ec = strus::writeFile( path + strus::dirSeparator() + SAMPLESFILE, content);
			if (ec) throw std::runtime_error("failed to write samples file");
		}
		std::cerr << "load model to categorize vectors" << std::endl;
		std::auto_ptr<strus::VectorSpaceModelInstanceInterface> categorizer( vmodel->createInstance( config));
		if (!categorizer.get())
		{
			throw std::runtime_error( g_errorhnd->fetchError());
		}
		std::cerr << "loaded trained model with " << categorizer->nofFeatures() << " features" << std::endl;
		typedef strus::SparseDim2Field<unsigned char> FeatureMatrix;
		typedef std::multimap<unsigned int,std::size_t> ClassesMap;
		typedef std::pair<unsigned int,std::size_t> ClassesElem;
		FeatureMatrix featureMatrix;
		FeatureMatrix featureInvMatrix;
		ClassesMap classesmap;

		std::vector<std::vector<double> >::const_iterator si = samplear.begin(), se = samplear.end();
		for (std::size_t sidx=0; si != se; ++si,++sidx)
		{
			std::vector<unsigned int> ctgar( categorizer->mapVectorToFeatures( *si));
			std::vector<unsigned int>::const_iterator ci = ctgar.begin(), ce = ctgar.end();
			std::cout << "[" << sidx << "] => ";
			for (std::size_t cidx=0; ci != ce; ++ci,++cidx)
			{
				featureMatrix( sidx, *ci-1) = 1;
				featureInvMatrix( *ci-1, sidx) = 1;
				if (cidx) std::cout << ", ";
				std::cout << *ci;
				classesmap.insert( ClassesElem( *ci, sidx));
			}
			std::cout << std::endl;
		}
		// Output classes:
		ClassesMap::const_iterator ci = classesmap.begin(), ce = classesmap.end();
		while (ci != ce)
		{
			unsigned int key = ci->first;
			std::cout << "(" << key << ") <= ";
			for (; ci != ce && ci->first == key; ++ci)
			{
				std::cout << ' ' << ci->second;
			}
			std::cout << std::endl;
		}
		std::cerr << "build sample to sample feature relation matrix:" << std::endl;
		strus::SparseDim2Field<unsigned char> outSimMatrix;
		unsigned int fi=0, fe=categorizer->nofFeatures();
		for (; fi != fe; ++fi)
		{
			std::vector<unsigned int> featureMembers;
			FeatureMatrix::const_row_iterator ri = featureInvMatrix.begin_row( fi), re = featureInvMatrix.end_row( fi);
			for (; ri != re; ++ri)
			{
				std::vector<unsigned int>::const_iterator
					mi = featureMembers.begin(), me = featureMembers.end();
				for (; mi != me; ++mi)
				{
					outSimMatrix( *mi, ri.col()) = 1;
					outSimMatrix( ri.col(), *mi) = 1;
				}
				featureMembers.push_back( ri.col());
			}
		}
		std::cerr << "test calculated feature assignments:" << std::endl;
		unsigned int nofMisses = 0;
		unsigned int nofFalsePositives = 0;
		unsigned int nofBadFalsePositives = 0;
		for (std::size_t sidx=0; sidx != nofSamples; ++sidx)
		{
			for (std::size_t oidx=0; oidx != nofSamples; ++oidx)
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

