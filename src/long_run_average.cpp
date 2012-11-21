/**
* IMCA is a analyzing tool for unbounded reachability probabilities, expected-
* time, and long-run averages for Interactive Markov Chains and Markov Automata.
* Copyright (C) RWTH Aachen, 2012
* 	Author: Dennis Guck
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*
* Source description: 
*	 Compute long-run average for an MA
*/

#include "long_run_average.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <string>
#include <vector>

#include "soplex.h"
#include "sccs.h"
#include "read_file.h"
#include "debug.h"

using namespace std;
using namespace soplex;

/**
* sets the objective function and bounds for goal states
*
* @param lp_model linear program
* @param ma the MA
* @param max identifier for maximum/minimum
*/
static void set_obj_function_lra(SoPlex& lp_model, SparseMatrix *ma, bool max, vector<bool> mec, bool *locks) {
	unsigned long state_nr;
	//bool *goals = ma->goals;
	DSVector dummycol(0);
	double inf = soplex::infinity;
	
	/* set objective function to max, resp. min */
	if(max)
		lp_model.changeSense(SPxLP::MINIMIZE);
	else
		lp_model.changeSense(SPxLP::MAXIMIZE);
	
	/* set objective and bounds resp. to goal states*/
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		if(locks[state_nr] && mec[state_nr]){
			lp_model.addCol(LPCol(0.0, dummycol, 0, 0));
		} else {
			lp_model.addCol(LPCol(0.0, dummycol, inf, 0));
		}
		
	}
	lp_model.addCol(LPCol(1.0, dummycol, inf, 0));
}

/**
* sets the objective function and bounds for goal states
*
* @param lp_model linear program
* @param ma the MA
* @param max identifier for maximum/minimum
*/
static void set_obj_function_ssp(SoPlex& lp_model, SparseMatrix *ma, SparseMatrixMEC *mecs, bool max, 
				 vector<bool> mec, vector<Real> lra, bool *locks, map<unsigned long,unsigned long>& ssp_nr) {
	unsigned long state_nr;
	//bool *goals = ma->goals;
	DSVector dummycol(0);
	unsigned long count=0;
	double inf = soplex::infinity;
	
	/* set objective function to max, resp. min */
	if(max)
		lp_model.changeSense(SPxLP::MINIMIZE);
	else
		lp_model.changeSense(SPxLP::MAXIMIZE);
	
	/* set objective and bounds resp. to goal states*/
	/*
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		if(mec[state_nr]){
			//lp_model.addCol(LPCol(0.0, dummycol, lra[state_nr], lra[state_nr]));
		} else if(!locks[state_nr]){
			lp_model.addCol(LPCol(1.0, dummycol, infinity, 0));
			ssp_nr.insert(pair<unsigned long,unsigned long>(state_nr,count));
			count++;
		} else {
			lp_model.addCol(LPCol(1.0, dummycol, 0, 0));
			ssp_nr.insert(pair<unsigned long,unsigned long>(state_nr,count));
			count++;
		}
		
	}*/
	// add states S\MEC
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		if(mec[state_nr]){
			//lp_model.addCol(LPCol(0.0, dummycol, lra[state_nr], lra[state_nr]));
		} else if(!locks[state_nr]){
			lp_model.addCol(LPCol(1.0, dummycol, inf, 0));
			ssp_nr.insert(pair<unsigned long,unsigned long>(state_nr,count));
			count++;
		} else {
			lp_model.addCol(LPCol(1.0, dummycol, 0, 0));
			ssp_nr.insert(pair<unsigned long,unsigned long>(state_nr,count));
			count++;
		}
		
	}
	// add descision states
	for(unsigned long mec_nr=0; mec_nr < mecs->n; mec_nr++) {
		lp_model.addCol(LPCol(1.0, dummycol, inf, 0));
		ssp_nr.insert(pair<unsigned long,unsigned long>(ma->n+mec_nr+1,count));
		count++;
	}
	// add MEC states
	for(unsigned long mec_nr=0; mec_nr < mecs->n; mec_nr++) {
		lp_model.addCol(LPCol(1.0, dummycol, inf, 0));
		ssp_nr.insert(pair<unsigned long,unsigned long>(ma->n+mec_nr+mecs->n+1,count));
		count++;
	}
}

