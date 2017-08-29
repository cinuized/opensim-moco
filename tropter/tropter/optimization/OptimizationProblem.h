#ifndef TROPTER_OPTIMIZATIONPROBLEM_H
#define TROPTER_OPTIMIZATIONPROBLEM_H

#include <tropter/common.h>
#include "AbstractOptimizationProblem.h"
#include <memory>

namespace tropter {

class OptimizationProblemProxy {
    // TODO rename all method names to verbs
public:
    OptimizationProblemProxy(const AbstractOptimizationProblem& problem) :
            m_problem(problem) {}
    unsigned num_variables() const
    {   return m_problem.get_num_variables(); }
    unsigned num_constraints() const
    {   return m_problem.get_num_constraints(); }
    const Eigen::VectorXd& variable_lower_bounds() const
    {   return m_problem.get_variable_lower_bounds(); }
    const Eigen::VectorXd& variable_upper_bounds() const
    {   return m_problem.get_variable_upper_bounds(); }
    const Eigen::VectorXd& constraint_lower_bounds() const
    {   return m_problem.get_constraint_lower_bounds(); }
    const Eigen::VectorXd& constraint_upper_bounds() const
    {   return m_problem.get_constraint_upper_bounds(); }
    /// Create an initial guess for this problem according to the
    /// following rules:
    ///   - unconstrained variable: 0.
    ///   - lower and upper bounds: midpoint of the bounds.
    ///   - only one bound: value of the bound.
    Eigen::VectorXd initial_guess_from_bounds() const;
    // TODO b/c of SNOPT, want to be able to ask for sparsity separately.
    // You must call this function first before calling objective(),
    // constraints(), etc.
    virtual void sparsity(const Eigen::VectorXd& variables,
            std::vector<unsigned int>& jacobian_row_indices,
            std::vector<unsigned int>& jacobian_col_indices,
            std::vector<unsigned int>& hessian_row_indices,
            std::vector<unsigned int>& hessian_col_indices) const = 0;
    // TODO provide alternatives that take raw buffers. This is the interface
    // for optimizers, after all...
    virtual void objective(unsigned num_variables, const double* variables,
            bool new_variables,
            double& obj_value) const = 0;
    virtual void constraints(unsigned num_variables, const double* variables,
            bool new_variables,
            unsigned num_constraints, double* constr) const = 0;
    virtual void gradient(unsigned num_variables, const double* variables,
            bool new_variables,
            double* grad) const = 0;
    virtual void jacobian(unsigned num_variables, const double* variables,
            bool new_variables,
            unsigned num_nonzeros, double* nonzeros) const = 0;
    // TODO this signature seems to be tailoring to Ipopt too much.
    virtual void hessian_lagrangian(
            unsigned num_variables, const double* variables,
            bool new_variables, double obj_factor,
            unsigned num_constraints, const double* lambda,
            bool new_lambda,
            unsigned num_nonzeros, double* nonzeros) const = 0;
    // TODO consider providing alternative signatures like:
    // void objective(const Eigen::VectorXd& variables,
    //         bool new_variables = true,
    //         double& obj_value) const = 0;
private:
    const AbstractOptimizationProblem& m_problem;
};

template<typename T>
class OptimizationProblem : public AbstractOptimizationProblem {
public:
    class Proxy;

    virtual ~OptimizationProblem() = default;

    OptimizationProblem() = default;

    OptimizationProblem(unsigned num_variables, unsigned num_constraints) :
            AbstractOptimizationProblem(num_variables, num_constraints) {}

    std::shared_ptr<OptimizationProblemProxy> make_proxy() const override final;

    // TODO can override to provide custom derivatives.
    //virtual void gradient(const std::vector<T>& x, std::vector<T>& grad) const;
    //virtual void jacobian(const std::vector<T>& x, TODO) const;
    //virtual void hessian() const;

    // TODO it might be weird for users to use/implement a method called "_impl"
    virtual void objective(const VectorX<T>& x, T& obj_value) const;

    virtual void constraints(const VectorX<T>& x,
            Eigen::Ref<VectorX<T>> constr) const;

    // TODO user may want to define gradient, jacobian, hessian on their own...

//    void objective(const Eigen::VectorXd& variables, double& obj_value) const;
//
//    void constraints(const Eigen::VectorXd& variables,
//            Eigen::Ref<Eigen::VectorXd> constr) const;
//    void objective(const VectorX<T>& x, T& obj_value) const;
//
//    void constraints(const VectorX<T>& x,
//            Eigen::Ref<VectorX<T>> constr) const;

