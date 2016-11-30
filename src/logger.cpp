/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Logger for reporting status of long during processes in the library
#include "internationalization.hpp"
#include "logger.hpp"
#include "strus/base/string_format.hpp"
#include <iostream>
#include <stdexcept>
#include <time.h>

using namespace strus;

Logger::Logger()
	:m_logout(&std::cerr)
{}

Logger::Logger( const char* logfile)
	:m_logout(0)
{
	if (logfile)
	{
		if (logfile[0] != '-' || logfile[1])
		{
			try
			{
				m_logfilestream.open( logfile, std::ofstream::out);
				m_logout = &m_logfilestream;
			}
			catch (const std::exception& err)
			{
				throw strus::runtime_error(_TXT("failed to open logfile '%s': %s"), logfile, err.what());
			}
		}
		else
		{
			m_logout = &std::cerr;
		}
	}
}

Logger& Logger::operator << (const std::string& line)
{
	if (m_logout)
	{
		time_t tim;
		struct tm timrec;
		char timstr[ 64];
	
		tim = time( NULL);
		localtime_r( &tim, &timrec);
		::strftime( timstr, sizeof(timstr), "%Y-%m-%d %H:%M:%S", &timrec);
		std::string logline( string_format( "%s %s", timstr, line.c_str()));
		utils::ScopedLock lock( m_mutex);
		(*m_logout) << logline << std::endl;
	}
	return *this;
}

void Logger::countItems( unsigned int addcnt)
{
	utils::ScopedLock lock( m_mutex);
	m_count += addcnt;
}

void Logger::printAccuLine( const char* format)
{
	unsigned int count_value;
	{
		utils::ScopedLock lock( m_mutex);
		count_value = m_count;
		m_count = 0;
	}
	std::string line( string_format( format, count_value));
	operator << (line);
}

