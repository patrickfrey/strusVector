/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Loading and storing of structures of the strus standard vector space model
#ifndef _STRUS_VECTOR_SPACE_MODEL_FILES_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_FILES_HPP_INCLUDED
#include "strus/base/stdint.h"
#include "strus/base/dataRecordFile.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "genModel.hpp"
#include "lshModel.hpp"
#include "stringList.hpp"
#include <vector>
#include <string>

namespace strus {

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

void checkVersionFile( const std::string& filename);
void writeVersionToFile( const std::string& filename);

void writeDumpToFile( const std::string& content, const std::string& filename);

std::vector<SimHash> readSimHashVectorFromFile( const std::string& filename);
void writeSimHashVectorToFile( const std::vector<SimHash>& ar, const std::string& filename);

VectorSpaceModelConfig readConfigurationFromFile( const std::string& filename, ErrorBufferInterface* errorhnd);
void writeConfigurationToFile( const VectorSpaceModelConfig& config, const std::string& filename);

LshModel readLshModelFromFile( const std::string& filename);
void writeLshModelToFile( const LshModel& model, const std::string& filename);

SimRelationMap readSimRelationMapFromFile( const std::string& filename);
void writeSimRelationMapToFile( const SimRelationMap& simrelmap, const std::string& filename);

SampleFeatureIndexMap readSampleFeatureIndexMapFromFile( const std::string& filename);
void writeSampleFeatureIndexMapToFile( const SampleFeatureIndexMap& map, const std::string& filename);

FeatureSampleIndexMap readFeatureSampleIndexMapFromFile( const std::string& filename);
void writeFeatureSampleIndexMapToFile( const FeatureSampleIndexMap& map, const std::string& filename);

StringList readSampleNamesFromFile( const std::string& filename);
void writeSampleNamesToFile( const StringList& names, const std::string& filename);

typedef std::vector<double> Vector;
std::vector<Vector> readVectorsFromFile( strus::DataRecordFile& vecfile);
Vector readVectorFromFile( std::size_t index, strus::DataRecordFile& vecfile);
void appendVectorsToFile( const std::vector<Vector>& vectors, strus::DataRecordFile& vecfile);

}//namespace
#endif