/**
* sets the constraints for the linear program
*
* @param lp_model linear program
* @param ma the MA
* @param max identifier for maximum/minimum
*/
static void set_constraints_lra(SoPlex& lp_model, SparseMatrix *ma, bool max, vector<bool> mec, bool *locks) {
	unsigned long i;
	unsigned long state_nr;
	unsigned long choice_nr;
	unsigned long states = ma->n + 1;
	bool *goals = ma->goals;
	//map<unsigned long,string> states_nr = ma->states_nr;
	unsigned long *row_starts = (unsigned long *) ma->row_counts;
	unsigned long *rate_starts = (unsigned long *) ma->rate_counts;
	unsigned long *choice_starts = (unsigned long *) ma->choice_counts;
	Real *non_zeros = ma->non_zeros;
	Real *exit_rates = ma->exit_rates;
	unsigned long *cols = ma->cols;
	Real prob;
	Real rate;
	int m=0; // greater equal 0
	if(!max)
		m=2; // less equal 0
	
	
	//DSVector row(states+1);
	bool loop;
	bool bad=false;
	
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		if(mec[state_nr]){// && !locks[state_nr]) {
			unsigned long state_start = row_starts[state_nr];
			unsigned long state_end = row_starts[state_nr + 1];
			for (choice_nr = state_start; choice_nr < state_end; choice_nr++) {
				DSVector row(states);
				loop=false;
				/* Add up all outgoing rates of the distribution */
				unsigned long i_start = choice_starts[choice_nr];
				unsigned long i_end = choice_starts[choice_nr + 1];
				for (i = i_start; i < i_end; i++) {
					if(mec[cols[i]]) {
						prob=non_zeros[i];
						rate=0;
						unsigned long r_start = rate_starts[state_nr];
						unsigned long r_end = rate_starts[state_nr + 1];
						for (unsigned long j = r_start; j < r_end; j++) {
							prob /= exit_rates[j];
							rate = -1/exit_rates[j];
						}
						//printf("%s - %lf -> %s\n",(states_nr.find(state_nr)->second).c_str(),prob,(states_nr.find(cols[i])->second).c_str());
						if(state_nr==cols[i]) {
							loop=true;
							row.add(state_nr,-1.0+prob);
						} else {
							row.add(cols[i],prob);
						}
					}
					else{
						bad=true;
					}
				}
				if(!bad) {
					if(!loop)
						row.add(state_nr,-1.0);
					rate=0;
					unsigned long r_start = rate_starts[state_nr];
					unsigned long r_end = rate_starts[state_nr + 1];
					for (unsigned long j = r_start; j < r_end; j++) {
						rate = -1/exit_rates[j];
					}
					if(rate < 0)
						row.add(ma->n,rate);
					if(goals[state_nr]) {
						lp_model.addRow(LPRow(row,LPRow::Type(m), rate));
					} else {
						lp_model.addRow(LPRow(row,LPRow::Type(m), 0));
					}
				}
				row.~DSVector();
				bad=false;
			}
		}
	}	
}

