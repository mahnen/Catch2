/*
 *  Created by Phil on 8/8/2017.
 *  Copyright 2017 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "catch_assertionhandler.h"
#include "catch_assertionresult.h"
#include "catch_interfaces_capture.h"
#include "catch_interfaces_runner.h"
#include "catch_interfaces_config.h"
#include "catch_context.h"
#include "catch_debugger.h"
#include "catch_interfaces_registry_hub.h"
#include "catch_capture_matchers.h"

#include <cassert>

namespace Catch {

    auto operator <<( std::ostream& os, ITransientExpression const& expr ) -> std::ostream& {
        expr.streamReconstructedExpression( os );
        return os;
    }

    LazyExpression::LazyExpression( bool isNegated )
    :   m_isNegated( isNegated )
    {}

    LazyExpression::LazyExpression( LazyExpression const& other ) : m_isNegated( other.m_isNegated ) {}

    LazyExpression::operator bool() const {
        return m_transientExpression != nullptr;
    }

    auto operator << ( std::ostream& os, LazyExpression const& lazyExpr ) -> std::ostream& {
        if( lazyExpr.m_isNegated )
            os << "!";

        if( lazyExpr ) {
            if( lazyExpr.m_isNegated && lazyExpr.m_transientExpression->isBinaryExpression() )
                os << "(" << *lazyExpr.m_transientExpression << ")";
            else
                os << *lazyExpr.m_transientExpression;
        }
        else {
            os << "{** error - unchecked empty expression requested **}";
        }
        return os;
    }

    AssertionHandler::AssertionHandler
        (   StringRef macroName,
            SourceLineInfo const& lineInfo,
            StringRef capturedExpression,
            ResultDisposition::Flags resultDisposition )
    :   m_assertionInfo{ macroName, lineInfo, capturedExpression, resultDisposition },
        m_resultCapture( getResultCapture() )
    {
        m_resultCapture.assertionStarting( m_assertionInfo );
    }
    AssertionHandler::~AssertionHandler() {
        if ( !m_completed ) {
            handle( ResultWas::ThrewException, "Exception translation was disabled by CATCH_CONFIG_FAST_COMPILE" );
            m_resultCapture.exceptionEarlyReported();
        }
    }

    void AssertionHandler::handle( ITransientExpression const& expr ) {

        bool negated = isFalseTest( m_assertionInfo.resultDisposition );
        bool result = expr.getResult() != negated;

        if(result && !getCurrentContext().getConfig()->includeSuccessfulResults())
        {
            m_resultCapture.assertionRun();
            m_resultCapture.assertionPassed();
            return;
        }

        handle( result ? ResultWas::Ok : ResultWas::ExpressionFailed, &expr, negated );
    }
    void AssertionHandler::handle( ResultWas::OfType resultType ) {
        handle( resultType, nullptr, false );
    }
    void AssertionHandler::handle( ResultWas::OfType resultType, StringRef const& message ) {
        AssertionResultData data( resultType, LazyExpression( false ) );
        data.message = message;
        handle( data, nullptr );
    }
    void AssertionHandler::handle( ResultWas::OfType resultType, ITransientExpression const* expr, bool negated ) {
        AssertionResultData data( resultType, LazyExpression( negated ) );
        handle( data, expr );
    }
    void AssertionHandler::handle( AssertionResultData const& resultData, ITransientExpression const* expr ) {

        m_resultCapture.assertionRun();

        AssertionResult assertionResult{ m_assertionInfo, resultData };
        assertionResult.m_resultData.lazyExpression.m_transientExpression = expr;

        m_resultCapture.assertionEnded( assertionResult );

        if( !assertionResult.isOk() ) {
            m_shouldDebugBreak = getCurrentContext().getConfig()->shouldDebugBreak();
            m_shouldThrow =
                    getCurrentContext().getRunner()->aborting() ||
                    (m_assertionInfo.resultDisposition & ResultDisposition::Normal);
        }
    }

    auto AssertionHandler::allowThrows() const -> bool {
        return getCurrentContext().getConfig()->allowThrows();
    }

    void AssertionHandler::complete() {
        setCompleted();
        if( m_shouldDebugBreak ) {

            // If you find your debugger stopping you here then go one level up on the
            // call-stack for the code that caused it (typically a failed assertion)

            // (To go back to the test and change execution, jump over the throw, next)
            CATCH_BREAK_INTO_DEBUGGER();
        }
        if( m_shouldThrow )
            throw Catch::TestFailureException();
    }
    void AssertionHandler::setCompleted() {
        m_completed = true;
    }

    void AssertionHandler::useActiveException() {
        handle( ResultWas::ThrewException, Catch::translateActiveException() );
    }

    // This is the overload that takes a string and infers the Equals matcher from it
    // The more general overload, that takes any string matcher, is in catch_capture_matchers.cpp
    void handleExceptionMatchExpr( AssertionHandler& handler, std::string const& str, StringRef matcherString  ) {
        handleExceptionMatchExpr( handler, Matchers::Equals( str ), matcherString );
    }

} // namespace Catch
