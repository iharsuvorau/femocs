/*
 * ProjectHeat.cpp
 *
 *  Created on: 4.5.2018
 *      Author: kyritsak
 */

#include "ProjectSpaceCharge.h"

namespace femocs {

ProjectSpaceCharge::ProjectSpaceCharge(AtomReader &reader, Config &conf) : ProjectRunaway(reader, conf) {
}

int ProjectSpaceCharge::run(int timestep, double time) {
    double tstart = omp_get_wtime();

    //***** Build or import mesh *****

    cout << "starting project space charge..." << endl;
    if (generate_mesh())
        return process_failed("Mesh generation failed!");
    cout << "here";
    check_return(!mesh_changed, "First meshing failed! Terminating...");

    cout << "Preparing solvers..." << endl;
    if (prepare_solvers())
        return process_failed("Preparation of FEM solvers failed!");

    vector<double> I_target;

    double E_orig = conf.field.E0, V_orig = conf.field.V0;

    for(auto factor : conf.field.apply_factors){
        conf.field.E0 = E_orig * factor;
        conf.field.V0 = V_orig * factor;

        if (converge_pic())
            return process_failed("Running field solver in a " + conf.field.solver + " mode failed!");

        I_target.push_back(emission.global_data.I_mean);
    }

    double Veff = find_Veff(I_target);

    cout << "Effective applied voltage: " << Veff << endl;

    return 0;
}

int ProjectSpaceCharge::converge_pic() {
    double time_window = 32 * conf.pic.dt_max; //time window to check convergence
    int i_max = 1024; //window iterations

    double I_mean_prev = emission.global_data.I_mean;

    start_msg(t0, "=== Converging PIC...\n");
    for (int i = 0; i < i_max; ++i) {
        pic_solver.stats_reinit();
        solve_pic(time_window, i==0);
        emission.calc_global_stats();
        double err = (emission.global_data.I_mean - I_mean_prev) / emission.global_data.I_mean;
        if (MODES.VERBOSE){
            printf("  i=%d, I_mean= %e A, I_std=%.2f%, error=%.2f%, inj=%d, del=%d", i, emission.global_data.I_mean,
                    100. * emission.global_data.I_std / emission.global_data.I_mean, 100 * err, pic_solver.get_injected(), pic_solver.get_removed());
            cout << endl;
        }
        I_mean_prev = emission.global_data.I_mean;

        bool converged = fabs(err) < conf.pic.convergence * emission.global_data.I_std /
                emission.global_data.I_mean && fabs(err) < 0.05 && pic_solver.is_stable() ;

        if (converged)
            return 0;
    }
    return 0;
}

void ProjectSpaceCharge::get_currents(double Vappl, vector<double> &curs){
    curs.resize(conf.field.apply_factors.size());

    int i = 0;
    for(auto factor : conf.field.apply_factors){
        emission.set_sfactor(factor);
        emission.calc_emission(conf.emission, Vappl);
        curs[i++] = emission.global_data.I_tot;
    }

}

double ProjectSpaceCharge::find_Veff(vector<double> I_target){


    solve_laplace(conf.field.E0, conf.field.V0);
    surface_fields.interpolate(ch_solver);
    surface_temperatures.interpolate(ch_solver);
    emission.initialize(mesh, true);

    int Nmax = 50;
    double errlim = 0.01;

    double Veff = conf.field.V0, Vhigh, Vlow, old_error;

    vector<double> currents;

    for(int i = 0; i < Nmax; ++i){ // first find two values that produce opposite sign errors
        get_currents(Veff, currents);
        double error = get_current_error(currents, I_target);
        if (i == 0)
            old_error = error;
        cout << " Veff = " << Veff << ", error = " <<  error << ", old_error = " << old_error << endl;
        if(error > errlim && error * old_error > 0){
            Vlow = Veff;
            Veff *= 2;
            Vhigh = Veff;
        } else if(error < -errlim && error * old_error > 0){
            Vhigh = Veff;
            Veff /= 2;
            Vlow = Veff;
        } else
            break;
        old_error = error;
    }

    Veff = .5 * (Vhigh + Vlow);

    for(int i = 0; i < Nmax; ++i){ // perform bisection
        get_currents(Veff, currents);
        double error = get_current_error(currents, I_target);
        cout  << " Veff = " << Veff << ", error = " <<  error << endl;
        if(error > errlim){
            Vlow = Veff;
            Veff = .5 * (Vhigh + Vlow);
        }
        else if(error < -errlim){
            Vhigh = Veff;
            Veff = .5 * (Vhigh + Vlow);
        }
        else
            return Veff;
    }
}

double ProjectSpaceCharge::get_current_error(vector<double> I_calc, vector<double> I_target){
    require(I_calc.size() == I_target.size(), "comparison of current vectors no equal sizes");
    double error = 0;
    for(int i = 0; i < I_calc.size(); i++){
        error += log(I_calc[i] / I_target[i]);
    }
    return error;
}




} /* namespace femocs */
