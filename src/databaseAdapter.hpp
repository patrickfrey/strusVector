/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Database abstraction (wrapper) for the standard strus vector space model
#ifndef _STRUS_VECTOR_SPACE_MODEL_DATABASE_ADAPTER_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_DATABASE_ADAPTER_HPP_INCLUDED
#include "strus/databaseInterface.hpp"
#include "strus/databaseClientInterface.hpp"
#include "strus/databaseTransactionInterface.hpp"
#include "strus/reference.hpp"
#include "strus/base/stdint.h"
#include "vectorSpaceModelConfig.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "genModel.hpp"
#include "lshModel.hpp"
#include "stringList.hpp"
#include <vector>
#include <string>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

#define MATRIXFILE   "model.mat"		// Matrices for LSH calculation
#define SIMRELFILE   "simrel.mat"		// Sparse matrix of imilarity relation map
#define RESVECFILE   "result.lsh"		// Result LSH values
#define INPVECFILE   "input.lsh"		// Input sample LSH values
#define CONFIGFILE   "config.txt"		// Configuration as text
#define VERSIONFILE  "version.hdr"		// Version for checking compatibility
#define DUMPFILE     "dump.txt"			// Textdump output of structures if used with LOWLEVEL DEBUG defined
#define FEATIDXFILE  "featsampleindex.fsi"	// map feature number to sample indices [FeatureSampleIndexMap]
#define IDXFEATFILE  "samplefeatindex.sfi"	// map sample index to feature numbers [SampleFeatureIndexMap]
#define VECNAMEFILE  "vecnames.lst"		// list of sample vector names
#define VECTORSFILE  "vectors.bin"		// list of sample vectors

class DatabaseAdapter
{
public:
	DatabaseAdapter( DatabaseInterface* database, const std::string& config, ErrorBufferInterface* errorhnd_);
	~DatabaseAdapter(){}

	void checkVersion();
	typedef std::vector<double> Vector;
	std::vector<Vector> readSampleVectors() const;
	Vector readSampleVector( const SampleIndex& sidx) const;
	std::vector<SimHash> readSampleSimhashVector() const;
	std::vector<SimHash> readResultSimhashVector() const;
	SimRelationMap readSimRelationMap() const;
	SampleFeatureIndexMap readSampleFeatureIndexMap() const;
	FeatureSampleIndexMap readFeatureSampleIndexMap() const;
	VectorSpaceModelConfig readConfig() const;
	LshModel readLshModel() const;

	void writeVersion();
	void writeSampleVectors( const std::vector<Vector>& ar);
	void writeSampleVector( const SampleIndex& sidx, const Vector& vec);
	void writeSampleSimhashVector( const std::vector<SimHash>& ar);
	void writeResultSimhashVector( const std::vector<SimHash>& ar);
	void writeSimRelationMap( const SimRelationMap& simrelmap);
	void writeSampleFeatureIndexMap( const SampleFeatureIndexMap& sfmap);
	void writeFeatureSampleIndexMap( const FeatureSampleIndexMap& fsmap);
	void writeConfig( const VectorSpaceModelConfig& config);
	void writeLshModel( const LshModel& model);
	void commit();

private:
	void beginTransaction();
	std::vector<SimHash> readSimhashVector( char prefix) const;
	void writeSimhashVector( char prefix, const std::vector<SimHash>& ar);

private:
	Reference<DatabaseClientInterface> m_database;
	Reference<DatabaseTransactionInterface> m_transaction;
	ErrorBufferInterface* m_errorhnd;
};


} //namespace
#endif

