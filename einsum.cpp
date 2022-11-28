// Include the headers
#include "builder/dyn_var.h"
#include "builder/static_var.h"
#include "blocks/extract_cuda.h"
#include "blocks/c_code_generator.h"
#include "blocks/rce.h"
#include <vector>

#define CTA_SIZE (512)
// Complete implementation of the Einsum Lang compiler that
// generates CPU and GPU code for expressions like A[i] = B[i][j]...
// Expand to read and change the implementation. 
namespace el {

// Implementation of the index type
struct index {
	// While in use, the iterator to use
	builder::dyn_var<int> m_iterator = 0;
	int m_index_bound = 0;	
};

// Type to hold expressions likes T[x]..
template<typename T>
struct tensor_access;

// Implementation for the index type
template <typename T>
struct tensor {
	int m_dims;

	// Statically known tensor sizes
	std::vector<int> m_sizes;

	// Static tracking for constant values
	builder::static_var<int> is_constant = false;
	builder::static_var<T> constant_val = 0;
	
	// Underlying data buffer
	builder::dyn_var<T*> m_buffer;

	tensor(const std::vector<int>& sizes, const builder::dyn_var<T*>& buffer):
		m_dims(sizes.size()), m_sizes(std::move(sizes)), m_buffer(buffer) {
	}

	// Operator to create a tensor_access from tensor
	tensor_access<T> operator [] (index &i);
	
};

// Base class for tensor access of any type
struct tensor_access_base {
	virtual builder::builder get_value() const { return 0;}
	virtual ~tensor_access_base() = default;
	virtual std::vector<index*> get_indices() const {return std::vector<index*>(); }
};

template <typename T>
struct tensor_access;

struct rhs_terms {
	enum term_type {
		tensor_access,
		product,
		sum,
		constant,
		index_access, 
	};

	enum term_type m_type;	
	const tensor_access_base* m_tab;
	const rhs_terms* m_term1;
	const rhs_terms* m_term2;
	std::shared_ptr<builder::dyn_var<int>> m_constant;
    const index* m_index;
    
	rhs_terms() {}
	
	rhs_terms(const index &i) {
	    m_index = &i;
	    m_type = index_access;
	}
	
	template <typename T>
	rhs_terms(const struct tensor_access<T>& t);

	rhs_terms(const int &x): rhs_terms(builder::dyn_var<int> (x)) {}
	
	template <typename T>	
	rhs_terms(const builder::dyn_var<T> &b) {
		m_type = constant;
		m_constant = std::make_shared<builder::dyn_var<int>>(b);
	}

	builder::builder get_value(void) const {
		switch(m_type) {
			case tensor_access: return m_tab->get_value();
			case product: return m_term1->get_value() * m_term2->get_value();
			case sum: return m_term1->get_value() + m_term2->get_value();
			case constant: return *m_constant;
			case index_access: return m_index->m_iterator;
		}
		return 0;
	}
	void get_indices(std::vector<index*> &set) const {
		if (m_type == product || m_type == sum) {
			m_term1->get_indices(set);
			m_term2->get_indices(set);
			return;
		}		
		if (m_type == tensor_access) {
			auto indices = m_tab->get_indices();
			for (auto a: indices) {
				if (std::find(set.begin(), set.end(), a) == set.end()) {
					set.push_back(a);
				}	
			}	
			return;
		}
		return;
	}
};

// Enums for tracking current device // Scheduling
enum device_type {
	SERIAL = 0,
	CPU_PARALLEL = 1,
	GPU_PARALLEL = 2
};
enum device_type current_device = SERIAL;


static std::vector<index*> get_reduce_indices(std::vector<index*> lhs_set, const rhs_terms& rhs) {
	// Next we will gather indices that are used on the right, but not on the left
	std::vector<index*> rhs_set;
	std::vector<index*> all_rhs_set;
	rhs.get_indices(all_rhs_set);
	for (auto x: all_rhs_set) {
		if (std::find(lhs_set.begin(), lhs_set.end(), x) == lhs_set.end()) {
			if (std::find(rhs_set.begin(), rhs_set.end(), x) == rhs_set.end()) {
				rhs_set.push_back(x);
			}
		}
	}
	return rhs_set;
}

// Tensor access implementation
template <typename T>
struct tensor_access: public tensor_access_base {
	tensor<T>& m_tensor;
	std::vector<index*> m_indices;

