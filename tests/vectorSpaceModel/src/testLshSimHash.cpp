/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program for the similarity Hash data structure
#include "simHash.hpp"
#include "strus/base/math.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <stdexcept>

#undef STRUS_LOWLEVEL_DEBUG

static void initRandomNumberGenerator()
{
	time_t nowtime;
	struct tm* now;

	::time( &nowtime);
	now = ::localtime( &nowtime);

	unsigned int seed = (now->tm_year+10000) + (now->tm_mon+100) + (now->tm_mday+1);
	std::srand( seed+2);
}

static strus::SimHash createDivBitSet( unsigned int size, unsigned int div)
{
	strus::SimHash rt( size, false, 0/*id*/);
	unsigned int ki=2, ke=size/2;
	for (; ki<=ke; ++ki)
	{
		if ((div*ki) <= size)
		{
			rt.set( (div*ki)-1, true);
		}
	}
	return rt;
}

static bool isPrimeNumber( unsigned int num)
{
	unsigned int di=2, de=(unsigned int)(strus::Math::sqrt(num));
	for (; di <= de; ++di)
	{
		if (num % di == 0) return false;
	}
	return true;
}

static strus::SimHash createPrimBitSet( unsigned int size)
{
	strus::SimHash rt( size, false, 0/*id*/);
	unsigned int ii=2;
	for (; ii<=size; ++ii)
	{
		if (isPrimeNumber( ii)) 
		{
			rt.set( ii-1, true);
		}
	}
	return rt;
}

static void doMatch( const char* text, const strus::SimHash& res, const strus::SimHash& exp)
{
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cout << "checking " << text << " of " << res.tostring() << std::endl;
#endif
	if (exp.tostring() != res.tostring())
	{
		std::cerr << "RES " << res.tostring() << std::endl;
		std::cerr << "EXP " << exp.tostring() << std::endl;
		throw std::runtime_error( std::string("matching of '") + text + "' failed");
	}
}

int main( int argc, const char** argv)
{
	try
	{
		enum {NofTests=100,MaxSize=1000};
		initRandomNumberGenerator();
		unsigned int sizear[ NofTests];
		sizear[0] = 32;
		sizear[1] = 64;
		sizear[2] = 128;
		sizear[3] = 31;
		sizear[4] = 63;
		sizear[5] = 127;
		sizear[6] = 33;
		sizear[7] = 65;
		sizear[8] = 129;
		unsigned int ti=9, te=NofTests;
		for (; ti != te; ++ti)
		{
			sizear[ti] = (rand() % MaxSize + 1);
		}
		for (ti=0; ti != te; ++ti)
		{
			{
				std::string expected( "0 ");
				std::cerr << "test RANDOM SET " << (ti+1) << " size " << sizear[ti] << std::endl;
				strus::SimHash res( sizear[ti], false, 0/*id*/);
				std::vector<unsigned int> elems;
				for (unsigned int ii=0; ii<10; ++ii)
				{
					unsigned int idx = rand() % sizear[ti];
					res.set( idx, true);
					elems.push_back( idx);
				}
				std::sort( elems.begin(), elems.end());
				std::vector<unsigned int>::const_iterator ei = elems.begin(), ee = elems.end();
				unsigned int prev = 0;
				for (; ei != ee; ++ei)
				{
					if (prev > *ei) continue;
					for (; prev < *ei; ++prev)
					{
						expected.push_back( '0');
						if ((prev+1) % 64 == 0)
						{
							expected.push_back( '|');
						}
					}
					expected.push_back( '1');
					if ((prev+1) % 64 == 0)
					{
						expected.push_back( '|');
					}
					++prev;
				}
				for (; prev < sizear[ti]; ++prev)
				{
					expected.push_back( '0');
					if ((prev+1) % 64 == 0)
					{
						expected.push_back( '|');
					}
				}
				if (expected.size() && expected[ expected.size()-1] == '|')
				{
					expected.resize( expected.size()-1);
				}
				std::string result( res.tostring());
#ifdef STRUS_LOWLEVEL_DEBUG
				std::cout << "expected " << expected << std::endl;
				std::cout << "result   " << result << std::endl;
#endif
				if (expected != result)
				{
					std::cerr << "RES " << res.tostring() << std::endl;
					std::cerr << "EXP " << expected << std::endl;
					throw std::runtime_error( "matching of 'random set' failed");
				}
			}
		}
		for (ti=0; ti != te; ++ti)
		{
			std::cerr << "test PRIME NUMBER SIEVE " << (ti+1) << " size " << sizear[ti] << std::endl;
			strus::SimHash expected = createPrimBitSet( sizear[ti]);
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cout << "expected " << expected.tostring() << std::endl;
#endif
			strus::SimHash res = createDivBitSet( sizear[ti], 2);
			res.set( 0, true);
			unsigned int di=3, de=sizear[ti]/2;
			for (; di < de; ++di)
			{
				res |= createDivBitSet( sizear[ti], di);
			}
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cout << "inv res  " << res.tostring() << std::endl;
#endif
			res = ~res;
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cout << "result   " << res.tostring() << std::endl;
#endif
			strus::SimHash zerofill( sizear[ti], false, 0/*id*/);
			doMatch( " primes inverse divisible", res, expected);
			doMatch( " result AND expected equals OR", res & expected, res | expected);
			doMatch( " result XOR expected equals reset", res ^ expected, zerofill);

			strus::SimHash res_AND( res); res_AND &= expected;
			strus::SimHash res_OR( res); res_OR |= expected;
			doMatch( " result AND ASSIGN expected equals OR ASSIGN", res_AND, res_OR);
			strus::SimHash res_XOR( res); res_XOR ^= expected;
			doMatch( " result AND ASSIGN expected equals OR ASSIGN", res_XOR, zerofill);
		}
		for (ti=0; ti != te; ++ti)
		{
			std::cerr << "test INV OF AND equals OR of INVs " << (ti+1) << " size " << sizear[ti] << std::endl;
			strus::SimHash aa = strus::SimHash::randomHash( sizear[ti], ti*987+1, 0/*id*/);
			strus::SimHash bb = strus::SimHash::randomHash( sizear[ti], ti*123+1, 0/*id*/);
			doMatch( " INV OF AND equals OR of INVs", ~(aa & bb), ~aa | ~bb);
		}
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		return -2;
	}
	catch (const std::logic_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return -3;
	}
}
