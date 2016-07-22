#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#include "ocp_qp_condensing_qpoases.h"
#include "condensing.h"

/* qpOASES specifics */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#pragma clang diagnostic ignored "-Wtautological-pointer-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#include "qpOASES_e/QProblem.h"
#pragma clang diagnostic pop
#define     NWSR_MAX    1000
QProblem    QP;
real_t      _A[NNN*NX*NVC] = {0};
real_t      cput;
int_t       nwsr;
real_t      qp_sol[NVC]                     = {0};  // QP primal solution vector
real_t      y[(NNN+1)*NX+NNN*(NX+NU)+NX]    = {0};  // QP dual solution vector
/* condensing specifics */
data_struct data = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, \
                    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}};

int_t get_num_opt_vars(int_t NN, int_t *nx, int_t *nu) {
    int_t num_opt_vars = 0;
    for (int_t i = 0; i < NN; i++)
        num_opt_vars += nx[i] + nu[i];
    num_opt_vars += nx[NN];
    return num_opt_vars;
}

int_t get_num_condensed_vars(int_t NN, int_t *nx, int_t *nu) {
    int_t num_condensed_vars = 0;
    #if FIXED_INITIAL_STATE == 0
    num_condensed_vars += nx[1];
    #endif
    for (int_t i = 0; i < NN; i++)
        num_condensed_vars += nu[i];
    return num_condensed_vars;
}

void write_array_to_file(FILE *outputFile, real_t *array, int_t size) {
    for (int_t i = 0; i < size; i++) fprintf(outputFile, "%g ", array[i]);
    fprintf(outputFile, "\n");
}

void write_QP_data_to_file() {
    FILE *outFile = fopen("QP_data.txt", "w");
    if (outFile == NULL) {
        fprintf(stderr, "%s\n", "OPEN FILE FAILED!");
    }
    write_array_to_file(outFile, data.Hc, NVC*NVC);
    write_array_to_file(outFile, data.gc, NVC);
    write_array_to_file(outFile, _A, NNN*NX*NVC);
    write_array_to_file(outFile, data.lbU, NVC);
    write_array_to_file(outFile, data.ubU, NVC);
    write_array_to_file(outFile, data.lbA, NNN*NX);
    write_array_to_file(outFile, data.ubA, NNN*NX);
    write_array_to_file(outFile, data.C, NNN*NX*NVC);
    write_array_to_file(outFile, data.d, NNN*NX);
}

