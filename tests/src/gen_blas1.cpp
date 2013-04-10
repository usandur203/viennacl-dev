/* =========================================================================
   Copyright (c) 2010-2012, Institute for Microelectronics,
                            Institute for Analysis and Scientific Computing,
                            TU Wien.
   Portions of this software are copyright by UChicago Argonne, LLC.

                            -----------------
                  ViennaCL - The Vienna Computing Library
                            -----------------

   Project Head:    Karl Rupp                   rupp@iue.tuwien.ac.at
               
   (A list of authors and contributors can be found in the PDF manual)

   License:         MIT (X11), see file LICENSE in the base directory
============================================================================= */

//
// *** System
//
#include <iostream>

//
// *** Boost
//
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/foreach.hpp>

//
// *** ViennaCL
//
#define VIENNACL_WITH_UBLAS 1

#define VIENNACL_DEBUG_ALL
#define VIENNACL_DEBUG_BUILD
#include "viennacl/vector.hpp"
#include "viennacl/matrix.hpp"
#include "viennacl/linalg/inner_prod.hpp"
#include "viennacl/linalg/norm_1.hpp"
#include "viennacl/linalg/norm_2.hpp"
#include "viennacl/linalg/norm_inf.hpp"
#include "viennacl/generator/custom_operation.hpp"

#define CHECK_RESULT(cpu,gpu, op) \
    if ( double delta = fabs ( diff ( cpu, gpu) ) > epsilon ) {\
        std::cout << "# Error at operation: " #op << std::endl;\
        std::cout << "  diff: " << delta << std::endl;\
        retval = EXIT_FAILURE;\
    }\


using namespace boost::numeric;
using namespace viennacl;

template <typename ScalarType, typename VCLMatrixType>
ScalarType diff(ublas::matrix<ScalarType> & mat1, VCLMatrixType & mat2)
{
    ublas::matrix<ScalarType> mat2_cpu(mat2.size1(), mat2.size2());
    viennacl::backend::finish();
    viennacl::copy(mat2, mat2_cpu);
    double ret = 0;
    double act = 0;
    for (unsigned int i = 0; i < mat2_cpu.size1(); ++i)
    {
      for (unsigned int j = 0; j < mat2_cpu.size2(); ++j)
      {
         act = fabs(mat2_cpu(i,j) - mat1(i,j)) / std::max( fabs(mat2_cpu(i, j)), fabs(mat1(i,j)) );
         if (act > ret)
           ret = act;
      }
    }
    //std::cout << ret << std::endl;
    return ret;
}

template <typename ScalarType, unsigned int Alignment>
ScalarType diff ( ublas::vector<ScalarType> & v1, viennacl::vector<ScalarType,Alignment> & v2 ) {
    ublas::vector<ScalarType> v2_cpu ( v2.size() );
    viennacl::copy( v2.begin(), v2.end(), v2_cpu.begin() );
    for ( unsigned int i=0; i<v1.size(); ++i ) {
        if ( std::max ( fabs ( v2_cpu[i] ), fabs ( v1[i] ) ) > 0 )
            v2_cpu[i] = fabs ( v2_cpu[i] - v1[i] ) / std::max ( fabs ( v2_cpu[i] ), fabs ( v1[i] ) );
        else
            v2_cpu[i] = 0.0;
    }
    return norm_inf ( v2_cpu );
}

template<typename ScalarType>
double diff(ScalarType s, viennacl::scalar<ScalarType> & gs){
    return s - gs;
}

