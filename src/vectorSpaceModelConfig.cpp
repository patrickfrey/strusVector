/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Configuration structure for the strus standard vector space model
#include "vectorSpaceModelConfig.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/configParser.hpp"
#include "utils.hpp"
#include "internationalization.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace strus;

VectorSpaceModelConfig::VectorSpaceModelConfig( const VectorSpaceModelConfig& o)
	:databaseConfig(o.databaseConfig),logfile(o.logfile)
	,threads(o.threads),commitsize(o.commitsize)
	,dim(o.dim),bits(o.bits),variations(o.variations)
	,maxdist(o.maxdist),simdist(o.simdist),raddist(o.raddist),eqdist(o.eqdist)
	,maxsimsam(o.maxsimsam),rndsimsam(o.rndsimsam)
	,mutations(o.mutations),votes(o.votes)
	,descendants(o.descendants),maxage(o.maxage),iterations(o.iterations)
	,assignments(o.assignments)
	,maxfeatures(o.maxfeatures)
	,isaf(o.isaf)
	,with_singletons(o.with_singletons)
	,with_probsim(o.with_probsim)
	,with_forcesim(o.with_forcesim)
	{}

VectorSpaceModelConfig::VectorSpaceModelConfig()
	:databaseConfig(),logfile(),threads(DefaultThreads),commitsize(DefaultCommitSize)
	,dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
	,maxdist(DefaultMaxDist),simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
	,maxsimsam(DefaultMaxSimSam),rndsimsam(DefaultRndSimSam)
	,mutations(DefaultMutations),votes(DefaultMutationVotes)
	,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
	,assignments(DefaultAssignments)
	,maxfeatures(DefaultMaxFeatures)
	,isaf((float)DefaultIsaf / 100)
	,with_singletons((bool)DefaultWithSingletons)
	,with_probsim((bool)DefaultWithProbSim)
	,with_forcesim((bool)DefaultWithForceSim)
	{}

VectorSpaceModelConfig::VectorSpaceModelConfig( const std::string& config, ErrorBufferInterface* errorhnd)
	:databaseConfig(),logfile(),threads(DefaultThreads),commitsize(DefaultCommitSize)
	,dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
	,maxdist(DefaultMaxDist),simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
	,maxsimsam(DefaultMaxSimSam),rndsimsam(DefaultRndSimSam)
	,mutations(DefaultMutations),votes(DefaultMutationVotes)
	,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
	,assignments(DefaultAssignments)
	,maxfeatures(DefaultMaxFeatures)
	,isaf((float)DefaultIsaf / 100)
	,with_singletons((bool)DefaultWithSingletons)
	,with_probsim((bool)DefaultWithProbSim)
	,with_forcesim((bool)DefaultWithForceSim)
{
	std::string src = config;
	if (extractStringFromConfigString( logfile, src, "logfile", errorhnd)){}
	if (extractUIntFromConfigString( threads, src, "threads", errorhnd)){}
	if (extractUIntFromConfigString( commitsize, src, "commit", errorhnd)){}
	if (extractUIntFromConfigString( dim, src, "dim", errorhnd)){}
	if (extractUIntFromConfigString( bits, src, "bit", errorhnd)){}
	if (extractUIntFromConfigString( variations, src, "var", errorhnd)){}
	if (extractUIntFromConfigString( maxdist, src, "maxdist", errorhnd)){}
	if (extractUIntFromConfigString( simdist, src, "simdist", errorhnd))
	{
		raddist = simdist;
		eqdist = simdist / 6;
	}
	if (simdist > maxdist)
	{
		throw strus::runtime_error(_TXT("the 'simdist' configuration parameter must not be bigger than 'maxdist'"));
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
	if (extractUIntFromConfigString( maxsimsam, src, "maxsimsam", errorhnd)){}
	if (extractUIntFromConfigString( rndsimsam, src, "rndsimsam", errorhnd)){}
	if (extractUIntFromConfigString( mutations, src, "mutations", errorhnd)){}
	if (extractUIntFromConfigString( votes, src, "votes", errorhnd)){}
	if (extractUIntFromConfigString( descendants, src, "descendants", errorhnd)){}
	if (extractUIntFromConfigString( iterations, src, "iterations", errorhnd))
	{
		maxage = iterations;
	}
	if (extractUIntFromConfigString( maxage, src, "maxage", errorhnd)){}
	if (extractUIntFromConfigString( assignments, src, "assignments", errorhnd)){}
	if (extractUIntFromConfigString( maxfeatures, src, "maxfeatures", errorhnd)){}
	double val;
	if (extractFloatFromConfigString( val, src, "isaf", errorhnd)){isaf=(float)val;}
	if (extractBooleanFromConfigString( with_singletons, src, "singletons", errorhnd)){}
	if (extractBooleanFromConfigString( with_probsim, src, "probsim", errorhnd)){}
	if (extractBooleanFromConfigString( with_forcesim, src, "forcesim", errorhnd)){}

	if (dim == 0 || bits == 0 || variations == 0 || mutations == 0 || descendants == 0 || maxage == 0 || iterations == 0)
	{
		throw strus::runtime_error(_TXT("error in vector space model configuration: dim, bits, var, mutations, descendants, maxage or iterations values must not be zero"));
	}
	databaseConfig = utils::trim(src);	//... rest is database configuration
	while (databaseConfig.size() && databaseConfig[ databaseConfig.size()-1] == ';') databaseConfig.resize( databaseConfig.size()-1);

	if (errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("error loading vector space model configuration: %s"), errorhnd->fetchError());
	}
}

std::string VectorSpaceModelConfig::tostring() const
{
	std::ostringstream buf;
	buf << std::fixed << std::setprecision(5);
	buf << databaseConfig;
	char const* ee = databaseConfig.c_str() + databaseConfig.size();
	while (ee != databaseConfig.c_str() && ((unsigned char)*(ee-1) <= 32 || *(ee-1) == ';')) --ee;
	buf << ";" << std::endl;
	buf << "logfile=" << logfile << ";" << std::endl;
	buf << "threads=" << threads << ";" << std::endl;
	buf << "commit=" << commitsize << ";" << std::endl;
	buf << "dim=" << dim << ";" << std::endl;
	buf << "bit=" << bits << ";" << std::endl;
	buf << "var=" << variations << ";" << std::endl;
	buf << "maxdist=" << maxdist << ";" << std::endl;
	buf << "simdist=" << simdist << ";" << std::endl;
	buf << "raddist=" << raddist << ";" << std::endl;
	buf << "eqdist=" << eqdist << ";" << std::endl;
	buf << "maxsimsam=" << maxsimsam << ";" << std::endl;
	buf << "rndsimsam=" << rndsimsam << ";" << std::endl;
	buf << "mutations=" << mutations << ";" << std::endl;
	buf << "votes=" << votes << ";" << std::endl;
	buf << "descendants=" << descendants << ";" << std::endl;
	buf << "maxage=" << maxage << ";" << std::endl;
	buf << "iterations=" << iterations << ";" << std::endl;
	buf << "assignments=" << assignments << ";" << std::endl;
	buf << "maxfeatures=" << maxfeatures << ";" << std::endl;
	buf << "isaf=" << isaf << ";" << std::endl;
	buf << "singletons=" << (with_singletons?"yes":"no") << ";" << std::endl;
	buf << "probsim=" << (with_probsim?"yes":"no") << ";" << std::endl;
	buf << "forcesim=" << (with_forcesim?"yes":"no") << ";" << std::endl;
	return buf.str();
}

