/*
 * ProjectHeat.cpp
 *
 *  Created on: 4.5.2018
 *      Author: kyritsak
 */

#include "ProjectHeat.h"

namespace femocs {

ProjectHeat::ProjectHeat(AtomReader &reader, Config &conf) : ProjectRunaway(reader, conf) {
}

int ProjectHeat::run(int timestep) {
    double tstart = omp_get_wtime();

    //***** Build or import mesh *****

    if (generate_mesh())
        return process_failed("Mesh generation failed!");

    check_return(!mesh_changed, "First meshing failed! Terminating...");

    if (prepare_solvers())
        return process_failed("Preparation of FEM solvers failed!");

    //***** Run FEM solvers *****

    for(auto factor : conf.field.apply_factors){
        conf.field.E0 *= factor;
        conf.field.V0 *= factor;

        if (run_field_solver())
            return process_failed("Running field solver in a " + conf.field.solver + " mode failed!");

        if (run_heat_solver())
            return process_failed("Running heat solver in a " + conf.heating.mode + " mode failed!");

        //***** Prepare for data export and next run *****
        if (prepare_export())
            return process_failed("Interpolating solution on atoms failed!");

        finalize(tstart);
        conf.field.E0 /= factor;
        conf.field.V0 /= factor;
    }

    return 0;
}

int ProjectHeat::run_field_solver() {
    if (conf.field.solver == "poisson") {
        if (conf.pic.mode == "transient")
            return solve_pic(conf.behaviour.timestep_fs, mesh_changed);
        else if (conf.pic.mode == "converge")
            return converge_pic(1.e4);
        else
            check_return(false, "Invalid PIC mode: " + conf.pic.mode);
    }

    if (mesh_changed && (conf.field.solver == "laplace" || conf.pic.mode == "none"))
        return solve_laplace(conf.field.E0, conf.field.V0);

    return 0;
}

int ProjectHeat::run_heat_solver() {
    int ccg, hcg;

    if (conf.heating.mode == "converge")
        return converge_heat(conf.heating.t_ambient);

    if (mesh_changed && conf.heating.mode == "transient")
        return solve_heat(conf.heating.t_ambient, GLOBALS.TIME - last_heat_time, true, ccg, hcg);

    return 0;
}

int ProjectHeat::converge_pic(double max_time) {
    double time_window; //time window to check convergence
    int i_max; //window iterations
    if (max_time < conf.pic.dt_max * 16) {
        time_window = max_time;
        i_max = 1;
    } else {
        i_max =  ceil(max_time / (16 * conf.pic.dt_max));
        time_window = max_time / i_max;
    }

    double I_mean = 0., I_mean_prev = 0., I_std = 0.;

    start_msg(t0, "=== Converging PIC with time window " + d2s(time_window, 2) + " fs\n");
    for (int i = 0; i < i_max; ++i) {
        solve_pic(time_window, i==0);
        I_mean = emission.get_global_stats(I_std);
        double err = (I_mean - I_mean_prev) / I_mean;
        if (MODES.VERBOSE){
            printf("  i=%d, I_mean= %e A, I_std=%.2f%, error=%.2f%",
                    i, I_mean, 100. * I_std / I_mean, 100 * err);
            cout << endl;
        }
        I_mean_prev = I_mean;

        if (fabs(err) < 0.05 && fabs(err) < conf.pic.convergence * I_std / I_mean)
            return 0;
    }
    return 0;
}

int ProjectHeat::converge_heat(double T_ambient) {
    const int max_steps = 1000;
    double delta_time = conf.heating.delta_time;
    int ccg, hcg, step, error;

    bool global_verbosity = MODES.VERBOSE;

    start_msg(t0, "=== Converging heat...\n");

    for (step = 0; step < max_steps; ++step) {

//        emission.write("emission_before.movie");

        // advance heat and current system for delta_time
        error = solve_heat(conf.heating.t_ambient, delta_time, step == 0, ccg, hcg);
        if (error) return error;

        // modify the advance time depending on how slowly the solution is changing
        if (conf.pic.mode == "none" || conf.pic.mode == "converge")
            GLOBALS.TIME += delta_time;

        if (hcg < (ccg - 10) && delta_time <= conf.heating.dt_max / 1.25) // heat changed too little?
            delta_time *= 1.25;
        else if (hcg > (ccg + 10)) // heat changed too much?
            delta_time /= 1.25;

        // write debug data
        if (global_verbosity)
            printf( "t= %e ps, dt= %.2e ps, Tmax= %e K\n",
                    GLOBALS.TIME * 1.e-3, delta_time * 1.e-3, ch_solver.heat.max_solution() );
        write_results(true);

        // check if the result has converged
        if (max(hcg, ccg) < 10) break;

        // update field - advance PIC for delta time
        if (conf.pic.mode == "transient")
            error = solve_pic(delta_time, false);
        else if (conf.pic.mode == "converge")
            error = converge_pic(delta_time);
        if (error) return error;

    }

    MODES.VERBOSE = global_verbosity;
    end_msg(t0);

    check_return(step < max_steps, "Failed to converge heat equation after " + d2s(max_steps) + " steps!");
    return 0;
}

} /* namespace femocs */