template< typename NumericT, typename Epsilon >
int test_vector ( Epsilon const& epsilon) {
    int retval = EXIT_SUCCESS;

    typedef viennacl::generator::vector<NumericT> vec;
    typedef viennacl::generator::scalar<NumericT> scal;


    unsigned int size = 256;

    ublas::vector<NumericT> cw(size);
    ublas::vector<NumericT> cx(size);
    ublas::vector<NumericT> cy(size);
    ublas::vector<NumericT> cz(size);

    NumericT s;



    for(unsigned int i=0; i<cw.size(); ++i){
        cw[i]=std::rand()/(NumericT)RAND_MAX;
    }

//    std::cout << "Running tests for vector of size " << cw.size() << std::endl;

    viennacl::vector<NumericT> w (size);
    viennacl::vector<NumericT> x (size);
    viennacl::vector<NumericT> y (size);
    viennacl::vector<NumericT> z (size);
    viennacl::scalar<NumericT> gs(0);

    cx = 2*cw;
    cy = 3*cw;
    cz = 4*cw;
    viennacl::copy (cw, w);
    viennacl::copy (cx, x);
    viennacl::copy (cy, y);
    viennacl::copy (cz, z);

    // --------------------------------------------------------------------------

    {
        std::cout << "w = x + y ..." << std::endl;
        cw = cx + cy;
        generator::custom_operation op;
        op.add(vec(w) = vec(x) + vec(y));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cw, w, w = x + y);
    }

    {
        std::cout << "w = x + y ..." << std::endl;
        cw = cx + cy;
        generator::custom_operation op;
        op.add(vec(w) = vec(x) + vec(y));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cw, w, w = x + y);
    }

    {
        std::cout << "x = w + y ..." << std::endl;
        cx = cw + cy;
        generator::custom_operation op;
        op.add(vec(x) = vec(w) + vec(y));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cx, x, x = w + y);
    }


    {
        std::cout << "w = x > 0.42" << std::endl;
        for(unsigned int i=0 ; i < size ; ++i){
            cw(i) = cx(i) > (float)0.42;
        }
        generator::custom_operation op;
        op.add(vec(w) = vec(x) > (float)0.42);
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cw, w, w = x > 1)
    }

    {
        std::cout << "w = -w ..." << std::endl;
        cw = -cw;
        generator::custom_operation op;
        op.add(vec(w) = -vec(w));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cw,w, w=-w);
    }

    {
        std::cout << "w = x + shift(x,-5) + shift(x,3) ..." << std::endl;
        for(int i=0 ; i<size; ++i){
            int ind1 = std::max(i - 5, 0);
            int ind2 = std::min(i + 3, (int)size-1);
            cw(i) = cx(i) + cx(ind1) + cx(ind2);
        }
        generator::custom_operation op;
        op.add(vec(w) = vec(x) + generator::shift(vec(x),-5) + generator::shift(vec(x),3));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cw,w, w = x + shift(x,-5) + shift(x,3) );
    }

    {
        std::cout << "s = inner_prod(x,y)..." << std::endl;
        s = ublas::inner_prod(cx,cy);
        generator::custom_operation op((scal(gs)= generator::inner_prod(vec(x), vec(y))));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(s,gs, s=inner_prod(x,y));
    }

    {
        std::cout << "s = max(x)..." << std::endl;
        s = *std::max_element(cx.begin(),cx.end());
        generator::custom_operation op((scal(gs)= generator::reduce<generator::fmax_type>(vec(x))));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(s,gs, s=max(x));
    }

    {
        std::cout << "Multiline ..." << std::endl;
        viennacl::generator::custom_operation op;
        op.add(vec(w) = 0.42 * (vec(x) - vec(y)));
        op.add(vec(y) = element_prod(vec(x), vec(x))
                          - vec(y)
                          - element_prod(vec(x), vec(z)));
        op.add(vec(z) = element_prod(vec(x), vec(y)) - 0.2 * vec(z));
        op.execute();
        viennacl::ocl::get_queue().finish();
        for(unsigned int i=0 ; i < size ; ++i){
            cw(i) = 0.42*(cx(i) - cy(i));
            cy(i) = cx(i)*cx(i) - cy(i) - cx(i)*cz(i);
            cz(i) = cx(i)*cy(i) - 0.2*cz(i);
        }
        CHECK_RESULT(cw, w, Multiline);
        CHECK_RESULT(cy, y, Multiline);
        CHECK_RESULT(cz, z, Multiline);
    }
    return retval;
}



