
#include "EuropaEngine.hh"
#include "ModuleConstraintEngine.hh"
#include "ModulePlanDatabase.hh"
#include "ModuleRulesEngine.hh"
#include "ModuleTemporalNetwork.hh"
#include "ModuleSolvers.hh"
#include "ModuleNddl.hh"
#ifndef NO_RESOURCES
#include "ModuleResource.hh"
#include "ModuleAnml.hh"
#endif

#include "ConstraintEngine.hh"
#include "PlanDatabase.hh"
#include "RulesEngine.hh"

// Solver Support
#include "Solver.hh"
#include "Context.hh"
#include "Filters.hh"
#include "PlanDatabaseWriter.hh"

#include <boost/cast.hpp>

namespace EUROPA {
//namespace System { //TODO: mcr

  //Logger  &EuropaEngine::LOGGER = Logger::getInstance( "EUROPA::System::EuropaEngine", Logger::DEBUG );
  LOGGER_CLASS_INSTANCE_IMPL( EuropaEngine, "EUROPA::System::EuropaEngine", DEBUG );

EuropaEngine::EuropaEngine() : m_totalNodes(0), m_finalDepth(0) {
  Error::doThrowExceptions(); // throw exceptions!
  Error::doDisplayErrors();
}

    EuropaEngine::~EuropaEngine()
    {
    }

    void EuropaEngine::initializeModules()
    {
        // Only do this once
        if (m_modules.size() == 0)
            createModules();
        EngineBase::initializeModules();
    }

    void EuropaEngine::createModules()
    {
        // TODO: make this data-driven
        addModule((new ModuleConstraintEngine())->getId());
        addModule((new ModuleConstraintLibrary())->getId());
        addModule((new ModulePlanDatabase())->getId());
        addModule((new ModuleRulesEngine())->getId());
        addModule((new ModuleTemporalNetwork())->getId());
        addModule((new ModuleSolvers())->getId());
        addModule((new ModuleNddl())->getId());
#ifndef NO_RESOURCES
        addModule((new ModuleResource())->getId());
        addModule((new ModuleAnml())->getId());
#endif
    }

ConstraintEngineId EuropaEngine::getConstraintEngine() const { return boost::polymorphic_cast<const ConstraintEngine*>(getComponent("ConstraintEngine"))->getId(); }
PlanDatabaseId     EuropaEngine::getPlanDatabase()     const { return boost::polymorphic_cast<const PlanDatabase*>(getComponent("PlanDatabase"))->getId(); }
RulesEngineId      EuropaEngine::getRulesEngine()      const { return boost::polymorphic_cast<const RulesEngine*>(getComponent("RulesEngine"))->getId(); }

const ConstraintEngine* EuropaEngine::getConstraintEnginePtr() const { return static_cast<const ConstraintEngine*>(getConstraintEngine());}
const PlanDatabase*     EuropaEngine::getPlanDatabasePtr()     const { return static_cast<const PlanDatabase*>(getPlanDatabase());}
const RulesEngine*      EuropaEngine::getRulesEnginePtr()      const { return static_cast<const RulesEngine*>(getComponent("RulesEngine")); }

    // TODO: remains of the old Assemblies, these are only used by test code, should be dropped, eventually.
    bool EuropaEngine::playTransactions(const char* txSource, const char* language)
    {
      check_error(txSource != NULL, "NULL transaction source provided.");

      static bool isFile = true;
      std::string result = executeScript(language,txSource,isFile);

      if (result.size()>0)
    	  std::cerr << "ERROR!:" << result << std::endl;

      return (result.size()==0 && getConstraintEnginePtr()->constraintConsistent());
    }

    void EuropaEngine::write(std::ostream& os) const
    {
      PlanDatabaseWriter::write(getPlanDatabasePtr()->getId(), os);
    }

    const char* EuropaEngine::TX_LOG() {
      static const char* sl_txLog = "TransactionLog.xml";
      return sl_txLog;
    }

    bool EuropaEngine::plan(const char* txSource, const char* plannerConfig, const char* language){
      TiXmlDocument doc(plannerConfig);
      doc.LoadFile();
      const TiXmlElement& config = *(doc.RootElement());

      SOLVERS::SolverId solver = (new SOLVERS::Solver(getPlanDatabase(), config))->getId();

      // Now process the transactions
      if(!playTransactions(txSource, language))
        return false;

      debugMsg("EuropaEngine:plan", "Initial state: " << std::endl << PlanDatabaseWriter::toString(getPlanDatabase()))
      //LOGGER << Logger::DEBUG << "plan: Initial state: " << Logger::eol << PlanDatabaseWriter::toString(getPlanDatabase());
      LOGGER_DEBUG_MSG( DEBUG, "Initial state: " << LOGGER_ENDL << PlanDatabaseWriter::toString(getPlanDatabase()) )

      // Configure the planner from data in the initial state
      std::list<ObjectId> configObjects;
      getPlanDatabase()->getObjectsByType("PlannerConfig", configObjects); // Standard configuration class

      std::ostringstream db; db << configObjects.size();
      check_error(configObjects.size() == 1,
		  "Expect exactly one instance of the class 'PlannerConfig', got: " + db.str());

      ObjectId configSource = configObjects.front();
      check_error(configSource.isValid());

      const std::vector<ConstrainedVariableId>& variables = configSource->getVariables();
      checkError(variables.size() == 4,
             "Expecting exactly 4 configuration variables.  Got " << variables.size());

      // Set up the horizon  from the model now. Will cause a refresh of the query, but that is OK.
      ConstrainedVariableId horizonStart = variables[0];
      ConstrainedVariableId horizonEnd = variables[1];
      ConstrainedVariableId plannerSteps = variables[2];
      ConstrainedVariableId maxDepth = variables[3];

      eint start = horizonStart->baseDomain().getSingletonValue();
      eint end = horizonEnd->baseDomain().getSingletonValue();
      solver->getContext()->put("horizonStart", cast_double(start));
      solver->getContext()->put("horizonEnd", cast_double(end));

      // Now get planner step max
      unsigned int steps =
          static_cast<unsigned int>(cast_int(plannerSteps->baseDomain().getSingletonValue()));
      unsigned int depth =
          static_cast<unsigned int>(cast_int(maxDepth->baseDomain().getSingletonValue()));

      bool retval = solver->solve(steps, depth);

      m_totalNodes = solver->getStepCount();
      m_finalDepth = solver->getDepth();

      delete static_cast<SOLVERS::Solver*>(solver);

      return retval;
    }

    unsigned long EuropaEngine::getTotalNodesSearched() const { return m_totalNodes; }

    unsigned long EuropaEngine::getDepthReached() const { return m_finalDepth; }
}

