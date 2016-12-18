/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Database abstraction (wrapper) for the standard strus vector storage
#ifndef _STRUS_VECTOR_DATABASE_HELPERS_HPP_INCLUDED
#define _STRUS_VECTOR_DATABASE_HELPERS_HPP_INCLUDED
#include "strus/base/hton.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/stdint.h"
#include "strus/index.hpp"
#include "internationalization.hpp"
#include <string>
#include <cstring>

namespace strus {

struct DatabaseKeyBuffer
{
public:
	explicit DatabaseKeyBuffer( char prefix)
		:m_itr(1)
	{
		m_buf[0] = prefix;
		m_buf[1] = '\0';
	}
	DatabaseKeyBuffer()
		:m_itr(0)
	{
		m_buf[0] = '\0';
	}
	template <typename ScalarType>
	DatabaseKeyBuffer& operator[]( const ScalarType& val)
	{
		if (8 + m_itr > MaxKeySize) throw strus::runtime_error(_TXT("array bound write in database key buffer"));
		std::size_t sz = utf8encode( m_buf + m_itr, val);
		if (!sz) throw strus::runtime_error( _TXT( "illegal unicode character (%s)"), __FUNCTION__);
		m_itr += sz;
		m_buf[ m_itr] = 0;
		return *this;
	}
	DatabaseKeyBuffer& operator()( const std::string& str)
	{
		if (str.size() + 1 + m_itr > MaxKeySize) throw strus::runtime_error(_TXT("array bound write in database key buffer"));
		std::memcpy( m_buf + m_itr, str.c_str(), str.size()+1);
		m_itr += str.size() + 1;
		m_buf[ m_itr] = 0;
		return *this;
	}

	const char* c_str() const	{return m_buf;}
	std::size_t size() const	{return m_itr;}

private:
	enum {MaxKeySize=256};
	char m_buf[ MaxKeySize];
	std::size_t m_itr;
};

struct DatabaseKeyScanner
{
public:
	explicit DatabaseKeyScanner( const std::string& blob)
		:m_itr(blob.c_str()),m_end(blob.c_str() + blob.size())
	{}
	explicit DatabaseKeyScanner( const char* blob, std::size_t blobsize)
		:m_itr(blob),m_end(blob + blobsize)
	{}
	DatabaseKeyScanner& operator()( char const*& str, std::size_t& strsize)
	{
		str = m_itr;
		while (m_itr != m_end && *m_itr != '\0') ++m_itr;
		if (m_itr == m_end)
		{
			throw strus::runtime_error(_TXT("illegal key in database"));
		}
		strsize = m_itr - str;
		++m_itr;
		return *this;
	}

	DatabaseKeyScanner& operator[]( Index& val)
	{
		unsigned char keylen = utf8charlen( *m_itr);
		if (!keylen || keylen > m_end - m_itr) throw strus::runtime_error(_TXT("array bound read in database key scanner"));
		val = utf8decode( m_itr, keylen);
		m_itr += keylen;
		return *this;
	}

	bool eof() const
	{
		return m_itr == m_end;
	}

private:
	char const* m_itr;
	const char* m_end;
};

struct DatabaseValueBuffer
{
public:
	DatabaseValueBuffer()
		:m_itr(0)
	{
		m_buf[0] = '\0';
	}
	template <typename ScalarType>
	DatabaseValueBuffer& operator[]( const ScalarType& val)
	{
		typedef typename ByteOrder<ScalarType>::net_value_type NetScalarType;

		if (sizeof(NetScalarType) + m_itr > MaxValueSize) throw strus::runtime_error(_TXT("array bound write in database value buffer"));
		NetScalarType val_n = ByteOrder<ScalarType>::hton( val);
		std::memcpy( m_buf + m_itr, &val_n, sizeof(val_n));
		m_itr += sizeof(val_n);
		return *this;
	}

	const char* c_str() const	{return m_buf;}
	std::size_t size() const	{return m_itr;}

private:
	enum {MaxValueSize=1024};
	char m_buf[ 256];
	std::size_t m_itr;
};

struct DatabaseValueScanner
{
public:
	explicit DatabaseValueScanner( const std::string& blob)
		:m_itr(blob.c_str()),m_end(blob.c_str() + blob.size())
	{}
	explicit DatabaseValueScanner( const char* blob, std::size_t blobsize)
		:m_itr(blob),m_end(blob + blobsize)
	{}
	template <typename ScalarType>
	DatabaseValueScanner& operator[]( ScalarType& val)
	{
		typedef typename ByteOrder<ScalarType>::net_value_type NetScalarType;

		if (sizeof(NetScalarType) > (unsigned int)(m_end - m_itr)) throw strus::runtime_error(_TXT("array bound read in database value scanner"));
		NetScalarType val_n;
		std::memcpy( &val_n, m_itr, sizeof(val_n));
		m_itr += sizeof(val_n);
		val = ByteOrder<ScalarType>::ntoh( val_n);
		return *this;
	}

	bool eof() const
	{
		return m_itr == m_end;
	}

private:
	char const* m_itr;
	const char* m_end;
};


} //namespace
#endif



