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


class DatabaseAdapter
{
public:
	DatabaseAdapter( const DatabaseInterface* database_, const std::string& config, ErrorBufferInterface* errorhnd_);
	~DatabaseAdapter(){}

	void checkVersion();

	typedef std::vector<double> Vector;
	Vector readSampleVector( const SampleIndex& sidx) const;
	std::string readSampleName( const SampleIndex& sidx) const;
	SampleIndex readSampleIndex( const std::string& name) const;
	SampleIndex readNofSamples() const;
	SampleIndex readNofFeatures() const;

	std::vector<SimHash> readSampleSimhashVector() const;
	std::vector<SimHash> readResultSimhashVector() const;

	std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const;
	SimRelationMap readSimRelationMap() const;
	std::vector<SampleIndex> readFeatureSampleIndices( const FeatureIndex& fidx) const;
	std::vector<FeatureIndex> readSampleFeatureIndices( const SampleIndex& sidx) const;
	VectorSpaceModelConfig readConfig() const;
	LshModel readLshModel() const;

	void writeVersion();
	void writeVariables( const SampleIndex& nofSamples, const FeatureIndex& nofFeatures);
	void writeSample( const SampleIndex& sidx, const std::string& name, const Vector& vec, const SimHash& simHash);
	void writeResultSimhashVector( const std::vector<SimHash>& ar);
	void writeSimRelationMap( const SimRelationMap& simrelmap);
	void writeSampleFeatureIndexMap( const SampleFeatureIndexMap& sfmap);
	void writeFeatureSampleIndexMap( const FeatureSampleIndexMap& fsmap);
	void writeConfig( const VectorSpaceModelConfig& config);
	void writeLshModel( const LshModel& model);

	void deleteVariables();
	void deleteSamples();
	void deleteSampleSimhashVector();
	void deleteResultSimhashVector();
	void deleteSimRelationMap();
	void deleteSampleFeatureIndexMap();
	void deleteFeatureSampleIndexMap();
	void commit();

public:
	enum KeyPrefix
	{
		KeyVersion = 'V',
		KeyVariable = '*',
		KeySampleVector = '$',
		KeySampleName = '@',
		KeySampleNameInv = '#',
		KeySampleSimHash = 'S',
		KeyResultSimHash = 'R',
		KeyConfig = 'C',
		KeyLshModel = 'L',
		KeySimRelationMap = 'M',
		KeySampleFeatureIndexMap = 'f',
		KeyFeatureSampleIndexMap = 's'
	};

private:
	void beginTransaction();
	std::vector<SimHash> readSimhashVector( const KeyPrefix& prefix) const;
	unsigned int readVariable( const char* name) const;

	void writeSimhash( const KeyPrefix& prefix, const SampleIndex& sidx, const SimHash& simHash);
	void writeSimhashVector( const KeyPrefix& prefix, const std::vector<SimHash>& ar);
	void writeSampleIndex( const SampleIndex& sidx, const std::string& name);
	void writeSampleName( const SampleIndex& sidx, const std::string& name);
	void writeSampleVector( const SampleIndex& sidx, const Vector& vec);
	void writeVariable( const char* name, unsigned int value);

	void deleteSubTree( const KeyPrefix& prefix);

private:
	Reference<DatabaseClientInterface> m_database;
	Reference<DatabaseTransactionInterface> m_transaction;
	ErrorBufferInterface* m_errorhnd;
};


} //namespace
#endif

