/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoJointReactionGoal.h                                      *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2019 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Nicholas Bianco                                                 *
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

#include "MocoJointReactionGoal.h"

#include "../MocoUtilities.h"

#include <OpenSim/Simulation/Model/Model.h>

using namespace OpenSim;

MocoJointReactionGoal::MocoJointReactionGoal() { constructProperties(); }

void MocoJointReactionGoal::constructProperties() {
    constructProperty_joint_path("");
    constructProperty_loads_frame("parent");
    constructProperty_expressed_in_frame_path("");
    constructProperty_reaction_measures();
    constructProperty_reaction_weights(MocoWeightSet());
}

void MocoJointReactionGoal::initializeOnModelImpl(const Model& model) const {

    // Cache the joint.
    OPENSIM_THROW_IF_FRMOBJ(get_joint_path().empty(), Exception,
            "Expected a joint path, but property joint_path is empty.");
    m_joint = &model.getComponent<Joint>(get_joint_path());

    m_denominator = model.getTotalMass(model.getWorkingState());
    const double gravityAccelMagnitude = model.get_gravity().norm();
    if (gravityAccelMagnitude > SimTK::SignificantReal) {
        m_denominator *= gravityAccelMagnitude;
    }

    // Get the frame from which the loads are computed.
    checkPropertyInSet(*this, getProperty_loads_frame(), {"parent", "child"});
    if (get_loads_frame() == "parent") {
        m_isParentFrame = true;
    } else if (get_loads_frame() == "child") {
        m_isParentFrame = false;
    }

    // Get the expressed-in frame.
    if (get_expressed_in_frame_path().empty()) {
        if (m_isParentFrame) {
            m_frame = &m_joint->getParentFrame();
        } else {
            m_frame = &m_joint->getChildFrame();
        }
    } else {
        m_frame = &model.getComponent<Frame>(get_expressed_in_frame_path());
    }

    // If user provided no reaction measure names, then set all measures to
    // to be minimized. Otherwise, loop through user-provided measure names
    // and check that they are all accepted measures.
    std::vector<std::string> reactionMeasures;
    std::vector<std::string> allowedMeasures = {"moment-x", "moment-y",
            "moment-z", "force-x", "force-y", "force-z"};
    if (getProperty_reaction_measures().empty()) {
        reactionMeasures = allowedMeasures;
    } else {
        for (int i = 0; i < getProperty_reaction_measures().size(); ++i) {
            if (std::find(allowedMeasures.begin(), allowedMeasures.end(),
                        get_reaction_measures(i)) == allowedMeasures.end()) {

                OPENSIM_THROW_FRMOBJ(Exception,
                        format("Reaction measure '%s' not recognized.",
                                get_reaction_measures(i)));
            }
            reactionMeasures.push_back(get_reaction_measures(i));
        }
    }

    // Loop through all reaction measures to minimize and get the
    // corresponding SpatialVec indices and weights.
    for (const auto& measure : reactionMeasures) {
        if (measure == "moment-x") {
            m_measureIndices.push_back({0, 0});
        } else if (measure == "moment-y") {
            m_measureIndices.push_back({0, 1});
        } else if (measure == "moment-z") {
            m_measureIndices.push_back({0, 2});
        } else if (measure == "force-x") {
            m_measureIndices.push_back({1, 0});
        } else if (measure == "force-y") {
            m_measureIndices.push_back({1, 1});
        } else if (measure == "force-z") {
            m_measureIndices.push_back({1, 2});
        }

        double compWeight = 1.0;
        if (get_reaction_weights().contains(measure)) {
            compWeight = get_reaction_weights().get(measure).getWeight();
        }
        m_measureWeights.push_back(compWeight);
    }

    setNumIntegralsAndOutputs(1, 1);
}

void MocoJointReactionGoal::calcIntegrandImpl(
        const SimTK::State& state, double& integrand) const {

    getModel().realizeAcceleration(state);
    const auto& ground = getModel().getGround();

    // Compute the reaction loads on the parent or child frame.
    SimTK::SpatialVec reactionInGround;
    if (m_isParentFrame) {
        reactionInGround =
                m_joint->calcReactionOnParentExpressedInGround(state);
    } else {
        reactionInGround = m_joint->calcReactionOnChildExpressedInGround(state);
    }

    // Re-express the reactions into the proper frame and repackage into a new
    // SpatialVec.
    SimTK::Vec3 moment;
    SimTK::Vec3 force;
    if (m_frame.get() == &getModel().getGround()) {
        moment = reactionInGround[0];
        force = reactionInGround[1];
    } else {
        moment = ground.expressVectorInAnotherFrame(
                state, reactionInGround[0], *m_frame);
        force = ground.expressVectorInAnotherFrame(
                state, reactionInGround[1], *m_frame);
    }
    SimTK::SpatialVec reaction(moment, force);

    // Compute cost.
    integrand = 0;
    for (int i = 0; i < (int)m_measureIndices.size(); ++i) {
        const auto index = m_measureIndices[i];
        const double weight = m_measureWeights[i];
        integrand += weight * pow(reaction[index.first][index.second], 2);
    }
}

void MocoJointReactionGoal::printDescriptionImpl(std::ostream& stream) const {
    stream << "        ";
    stream << "joint path: " << get_joint_path() << std::endl;
    stream << "        ";
    stream << "loads frame: " << get_loads_frame() << std::endl;
    stream << "        ";
    stream << "expressed: " << get_expressed_in_frame_path() << std::endl;
    stream << "        ";
    stream << "reaction measures: ";
    for (int i = 0; i < getProperty_reaction_measures().size(); i++) {
        stream << get_reaction_measures(i);
        if (i < getProperty_reaction_measures().size() - 1) { stream << ", "; }
    }
    stream << std::endl;
    stream << "        ";
    stream << "reaction weights: ";
    for (int i = 0; i < (int)m_measureWeights.size(); ++i) {
        stream << m_measureWeights[i];
        if (i < (int)m_measureWeights.size() - 1) { stream << ", "; }
    }
    stream << std::endl;
}
