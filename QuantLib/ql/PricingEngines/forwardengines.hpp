
/*
 Copyright (C) 2002, 2003 Ferdinando Ametrano

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it under the
 terms of the QuantLib license.  You should have received a copy of the
 license along with this program; if not, please email ferdinando@ametrano.net
 The license is also available online at http://quantlib.org/html/license.html

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file forwardengines.hpp
    \brief Forward (strike-resetting) option engines

    \fullpath
    ql/PricingEngines/%forwardengines.hpp
*/

// $Id$

#ifndef quantlib_forward_engines_h
#define quantlib_forward_engines_h

#include <ql/PricingEngines/vanillaengines.hpp>
#include <ql/Volatilities/impliedvoltermstructure.hpp>
#include <ql/TermStructures/impliedtermstructure.hpp>

namespace QuantLib {

    namespace PricingEngines {

        //! arguments for forward (strike-resetting) option calculation
        template<class ArgumentsType>
        class ForwardOptionArguments : public ArgumentsType {
          public:
            ForwardOptionArguments() : moneyness(Null<double>()),
                                       resetDate(Null<Date>()) {}
            void validate() const;
            double moneyness;
            Date resetDate;
        };

        template<class ArgumentsType>
        void ForwardOptionArguments<ArgumentsType>::validate() const {
            ArgumentsType::validate();
            QL_REQUIRE(moneyness != Null<double>(),
                       "ForwardOptionArguments::validate() : "
                       "null moneyness given");
            QL_REQUIRE(moneyness > 0.0,
                       "ForwardOptionArguments::validate() : "
                       "negative or zero moneyness given");
            QL_REQUIRE(resetDate != Null<Date>(),
                       "ForwardOptionArguments::validate() : "
                       "null reset date given");
            Time resetTime = riskFreeTS->dayCounter().yearFraction(
                riskFreeTS->referenceDate(), resetDate);
            QL_REQUIRE(resetTime >=0,
                       "ForwardOptionArguments::validate() : "
                       "negative reset time given");
            QL_REQUIRE(maturity >= resetTime,
                       "ForwardOptionArguments::validate() : "
                       "reset time greater than maturity");
        }

        //! Forward engine base class
        template<class ArgumentsType, class ResultsType>
        class ForwardEngine : public
            GenericEngine<ForwardOptionArguments<ArgumentsType>,
                          ResultsType> {
        public:
            ForwardEngine(const Handle<GenericEngine<ArgumentsType,
                ResultsType> >&);
            void setOriginalArguments() const;
            void calculate() const;
            void getOriginalResults() const;
        protected:
            Handle<GenericEngine<ArgumentsType, ResultsType> > originalEngine_;
            ArgumentsType* originalArguments_;
            const ResultsType* originalResults_;
        };

        template<class ArgumentsType, class ResultsType>
        ForwardEngine<ArgumentsType, ResultsType>::ForwardEngine(
            const Handle<GenericEngine<ArgumentsType, ResultsType> >&
            originalEngine)
        : originalEngine_(originalEngine) {
            QL_REQUIRE(!originalEngine_.isNull(),
                "ForwardEngine::ForwardEngine : "
                "null engine or wrong engine type");
            originalResults_ = dynamic_cast<const ResultsType*>(
                originalEngine_->results());
            originalArguments_ = dynamic_cast<ArgumentsType*>(
                originalEngine_->arguments());
        }