	tensor_access(tensor<T>& _t): m_tensor(_t) {}

	// Operator for multiple indices chaining
	tensor_access<T> operator [] (index &i);	


	void create_increment(const rhs_terms &rhs, std::vector<index*> reduce_indices, builder::dyn_var<int>& buffer_index) {
		if (reduce_indices.size())
			m_tensor.m_buffer[buffer_index] = m_tensor.m_buffer[buffer_index] + rhs.get_value();
		else 
			m_tensor.m_buffer[buffer_index] = rhs.get_value();
	}	

	builder::dyn_var<int> create_index(int idx) {
		if (idx == 0)
			return (m_indices[0]->m_iterator);
		return create_index(idx - 1) * (int) (m_tensor.m_sizes[idx]) + (m_indices[idx]->m_iterator);
	}

	void create_assign(const rhs_terms &rhs, std::vector<index*> reduce_indices) {
		builder::dyn_var<int> v = create_index(m_tensor.m_dims-1);
		if (reduce_indices.size())
			m_tensor.m_buffer[v] = 0;
		induce_reduce_loop(0, rhs, reduce_indices, v);	
	}

	
	// Functions for create loops on the RHS
	void induce_reduce_loop(int idx, const rhs_terms &rhs, std::vector<index*> reduce_indices, 
		builder::dyn_var<int>& buffer_index) {
		if (idx == (int)reduce_indices.size()) {
			create_increment(rhs, reduce_indices, buffer_index);
			return;
		}
		// Now add a new loop for a reduce index	
		builder::dyn_var<int> &iter = reduce_indices[idx]->m_iterator;
		for (iter = 0; iter < reduce_indices[idx]->m_index_bound; iter = iter + 1) {
			induce_reduce_loop(idx + 1, rhs, reduce_indices, buffer_index);
		}
	}	
	
	// Functions to create loops on the LHS
	void induce_loops(int idx, const rhs_terms& rhs, std::vector<index*> reduce_indices) {
		if (idx == m_tensor.m_dims) {
			create_assign(rhs, reduce_indices);
			return;
		} 	
		if (idx == 0 && current_device == GPU_PARALLEL) {
			int num_cta = (m_tensor.m_sizes[idx] + CTA_SIZE - 1) / CTA_SIZE;
			builder::annotate(CUDA_KERNEL);
			for (builder::dyn_var<int> cta = 0; cta < num_cta; cta = cta + 1) {
				for (builder::dyn_var<int> tid = 0; tid < CTA_SIZE; tid = tid + 1) {
					builder::dyn_var<int> thread = cta * CTA_SIZE + tid;
					if ((m_tensor.m_sizes[idx] % CTA_SIZE == 0) || (bool)(thread < m_tensor.m_sizes[idx])) {
						m_indices[idx]->m_iterator = thread;
						induce_loops(idx + 1, rhs, reduce_indices);	
					}
				}
			}
		} else {
			// Implement a level of loop and recurse	
			if (idx == 0 && current_device == CPU_PARALLEL) {
				builder::annotate("pragma: omp parallel for");
			}
			builder::dyn_var<int> &iter = m_indices[idx]->m_iterator;
			for (iter = 0; iter < m_tensor.m_sizes[idx]; iter = iter + 1) {
				induce_loops(idx + 1, rhs, reduce_indices);	
			}
		}
	}
		
	// Operator over load for = 
	void operator= (const rhs_terms &rhs) {
		// First we will assert that we have all the indices we need 
		assert(m_indices.size() == (size_t)(m_tensor.m_dims) && "Not enough indices supplied for definition");
		std::vector<index*> reduce_indices = get_reduce_indices(m_indices, rhs);	
		induce_loops(0, rhs, reduce_indices);	
		m_tensor.is_constant = false;
		m_tensor.constant_val = 0;
	}

	template<typename T2>
	void operator = (const tensor_access<T2> &a) {
		*this = std::move((rhs_terms)a);
	}
	void operator = (const tensor_access<T> &a) {
		*this = std::move((rhs_terms)a);
	}
	void operator = (const T& x) {
		*this = ((rhs_terms)(builder::dyn_var<T>)x);
		m_tensor.is_constant = true;
		m_tensor.constant_val = x;
	}	