/**
* sets the constraints for the linear program
*
* @param lp_model linear program
* @param ma the MA
* @param max identifier for maximum/minimum
*/
static void set_constraints_ssp(SoPlex& lp_model, SparseMatrix *ma,SparseMatrixMEC *mecs, bool max, vector<bool> mec, bool *locks, vector<Real> lra, map<unsigned long,unsigned long> ssp_nr, vector<Real> mecNr,vector<Real> lra_mec) {
	unsigned long i;
	unsigned long state_nr;
	unsigned long choice_nr;
	unsigned long states = ma->n + 1;
	//map<unsigned long,string> states_nr = ma->states_nr;
	unsigned long *row_starts = (unsigned long *) ma->row_counts;
	unsigned long *rate_starts = (unsigned long *) ma->rate_counts;
	unsigned long *choice_starts = (unsigned long *) ma->choice_counts;
	Real *non_zeros = ma->non_zeros;
	Real *exit_rates = ma->exit_rates;
	unsigned long *cols = ma->cols;
	Real prob;
	Real rate;
	Real goal;
	int m=0; // greater equal 0
	if(!max)
		m=2; // less equal 0
	
	
	//DSVector row(states+1);
	bool loop;
	/*
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		if(!mec[state_nr] && !locks[state_nr]) {
			unsigned long state_start = row_starts[state_nr];
			unsigned long state_end = row_starts[state_nr + 1];
			for (choice_nr = state_start; choice_nr < state_end; choice_nr++) {
				DSVector row(states);
				loop=false;
				goal=0;
				// Add up all outgoing rates of the distribution
				unsigned long i_start = choice_starts[choice_nr];
				unsigned long i_end = choice_starts[choice_nr + 1];
				for (i = i_start; i < i_end; i++) {
					prob=non_zeros[i];
					rate=0;
					unsigned long r_start = rate_starts[state_nr];
					unsigned long r_end = rate_starts[state_nr + 1];
					for (unsigned long j = r_start; j < r_end; j++) {
						prob /= exit_rates[j];
						rate = -1/exit_rates[j];
					}
					//printf("%s - %lf -> %s\n",(states_nr.find(state_nr)->second).c_str(),prob,(states_nr.find(cols[i])->second).c_str());
					if(state_nr==cols[i]) {
						loop=true;
						//row.add(state_nr,-1.0+prob);
						row.add(ssp_nr.find(state_nr)->second,-1.0+prob);
					} else if(mec[cols[i]]){
						goal += prob*lra[cols[i]];
					} else {
						//row.add(cols[i],prob);
						row.add(ssp_nr.find(cols[i])->second,prob);
					}
				}
				if(!loop)
					//row.add(state_nr,-1.0);
					row.add(ssp_nr.find(state_nr)->second,-1.0);
				if(goal == 0){
					lp_model.addRow(LPRow(row,LPRow::Type(m), 0));
				} else {
					goal *= -1;
					lp_model.addRow(LPRow(row,LPRow::Type(m), goal));
				}
				row.~DSVector();
			}
		}
	}
	*/
	
	// NOTE: fast fix, have to be reimplemented!
	
	// add transitions for S\MEC
	vector<Real> mec_prob(ma->n,0);
	vector<Real> mec_prob_tmp(ma->n,0);
	vector<bool> isMec(ma->n,false);
	vector<bool> isMec_tmp(ma->n,false);
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		if(!mec[state_nr] && !locks[state_nr]) {
			unsigned long state_start = row_starts[state_nr];
			unsigned long state_end = row_starts[state_nr + 1];
			for (choice_nr = state_start; choice_nr < state_end; choice_nr++) {
				DSVector row(states);
				loop=false;
				goal=0;
				// Add up all outgoing rates of the distribution
				unsigned long i_start = choice_starts[choice_nr];
				unsigned long i_end = choice_starts[choice_nr + 1];
				for (i = i_start; i < i_end; i++) {
					prob=non_zeros[i];
					rate=0;
					unsigned long r_start = rate_starts[state_nr];
					unsigned long r_end = rate_starts[state_nr + 1];
					for (unsigned long j = r_start; j < r_end; j++) {
						prob /= exit_rates[j];
						rate = -1/exit_rates[j];
					}
					//printf("%s - %lf -> %s\n",(states_nr.find(state_nr)->second).c_str(),prob,(states_nr.find(cols[i])->second).c_str());
					if(state_nr==cols[i]) {
						loop=true;
						//row.add(state_nr,-1.0+prob);
						row.add(ssp_nr.find(state_nr)->second,-1.0+prob);
					} else if(mec[cols[i]]){
						//mec_lra[mecNr[mec[cols[i]]-1]] += prob*lra[cols[i]];
						isMec[mecNr[cols[i]]-1]=true;
						mec_prob[mecNr[cols[i]]-1] += prob;
					} else {
						//row.add(cols[i],prob);
						row.add(ssp_nr.find(cols[i])->second,prob);
					}
				}
				for(unsigned long x=0; x<mecs->n; x++) {
					if(isMec[x])
						row.add(ssp_nr.find(ma->n+x+1)->second,mec_prob[x]);
				}
				mec_prob=mec_prob_tmp;
				isMec=isMec_tmp;
				if(!loop)
					//row.add(state_nr,-1.0);
					row.add(ssp_nr.find(state_nr)->second,-1.0);
				if(goal == 0){
					lp_model.addRow(LPRow(row,LPRow::Type(m), 0));
				} else {
					goal *= -1;
					lp_model.addRow(LPRow(row,LPRow::Type(m), goal));
				}
				row.~DSVector();
			}
		}
	}
	bool bad=false;
	// add transitions for descision states
	mec_prob=mec_prob_tmp;
	isMec=isMec_tmp;
	for(unsigned long m_nr=0; m_nr<mecs->n; m_nr++) {
		DSVector row(states);
		row.add(ssp_nr.find(ma->n+m_nr+1)->second,-1.0);
		row.add(ssp_nr.find(ma->n+m_nr+1+mecs->n)->second,1.0);
		lp_model.addRow(LPRow(row,LPRow::Type(m), 0));
		row.~DSVector();
		for (state_nr = 0; state_nr < ma->n; state_nr++) {
			if(mecNr[state_nr]==m_nr+1){
				unsigned long state_start = row_starts[state_nr];
				unsigned long state_end = row_starts[state_nr + 1];
				for (choice_nr = state_start; choice_nr < state_end; choice_nr++) {
					DSVector row(states);
					row.add(ssp_nr.find(ma->n+mecNr[state_nr])->second,-1.0);
					/* Add up all outgoing rates of the distribution */
					unsigned long i_start = choice_starts[choice_nr];
					unsigned long i_end = choice_starts[choice_nr + 1];
					for (i = i_start; i < i_end; i++) {
						if(mecNr[cols[i]]!=m_nr+1) {
							bad=true;
							prob=non_zeros[i];
							rate=0;
							unsigned long r_start = rate_starts[state_nr];
							unsigned long r_end = rate_starts[state_nr + 1];
							for (unsigned long j = r_start; j < r_end; j++) {
								prob /= exit_rates[j];
								rate = -1/exit_rates[j];
							}
							//printf("%s - %lf -> %s\n",(ma->states_nr.find(state_nr)->second).c_str(),prob,(ma->states_nr.find(cols[i])->second).c_str());
							isMec[mecNr[cols[i]]-1]=true;
							mec_prob[mecNr[cols[i]]-1] += prob;
							//row.add(mecNr[cols[i]],prob);
						}
					}
					for(unsigned long x=0; x<mecs->n; x++) {
						if(isMec[x])
							row.add(ssp_nr.find(ma->n+x+1+mecs->n)->second,mec_prob[x]);
					}
					mec_prob=mec_prob_tmp;
					isMec=isMec_tmp;
					if(bad) {
						lp_model.addRow(LPRow(row,LPRow::Type(m), 0));
					}
					row.~DSVector();
					bad=false;
				}
			}
		}
	}
	// add LRA costs for MEC states
	for(state_nr=0; state_nr<mecs->n; state_nr++) {
		DSVector row(states);
		row.add(ssp_nr.find(ma->n+state_nr+1+mecs->n)->second,-1.0);
		goal = lra_mec[state_nr];
		goal *= -1;
		lp_model.addRow(LPRow(row,LPRow::Type(m), goal));
		row.~DSVector();
	}
	
}