template< typename NumericT, class Layout, typename Epsilon >
int test_matrix ( Epsilon const& epsilon) {
    int retval = EXIT_SUCCESS;

    unsigned int size1 = 1024;
    unsigned int size2 = 1024;

    unsigned int pattern_size1 = 256;
    unsigned int pattern_size2 = 128;

    unsigned int n_rep1 = 4;
    unsigned int n_rep2 = 8;

    typedef viennacl::generator::matrix<viennacl::matrix<NumericT,Layout> > dm_t;
    typedef viennacl::generator::vector<NumericT> dv_t;

    ublas::matrix<NumericT> cA(size1,size2);
    ublas::matrix<NumericT> cB(size1,size2);
    ublas::matrix<NumericT> cC(size1,size2);

    ublas::matrix<NumericT> cPattern(pattern_size1,pattern_size2);

    ublas::vector<NumericT> cx(size1);


    for(unsigned int i=0; i<size1; ++i)
        for(unsigned int j=0 ; j<size2; ++j)
            cA(i,j)=(double)std::rand()/RAND_MAX;

    for(unsigned int i = 0 ; i < pattern_size1 ; ++i)
        for(unsigned int j = 0 ; j < pattern_size2 ; ++j)
            cPattern(i,j) = (double)std::rand()/RAND_MAX;


    for(unsigned int i=0; i<size2; ++i){
        cx(i) = (double)std::rand()/RAND_MAX;
    }

//    std::cout << "Running tests for matrix of size " << cA.size1() << "," << cA.size2() << std::endl;

    viennacl::matrix<NumericT,Layout> A (size1, size2);
    viennacl::matrix<NumericT,Layout> B (size1, size2);
    viennacl::matrix<NumericT,Layout> C (size1, size2);

    viennacl::matrix<NumericT, Layout> pattern(pattern_size1, pattern_size2);

    viennacl::vector<NumericT> x(size1);

    cB = cA;
    cC = cA;
    viennacl::copy(cA,A);
    viennacl::copy(cB,B);
    viennacl::copy(cC,C);

    viennacl::copy(cPattern,pattern);

    viennacl::copy(cx,x);

//    {
//        std::cout << "C += I ..." << std::endl;
//        for(unsigned int i = 0 ; i < size2 ; ++i){
//            cC(i,i) += 1;
//        }
//        generator::custom_operation op;
//        op.add(generator::diag(dm_t(C)) += 1);
//        op.execute();
//        viennacl::ocl::get_queue().finish();
//        CHECK_RESULT(cC, C, C+=I)
//    }

//    {
//        std::cout << "x = diag(A) ..." << std::endl;
//        for(unsigned int i = 0; i < size1 ; ++i){
//            cx(i) = cA(i,i);
//        }
//        generator::custom_operation op((dv_t(x) = generator::diag(dm_t(A))));
//        op.execute();
//        viennacl::ocl::get_queue().finish();
//        CHECK_RESULT(cx,x, x = diag(A));
//    }

    {
        std::cout << "C = A + B ..." << std::endl;
        cC     = ( cA + cB );
        generator::custom_operation op((dm_t(C) = dm_t(A) + dm_t(B)));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cC, C, C=A+B)
    }

    {
        std::cout << "C = repmat(P, M, N) ..." << std::endl;
        for(unsigned int i = 0 ; i < size1 ; ++i)
            for(unsigned int j = 0 ; j < size2 ; ++j)
                cC(i,j) = cPattern(i%pattern_size1, j%pattern_size2);
        generator::custom_operation op((dm_t(C) = generator::repmat(dm_t(pattern),n_rep1,n_rep2)));
        op.execute();
        viennacl::ocl::get_queue().finish();
        CHECK_RESULT(cC, C, C = repmat(P, M, N))
    }

//    {
//        std::cout << "C = repmat(x, 1, N) ..." << std::endl;
//        for(unsigned int i = 0 ; i < size1 ; ++i)
//            for(unsigned int j = 0 ; j < size2 ; ++j)
//                cC(i,j) = cx(i);
//        generator::custom_operation op((dm_t(C) = generator::repmat(dv_t(x),1,C.size2())));
//        op.execute();
//        viennacl::ocl::get_queue().finish();
//        CHECK_RESULT(cC, C, C = repmat(x, 1, N))
//    }

//    {
//        std::cout << "C = trans(repmat(x, 1, N)) ..." << std::endl;
//        for(unsigned int i = 0 ; i < size1 ; ++i)
//            for(unsigned int j = 0 ; j < size2 ; ++j)
//                cC(i,j) = cx(j);
//        generator::custom_operation op((dm_t(C) = generator::trans(generator::repmat(dv_t(x),1,C.size2()))));
//        op.execute();
//        viennacl::ocl::get_queue().finish();
//        CHECK_RESULT(cC, C, C = repmat(x, 1, N))
//    }


