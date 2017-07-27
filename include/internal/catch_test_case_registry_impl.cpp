/*
 *  Created by Martin on 25/07/2017
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "catch_test_case_registry_impl.hpp"

#include "catch_context.h"
#include "catch_interfaces_registry_hub.h"
#include "catch_string_manip.h"
#include "catch_test_case_info.h"

#include <sstream>

namespace Catch {

    RandomNumberGenerator::result_type RandomNumberGenerator::operator()( result_type n ) const { return std::rand() % n; }
    RandomNumberGenerator::result_type RandomNumberGenerator::operator()() const { return std::rand() % max(); }

    std::vector<TestCase> sortTests( IConfig const& config, std::vector<TestCase> const& unsortedTestCases ) {

        std::vector<TestCase> sorted = unsortedTestCases;

        switch( config.runOrder() ) {
            case RunTests::InLexicographicalOrder:
                std::sort( sorted.begin(), sorted.end() );
                break;
            case RunTests::InRandomOrder:
                seedRng( config );
                RandomNumberGenerator::shuffle( sorted );
                break;
            case RunTests::InDeclarationOrder:
                // already in declaration order
                break;
        }
        return sorted;
    }
    bool matchTest( TestCase const& testCase, TestSpec const& testSpec, IConfig const& config ) {
        return testSpec.matches( testCase ) && ( config.allowThrows() || !testCase.throws() );
    }

    void enforceNoDuplicateTestCases( std::vector<TestCase> const& functions ) {
        std::set<TestCase> seenFunctions;
        for( auto const& function : functions ) {
            auto prev = seenFunctions.insert( function );
            CATCH_ENFORCE( prev.second,
                    "error: TEST_CASE( \"" << function.name << "\" ) already defined.\n"
                    << "\tFirst seen at " << prev.first->getTestCaseInfo().lineInfo << "\n"
                    << "\tRedefined at " << function.getTestCaseInfo().lineInfo );
        }
    }

    std::vector<TestCase> filterTests( std::vector<TestCase> const& testCases, TestSpec const& testSpec, IConfig const& config ) {
        std::vector<TestCase> filtered;
        filtered.reserve( testCases.size() );
        for( auto const& testCase : testCases )
            if( matchTest( testCase, testSpec, config ) )
                filtered.push_back( testCase );
        return filtered;
    }
    std::vector<TestCase> const& getAllTestCasesSorted( IConfig const& config ) {
        return getRegistryHub().getTestCaseRegistry().getAllTestsSorted( config );
    }

    void TestRegistry::registerTest( TestCase const& testCase ) {
        std::string name = testCase.getTestCaseInfo().name;
        if( name.empty() ) {
            std::ostringstream oss;
            oss << "Anonymous test case " << ++m_unnamedCount;
            return registerTest( testCase.withName( oss.str() ) );
        }
        m_functions.push_back( testCase );
    }

    std::vector<TestCase> const& TestRegistry::getAllTests() const {
        return m_functions;
    }
    std::vector<TestCase> const& TestRegistry::getAllTestsSorted( IConfig const& config ) const {
        if( m_sortedFunctions.empty() )
            enforceNoDuplicateTestCases( m_functions );

        if(  m_currentSortOrder != config.runOrder() || m_sortedFunctions.empty() ) {
            m_sortedFunctions = sortTests( config, m_functions );
            m_currentSortOrder = config.runOrder();
        }
        return m_sortedFunctions;
    }



    ///////////////////////////////////////////////////////////////////////////
    TestInvokerAsFunction::TestInvokerAsFunction( void(*testAsFunction)() ) noexcept : m_testAsFunction( testAsFunction ) {}

    void TestInvokerAsFunction::invoke() const {
        m_testAsFunction();
    }

    std::string extractClassName( std::string const& classOrQualifiedMethodName ) {
        std::string className = classOrQualifiedMethodName;
        if( startsWith( className, '&' ) )
        {
            std::size_t lastColons = className.rfind( "::" );
            std::size_t penultimateColons = className.rfind( "::", lastColons-1 );
            if( penultimateColons == std::string::npos )
                penultimateColons = 1;
            className = className.substr( penultimateColons, lastColons-penultimateColons );
        }
        return className;
    }

} // end namespace Catch