Real compute_stochastic_shortest_path_problem(SparseMatrix *ma, SparseMatrixMEC *mecs, vector<Real> lra_mec, bool max) {
	SoPlex lp_model;
	vector<bool> mec(ma->n,false);
	vector<Real> lra(ma->n,0);
	vector<Real> mecNr(ma->n,0);
	map<unsigned long,unsigned long> ssp_nr;
	
	unsigned long *row_starts = (unsigned long *) mecs->row_counts;
	unsigned long *cols = mecs->cols;
	
	for(unsigned long mec_nr=0; mec_nr < mecs->n; mec_nr++) {
		unsigned long mec_start = row_starts[mec_nr];
		unsigned long mec_end = row_starts[mec_nr + 1];
		for(unsigned long state_nr=mec_start; state_nr < mec_end; state_nr++) {
			mec[cols[state_nr]]=true;
			lra[cols[state_nr]]=lra_mec[mec_nr];
			mecNr[cols[state_nr]]=mec_nr+1;
		}
	}
	
	bool *initials = ma->initials;
	bool *bad=(bool *) malloc(ma->n * sizeof(bool));
	unsigned long state_nr;
	for (state_nr = 0; state_nr < ma->n; state_nr++) {
		bad[state_nr]=true;
		if(initials[state_nr])
			dbg_printf("test %s\n",(ma->states_nr.find(state_nr)->second).c_str());
		if(initials[state_nr] && mec[state_nr]){
			return lra[state_nr];
		}
		if(mec[state_nr])
			bad[state_nr]=true;
	}
	//printf("no init\n");
	
	bool *locks;
	if(max) {
		locks=compute_locks_strong(ma,bad);
	} else {
		locks=compute_locks_weak(ma,bad);
	}
	
	/* first step: build the lp model */
	dbg_printf("make obj fct\n");
	set_obj_function_ssp(lp_model,ma,mecs,max,mec,lra,locks,ssp_nr);
	dbg_printf("make constraints\n");
	set_constraints_ssp(lp_model,ma,mecs,max,mec,locks,lra,ssp_nr,mecNr,lra_mec);
	
	lp_model.writeFile("file.lp", NULL, NULL, NULL);
	// TODO: find out why LP model causes segfault in some cases (temorary BUGFIX: load model from file)
	lp_model.readFile("file.lp");
	
	/* solve the LP */
	SPxSolver::Status stat;
	dbg_printf("solve model\n");
	stat = lp_model.solve();
	
	/* find the max or min prob. for an initial state */
	Real obj;
	if(max)
		obj=0;
	else
		obj=1;
			
	/* show if optimal solution */
	if( stat == SPxSolver::OPTIMAL ) {
		printf("LP solved to optimality.\n\n");
		//printf("Objective value is %lf.\n",lp_model.objValue());
		//printf("before\n");
		DVector probs(lp_model.nCols());
		lp_model.getPrimal(probs);
		
		for (state_nr = 0; state_nr < ma->n; state_nr++) {
			if(initials[state_nr] && !mec[state_nr]){
				//cout << ssp_nr.find(state_nr)->second << endl;
				Real tmp = probs[ssp_nr.find(state_nr)->second];
				//cout << tmp << endl;
				if(max && tmp > obj)
					obj=tmp;
				else if(!max && tmp < obj)
					obj = tmp;
			}else if(initials[state_nr] && mec[state_nr]) {
				Real tmp = lra[state_nr];
				if(max && tmp > obj)
					obj=tmp;
				else if(!max && tmp < obj)
					obj = tmp;
			}
		}
		//printf("after\n");
	} else if ( stat == SPxSolver::INFEASIBLE) {
		fprintf(stderr, "LP is infeasible.\n\n");
	} else {
		obj = 0;
	}
		
	return obj;
}


