/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Configuration structure for the strus standard vector space model
#ifndef _STRUS_VECTOR_SPACE_MODEL_CONFIG_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_CONFIG_HPP_INCLUDED
#include "simGroup.hpp"
#include <string>
#include <map>
#include <limits>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

struct GenModelConfig
{
	enum Defaults {
		DefaultSimDist = 340,	//< 340 out of 2K (32*64) ~ cosine dist 0.9
		DefaultRadDist = 320,	//< 340 out of 2K (32*64) ~ cosine dist 0.9
		DefaultEqDist = 60,
		DefaultMutations = 50,
		DefaultMutationVotes = 13,
		DefaultDescendants = 10,
		DefaultMaxAge = 20,
		DefaultIterations = 20,
		DefaultAssignments = 7,
		DefaultGreediness = 3,
		DefaultIsaf = 60,
		DefaultBadFitnessFactor = 10,
		DefaultEqdiff = 25,
		DefaultWithSingletons = 0
	};

	GenModelConfig( const GenModelConfig& o);
	GenModelConfig();
	GenModelConfig( const std::string& config, unsigned int maxdist, ErrorBufferInterface* errorhnd, const GenModelConfig& defaultcfg=GenModelConfig());

	void parse( std::string& src, unsigned int maxdist, ErrorBufferInterface* errorhnd);
	std::string tostring( bool eolnsep=true) const;

	unsigned int simdist;		///< maximum distance to be considered similar
	unsigned int raddist;		///< maximum radius distance of group elements to centroid
	unsigned int eqdist;		///< maximum distance to be considered equal
	unsigned int mutations;		///< maximum number of random selected mutation candidates of the non kernel elements of a group
	unsigned int votes;		///< maximum number of votes to use to determine a mutation direction
	unsigned int descendants;	///< number of descendants of which the fittest is selected
	unsigned int maxage;		///< upper bound value used for calculate number of mutations (an older individuum mutates less)
	unsigned int iterations;	///< number of iterations
	unsigned int assignments;	///< maximum number of assignments on a sample to a group
	unsigned int greediness;	///< non-negative integer specifying the greediness of collecting near elements into a newly created group (ascending)
	float isaf;			///< fraction of samples of a superset that has to be in a subset for declaring the subset as dependent (is a) of the superset
	float baff;			///< factor of fitness considered bad compared with the best fitness of the group, used to decide wheter a group can be left when maximum capacity of releations is reached
	float eqdiff;			///< fraction of maximum elements that can to differ for sets to considered to be equal (unfittest competitor elimination)
	bool with_singletons;		///< true, if singletons should also get into the result
};

typedef std::map<std::string,GenModelConfig> GenModelConfigMap;


struct VectorSpaceModelConfig
{
	enum Defaults {
		DefaultThreads = 0,
		DefaultCommitSize = 0,
		DefaultDim = 300,
		DefaultBits = 64,
		DefaultVariations = 32,
		DefaultMaxDist = 640,	//< 640 out of 2K (32*64)
		DefaultMaxSimSam = 0,
		DefaultRndSimSam = 0,
		DefaultMaxFeatures = (1<<31),
		DefaultWithSingletons = 0,
		DefaultWithProbSim = 1,
		DefaultWithForceSim = 0
	};
	VectorSpaceModelConfig( const VectorSpaceModelConfig& o);
	VectorSpaceModelConfig();
	VectorSpaceModelConfig( const std::string& config, ErrorBufferInterface* errorhnd, const VectorSpaceModelConfig& defaultcfg=VectorSpaceModelConfig());

	bool isBuildCompatible( const VectorSpaceModelConfig& o) const;
	std::string tostring( bool eolnsep=true) const;

	const GenModelConfig& genModelConfig( const std::string& name) const
	{
		if (name.empty())
		{
			return gencfg;
		}
		else
		{
			GenModelConfigMap::const_iterator ai = altgenmap.find(name);
			if (ai == altgenmap.end()) throw strus::runtime_error(_TXT("undefined gen config '%s'"), name.c_str());
			return ai->second;
		}
	}

	std::string databaseConfig;	///< path of model
	std::string logfile;		///< file where to log some status data
	unsigned int threads;		///< maximum number of threads to use (0 = no threading)
	unsigned int commitsize;	///< auto commit after this amount of operations to reduce memory consumption
	unsigned int dim;		///< input vector dimension
	unsigned int bits;		///< number of bits to calculate for an LSH per variation
	unsigned int variations;	///< number of variations
	unsigned int maxdist;		///< maximum LSH edit distance precalculated in the similarity relation map. Maximum for any other similarity distance (simdist,eqdist,raddist)
	GenModelConfig gencfg;		///< main configuration for categorization
	unsigned int maxsimsam;		///< maximum number of nearest neighbours put input similarity relation map
	unsigned int rndsimsam;		///< select number of random samples put into similarity relation map besides 'maxsimsam'
	unsigned int maxfeatures;	///< restrict the number of features loaded
	unsigned int maxconcepts;	///< restrict the number of concepts created (also temporarily)
	bool with_probsim;		///< true, if probabilistic function is used as prefilter for the candidates of the similarity matrix (faster)
	bool with_forcesim;		///< true, if the similarity relation matrix calculation should be forced
	GenModelConfigMap altgenmap;	///< alternative runs of categorization
	typedef std::pair<std::string,std::string> ConceptClassDependency;
	std::vector<ConceptClassDependency> conceptClassDependecies; ///< Dependencies to calculate between concept classes
};

}//namespace
#endif


