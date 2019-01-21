/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for retrieval of the most similar LSH values
#include "simHashReader.hpp"
#include "internationalization.hpp"

using namespace strus;

SimHashReaderDatabase::SimHashReaderDatabase( const DatabaseAdapter* database_, const std::string& type_)
	:m_database(database_),m_type(type_),m_typeno(database_->readTypeno( type_)),m_aridx(0),m_ar()
{
	if (!m_typeno) throw strus::runtime_error( _TXT("error instantiating similarity hash reader: unknown type %s"), m_type.c_str());
}

const SimHash* SimHashReaderDatabase::loadFirst()
{
	m_aridx = 0;
	m_ar = m_database->readSimHashVector( m_typeno, 1/*featnostart*/, ReadChunkSize);
	if (m_ar.empty()) return NULL;
	return &m_ar[ m_aridx++];
}

const SimHash* SimHashReaderDatabase::loadNext()
{
	if (m_aridx >= m_ar.size())
	{
		if (m_ar.empty()) return loadFirst();
		Index featnostart = m_ar.back().id()+1;
		m_ar = m_database->readSimHashVector( m_typeno, featnostart, ReadChunkSize);
		if (m_ar.empty()) return NULL;
		m_aridx = 0;
	}
	return &m_ar[ m_aridx++];
}

SimHash SimHashReaderDatabase::load( const Index& featno) const
{
	return m_database->readSimHash( m_typeno, featno);
}



