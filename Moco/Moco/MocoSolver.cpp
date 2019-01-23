/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoSolver.cpp                                               *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2017 Stanford University and the Authors                     *
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
#include "MocoSolver.h"

#include "MocoProblem.h"

using namespace OpenSim;

void MocoSolver::resetProblem(const MocoProblem& problem) {
    m_problem.reset(&problem);
    m_problemRep = problem.createRep();
    resetProblemImpl(m_problemRep);
}

MocoSolution MocoSolver::solve() const {
    OPENSIM_THROW_IF(!m_problem, Exception, "Problem not set.");
    return solveImpl();
}

void MocoSolver::setSolutionStats(MocoSolution& sol,
        bool success, const std::string& status, int numIterations) {
    sol.setSuccess(success);
    sol.setStatus(status);
    sol.setNumIterations(numIterations);
}