int_t ocp_qp_condensing_qpoases(int_t NN, int_t *nx, int_t *nu, int_t *nb, int_t *ng, \
    double **A, double **B, double **b, \
    double **Q, double **S, double **R, double **q, double **r, \
    int_t **idxb, double **lb, double **ub, \
    double **C, double **D, double **ld, double **ud, \
    double **x, double **u, \
    struct ocp_qp_condensing_qpoases_args *args, double *work) {

    int_t return_flag = -1;
    /* FILL IN CONDENSING VARIABLES */
    memset(&data, 0, sizeof(data_struct));  // Condensing implicitly assumes zeros initialisation
    // OBJECTIVE
    for (int_t i = 0; i < NN+1; i++) {
        for (int_t j = 0; j < nx[i]; j++) {
            for (int_t k = 0; k < nx[i]; k++) data.Q[i*NX*NX+j*NX+k] = Q[i][j*NX+k];
        }
    }
    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < nx[i]; j++) {
            for (int_t k = 0; k < nu[i]; k++) data.S[i*NU*NX+j*NU+k] = S[i][j*NU+k];
        }
    }
    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < nu[i]; j++) {
            for (int_t k = 0; k < nu[i]; k++) data.R[i*NU*NU+j*NU+k] = R[i][j*NU+k];
        }
    }
    int_t start_of_current_block = 0;
    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < nx[i]; j++)
            data.g[start_of_current_block+j] = q[i][j];
        for (int_t j = 0; j < nu[i]; j++)
            data.g[start_of_current_block+NX+j] = r[i][j];
        start_of_current_block += (NX + NU);
    }
    for (int_t j = 0; j < nx[NNN]; j++) data.g[NNN*(NX+NU)+j] = q[NNN][j];
    // DYNAMICS
    start_of_current_block = 0;
    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < nx[i]; j++) {
            for (int_t k = 0; k < nx[i+1]; k++) {
                data.A[start_of_current_block+j*nx[i+1]+k] = A[i][j*nx[i+1]+k];
            }
        }
        start_of_current_block += nx[i+1]*nx[i+1];
    }

    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < nx[i+1]; j++) data.b[i*NX+j] = b[i][j];
    }
    start_of_current_block = 0;
    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < nu[i]; j++) {
            for (int_t k = 0; k < nx[i+1]; k++) {
                data.B[start_of_current_block+j*nx[i+1]+k] = B[i][j*nx[i+1]+k];
            }
        }
        start_of_current_block += nu[i]*nx[i+1];
    }
    // BOUNDS
    for (int_t j = 0; j < nu[0]; j++) {
        data.lb[NX+idxb[0][j]] = lb[0][idxb[0][j]];
        data.ub[NX+idxb[0][j]] = ub[0][idxb[0][j]];
    }
    for (int_t i = 1; i < NN; i++) {
        for (int_t j = 0; j < nu[i]; j++) {
            // the interface assumes [u,x] ordering
            data.lb[NX+i*(nu[i]+nx[i])+idxb[i][j]] = lb[i][idxb[i][j]];
            data.ub[NX+i*(nu[i]+nx[i])+idxb[i][j]] = ub[i][idxb[i][j]];
        }
        for (int_t j = nu[i]; j < nb[i]; j++) {
            data.lb[-NU+i*(nu[i]+nx[i])+idxb[i][j]] = lb[i][idxb[i][j]];
            data.ub[-NU+i*(nu[i]+nx[i])+idxb[i][j]] = ub[i][idxb[i][j]];
        }
    }
    for (int_t j = 0; j < nb[NN]; j++) {
        data.lb[NN*(NX+NU)+idxb[NN][j]] = lb[NN][idxb[NN][j]];
        data.ub[NN*(NX+NU)+idxb[NN][j]] = ub[NN][idxb[NN][j]];
    }
    // POLYTOPIC CONSTRAINTS: TODO
    condensingN2_fixed_initial_state();

    // SOLVE QP
    int_t num_condensed_vars = get_num_condensed_vars(NN, nx, nu);
    nwsr = NWSR_MAX;
    cput = 100.0;
    // Convert C to row major in A
    for (int_t i = 0; i < NN*NX; i++) {
        for (int_t j = 0; j < num_condensed_vars; j++) {
            _A[i*num_condensed_vars+j] = data.C[j*NN*NX+i];
        }
    }
    // Symmetrize Hc
    for (int_t i = 1; i < num_condensed_vars; i++) {
        for (int_t j = 0; j < i; j++) {
            data.Hc[i*num_condensed_vars+j] = data.Hc[j*num_condensed_vars+i];
        }
    }

    return_flag = QProblem_initW(&QP, &(data.Hc[0]), &(data.gc[0]), &(_A[0]), &(data.lbU[0]), \
                        &(data.ubU[0]), &(data.lbA[0]), &(data.ubA[0]), \
                        &nwsr, &cput, NULL, &(y[0]), NULL, NULL, NULL);
    QProblem_getPrimalSolution(&QP, &(qp_sol[0]));

    // Recover state trajectory
    for (int_t i = 0; i < NN; i++) {
        for (int_t j = 0; j < NX; j++) {
            x[i+1][j] = 0.0;
            for (int_t k = 0; k < NVC; k++) {
                x[i+1][j] = x[i+1][j] + data.C[i*NX+j+k*NN*NX]*qp_sol[k];
            }
            x[i+1][j] = x[i+1][j] + data.d[i*NX+j];
        }
        #if FIXED_INITIAL_STATE == 1
        for (int_t j = 0; j < NU; j++) u[i][j] = qp_sol[i*NU+j];
        #else
        for (int_t j = 0; j < NU; j++) u[i][j] = qp_sol[NX+i*NU+j];
        #endif
    }
    QProblem_getDualSolution(&QP, &(y[0]));
    return return_flag;
}

void initialise_qpoases() {
    QProblemCON(&QP, NVC, 0, HST_POSDEF);
    QProblem_setPrintLevel(&QP, PL_NONE);
    QProblem_printProperties(&QP);
}
#pragma clang diagnostic pop