/**
* Computes expected time for MA.
*
* @param ma file to read MA from
* @param max identifier for min or max
* @return lra
*/
Real compute_long_run_average(SparseMatrix *ma, bool max) {	
	bool *locks;
	if(max) {
		locks=compute_locks_strong(ma);
	} else {
		locks=compute_locks_weak(ma);
	}
	
	SparseMatrixMEC *mecs;
	if(max) {
		mecs=compute_maximal_end_components(ma);
	} else {
		//mecs=compute_bottom_strongly_connected_components(ma);
		mecs=compute_maximal_end_components(ma);
	}
	
	// printf("LP computation start.\n");
	vector<bool> mec(ma->n,false);
	vector<bool> mec_tmp(ma->n,false);
	vector<Real> lra_mec(mecs->n);
	
	unsigned long *row_starts = (unsigned long *) mecs->row_counts;
	unsigned long *cols = mecs->cols;
	
	
	/* TODO: Problem with LRA computation for some models! Need to be solved! */
	for(unsigned long mec_nr=0; mec_nr < mecs->n; mec_nr++) {
		unsigned long mec_start = row_starts[mec_nr];
		unsigned long mec_end = row_starts[mec_nr + 1];
		for(unsigned long state_nr=mec_start; state_nr < mec_end; state_nr++) {
			mec[cols[state_nr]]=true;
		}
		SoPlex lp_model;
		/* first step: build the lp model */
		dbg_printf("set obj\n");
		set_obj_function_lra(lp_model,ma,max,mec,locks);
		dbg_printf("set const\n");
		set_constraints_lra(lp_model,ma,max,mec,locks);
		if(max){
			lp_model.writeFile("filemax.lp", NULL, NULL, NULL);
			//lp_model.clear();
			// TODO: find out why LP model causes segfault in some cases (temorary BUGFIX: load model from file)
			//lp_model.readFile("filemax.lp");
		}else {
			lp_model.writeFile("filemin.lp", NULL, NULL, NULL);
			//lp_model.clear();
			// TODO: find out why LP model causes segfault in some cases (temorary BUGFIX: load model from file)
			//lp_model.readFile("filemin.lp");
		}
		dbg_printf("solve\n");
		/* solve the LP */
		SPxSolver::Status stat;
		lp_model.setDelta(1e-6);
		stat = lp_model.solve();
		dbg_printf("LRA Mec %ld: %.10lg\n",mec_nr+1,lp_model.objValue());
		lra_mec[mec_nr]=lp_model.objValue();
		mec=mec_tmp;
		//lp_model.clear();
		//lp_model.clearBasis();
		
		/* DEBUG OUTPUT */
		/*DVector probs(lp_model.nCols());
		lp_model.getPrimal(probs);
		unsigned long state_nr;
		for (state_nr = 0; state_nr < ma->n; state_nr++) {
			Real tmp = probs[state_nr];
			cout << state_nr << ": prob " << tmp << endl;
		}*/
		
	}
	
	dbg_printf("SSP\n");
	Real lra=0;
	lra = compute_stochastic_shortest_path_problem(ma,mecs,lra_mec,max);
	dbg_printf("SSP end\n");
	free(locks);
	free(mecs);

	return lra;
}