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
	time_t tim;
	struct tm timrec;
	char timstr[ 64];

	tim = time( NULL);
	localtime_r( &tim, &timrec);
	::strftime( timstr, sizeof(timstr), "%Y-%m-%d %H:%M:%S", &timrec);
	std::string logline( string_format( "%s %s", timstr, line.c_str()));
	if (m_logout) (*m_logout) << logline << std::endl;
	return *this;
}

