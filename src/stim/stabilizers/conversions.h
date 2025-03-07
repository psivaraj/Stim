/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _STIM_STABILIZERS_CONVERSIONS_H
#define _STIM_STABILIZERS_CONVERSIONS_H

#include "stim/circuit/circuit.h"
#include "stim/dem/dem_instruction.h"
#include "stim/stabilizers/flex_pauli_string.h"
#include "stim/stabilizers/tableau.h"

namespace stim {

inline uint8_t floor_lg2(size_t value) {
    uint8_t result = 0;
    while (value > 1) {
        result += 1;
        value >>= 1;
    }
    return result;
}

inline uint8_t is_power_of_2(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/// Converts a tableau into a unitary matrix.
template <size_t W>
std::vector<std::vector<std::complex<float>>> tableau_to_unitary(const Tableau<W> &tableau, bool little_endian);

/// Inverts the given circuit, as long as it only contains unitary operations.
inline Circuit unitary_circuit_inverse(const Circuit &unitary_circuit) {
    Circuit inverted;
    unitary_circuit.for_each_operation_reverse([&](const CircuitInstruction &op) {
        const auto &gate_data = GATE_DATA[op.gate_type];
        if (!(gate_data.flags & GATE_IS_UNITARY)) {
            throw std::invalid_argument("Not unitary: " + op.str());
        }
        size_t step = (gate_data.flags & GATE_TARGETS_PAIRS) ? 2 : 1;
        auto s = op.targets.ptr_start;
        const auto &inv_gate = gate_data.inverse();
        for (size_t k = op.targets.size(); k > 0; k -= step) {
            inverted.safe_append(inv_gate.id, {s + k - step, s + k}, op.args);
        }
    });
    return inverted;
}

/// Synthesizes a circuit to generate the given state vector.
///
/// Args:
///     stabilizer_state_vector: The vector of amplitudes to produce using a circuit.
///     little_endian: Whether the vector is using little endian or big endian ordering.
///     inverted_circuit: If false, returns a circuit that sends |000...0> to the state vector.
///         If true, returns a circuit that sends the state vector to |000...0> instead of a cir.
///
/// Returns:
///     A circuit that outputs the given state vector (up to global phase).
///
/// Throws:
///     std::invalid_argument: The given state vector cannot be produced by a stabilizer circuit.
template <size_t W>
Circuit stabilizer_state_vector_to_circuit(
    const std::vector<std::complex<float>> &stabilizer_state_vector, bool little_endian);

/// Compiles the given circuit into a tableau.
///
/// Args:
///     circuit: The circuit to compile. Should only contain unitary operations.
///     ignore_noise: If the circuit contains noise channels, ignore them instead of raising an exception.
///     ignore_measurement: If the circuit contains measurements, ignore them instead of raising an exception.
///     ignore_reset: If the circuit contains resets, ignore them instead of raising an exception.
///
/// Returns:
///     A tableau encoding the given circuit's Clifford operation.
template <size_t W>
Tableau<W> circuit_to_tableau(const Circuit &circuit, bool ignore_noise, bool ignore_measurement, bool ignore_reset);

/// Simulates the given circuit and outputs a state vector.
///
/// Args:
///     circuit: The circuit to simulate. Cannot contain noisy or dissipative operations.
///     little_endian: Whether the returned vector uses little endian or big endian qubit order.
///
/// Returns:
///     The state vector, using the requested endianness.
template <size_t W>
std::vector<std::complex<float>> circuit_to_output_state_vector(const Circuit &circuit, bool little_endian);

/// Synthesizes a circuit that implements the given tableau's Clifford operation.
///
/// This method is allowed to output different circuits, from call to call or version
/// to version, for the same input tableau.
///
/// Args:
///     tableau: The tableau to synthesize into a circuit.
///     method: The method to use when synthesizing the circuit. Available values:
///         "elimination": Cancels off-diagonal terms using Gaussian elimination.
///             Gate set: H, S, CX
///             Circuit qubit count: n
///             Circuit operation count: O(n^2)
///             Circuit depth: O(n^2)
///         "graph_state": Prepares the tableau's state using a graph state circuit.
///             Gate set: RX, CZ, H, S, X, Y, Z
///             Circuit qubit count: n
///             Circuit operation count: O(n^2)
///
///             The circuit will be made up of three layers:
///                 1. An RX layer initializing all qubits.
///                 2. A CZ layer coupling the qubits.
///                     an edge in the graph state.)
///                 3. A single qubit rotation layer.
///
///             Note: "graph_state" treats the tableau as a state instead of as a
///             Clifford operation. It will preserve the set of stabilizers, but
///             not the exact choice of generators.
///
/// Returns:
///     The synthesized circuit.
template <size_t W>
Circuit tableau_to_circuit(const Tableau<W> &tableau, const std::string &method);
template <size_t W>
Circuit tableau_to_circuit_graph_method(const Tableau<W> &tableau);
template <size_t W>
Circuit tableau_to_circuit_mpp_method(const Tableau<W> &tableau, bool skip_sign);
template <size_t W>
Circuit tableau_to_circuit_elimination_method(const Tableau<W> &tableau);

/// Converts a unitary matrix into a stabilizer tableau.
///
/// Args:
///     matrix: The unitary matrix to convert. Must correspond to a Clifford.
//      little_endian: Whether the amplitude ordering is little endian or big endian.
///
/// Returns:
///     A tableau implementing the same operation as the unitary matrix (up to global phase).
///
/// Throws:
///     std::invalid_argument: The given unitary matrix isn't a Clifford operation.
template <size_t W>
Tableau<W> unitary_to_tableau(const std::vector<std::vector<std::complex<float>>> &matrix, bool little_endian);

/// Computes destabilizers for the given stabilizers, and packages into a tableau.
///
/// Args:
///     stabilizers: The desired stabilizers for the tableau. Every stabilizer must have the same number of qubits.
///     allow_redundant: If false, including a redundant stabilizer will result in an error.
///         If true, redundant stabilizers are quietly dropped.
///     allow_underconstrained: If false, the number of independent stabilizers must equal the number of qubits in each
///         stabilizer. If true, the returned result will arbitrarily fill in missing stabilizers.
///     invert: Return the inverse tableau instead of the tableau with the stabilizers as its Z outputs.
///
/// Returns:
///     A tableau containing the given stabilizers, but extended to also include matching stabilizers.
///     The Z outputs of the tableau will be the given stabilizers (skipping any redundant ones).
template <size_t W>
Tableau<W> stabilizers_to_tableau(
    const std::vector<stim::PauliString<W>> &stabilizers,
    bool allow_redundant,
    bool allow_underconstrained,
    bool invert);

void independent_to_disjoint_xyz_errors(double x, double y, double z, double *out_x, double *out_y, double *out_z);
bool try_disjoint_to_independent_xyz_errors_approx(
    double x, double y, double z, double *out_x, double *out_y, double *out_z, size_t max_steps = 50);
double depolarize1_probability_to_independent_per_channel_probability(double p);
double depolarize2_probability_to_independent_per_channel_probability(double p);
double independent_per_channel_probability_to_depolarize1_probability(double p);
double independent_per_channel_probability_to_depolarize2_probability(double p);

std::map<DemTarget, std::map<uint64_t, FlexPauliString>> circuit_to_detecting_regions(
    const Circuit &circuit,
    std::set<stim::DemTarget> included_targets,
    std::set<uint64_t> included_ticks,
    bool ignore_anticommutation_errors);

}  // namespace stim

#include "stim/stabilizers/conversions.inl"

#endif
