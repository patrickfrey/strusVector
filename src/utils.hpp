/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some utility functions that are centralised to control dependencies to boost
#ifndef _STRUS_UTILS_HPP_INCLUDED
#define _STRUS_UTILS_HPP_INCLUDED
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/unordered_map.hpp>
#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition_variable.hpp> 

namespace strus {
namespace utils {

std::string tolower( const char* val);
std::string tolower( const std::string& val);
std::string trim( const std::string& val);
bool caseInsensitiveEquals( const std::string& val1, const std::string& val2);
bool caseInsensitiveStartsWith( const std::string& val, const std::string& prefix);
int toint( const std::string& val);
std::string tostring( int val);

void aligned_free( void *ptr);
void* aligned_malloc( std::size_t size, std::size_t alignment);

template<typename Key, typename Elem>
class UnorderedMap
	:public boost::unordered_map<Key,Elem>
{
public:
	UnorderedMap(){}
	UnorderedMap( const UnorderedMap& o)
		:boost::unordered_map<Key,Elem>(){}
};

template <class X>
class SharedPtr
	:public boost::shared_ptr<X>
{
public:
	SharedPtr( X* ptr)
		:boost::shared_ptr<X>(ptr){}
	SharedPtr( const SharedPtr& o)
		:boost::shared_ptr<X>(o){}
	SharedPtr()
		:boost::shared_ptr<X>(){}
};

template <typename IntegralCounterType>
class AtomicCounter
	:public boost::atomic<IntegralCounterType>
{
public:
	///\brief Constructor
	AtomicCounter( IntegralCounterType initialValue_=0)
		:boost::atomic<IntegralCounterType>(initialValue_)
	{}

	///\brief Increment of the counter
	void increment( IntegralCounterType val = 1)
	{
		boost::atomic<IntegralCounterType>::fetch_add( val, boost::memory_order_acquire);
	}

	///\brief Decrement of the counter
	void decrement( IntegralCounterType val = 1)
	{
		boost::atomic<IntegralCounterType>::fetch_sub( val, boost::memory_order_acquire);
	}

	///\brief Increment of the counter
	///\return the new value of the counter after the increment operation
	IntegralCounterType allocIncrement( IntegralCounterType val = 1)
	{
		return boost::atomic<IntegralCounterType>::fetch_add( val, boost::memory_order_acquire);
	}

	///\brief Increment of the counter
	///\return the current value of the counter
	IntegralCounterType value() const
	{
		return boost::atomic<IntegralCounterType>::load( boost::memory_order_acquire);
	}

	///\brief Initialization of the counter
	///\param[in] val the value of the counter
	void set( const IntegralCounterType& val)
	{
		boost::atomic<IntegralCounterType>::store( val);
	}

	///\brief Compare current value with 'testval', change it to 'newval' if matches
	///\param[in] testval the value of the counter
	///\param[in] newval the value of the counter
	///\return true on success
	bool test_and_set( IntegralCounterType testval, IntegralCounterType newval)
	{
		return boost::atomic<IntegralCounterType>::compare_exchange_strong( testval, newval, boost::memory_order_acquire, boost::memory_order_acquire);
	}
};

typedef boost::mutex Mutex;
typedef boost::recursive_mutex RecursiveMutex;
typedef boost::mutex::scoped_lock ScopedLock;
typedef boost::unique_lock<boost::mutex> UniqueLock;
typedef boost::unique_lock<boost::recursive_mutex> RecursiveUniqueLock;
typedef boost::condition_variable ConditionVariable;

}} //namespace
#endif


