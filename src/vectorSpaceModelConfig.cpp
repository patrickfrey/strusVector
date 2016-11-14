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

GenModelConfig::GenModelConfig( const GenModelConfig& o)
	:simdist(o.simdist),raddist(o.raddist),eqdist(o.eqdist)
	,mutations(o.mutations),votes(o.votes)
	,descendants(o.descendants),maxage(o.maxage),iterations(o.iterations)
	,assignments(o.assignments)
	,isaf(o.isaf)
	,with_singletons(o.with_singletons)
	{}

GenModelConfig::GenModelConfig()
	:simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
	,mutations(DefaultMutations),votes(DefaultMutationVotes)
	,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
	,assignments(DefaultAssignments)
	,isaf((float)DefaultIsaf / 100)
	,with_singletons((bool)DefaultWithSingletons)
	{}


GenModelConfig::GenModelConfig( const std::string& config, unsigned int maxdist, ErrorBufferInterface* errorhnd)
	:simdist(DefaultSimDist),raddist(DefaultRadDist),eqdist(DefaultEqDist)
	,mutations(DefaultMutations),votes(DefaultMutationVotes)
	,descendants(DefaultDescendants),maxage(DefaultMaxAge),iterations(DefaultIterations)
	,assignments(DefaultAssignments)
	,isaf((float)DefaultIsaf / 100)
	,with_singletons((bool)DefaultWithSingletons)
{
	std::string src = config;
	parse( src, maxdist, errorhnd);
	std::string::const_iterator si = src.begin(), se = src.end();
	for (; si != se && ((unsigned char)*si <= 32 || *si == ';'); ++si)
	{}
	if (si != se) throw strus::runtime_error(_TXT("unknown configuration parameter for gen model (vector space model)"));
}

void GenModelConfig::parse( std::string& src, unsigned int maxdist, ErrorBufferInterface* errorhnd)
{
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

	if (mutations == 0 || descendants == 0 || maxage == 0 || iterations == 0)
	{
		throw strus::runtime_error(_TXT("error in vector space model configuration: mutations, descendants, maxage or iterations values must not be zero"));
	}
	if (errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("error loading vector space model configuration: %s"), errorhnd->fetchError());
	}
}

bool GenModelConfig::isBuildCompatible( unsigned int maxdist) const
{
	return simdist <= maxdist
		&& raddist <= maxdist
		&& eqdist <= maxdist
	;
}

template <typename ValueType>
static void printConfigItem( std::ostream& buf, const std::string& name, const ValueType& value, bool eolnsep)
{
	buf << name << "=" << value << ";";
	if (eolnsep) buf << std::endl;
}

static void printConfig( std::ostream& buf, const std::string& cfg, bool eolnsep)
{
	char const* ee = cfg.c_str() + cfg.size();
	while (ee != cfg.c_str() && ((unsigned char)*(ee-1) <= 32 || *(ee-1) == ';')) --ee;
	buf << std::string( cfg.c_str(), ee - cfg.c_str()) << ";";
	if (eolnsep) buf << std::endl;
}

std::string GenModelConfig::tostring( bool eolnsep) const
{
	std::ostringstream buf;
	buf << std::fixed << std::setprecision(5);
	printConfigItem( buf, "simdist", simdist, eolnsep);
	printConfigItem( buf, "raddist", raddist, eolnsep);
	printConfigItem( buf, "eqdist", eqdist, eolnsep);
	printConfigItem( buf, "mutations", mutations, eolnsep);
	printConfigItem( buf, "votes", votes, eolnsep);
	printConfigItem( buf, "descendants", descendants, eolnsep);
	printConfigItem( buf, "maxage", maxage, eolnsep);
	printConfigItem( buf, "iterations", iterations, eolnsep);
	printConfigItem( buf, "assignments", assignments, eolnsep);
	printConfigItem( buf, "isaf", isaf, eolnsep);
	printConfigItem( buf, "singletons", with_singletons, eolnsep);
	return buf.str();
}


VectorSpaceModelConfig::VectorSpaceModelConfig( const VectorSpaceModelConfig& o)
	:databaseConfig(o.databaseConfig),logfile(o.logfile)
	,threads(o.threads),commitsize(o.commitsize)
	,dim(o.dim),bits(o.bits),variations(o.variations)
	,maxdist(o.maxdist),gencfg(o.gencfg)
	,maxsimsam(o.maxsimsam),rndsimsam(o.rndsimsam)
	,maxfeatures(o.maxfeatures)
	,with_probsim(o.with_probsim)
	,with_forcesim(o.with_forcesim)
	,altgenmap(o.altgenmap)
	{}

VectorSpaceModelConfig::VectorSpaceModelConfig()
	:databaseConfig(),logfile(),threads(DefaultThreads),commitsize(DefaultCommitSize)
	,dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
	,maxdist(DefaultMaxDist),gencfg()
	,maxsimsam(DefaultMaxSimSam),rndsimsam(DefaultRndSimSam)
	,maxfeatures(DefaultMaxFeatures)
	,with_probsim((bool)DefaultWithProbSim)
	,with_forcesim((bool)DefaultWithForceSim)
	,altgenmap()
	{}

