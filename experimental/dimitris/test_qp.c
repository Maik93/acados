#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

// #include "hpmpc/include/aux_d.h"

#include "blasfeo/include/blasfeo_target.h"
#include "blasfeo/include/blasfeo_common.h"
#include "blasfeo/include/blasfeo_d_aux.h"
#include "blasfeo/include/blasfeo_i_aux.h"


#include "acados/utils/types.h"
#include "acados/utils/allocate_ocp_qp.h"
#include "acados/ocp_qp/ocp_qp_common.h"
#include "acados/utils/read_ocp_qp_in.h"

#include "acados/ocp_qp/ocp_qp_ooqp.h"
#include "acados/ocp_qp/ocp_qp_condensing_qpoases.h"
#include "acados/ocp_qp/ocp_qp_hpmpc.h"

// #define NREP 1000
// #define MAXITER 10
// #define TOL 1e-8
// #define MINSTEP 1e-8

// // flush denormals to zero
// #if defined(TARGET_X64_AVX2) || defined(TARGET_X64_AVX) ||  \
//     defined(TARGET_X64_SSE3) || defined(TARGET_X86_ATOM) || \
//     defined(TARGET_AMD_SSE3)
// #include <xmmintrin.h>  // needed to flush to zero sub-normals with _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON); in the main()
// #endif

int_t main( ) {
    /* code */
    int N, return_value;
    ocp_qp_in qp_in;
    ocp_qp_out qp_out;

    #if SOLVER == 1
    ocp_qp_ooqp_args args;
    ocp_qp_ooqp_memory mem;
    ocp_qp_ooqp_workspace work;
    args.printLevel = 0;
    #elif SOLVER == 2
    ocp_qp_condensing_qpoases_args args;
    args.dummy = 42;
    #elif SOLVER == 3
    ocp_qp_hpmpc_args args;
    // args.tol = TOL;
    // args.max_iter = MAXITER;
    // args.mu0 = 0.0;
    // args.warm_start = 0;
    // args.N2 = N;
    // double inf_norm_res[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    // args.inf_norm_res = &inf_norm_res[0];
    #endif

    int_t BOUNDS = 1;
    int_t CONSTRAINTS = 1;
    int_t MPC = 1;
    int_t QUIET = 1;

    read_ocp_qp_in(&qp_in, "LTV/", BOUNDS, CONSTRAINTS, MPC, QUIET);
    allocate_ocp_qp_out(&qp_in, &qp_out);

    N = qp_in.N;
    int nPrimalVars = 0;
    for (int kk = 0; kk < N; kk++) {
        nPrimalVars += qp_in.nx[kk] + qp_in.nu[kk];
    }
    nPrimalVars += qp_in.nx[N];

    #if SOLVER == 1
    ocp_qp_ooqp_create_memory(&qp_in, &args, &mem);
    ocp_qp_ooqp_create_workspace(&qp_in, &args, &work);
    #elif SOLVER == 2
    initialise_qpoases(&qp_in);
    #elif SOLVER == 3
    int work_space_size =
        ocp_qp_hpmpc_workspace_size_bytes(N, (int_t *)qp_in.nx, (int_t *)qp_in.nu, (int_t *)qp_in.nb, (int_t *)qp_in.nc, (int_t **)qp_in.idxb, &args);
    printf("\nwork space size: %d bytes\n", work_space_size);
    void *workspace = malloc(work_space_size);
    #endif

    #if SOLVER == 1
    return_value = ocp_qp_ooqp(&qp_in, &qp_out, &args, &mem, &work);
    #elif SOLVER == 2
    return_value = ocp_qp_condensing_qpoases(&qp_in, &qp_out, &args, NULL);
    #endif

    printf("\nRETURN VALUE = %d\n\n", return_value);
    if (!QUIET) {
        printf("\nSOLUTION= \n");
        for (int ii=0; ii <nPrimalVars; ii++) printf("%10.8f\n",qp_out.x[0][ii]);
    }

    #if SOLVER == 1
    ocp_qp_ooqp_free_workspace(&work);
    ocp_qp_ooqp_free_memory(&mem);
    #endif

    free_ocp_qp_in(&qp_in);
    free_ocp_qp_out(&qp_out);

    return 0;
}