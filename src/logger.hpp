/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Logger for reporting status of long during processes in the library
#ifndef _STRUS_VECTOR_LOGGER_HPP_INCLUDED
#define _STRUS_VECTOR_LOGGER_HPP_INCLUDED
#include "strus/base/thread.hpp"
#include <iostream>
#include <fstream>

namespace strus {

class Logger
{
public:
	Logger();
	Logger( const char* logfile);
	Logger& operator << (const std::string& line);

	operator bool() const
	{
		return m_logout != 0;
	}

	void countItems( unsigned int addcnt);
	void printAccuLine( const char* format);

private:
	Logger( const Logger&){}		//< non copyable
	void operator=(const Logger&){}		//< non copyable

private:
	strus::mutex m_mutex;
	unsigned int m_count;
	std::ostream* m_logout;
	std::ofstream m_logfilestream;
};

}//namespace
#endif
