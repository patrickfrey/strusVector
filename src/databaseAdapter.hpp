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
#include "strus/databaseCursorInterface.hpp"
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
#include <iostream>

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
	SampleIndex readNofSimRelations() const;
	ConceptIndex readNofConcepts( const std::string& clname) const;
	unsigned int readState() const;
	std::vector<std::pair<std::string,uint64_t> > readVariables() const;

	std::vector<std::string> readConceptClassNames() const;

	SimHash readSampleSimhash( const SampleIndex& sidx) const;
	std::vector<SimHash> readSampleSimhashVector() const;
	std::vector<SimHash> readConceptSimhashVector( const std::string& clname) const;

	SampleIndex readLastSimRelationIndex() const;
	std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const;
	SimRelationMap readSimRelationMap() const;
	std::vector<SampleIndex> readSimSingletons() const;
	std::vector<SampleIndex> readConceptSampleIndices( const std::string& clname, const ConceptIndex& cidx) const;
	ConceptSampleIndexMap readConceptSampleIndexMap( const std::string& clname) const;
	std::vector<ConceptIndex> readSampleConceptIndices( const std::string& clname, const SampleIndex& sidx) const;
	SampleConceptIndexMap readSampleConceptIndexMap( const std::string& clname) const;
	std::vector<SampleIndex> readConceptSingletons( const std::string& clname) const;
	std::vector<ConceptIndex> readConceptDependencies( const std::string& clname, const ConceptIndex& cidx, const std::string& depclname) const;

	VectorSpaceModelConfig readConfig() const;
	LshModel readLshModel() const;

	void writeVersion();
	void writeNofSamples( const SampleIndex& nofSamples);
	void writeNofConcepts( const std::string& clname, const ConceptIndex& nofConcepts);
	void writeNofSimRelations( const SampleIndex& nofSamples);
	void writeConceptClassNames( const std::vector<std::string>& clnames);
	void writeState( unsigned int state);
	void writeSample( const SampleIndex& sidx, const std::string& name, const Vector& vec, const SimHash& simHash);
	void writeConceptSimhashVector( const std::string& clname, const std::vector<SimHash>& ar);
	void writeSimRelationMap( const SimRelationMap& simrelmap);
	void writeSimRelationRow( const SampleIndex& sidx, const SimRelationMap::Row& row);
	void writeSampleConceptIndexMap( const std::string& clname, const SampleConceptIndexMap& sfmap);
	void writeConceptSampleIndexMap( const std::string& clname, const ConceptSampleIndexMap& fsmap);
	void writeConceptDependencies( const std::string& clname, const ConceptIndex& cidx, const std::string& depclname, const std::vector<ConceptIndex>& deplist);
	void writeConfig( const VectorSpaceModelConfig& config);
	void writeLshModel( const LshModel& model);

	bool isempty();
	void clear();

	void deleteConfig();
	void deleteVariables();
	void deleteConceptClassNames();
	void deleteSamples();
	void deleteSampleSimhashVectors();
	void deleteConceptSimhashVectors();
	void deleteSimRelationMap();
	void deleteSampleConceptIndexMaps();
	void deleteConceptSampleIndexMaps();
	void deleteConceptDependencies();
	void deleteLshModel();
	void commit();

	std::string config() const		{return m_database->config();}

	bool dumpFirst( std::ostream& out, const std::string& keyprefix_);
	bool dumpNext( std::ostream& out);

public:
	enum KeyPrefix
	{
		KeyVersion = 'V',
		KeyVariable = '*',
		KeyConceptClassNames = 'K',
		KeySampleVector = '$',
		KeySampleName = '@',
		KeySampleNameInv = '#',
		KeySampleSimHash = 'S',
		KeyConceptSimhash = 'R',
		KeyConfig = 'C',
		KeyLshModel = 'L',
		KeySimRelationMap = 'M',
		KeySampleConceptIndexMap = 'f',
		KeyConceptSampleIndexMap = 's',
		KeyConceptDependency = '>'
	};

private:
	void beginTransaction();
	SimHash readSimhash( const KeyPrefix& prefix, const std::string& clname, const SampleIndex& sidx) const;
	std::vector<SimHash> readSimhashVector( const KeyPrefix& prefix, const std::string& clname) const;
	unsigned int readVariable( const std::string& name) const;
	IndexListMap<strus::Index,strus::Index> readIndexListMap( const KeyPrefix& prefix, const std::string& clname) const;

	void writeSimhash( const KeyPrefix& prefix, const std::string& clname, const SampleIndex& sidx, const SimHash& simHash);
	void writeSimhashVector( const KeyPrefix& prefix, const std::string& clname, const std::vector<SimHash>& ar);
	void writeSampleIndex( const SampleIndex& sidx, const std::string& name);
	void writeSampleName( const SampleIndex& sidx, const std::string& name);
	void writeSampleVector( const SampleIndex& sidx, const Vector& vec);
	void writeVariable( const std::string& name, unsigned int value);

	void deleteSubTree( const KeyPrefix& prefix);

	void dumpKeyValue( std::ostream& out, const strus::DatabaseCursorInterface::Slice& key, const strus::DatabaseCursorInterface::Slice& value);

private:
	Reference<DatabaseClientInterface> m_database;
	Reference<DatabaseTransactionInterface> m_transaction;
	Reference<DatabaseCursorInterface> m_cursor;
	std::string m_cursorkey;
	ErrorBufferInterface* m_errorhnd;
};


} //namespace
#endif