//    {
//        std::cout << "C = -A ..." << std::endl;
//        for(unsigned int i = 0 ; i < size1 ; ++i)
//            for(unsigned int j = 0 ; j < size2 ; ++j)
//                cC(i,j) = -cA(i,j);
//        generator::custom_operation op((dm_t(C) = -dm_t(A)));
//        op.execute();
//        viennacl::ocl::get_queue().finish();
//        CHECK_RESULT(cC, C, C = -A)
//    }

//    {
//        std::cout << "C = 1/(1+EXP(-A)) ..." << std::endl;
//        for(unsigned int i = 0 ; i < size1 ; ++i)
//            for(unsigned int j = 0 ; j < size2 ; ++j)
//                cC(i,j) = 1.0f/(1.0f+std::exp(-cA(i,j)));
//        generator::custom_operation op((dm_t(C) = 1.0f/(1.0f+generator::exp(-dm_t(A)))));
//        op.execute();
//        viennacl::ocl::get_queue().finish();
//        CHECK_RESULT(cC, C, C = 1/(1+EXP(-A)))
//    }


    return retval;
}


int main(int argc, char* argv[]){
    std::vector<std::string> args(argv,argv+argc);
    if(argc!=2){
        std::cerr << "USAGE : PROGRAM_NAME DEVICE" << std::endl;
        exit(1);
    }
    int retval = EXIT_SUCCESS;
    unsigned int requested_device = atoi(args[1].c_str());

    typedef std::vector< viennacl::ocl::platform > platforms_type;
    typedef std::vector<viennacl::ocl::device> devices_type;
    typedef std::vector<cl_device_id> cl_devices_type;

    platforms_type platforms = viennacl::ocl::get_platforms();
    size_t num_platforms = platforms.size();

    unsigned int current_device = 0;

    for(unsigned int k=0 ; k < num_platforms ; ++k)
    {
        viennacl::ocl::platform pf(k);
        viennacl::ocl::set_context_device_type(k,CL_DEVICE_TYPE_ALL);
        viennacl::ocl::set_context_platform_index(k,k);
        viennacl::ocl::switch_context(k);
        devices_type dev = viennacl::ocl::current_context().devices();
        for(devices_type::iterator it = dev.begin() ; it != dev.end() ; ++it){

            if(current_device++ == requested_device ){
                viennacl::ocl::switch_device(*it);
                std::cout << std::endl;
                std::cout << "----------------------------------------------" << std::endl;
                std::cout << "               Device Info" << std::endl;
                std::cout << "----------------------------------------------" << std::endl;
                std::cout << viennacl::ocl::current_device().info() << std::endl;

//                std::cout << std::endl;
//                std::cout << "----------------------------------------------" << std::endl;
//                std::cout << "----------------------------------------------" << std::endl;
//                std::cout << "## Test :: Vector" << std::endl;
//                std::cout << "----------------------------------------------" << std::endl;

//                {
//                    double epsilon = 1.0E-4;
//                    std::cout << "# Testing setup:" << std::endl;
//                    std::cout << "  numeric: double" << std::endl;
//                    retval = test_vector<double> (epsilon);

//                    if ( retval == EXIT_SUCCESS )
//                        std::cout << "# Test passed" << std::endl;
//                    else
//                        return retval;
//                }

                std::cout << std::endl;
                std::cout << "----------------------------------------------" << std::endl;
                std::cout << "----------------------------------------------" << std::endl;
                std::cout << "## Test :: Matrix" << std::endl;
                std::cout << "----------------------------------------------" << std::endl;

                {
                    double epsilon = 1.0E-4;
                    std::cout << "# Testing setup:" << std::endl;
                    std::cout << "  numeric: double" << std::endl;
                    std::cout << "  --------------" << std::endl;
                    std::cout << "  Row-Major"      << std::endl;
                    std::cout << "  --------------" << std::endl;
                    retval = test_matrix<double, viennacl::row_major> (epsilon);
//                    std::cout << "  --------------" << std::endl;
//                    std::cout << "  Column-Major"      << std::endl;
//                    std::cout << "  --------------" << std::endl;
//                    retval &= test_matrix<double, viennacl::column_major> (epsilon);
                    if ( retval == EXIT_SUCCESS )
                        std::cout << "# Test passed" << std::endl;
                    else
                        return retval;
                }

            }
        }
    }
}
