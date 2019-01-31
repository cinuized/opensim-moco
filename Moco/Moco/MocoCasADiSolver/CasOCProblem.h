#ifndef MOCO_CASOCPROBLEM_H
#define MOCO_CASOCPROBLEM_H
/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoCasOCProblem.h                                           *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2018 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Christopher Dembia                                              *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0          *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

#include "CasOCFunction.h"
#include <casadi/casadi.hpp>
#include <string>
#include <unordered_map>

#include "../MocoUtilities.h"

/// CasOC is a namespace containing classes for solving multibody optimal
/// control problems with CasADi. CasOC is not designed to solve generic optimal
/// control problems. For example, CasOC does not require the user to provide a
/// system of first-order differential equations.
///
/// CasOC does not conceptually depend on OpenSim or Moco, though CasOC may use
/// OpenSim/Moco utilities (e.g., exception handling).

/// CasADi Optimal Control.
namespace CasOC {

enum Var {
    initial_time,
    final_time,
    states,
    controls,
    multipliers,
    derivatives,
    parameters
};

template <typename T>
using Variables = std::unordered_map<Var, T, std::hash<int>>;

using VariablesDM = Variables<casadi::DM>;
using VariablesMX = Variables<casadi::MX>;

struct Iterate {
    VariablesDM variables;
    casadi::DM times;
    std::vector<std::string> state_names;
    std::vector<std::string> control_names;
    std::vector<std::string> multiplier_names;
    std::vector<std::string> derivative_names;
    std::vector<std::string> parameter_names;
    Iterate resample(const casadi::DM& newTimes) const;
};

struct Solution : public Iterate {
    casadi::Dict stats;
};

struct Bounds {
    Bounds() = default;
    Bounds(double lower, double upper) : lower(lower), upper(upper) {}
    double lower = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
    bool isSet() const { return !std::isnan(lower) && !std::isnan(upper); }
};
enum class StateType { Coordinate, Speed, Auxiliary };
struct StateInfo {
    std::string name;
    StateType type;
    Bounds bounds;
    Bounds initialBounds;
    Bounds finalBounds;
};
struct ControlInfo {
    std::string name;
    Bounds bounds;
    Bounds initialBounds;
    Bounds finalBounds;
};

class Solver;
class TrapezoidalSolver;

class Problem {
public:
    /// @name Interface for problem builder
    /// @{
    void setTimeBounds(Bounds initial, Bounds final) {
        m_timeInitialBounds = std::move(initial);
        m_timeFinalBounds = std::move(final);
    }
    /// The state variables must be added in the order Q, U, Z.
    // TODO create separate addDegreeOfFreedom() and addAuxiliaryState()?
    void addState(std::string name, StateType type, Bounds bounds,
            Bounds initialBounds, Bounds finalBounds) {
        m_stateInfos.push_back({std::move(name), type, std::move(bounds),
                std::move(initialBounds), std::move(finalBounds)});
    }
    void addControl(std::string name, Bounds bounds, Bounds initialBounds,
            Bounds finalBounds) {
        m_controlInfos.push_back({std::move(name), std::move(bounds),
                std::move(initialBounds), std::move(finalBounds)});
    }
    template <typename FunctionType, typename... Args>
    void setIntegralCost(Args&&... args) {
        m_integralCostFunc =
                OpenSim::make_unique<FunctionType>(std::forward<Args>(args)...);
        m_integralCostFunc->constructFunction(this, "integral_cost_integrand");
    }
    template <typename FunctionType, typename... Args>
    void setEndpointCost(Args&&... args) {
        m_endpointCostFunc =
                OpenSim::make_unique<FunctionType>(std::forward<Args>(args)...);
        m_endpointCostFunc->constructFunction(this, "endpoint_cost");
    }
    template <typename FunctionType, typename... Args>
    void setMultibodySystem(Args&&... args) {
        m_multibodyFunc =
                OpenSim::make_unique<FunctionType>(std::forward<Args>(args)...);
        m_multibodyFunc->constructFunction(this, "multibody_system");
    }
    template <typename FunctionType, typename... Args>
    void setPathConstraints(Args&&... args) {
        m_pathFunc = std::unique_ptr<FunctionType>(std::forward<Args>(args)...);
        m_pathFunc->constructFunction(this, "path_constraints");
    }