	builder::dyn_var<int> create_index(int idx) const {
		if (idx == 0)
			return (m_indices[0]->m_iterator);
		return create_index(idx - 1) * (int) (m_tensor.m_sizes[idx]) + (m_indices[idx]->m_iterator);
	}
	builder::builder get_value() const override {	
		if (m_tensor.is_constant) 
			return m_tensor.constant_val;
		builder::dyn_var<int> v = create_index(m_tensor.m_dims - 1);	
		return m_tensor.m_buffer[v];
	}
	std::vector<index*> get_indices() const override {
		for (unsigned int i = 0; i < m_indices.size(); i++) {
			m_indices[i]->m_index_bound = m_tensor.m_sizes[i];
		}
		return m_indices;
	}
};


template <typename T>
rhs_terms::rhs_terms(const struct tensor_access<T>& t) {
	m_type = tensor_access;
	m_tab = &t;
}	

// Operator overload for tensor types
template <typename T>
tensor_access<T> tensor<T>::operator [] (index &i) {
	tensor_access<T> t (*this);
	t.m_indices.push_back(&i);	
	return t;
}

template <typename T>
tensor_access<T> tensor_access<T>::operator [] (index &i) {
	tensor_access<T> t(m_tensor);
	// We can't use this tensor access anymore after this
	t.m_indices = std::move(m_indices);
	t.m_indices.push_back(&i);
	return t;
}

// Operator overloads for RHS expressions
rhs_terms operator* (const rhs_terms &a, const rhs_terms &b);
rhs_terms operator* (const rhs_terms &a, const rhs_terms &b) {
	rhs_terms new_terms;
	new_terms.m_type = rhs_terms::product;
	new_terms.m_term1 = &a;
	new_terms.m_term2 = &b;
	
	return new_terms;
}

rhs_terms operator+ (const rhs_terms &a, const rhs_terms &b);
rhs_terms operator+ (const rhs_terms &a, const rhs_terms &b) {
	rhs_terms new_terms;
	new_terms.m_type = rhs_terms::sum;
	new_terms.m_term1 = &a;
	new_terms.m_term2 = &b;
	
	return new_terms;
}

class einsum_code_generator: public block::c_code_generator {
	using block::c_code_generator::visit;
	using block::c_code_generator::c_code_generator;
	virtual void visit(block::for_stmt::Ptr);
public:
	static void generate_code(block::block::Ptr ast, std::ostream &oss, int indent = 0) {
		einsum_code_generator generator(oss);
		generator.curr_indent = indent;
		ast->accept(&generator);
		oss << std::endl;
	}
};

static void run_einsum_pipeline(block::block::Ptr ast, std::ostream &oss) {
	// Run a preprocessing pass to eliminate all redundant variables
	block::eliminate_redundant_vars(ast);	
	auto new_decls = block::extract_cuda_from(block::to<block::func_decl>(ast)->body);
	for (auto a: new_decls)
		block::c_code_generator::generate_code(a, oss, 0);
	einsum_code_generator::generate_code(ast, oss, 0);
}

void einsum_code_generator::visit(block::for_stmt::Ptr s) {
	std::string pragma_prefix ("pragma: ");
	if (!s->annotation.compare(0, pragma_prefix.size(), pragma_prefix)) {
		std::string pragma_value = s->annotation.substr(pragma_prefix.size());
		oss << "_Pragma(\"" << pragma_value << "\")" << std::endl;
		printer::indent(oss, curr_indent);
	}
	block::c_code_generator::visit(s);
}



}

static void matrix_vector_multiplication(builder::dyn_var<int*> C, builder::dyn_var<int*> A, builder::dyn_var<int*> B, int M, int N) {
        el::current_device = el::SERIAL;

        el::index i, j;
        el::tensor<int> c({M}, C);
        el::tensor<int> a({M, N}, A);
        el::tensor<int> b({N}, B);

	// Initalization of tensors
	// Can also be initialized outside the DSL
        b[j] = 1;
	a[i][j] = i * N + j;

        c[i] = 2 * a[i][j] * b[j];

}

int main(int argc, char* argv[]) {
	std::cout << "#include \"d2x_runtime/d2x_runtime.h\"" << std::endl;
        el::run_einsum_pipeline(
                builder::builder_context().extract_function_ast(
                        matrix_vector_multiplication, "matrix_vector", 1024, 512),
                std::cout);


        return 0;
}