        template<class ArgumentsType, class ResultsType>
        void ForwardEngine<ArgumentsType, ResultsType>::setOriginalArguments() const {

            originalArguments_->type = arguments_.type;
            // maybe the forward value is "better", in some fashion
            // the right level is needed in order to interpolate
            // the vol 
            originalArguments_->underlying = arguments_.underlying;
            originalArguments_->strike = arguments_.moneyness *
                                            arguments_.underlying;
            originalArguments_->dividendTS = RelinkableHandle<TermStructure>(
                Handle<TermStructure>(new
                    TermStructures::ImpliedTermStructure(
                        arguments_.dividendTS, arguments_.resetDate,
                        arguments_.resetDate)));
            originalArguments_->riskFreeTS = RelinkableHandle<TermStructure>(
                Handle<TermStructure>(new
                    TermStructures::ImpliedTermStructure(
                        arguments_.riskFreeTS, arguments_.resetDate,
                        arguments_.resetDate)));

            // The following approach is ok if the vol is at most
            // time dependant. It is plain wrong if it is asset dependant.
            // In the latter case the right solution would be stochastic
            // volatility or at least local volatility (which unfortunately
            // implies an unrealistic time-decreasing smile)
            originalArguments_->volTS =
                RelinkableHandle<BlackVolTermStructure>(
                    Handle<BlackVolTermStructure>(new
                        VolTermStructures::ImpliedVolTermStructure(
                            arguments_.volTS, arguments_.resetDate)));

            originalArguments_->exerciseType  = arguments_.exerciseType;
            originalArguments_->stoppingTimes = arguments_.stoppingTimes;
            originalArguments_->maturity      = arguments_.maturity;


            originalArguments_->validate();
        }



        template<class ArgumentsType, class ResultsType>
        void ForwardEngine<ArgumentsType, ResultsType>::calculate() const {
            originalEngine_->reset();
            setOriginalArguments();
            originalEngine_->calculate();
            getOriginalResults();
        }

        template<class ArgumentsType, class ResultsType>
        void ForwardEngine<ArgumentsType, ResultsType>::getOriginalResults() const {

            Time resetTime = arguments_.riskFreeTS->dayCounter().yearFraction(
                arguments_.riskFreeTS->referenceDate(), arguments_.resetDate);
            double discQ = arguments_.dividendTS->discount(
                arguments_.resetDate);

            results_.value = discQ * originalResults_->value;
            // I need the strike derivative here ...
            results_.delta = discQ * (originalResults_->delta +
                arguments_.moneyness * originalResults_->strikeSensitivity);
            results_.gamma = 0.0;
            results_.theta = arguments_.dividendTS->zeroYield(
                arguments_.resetDate) * results_.value;
            results_.vega  = discQ * originalResults_->vega;
            results_.rho   = discQ *  originalResults_->rho;
            results_.dividendRho = - resetTime * results_.value
                           + discQ * originalResults_->dividendRho;

        }



        
        
        
        
        
        
        
        //! Forward Performance engine base class
        template<class ArgumentsType, class ResultsType>
        class ForwardPerformanceEngine : public ForwardEngine<ArgumentsType, ResultsType> {
        public:
            ForwardPerformanceEngine(const Handle<GenericEngine<ArgumentsType,
                ResultsType> >&);
            void calculate() const;
            void getOriginalResults() const;
        };

        template<class ArgumentsType, class ResultsType>
        ForwardPerformanceEngine<ArgumentsType, ResultsType>::ForwardPerformanceEngine(
            const Handle<GenericEngine<ArgumentsType, ResultsType> >&
            originalEngine)
        : ForwardEngine<ArgumentsType, ResultsType>(originalEngine) {}

        template<class ArgumentsType, class ResultsType>
        void ForwardPerformanceEngine<ArgumentsType, ResultsType>::calculate() const {

            setOriginalArguments();
            originalEngine_->calculate();
            getOriginalResults();
        }

        template<class ArgumentsType, class ResultsType>
        void ForwardPerformanceEngine<ArgumentsType, ResultsType>::getOriginalResults() const {

            Time resetTime = arguments_.riskFreeTS->dayCounter().yearFraction(
                arguments_.riskFreeTS->referenceDate(), arguments_.resetDate);
            double discR = arguments_.riskFreeTS->discount(arguments_.resetDate);
            // it's a performance option
            discR /= arguments_.underlying;

            double temp = originalResults_->value;
            results_.value = discR * temp;
            results_.delta = 0.0;
            results_.gamma = 0.0;
            results_.theta = arguments_.riskFreeTS->zeroYield(
                arguments_.resetDate) * results_.value;
            results_.vega = discR * originalResults_->vega;
            results_.rho = - resetTime * results_.value +
                discR * originalResults_->rho;
            results_.dividendRho = discR * originalResults_->dividendRho;

        
        }

    }

}

#endif