    template <typename IterateType = Iterate>
    IterateType createIterate() const {
        IterateType it;
        for (const auto& info : m_stateInfos)
            it.state_names.push_back(info.name);
        for (const auto& info : m_controlInfos)
            it.control_names.push_back(info.name);
        return it;
    }

    void initialize() {
        for (const auto& info : m_stateInfos) {
            if (info.type == StateType::Coordinate)
                ++m_numCoordinates;
            else if (info.type == StateType::Speed)
                ++m_numSpeeds;
            else if (info.type == StateType::Auxiliary)
                ++m_numAuxiliaryStates;
        }
    }

    /// @}

    /// @name Interface for CasOC functions
    /// @{
    // TODO: Skip over empty slots for quaternions.
    int getNumStates() const { return (int)m_stateInfos.size(); }
    int getNumControls() const { return (int)m_controlInfos.size(); }
    int getNumParameters() const { return 0; /* TODO */ }
    int getNumMultipliers() const { return 0; /* TODO */ }
    /// This is the number of generalized coordinates, which may be greater
    /// than the number of generalized speeds.
    int getNumCoordinates() const { return m_numCoordinates; }
    int getNumSpeeds() const { return m_numSpeeds; }
    int getNumAuxiliaryStates() const { return m_numAuxiliaryStates; }
    int getNumKinematicConstraintEquations() const { return 0; /* TODO */ }
    const Bounds& getTimeInitialBounds() const { return m_timeInitialBounds; }
    const Bounds& getTimeFinalBounds() const { return m_timeFinalBounds; }
    const std::vector<StateInfo>& getStateInfos() const { return m_stateInfos; }
    const std::vector<ControlInfo>& getControlInfos() const {
        return m_controlInfos;
    }

    const casadi::Function& getIntegralCostIntegrand() const {
        return *m_integralCostFunc;
    }
    const casadi::Function& getEndpointCost() const {
        return *m_endpointCostFunc;
    }
    const casadi::Function& getMultibodySystem() const {
        return *m_multibodyFunc;
    }
    const casadi::Function& getPathConstraints() const { return *m_pathFunc; }
    /// @}

private:
    Bounds m_timeInitialBounds;
    Bounds m_timeFinalBounds;
    std::vector<StateInfo> m_stateInfos;
    int m_numCoordinates = 0;
    int m_numSpeeds = 0;
    int m_numAuxiliaryStates = 0;
    std::vector<ControlInfo> m_controlInfos;
    std::unique_ptr<IntegralCostIntegrand> m_integralCostFunc;
    std::unique_ptr<EndpointCost> m_endpointCostFunc;
    std::unique_ptr<MultibodySystem> m_multibodyFunc;
    std::unique_ptr<PathConstraints> m_pathFunc;
};

class Transcription;

class Solver {
public:
    Solver(const Problem& problem) : m_problem(problem) {}
    void setNumMeshPoints(int numMeshPoints) {
        m_numMeshPoints = numMeshPoints;
    }
    int getNumMeshPoints() const { return m_numMeshPoints; }

    void setTranscriptionScheme(std::string scheme) {
        m_transcriptionScheme = std::move(scheme);
    }
    const std::string& getTranscriptionScheme() const {
        return m_transcriptionScheme;
    }

    void setOptimSolver(std::string optimSolver) {
        m_optimSolver = std::move(optimSolver);
    }
    const std::string getOptimSolver() const { return m_optimSolver; }

    void setPluginOptions(casadi::Dict opts) {
        m_pluginOptions = std::move(opts);
    }
    const casadi::Dict& getPluginOptions() const { return m_pluginOptions; }

    void setSolverOptions(casadi::Dict solverOptions) {
        m_solverOptions = std::move(solverOptions);
    }
    const casadi::Dict getSolverOptions() const { return m_solverOptions; }


    Iterate createInitialGuessFromBounds() const;
    Iterate createRandomIterateWithinBounds() const;

    Solution solve(const Iterate& guess) const;

private:

    std::unique_ptr<Transcription> createTranscription() const;

    const Problem& m_problem;
    int m_numMeshPoints;
    std::string m_transcriptionScheme;
    casadi::Dict m_pluginOptions;
    casadi::Dict m_solverOptions;
    std::string m_optimSolver;
};

} // namespace CasOC

#endif // MOCO_CASOCPROBLEM_H