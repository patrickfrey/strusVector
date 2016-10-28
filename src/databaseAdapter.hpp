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
	ConceptIndex readNofConcepts() const;
	unsigned int readState() const;

	std::vector<SimHash> readSampleSimhashVector() const;
	std::vector<SimHash> readResultSimhashVector() const;

	std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const;
	SimRelationMap readSimRelationMap() const;
	std::vector<SampleIndex> readConceptSampleIndices( const ConceptIndex& fidx) const;
	ConceptSampleIndexMap readConceptSampleIndexMap();
	std::vector<ConceptIndex> readSampleConceptIndices( const SampleIndex& sidx) const;
	SampleConceptIndexMap readSampleConceptIndexMap();
	VectorSpaceModelConfig readConfig() const;
	LshModel readLshModel() const;

	void writeVersion();
	void writeNofSamples( const SampleIndex& nofSamples);
	void writeNofConcepts( const ConceptIndex& nofConcepts);
	void writeState( unsigned int state);
	void writeSample( const SampleIndex& sidx, const std::string& name, const Vector& vec, const SimHash& simHash);
	void writeResultSimhashVector( const std::vector<SimHash>& ar);
	void writeSimRelationMap( const SimRelationMap& simrelmap, unsigned int commitsize);
	void writeSampleConceptIndexMap( const SampleConceptIndexMap& sfmap);
	void writeConceptSampleIndexMap( const ConceptSampleIndexMap& fsmap);
	void writeConfig( const VectorSpaceModelConfig& config);
	void writeLshModel( const LshModel& model);

	bool isempty();
	void clear();

	void deleteConfig();
	void deleteVariables();
	void deleteSamples();
	void deleteSampleSimhashVector();
	void deleteResultSimhashVector();
	void deleteSimRelationMap();
	void deleteSampleConceptIndexMap();
	void deleteConceptSampleIndexMap();
	void deleteLshModel();
	void commit();

	std::string config() const		{return m_database->config();}

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
		KeySampleConceptIndexMap = 'f',
		KeyConceptSampleIndexMap = 's'
	};

private:
	void beginTransaction();
	std::vector<SimHash> readSimhashVector( const KeyPrefix& prefix) const;
	unsigned int readVariable( const char* name) const;
	IndexListMap<strus::Index,strus::Index> readIndexListMap( const KeyPrefix& prefix) const;

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