    // TODO for both hessian and jacobian.
    // TODO what about multiple points used to determine sparsity?
    // TODO this would move to a proxy class.
//    void determine_sparsity(const Eigen::VectorXd& variables,
//            std::vector<unsigned int>& jacobian_row_indices,
//            std::vector<unsigned int>& jacobian_col_indices,
//            std::vector<unsigned int>& hessian_row_indices,
//            std::vector<unsigned int>& hessian_col_indices) const;

private:

    // TODO this feels too Ipopt-specific..obj_factor?
	// TODO can delete this now; the TODO above is outdated.
    // void lagrangian(double obj_factor, const VectorX<T>& x,
    //         const Eigen::VectorXd& lambda, T& result) const;

};

template<typename T>
std::shared_ptr<OptimizationProblemProxy> OptimizationProblem<T>::make_proxy()
const
{
    // TODO is this what we want? a shared_ptr??
    //auto* proxy = new Proxy(*this);
    //return std::shared_ptr<OptimizationProblemProxy>
    //        (proxy);
    return std::make_shared<Proxy>(*this);
}

// TODO rename back to "objective()"
template<typename T>
void OptimizationProblem<T>::objective(const VectorX<T>&, T&) const
{
    // TODO proper error messages.
    throw std::runtime_error("Not implemented.");
}

template<typename T>
void OptimizationProblem<T>::constraints(const VectorX<T>&,
        Eigen::Ref<VectorX<T>>) const
{
// TODO throw std::runtime_error("Not implemented.");
}


// TODO The non-specialized version is empty!
template<typename T>
class OptimizationProblem<T>::Proxy : public OptimizationProblemProxy {
};

template<>
class OptimizationProblem<adouble>::Proxy : public OptimizationProblemProxy {
public:
    Proxy(const OptimizationProblem<adouble>& problem);
    /// Delete memory allocated by ADOL-C.
    virtual ~Proxy();
    void sparsity(const Eigen::VectorXd& variables,
            std::vector<unsigned int>& jacobian_row_indices,
            std::vector<unsigned int>& jacobian_col_indices,
            std::vector<unsigned int>& hessian_row_indices,
            std::vector<unsigned int>& hessian_col_indices) const override;
    void objective(unsigned num_variables, const double* variables,
            bool new_variables,
            double& obj_value) const override;
    void constraints(unsigned num_variables, const double* variables,
            bool new_variables,
            unsigned num_constraints, double* constr) const override;
    void gradient(unsigned num_variables, const double* variables,
            bool new_variables,
            double* grad) const override;
    void jacobian(unsigned num_variables, const double* variables,
            bool new_variables,
            unsigned num_nonzeros, double* nonzeros) const override;
    void hessian_lagrangian(unsigned num_variables, const double* variables,
            bool new_variables, double obj_factor,
            unsigned num_constraints, const double* lambda,
            bool new_lambda,
            unsigned num_nonzeros, double* nonzeros) const override;
private:
    void trace_objective(short int tag,
            unsigned num_variables, const double* variables,
            double& obj_value) const;
    void trace_constraints(short int tag,
            unsigned num_variables, const double* variables,
            unsigned num_constraints, double* constr) const;
    void trace_lagrangian(short int tag,
            unsigned num_variables, const double* variables,
            const double& obj_factor,
            unsigned num_constraints, const double* lambda,
            double& lagrangian_value) const;

    const OptimizationProblem<adouble>& m_problem;
    // TODO if we want to be able to solve multiple problems at once, these
    // cannot be static. We could create a registry of tags, and the tags can
    // be "checked out" and "returned."
    static const short int m_objective_tag   = 1;
    static const short int m_constraints_tag = 2;
    static const short int m_lagrangian_tag  = 3;

    // We must hold onto the sparsity pattern for the Jacobian and
    // Hessian so that we can pass them to subsequent calls to sparse_jac().
    // ADOL-C allocates this memory, but we must delete it.
    mutable int m_jacobian_num_nonzeros = -1;
    mutable unsigned int* m_jacobian_row_indices = nullptr;
    mutable unsigned int* m_jacobian_col_indices = nullptr;
    std::vector<int> m_sparse_jac_options;

    mutable int m_hessian_num_nonzeros = -1;
    mutable unsigned int* m_hessian_row_indices = nullptr;
    mutable unsigned int* m_hessian_col_indices = nullptr;
    // Working memory for lambda multipliers and the "obj_factor."
    mutable std::vector<double> m_hessian_obj_factor_lambda;
    std::vector<int> m_sparse_hess_options;
};

} // namespace tropter

#endif // TROPTER_OPTIMIZATIONPROBLEM_H