VectorSpaceModelConfig::VectorSpaceModelConfig( const std::string& config, ErrorBufferInterface* errorhnd)
	:databaseConfig(),logfile(),threads(DefaultThreads),commitsize(DefaultCommitSize)
	,dim(DefaultDim),bits(DefaultBits),variations(DefaultVariations)
	,maxdist(DefaultMaxDist),gencfg()
	,maxsimsam(DefaultMaxSimSam),rndsimsam(DefaultRndSimSam)
	,maxfeatures(DefaultMaxFeatures)
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
	gencfg.parse( src, maxdist, errorhnd);
	if (extractUIntFromConfigString( maxsimsam, src, "maxsimsam", errorhnd)){}
	if (extractUIntFromConfigString( rndsimsam, src, "rndsimsam", errorhnd)){}
	if (extractUIntFromConfigString( maxfeatures, src, "maxfeatures", errorhnd)){}
	if (extractBooleanFromConfigString( with_probsim, src, "probsim", errorhnd)){}
	if (extractBooleanFromConfigString( with_forcesim, src, "forcesim", errorhnd)){}
	std::string altgencfgstr;
	while (extractStringFromConfigString( altgencfgstr, src, "altgen", errorhnd))
	{
		char const* ee = altgencfgstr.c_str();
		while (*ee && (unsigned char)*ee <= 32) ++ee;
		char const* namestart = ee;
		while (*ee && (unsigned char)*ee != ':' && (unsigned char)*ee != ' ')
		{
			if ((*ee|32) < 'a' || (*ee|32) > 'z') throw strus::runtime_error(_TXT("expected identifier at start of alternative gen configuration in vector space model"));
			++ee;
		}
		std::string name( namestart, ee - namestart);
		while (*ee && (unsigned char)*ee <= 32) ++ee;
		if (*ee != ':') throw strus::runtime_error(_TXT("expected colon ':' after identifier at start of alternative gen configuration in vector space model"));
		++ee;
		GenModelConfig altgencfg( ee, maxdist, errorhnd);
		if (altgenmap.find( name) != altgenmap.end()) throw strus::runtime_error(_TXT("duplicate definition of alternative gen configuration in vector space model"));
		altgenmap[ name] = altgencfg;
	}
	if (dim == 0 || bits == 0 || variations == 0)
	{
		throw strus::runtime_error(_TXT("error in vector space model configuration: dim, bits or var values must not be zero"));
	}
	databaseConfig = utils::trim(src);	//... rest is database configuration
	while (databaseConfig.size() && databaseConfig[ databaseConfig.size()-1] == ';') databaseConfig.resize( databaseConfig.size()-1);

	if (errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("error loading vector space model configuration: %s"), errorhnd->fetchError());
	}
}

bool VectorSpaceModelConfig::isBuildCompatible( const VectorSpaceModelConfig& o) const
{
	if (dim == o.dim
		&& bits == o.bits
		&& variations == o.variations
		&& maxdist <= o.maxdist
		&& gencfg.isBuildCompatible( maxdist))
	{
		GenModelConfigMap::const_iterator ai = altgenmap.begin(), ae = altgenmap.end();
		for (; ai != ae; ++ai)
		{
			if (!ai->second.isBuildCompatible( maxdist)) return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

std::string VectorSpaceModelConfig::tostring( bool eolnsep) const
{
	std::ostringstream buf;
	buf << std::fixed << std::setprecision(5);
	printConfig( buf, databaseConfig, eolnsep);
	printConfigItem( buf, "logfile", logfile, eolnsep);
	printConfigItem( buf, "threads", threads, eolnsep);
	printConfigItem( buf, "commit", commitsize, eolnsep);
	printConfigItem( buf, "dim", dim, eolnsep);
	printConfigItem( buf, "bit", bits, eolnsep);
	printConfigItem( buf, "var", variations, eolnsep);
	printConfigItem( buf, "maxdist", maxdist, eolnsep);
	printConfig( buf, gencfg.tostring(eolnsep), eolnsep);
	GenModelConfigMap::const_iterator ai = altgenmap.begin(), ae = altgenmap.end();
	for (; ai != ae; ++ai)
	{
		std::ostringstream subcfg;
		subcfg << ai->first << ":" << ai->second.tostring( false);
		printConfigItem( buf, "altgen", subcfg.str(), eolnsep);
	}
	printConfigItem( buf, "maxsimsam", maxsimsam, eolnsep);
	printConfigItem( buf, "rndsimsam", rndsimsam, eolnsep);
	printConfigItem( buf, "maxfeatures", maxfeatures, eolnsep);
	printConfigItem( buf, "probsim", (with_probsim?"yes":"no"), eolnsep);
	printConfigItem( buf, "forcesim", (with_forcesim?"yes":"no"), eolnsep);
	return buf.str();
}

