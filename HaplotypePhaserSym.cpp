
#include <math.h>
#include <string.h>
#include <omp.h>

#include "HaplotypePhaserSym.h"
#include "MemoryAllocators.h"




HaplotypePhaserSym::~HaplotypePhaserSym(){
	FreeCharMatrix(haplotypes, ped.count*2);
	//	FreeCharMatrix(genotypes, ped.count);
	delete [] phred_probs;
	FreeDoubleMatrix(s_forward, num_markers);
	FreeDoubleMatrix(s_backward, num_markers);

	//TODO need to delete ped?

}


void HaplotypePhaserSym::AllocateMemory(){
	String::caseSensitive = false;

	num_haps = (ped.count-1)*2;
	num_states = ((num_haps)*(num_haps+1)) / 2;

	for(int i = 0; i < num_haps; i++) {
		for(int j = i; j < num_haps; j++) {
			states.push_back(ChromosomePair(i,j));
		}
	}

	num_markers = Pedigree::markerCount;
	num_inds = ped.count;

	//	error = 0.01;
	//	theta = 0.01;

	distances.resize(num_markers,0.01);
	//	errors.resize(num_markers,0.01);

	phred_probs = new float[256];

	for(int i = 0; i < 256; i++){
		phred_probs[i] = pow(10.0, -i * 0.1);
	};

	haplotypes = AllocateCharMatrix(num_inds*2, num_markers);
	sample_gls.resize(num_markers*3);


	s_forward = AllocateDoubleMatrix(num_markers, num_states);
	s_backward = AllocateDoubleMatrix(num_markers, num_states);
	normalizers = new double[num_markers];


	//	s_forward2 = AllocateDoubleMatrix(num_markers, num_states);
	//	s_backward2 = AllocateDoubleMatrix(num_markers, num_states);
	//	normalizers2 = new double[num_markers];

};

void HaplotypePhaserSym::DeAllocateMemory(){
	FreeCharMatrix(haplotypes, ped.count*2);
};
void HaplotypePhaserSym::setDistanceCode(int c) {
	distance_code = c;
};

/**
 * Load vcf data from files.
 * Reference haplotypes are loaded from ref_file.
 * Unphased sample is loaded from sample_file, the individual with sample_index.
 *
 */
void HaplotypePhaserSym::LoadData(const String &ref_file, const String &sample_file, int sample_index, const String &map_file){
	VcfUtils::LoadReferenceMarkers(ref_file);
	VcfUtils::LoadIndividuals(ped, ref_file, sample_file, sample_index);
	AllocateMemory();
	VcfUtils::LoadHaplotypes(ref_file, ped, haplotypes);
	VcfUtils::LoadGenotypeLikelihoods(sample_file, ped, sample_gls, sample_index);
	VcfUtils::LoadGeneticMap(map_file, ped, distances);

};

/**
 * Load vcf data from files. Only reference data.
 *
 */
void HaplotypePhaserSym::LoadReferenceData(const String &ref_file, const String &sample_file, int sample_index){
	VcfUtils::LoadReferenceMarkers(ref_file);
	VcfUtils::LoadIndividuals(ped,ref_file, sample_file, sample_index);
	AllocateMemory();
	VcfUtils::LoadHaplotypes(ref_file, ped, haplotypes);
};

/**
 * Load vcf data from files. Only reference data.
 *
 */
void HaplotypePhaserSym::LoadSampleData(const String &ref_file, const String &sample_file, int sample_index){
	VcfUtils::LoadGenotypeLikelihoods(sample_file, ped, sample_gls, sample_index);
	VcfUtils::LoadGeneticMap("/home/kristiina/Projects/Data/1KGData/maps/chr20.OMNI.interpolated_genetic_map", ped, distances);
	//		VcfUtils::LoadGeneticMap("data/chr20.OMNI.interpolated_genetic_map", ped, distances);

};



/**
 * Get the emission probability of the observed genotype likelihoods at marker
 * forall hidden states
 *
 * P(GLs(marker)|s) for all s in 0...num_states-1
 *
 */

void HaplotypePhaserSym::CalcEmissionProbs(int marker, double * probs) {
	int h1;
	int h2;
	double sum;

	double case_1 = (pow(1 - error, 2) + pow(error, 2));
	double case_2 = 2 * (1 - error) * error;
	double case_3 = pow(1 - error, 2);
	double case_4 = (1 - error) * error;
	double case_5 = pow(error,2);



	// OPT1
	for (int state = 0; state < num_states; state++) {
		//	for (ChromosomePair state  : states) {

		ChromosomePair chrom_state = states[state];

		// First reference hapotype at state (0: REF 1: ALT)
		h1 = haplotypes[chrom_state.first][marker];
		// Second reference hapotype at state (0: REF 1: ALT)
		h2 = haplotypes[chrom_state.second][marker];

		sum = 0.0;
		// case1: g = 0

		if(h1 == 0 and h2 == 0){
			sum += case_3 *  sample_gls[marker * 3];
			//				printf("adding = %f \n", pow(1 - errors[marker], 2) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3]]);
		}
		else {
			if((h1 == 0 and h2 == 1) or (h1 == 1 and h2 == 0)){

				sum += case_4 * sample_gls[marker * 3];
				//						printf("adding = %f \n", (1 - errors[marker]) * error * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3]]);

			}
			else{
				sum +=  case_5 *  sample_gls[marker * 3];
				//						printf("adding = %f \n", errors[marker] * 2 * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3]]);

			}
		}

		// case2: g = 1
		if((h1 == 0 and h2 == 1) or (h1 == 1 and h2 == 0)){
			sum += case_1 *  sample_gls[marker * 3 + 1];
			//				printf("adding1 = %f \n", (pow(1 - errors[marker], 2) + pow(errors[marker], 2)) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 1]]);

		}
		else{
			sum +=case_2 * sample_gls[marker * 3 + 1];
			//				printf("adding2 = %f \n", 2 * (1 - errors[marker]) * errors[marker] * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 1]]);

		}

		// case3: g = 2
		if(h1 == 1 and h2 == 1){
			sum += case_3 * sample_gls[marker * 3 + 2];
			//				printf("adding = %f \n", pow(1 - errors[marker], 2) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 2]]);

		} else{
			if((h1 == 0 and h2 == 1) or (h1 == 1 and h2 == 0)){
				sum += case_4 * sample_gls[marker * 3 + 2];
				//						printf("adding = %f \n", (1 - errors[marker]) * error * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 2]]);

			}
			else{
				sum += case_5 * sample_gls[marker * 3 + 2];
				//						printf("adding = %f \n", pow(errors[marker],2) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 2]]);

			}
		}
		probs[state] = sum;
	}

	//	// ORIGINAL
	//	for (int state = 0; state < num_states; state++) {
	//
	//		h1 = haplotypes[state / num_h][marker];
	//		h2 = haplotypes[state % num_h][marker];
	//
	//		sum = 0.0;
	//
	//
	//		//	printf(" GetEmissionProb: state = %d marker = %d \n", state, marker);
	//		//	int ph1 = (unsigned char) genotypes[num_inds-1][marker * 3];
	//		//	int ph2 = (unsigned char) genotypes[num_inds-1][marker * 3 + 1];
	//		//	int ph3 = (unsigned char) genotypes[num_inds-1][marker * 3 + 2];
	//		//
	//		//	printf("indices : %d %d %d \n", ph1, ph2, ph3);
	//
	//
	//		// case1: g = 0
	//		if(h1 == 0 and h2 == 0){
	//			sum += pow(1 - errors[marker], 2) *  sample_gls[marker * 3];
	//			//				printf("adding = %f \n", pow(1 - errors[marker], 2) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3]]);
	//		}
	//		else {
	//			if((h1 == 0 and h2 == 1) or (h1 == 1 and h2 == 0)){
	//
	//				sum += (1 - errors[marker]) * error * sample_gls[marker * 3];
	//				//						printf("adding = %f \n", (1 - errors[marker]) * error * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3]]);
	//
	//			}
	//			else{
	//				sum +=  pow(errors[marker],2) *  sample_gls[marker * 3];
	//				//						printf("adding = %f \n", errors[marker] * 2 * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3]]);
	//
	//			}
	//		}
	//
	//		// case2: g = 1
	//		if((h1 == 0 and h2 == 1) or (h1 == 1 and h2 == 0)){
	//			sum += (pow(1 - errors[marker], 2) + pow(errors[marker], 2)) *  sample_gls[marker * 3 + 1];
	//			//				printf("adding1 = %f \n", (pow(1 - errors[marker], 2) + pow(errors[marker], 2)) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 1]]);
	//
	//		}
	//		else{
	//			sum += 2 * (1 - errors[marker]) * errors[marker] * sample_gls[marker * 3 + 1];
	//			//				printf("adding2 = %f \n", 2 * (1 - errors[marker]) * errors[marker] * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 1]]);
	//
	//		}
	//
	//		// case3: g = 2
	//		if(h1 == 1 and h2 == 1){
	//			sum += pow(1 - errors[marker], 2) * sample_gls[marker * 3 + 2];
	//			//				printf("adding = %f \n", pow(1 - errors[marker], 2) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 2]]);
	//
	//		} else{
	//			if((h1 == 0 and h2 == 1) or (h1 == 1 and h2 == 0)){
	//				sum += (1 - errors[marker]) * error * sample_gls[marker * 3 + 2];
	//				//						printf("adding = %f \n", (1 - errors[marker]) * error * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 2]]);
	//
	//			}
	//			else{
	//				sum += pow(errors[marker],2) * sample_gls[marker * 3 + 2];
	//				//						printf("adding = %f \n", pow(errors[marker],2) * phred_probs[(unsigned char) genotypes[num_inds-1][marker * 3 + 2]]);
	//
	//			}
	//		}
	//		probs[state] = sum;
	//	}
	//
	//



}

/**
 * Calculate transition probabilities.
 * Array probs is given values:
 *
 * P(S_marker = marker_state | S_marker-1 = x) for x in {0...num_states-1} for marker_state in {0...num_states-1}
 *
 * <=>
 *
 * P(S_marker = x | S_marker-1 = marker_state) for x in {0...num_states-1} for marker_state in {0...num_states-1}
 *
 *
 *
 */
//void HaplotypePhaserSym::CalcTransitionProbs(int marker, double ** probs){
//	int num_h = 2*num_inds - 2;
//
//	double scaled_dist;
//
//		if(distance_code == 1) {
//			scaled_dist = 1-exp(-distances[marker]);
//		}
//
//		if(distance_code == 2) {
//			scaled_dist = 1-exp(-distances[marker]/num_h);
//		}
//
//		if(distance_code == 3) {
//			scaled_dist = 1-exp(-(distances[marker]*4*11000)/num_h);
//		}
//
//		if(distance_code == 4) {
//			scaled_dist = 0.01;
//		}
//
//		if(distance_code == 5) {
//			scaled_dist = 1-exp(-distances[marker]/(num_h*100));
//		}
//
//		if(distance_code == 6) {
//	scaled_dist = 1-exp(-(distances[marker]*4*11418)/(num_h*100));
//		}
//
//
//	// OPT 2
//
//	double tprobs[3];
//
//	//both_switch
//	tprobs[0] = pow(scaled_dist/num_h, 2);
//
//	//one switch
//	tprobs[1] =  (((1 - scaled_dist) * scaled_dist) / num_h) + pow(scaled_dist/num_h, 2);
//
//	// no switch
//	tprobs[2] = pow(1 - scaled_dist, 2) + ((2 * (1 - scaled_dist) * scaled_dist) / num_h) + (pow(scaled_dist,2) / pow(num_h,2));
//
//	//		double no_switch = pow(1 - scaled_dist, 2) + ((2 * (1 - scaled_dist) * scaled_dist) / num_h) + (pow(scaled_dist,2) / pow(num_h,2));
//	//		double both_switch = pow(scaled_dist/num_h, 2);
//	//		double one_switch = (((1 - scaled_dist) * scaled_dist) / num_h) + pow(scaled_dist/num_h, 2);
//
//#pragma omp parallel for
//	for(int marker_state = 0; marker_state < num_states; marker_state++) {
//
//		int marker_c1 = marker_state / num_h;
//		int marker_c2 = marker_state % num_h;
//
//#pragma GCC ivdep
//		for(int i = 0; i < num_states; i++){
//
//			int other_c1 = i / num_h;
//			int other_c2 = i % num_h;
//
//
//			int b = marker_c2-other_c2;
//			int a = marker_c1-other_c1;
//
//
//			probs[marker_state][i] = tprobs[!a+!b];
//
////			int b = (marker_state-i+num_h) % num_h;
////			int a = (marker_state-i-b) / num_h;
////			if(other_c1 - marker_c1 == 0 and other_c2 - marker_c2 == 0) {
////				if(!a+!b != 2 ) {
////					printf("NOT 2: % d and %d \n", marker_state, i);
////				}			}
////			else {
////				// both switch
////				if(other_c1-marker_c1 != 0 and other_c2 - marker_c2 != 0) {
////					if(!a != 0) {
////						printf("NOT 0: % d and %d : %d = %d + %d    ( %d , %d) ( %d , %d) a = %d b = %d\n", marker_state, i,!a+!b, !a, !b,marker_c1, marker_c2, other_c1, other_c2, a, b);
////					}
////				}
////				// one switch
////				else{
////					if(!a+!b != 1 ) {
////						printf("NOT 1 top = %d : % d and %d : %d = %d + %d    ( %d , %d) ( %d , %d) a = %d b = %d\n",marker_state-i-b, marker_state, i,!a+!b, !a, !b,marker_c1, marker_c2, other_c1, other_c2, a, b);
////					}
////				}
////			}
//
//
//
//		}
//	}
//
//	// ORIGINAL
//	//#pragma omp parallel for
//	//	for(int marker_state = 0; marker_state < num_states; marker_state++) {
//	//
//	//		int marker_c1 = marker_state / num_h;
//	//		int marker_c2 = marker_state % num_h;
//	//
//	//#pragma GCC ivdep
//	//		for(int i = 0; i < num_states; i++){
//	//			int other_c1 = i / num_h;
//	//			int other_c2 = i % num_h;
//	//
//	//			//no switch
//	//			if(other_c1 - marker_c1 == 0 and other_c2 - marker_c2 == 0) {
//	//				probs[marker_state][i] = pow(1 - scaled_dist, 2) + ((2 * (1 - scaled_dist) * scaled_dist) / num_h) + (pow(scaled_dist,2) / pow(num_h,2));
//	//			}
//	//			else {
//	//				// both switch
//	//				if(other_c1-marker_c1 != 0 and other_c2 - marker_c2 != 0) {
//	//					probs[marker_state][i] = pow(scaled_dist/num_h, 2);
//	//
//	//				}
//	//				// one switch
//	//				else{
//	//					probs[marker_state][i] = (((1 - scaled_dist) * scaled_dist) / num_h) + pow(scaled_dist/num_h, 2);
//	//
//	//				}
//	//			}
//	//		}
//	//	}
//
//}



void HaplotypePhaserSym::InitPriorScaledForward(){
	double * emission_probs = new double[num_states];
	double prior = 1.0 / num_states;
	double c1 = 0.0;


	CalcEmissionProbs(0, emission_probs);

	for(int s = 0; s < num_states; s++){
		c1 += emission_probs[s];
	};

	normalizers[0] = 1.0/(prior*c1);
	//	printf("First Normalizer = %e Prior = %e c1 = %e \n", normalizers[0], prior, c1);

	for(int s = 0; s < num_states; s++){
		s_forward[0][s] = (prior*emission_probs[s]) * normalizers[0];
	};

	delete [] emission_probs;
};

void HaplotypePhaserSym::InitPriorScaledBackward(){

	for(int s = 0; s < num_states; s++){
		s_backward[num_markers-1][s] = normalizers[num_markers-1];
	};
};


void HaplotypePhaserSym::CalcScaledForward(){
	double * emission_probs = new double[num_states];
	int num_h = 2 * num_inds - 2;
	double c;
	double probs[3];
	double probs_diff[3];
	double probs_same[3];

	double scaled_dist;
	double c1,c2;

//	printf("Num states = %d \n", num_states);

	// original
	double pop_const = (4.0 * Ne) / 100.0;

	//	// times 1.5
	//	double pop_const = (4.0 * 11418.0 * 1.5) / 100.0;


	// div by 1.5
	//	double pop_const = (4.0 * 11418.0) / 150.0;

	// times 2.0
	//	double pop_const = (4.0 * Ne * 2.0) / 100.0;


	InitPriorScaledForward();

	for(int m = 1; m < num_markers; m++){
		CalcEmissionProbs(m, emission_probs);
		c = 0.0;

		scaled_dist = 1-exp(-(distances[m] * pop_const)/num_h);

		c1 = scaled_dist/num_h;
		c2 = 1 - scaled_dist;

		//both_switch
		probs[0] = pow(c1, 2);
		//one switch
		probs[1] =  (c2*c1) + pow(c1, 2);
		// no switch
		probs[2] = pow(c2, 2) + (2*c2*c1) + pow(c1, 2);


		double norm_const_case_diff = ((num_h-1) * (num_h-2) /2)*(probs[0]) + (2*(num_h-1)) * probs[1] + probs[2];
		double norm_const_case_same = (num_h*(num_h-1)/2) * (probs[0]) + (num_h-1) * probs[1] + probs[2];

		probs_diff[0] = probs[0] / norm_const_case_diff;
		//one switch
		probs_diff[1] =  probs[1] / norm_const_case_diff;
		// no switch
		probs_diff[2] = probs[2] / norm_const_case_diff;


		probs_same[0] = probs[0] / norm_const_case_same;
		//one switch
		probs_same[1] =  probs[1] / norm_const_case_same;
		// no switch
		probs_same[2] = probs[2] / norm_const_case_same;



#pragma omp parallel for schedule(dynamic,32)
		for(int s = 0; s < num_states; s++){

			double sum = 0.0;
			//			int marker_c1 = s / num_h;
			//			int marker_c2 = s % num_h;


			//			int case_counter[3];
			//			case_counter[0] = 0;
			//			case_counter[1] = 0;
			//			case_counter[2] = 0;

#pragma GCC ivdep
			for(int j = 0; j < num_states; j++){

				int chrom_case = (states[s]).NumEquals2(states[j]);


				// original haplotyper sym
				//				sum += s_forward[m-1][j] * probs[chrom_case];


				// haplotyper_sym pdf - decide which pdf we are using
				if(states[j].first == states[j].second) {
					sum += s_forward[m-1][j] * probs_same[chrom_case];
				}
				else{
					sum += s_forward[m-1][j] * probs_diff[chrom_case];
				}

				//				case_counter[chrom_case] += 1;

				// prob here is p_trans j -> s
				// conditioning on j
				//				if (j==100) {
				//					if(states[j].first == states[j].second) {
				//						prob_test += probs_same[chrom_case];
				//					}
				//					else{
				//						prob_test += probs_diff[chrom_case];
				//					}
				//				}

			}
			s_forward[m][s] =  emission_probs[s] * sum;
			//			if(s<=40 && m==65) {
			//			printf("s= %d : (%d,%d) case counts: 0:%d 1:%d 2:%d \n", s, states[s].first, states[s].second,case_counter[0], case_counter[1], case_counter[2]);
			//
			//			}
		}

		//				if(abs(prob_test-1.0) > 0.0001 && m < 100) {
		//					printf(" m = % d !! Probtest = %f !! \n", m, prob_test);
		//				}

		for(int s = 0; s < num_states; s++){
			c+= s_forward[m][s];
		}
		normalizers[m] = 1.0/c;

		for(int s = 0; s < num_states; s++){
			s_forward[m][s] = s_forward[m][s] * normalizers[m];
		}
	}

	delete [] emission_probs;

}

void HaplotypePhaserSym::CalcScaledBackward(){
	double * emission_probs = new double[num_states];
	int num_h = 2*num_inds - 2;
	double probs[3];
	double probs_diff[3];
	double probs_same[3];
	double scaled_dist;
	double c1;
	double c2;

	//original
	double pop_const = (4.0 * Ne) / 100.0;

	// times 1.5
	//	double pop_const = (4.0 * 11418.0 * 1.5) / 100.0;

	// times 2.0
	//	double pop_const = (4.0 * Ne * 2.0) / 100.0;


	InitPriorScaledBackward();

	for(int m = num_markers-2; m >= 0; m--){
		scaled_dist = 1.0 - exp(-(distances[m+1] * pop_const)/num_h);

		c1 = scaled_dist/num_h;
		c2 = 1 - scaled_dist;

		//both_switch
		probs[0] = pow(c1, 2);
		//one switch
		probs[1] =  (c2*c1) + pow(c1, 2);
		// no switch
		probs[2] = pow(c2, 2) + (2*c2*c1) + pow(c1, 2);


		double norm_const_case_diff = ((num_h-1) * (num_h-2) /2)*(probs[0]) + (2*(num_h-1)) * probs[1] + probs[2];
		double norm_const_case_same = (num_h*(num_h-1)/2) * (probs[0]) + (num_h-1) * probs[1] + probs[2];



		probs_diff[0] = probs[0] / norm_const_case_diff;
		//one switch
		probs_diff[1] =  probs[1] / norm_const_case_diff;
		// no switch
		probs_diff[2] = probs[2] / norm_const_case_diff;


		probs_same[0] = probs[0] / norm_const_case_same;
		//one switch
		probs_same[1] =  probs[1] / norm_const_case_same;
		// no switch
		probs_same[2] = probs[2] / norm_const_case_same;
		CalcEmissionProbs(m+1, emission_probs);

#pragma omp parallel for
		for(int s = 0; s < num_states; s++){
			double sum = 0.0;
			//			int marker_c1 = s / num_h;
			//			int marker_c2 = s % num_h;

#pragma GCC ivdep
			for(int j = 0; j < num_states; j++){

				int chrom_case = (states[s]).NumEquals(states[j]);

				// haplotyper sym orig
				//sum += s_backward[m+1][j] * probs[chrom_case] * emission_probs[j];

				// haplotyper_sym pdf - decide which pdf we are using
				if(states[s].first == states[s].second) {
					sum += s_backward[m+1][j] * probs_same[chrom_case] * emission_probs[j];
				}
				else{
					sum += s_backward[m+1][j] * probs_diff[chrom_case] * emission_probs[j];
				}

			}
			s_backward[m][s] = sum * normalizers[m];
		}
	}
	delete [] emission_probs;
}


// ORIGINAL
//void HaplotypePhaserSym::CalcScaledForward(){
//
//	double * emission_probs = new double[num_states];
//	double ** transition_probs = AllocateDoubleMatrix(num_states, num_states);
//
//	double c;
//	double c2;
//
//
//	InitPriorScaledForward();
//
//	for(int m = 1; m < num_markers; m++){
//		CalcEmissionProbs(m, emission_probs);
//		CalcTransitionProbs(m, transition_probs);
//		c = 0.0;
//		c2 = 0.0;
//#pragma omp parallel for schedule(dynamic,32)
//		for(int s = 0; s < num_states; s++){
//			double sum = 0.0;
//			//			double sum2 = 0.0;
//#pragma GCC ivdep
//			for(int j = 0; j < num_states; j++){
//				sum += s_forward[m-1][j] * transition_probs[s][j];
//				//				sum2 += s_forward2[m-1][j] * transition_probs[s][j];
//
//			}
//			s_forward[m][s] =  emission_probs[s] * sum;
//			//			s_forward2[m][s] =  emission_probs[s] * sum2;
//
//			//			c += emission_probs[s]*sum;
//
//
//		}
//
//		for(int s = 0; s < num_states; s++){
//			c+= s_forward[m][s];
//		}
//
//		//#pragma omp parallel for ordered reduction(+:c2)
//		//		for(int s = 0; s < num_states; s++){
//		//			c2+= s_forward[m][s];
//		//		}
//		//
//		//		if(c != c2) {
//		//			printf("DIFF C %e at %d \n", abs(c-c2), m);
//		//		}
//
//
//		//		normalizers[m] = c;
//		//		normalizers2[m] = 1.0/c2;
//		normalizers[m] = 1.0/c;
//
//
//
//		for(int s = 0; s < num_states; s++){
//			//			s_forward[m][s] = s_forward[m][s] / normalizers[m];
//			//			s_forward2[m][s] = s_forward2[m][s] * normalizers2[m];
//			s_forward[m][s] = s_forward[m][s] * normalizers[m];
//
//
//		}
//	}
//
//	FreeDoubleMatrix(transition_probs, num_states);
//
//	delete [] emission_probs;
//
//}
//
//
//
//


//// ORIGINAL
//void HaplotypePhaserSym::CalcScaledBackward(){
//	double * emission_probs = new double[num_states];
//	//	double * transition_probs = new double[num_states];
//	double ** transition_probs = AllocateDoubleMatrix(num_states, num_states);
//
//
//	InitPriorScaledBackward();
//
//
//	for(int m = num_markers-2; m >= 0; m--){
//
//		CalcEmissionProbs(m+1, emission_probs);
//		CalcTransitionProbs(m+1, transition_probs);
//#pragma omp parallel for
//		for(int s = 0; s < num_states; s++){
//			double sum = 0.0;
//			//			double sum2 = 0.0;
//
//#pragma GCC ivdep
//			for(int i = 0; i < num_states; i++){
//
//				sum += s_backward[m+1][i] * transition_probs[s][i] * emission_probs[i];
//				//				sum2 += s_backward2[m+1][i] * transition_probs[s][i] * emission_probs[i];
//
//			}
//			//			s_backward[m][s] = sum / normalizers[m];
//			//			s_backward2[m][s] = sum2 * normalizers2[m];
//			s_backward[m][s] = sum * normalizers[m];
//
//
//		}
//	}
//
//	FreeDoubleMatrix(transition_probs, num_states);
//	delete [] emission_probs;
//}



/**
 * For every marker m, get the state with highest posterior probability at that location, given the entire observation sequence
 * (not most likely sequence of states)
 *
 * Calculate array ml_states s.t.
 * ml_states[m] = s_i that maximises P(Q_m = s_i | O_1 ... O_num_markers)
 *
 * for m in {0 ... num_markers-1}
 *
 */
void HaplotypePhaserSym::GetMLHaplotypes(int * ml_states){


#pragma omp parallel for
	for(int m = 0; m < num_markers; m++) {
		double posterior;
		double max_posterior = 0.0;
		int ml_state = -1;
		double norm = 0.0;
#pragma GCC ivdep
		for(int i = 0; i < num_states; i++) {
			norm += s_forward[m][i] * s_backward[m][i];
		}
#pragma GCC ivdep
		for(int s = 0; s < num_states; s++) {
			posterior = s_forward[m][s] * s_backward[m][s] / norm;

			if(posterior > max_posterior) {
				ml_state = s;
				max_posterior = posterior;
			}
		}
		ml_states[m] = ml_state;

	}
}



/**
 * For every marker m, get the state with highest posterior probability at that location, given the entire observation sequence
 * (not most likely sequence of states)
 *
 * Calculate array ml_states s.t.
 * ml_states[m] = s_i that maximises P(Q_m = s_i | O_1 ... O_num_markers)
 *
 * for m in {0 ... num_markers-1}
 *
 */
vector<vector<double>>  HaplotypePhaserSym::GetPosteriorStats(const char * filename){
	int num_h = 2*num_inds - 2;
	vector<vector<double>> stats;
	vector<vector<double>> geno_probs;


	for(int m = 0; m < num_markers; m++) {
		stats.push_back({});
		stats[m].resize(44,-1.0);
	}

	for(int m = 0; m < num_markers; m++) {
		geno_probs.push_back({});
		geno_probs[m].resize(3,0.0);
	}


	for(int m = 0; m < num_markers; m++) {
		vector<double> posteriors;
		posteriors.resize(num_states, -1);

		double norm = 0.0;
		for(int i = 0; i < num_states; i++) {
			norm += s_forward[m][i] * s_backward[m][i];
		}

		double sum = 0.0;
		for(int s = 0; s < num_states; s++) {
			posteriors[s] = s_forward[m][s] * s_backward[m][s] / norm;
			sum += posteriors[s];


			//////////genotype probability/////////////////
			int ref_hap1 = states[s].first;
			int ref_hap2 = states[s].second;


			// AGCT allele
			//			String allele1 = Pedigree::GetMarkerInfo(m)->GetAlleleLabel(haplotypes[ref_hap1][m]+1);
			//			String allele2 = Pedigree::GetMarkerInfo(m)->GetAlleleLabel(haplotypes[ref_hap2][m]+1);

			// 00, 01, 10, 11
			int hapcode1 = haplotypes[ref_hap1][m];
			int hapcode2 = haplotypes[ref_hap2][m];

			int geno_code;

			if(hapcode1 != hapcode2) {
				geno_code = 1;
			}
			else {
				geno_code = (hapcode1 == 0) ? 0 : 2;
			}

			geno_probs[m][geno_code] += posteriors[s];

		}

		float check_sum = 0.0;
		for(int i = 0; i < 3 ; i++) {
			check_sum += geno_probs[m][i];
		}
		if(abs(check_sum - 1.0) > 0.000001 ) {
			printf("!!!!!!!!!!!!!!!!!!!!!!!!!Sum of all geno probs is %f at marker %d !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1\n ", check_sum, m);
		}


		vector<size_t> res = sort_indexes(posteriors);

		//add lowest elements to stats[m]
		for(int i = 0; i < 10; i++) {
			stats[m][i] = posteriors[res[i]];
		}
		for(int i = 0; i < 10; i++) {
			stats[m][10 + i] = res[i];
		}
		for(int i = 0; i < 10; i++) {
			stats[m][20 + i] = posteriors[res[posteriors.size() - 10 + i]];
		}
		for(int i = 0; i < 10; i++) {
			stats[m][30 + i] = res[posteriors.size() - 10 + i];
		}

		stats[m][40] = sum / posteriors.size();
		stats[m][41] = geno_probs[m][0];
		stats[m][42] = geno_probs[m][1];
		stats[m][43] = geno_probs[m][2];


	}
	writeVectorToCSV(filename, stats, "w");
	return stats;
}

/**
 * Read stats from filename
 */
vector<vector<double>>  HaplotypePhaserSym::ReadPosteriorStats(const char * filename){

	vector<vector<double>> stats;


	FILE * statstream = fopen(filename, "r");

	printf("Opened statsream for %s \n", filename);
	float low1;
	float low2;
	float low3;
	float low4;
	float low5;
	float low6;
	float low7;
	float low8;
	float low9;
	float low10;

	float low1_i;
	float low2_i;
	float low3_i;
	float low4_i;
	float low5_i;
	float low6_i;
	float low7_i;
	float low8_i;
	float low9_i;
	float low10_i;

	float high1;
	float high2;
	float high3;
	float high4;
	float high5;
	float high6;
	float high7;
	float high8;
	float high9;
	float high10;

	float high1_i;
	float high2_i;
	float high3_i;
	float high4_i;
	float high5_i;
	float high6_i;
	float high7_i;
	float high8_i;
	float high9_i;
	float high10_i;

	float average;
	int result;


	for(int m = 0; m < num_markers; m++) {
		stats.push_back({});
		stats[m].resize(41,-1.0);
	}

	for(int m = 0; m < num_markers; m++) {
		printf("scanning for marker = %d \n", m);
		result = fscanf(statstream, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,\n",
				&low1,&low2,&low3,&low4,&low5,&low6,&low7,&low8,&low9,&low10,&low1_i,&low2_i,&low3_i,&low4_i,&low5_i,&low6_i,&low7_i,&low8_i,&low9_i,&low10_i,
				&high1,&high2,&high3,&high4,&high5,&high6,&high7,&high8,&high9,&high10,&high1_i,&high2_i,&high3_i,&high4_i,&high5_i,&high6_i,&high7_i,&high8_i,&high9_i,&high10_i,&average);


		printf("result = %d \n", result);

		printf("scanned for marker = %d \n", m);
		stats[m][0] = low1;
		stats[m][1] = low2;
		stats[m][2] = low3;
		stats[m][3] = low4;
		stats[m][4] = low5;
		stats[m][5] = low6;
		stats[m][6] = low7;
		stats[m][7] = low8;
		stats[m][8] = low9;
		stats[m][9] = low10;

		stats[m][10] = low1_i;
		stats[m][11] = low2_i;
		stats[m][12] = low3_i;
		stats[m][13] = low4_i;
		stats[m][14] = low5_i;
		stats[m][15] = low6_i;
		stats[m][16] = low7_i;
		stats[m][17] = low8_i;
		stats[m][18] = low9_i;
		stats[m][19] = low10_i;

		stats[m][20] = high1;
		stats[m][21] = high2;
		stats[m][22] = high3;
		stats[m][23] = high4;
		stats[m][24] = high5;
		stats[m][25] = high6;
		stats[m][26] = high7;
		stats[m][27] = high8;
		stats[m][28] = high9;
		stats[m][29] = high10;

		stats[m][30] = high1_i;
		stats[m][31] = high2_i;
		stats[m][32] = high3_i;
		stats[m][33] = high4_i;
		stats[m][34] = high5_i;
		stats[m][35] = high6_i;
		stats[m][36] = high7_i;
		stats[m][37] = high8_i;
		stats[m][38] = high9_i;
		stats[m][39] = high10_i;

		stats[m][40] = average;


	}
	return stats;
}

/**
 * Translate genotype probabilities to most likely genotypes (represented as haplotype pair).
 *
 * Haplotypes printed to out_file and returned.
 *
 */
HaplotypePair HaplotypePhaserSym::PrintGenotypesToFile(vector<vector<double>> & stats, const char * out_file){
	std::vector<String> h1;
	std::vector<String> h2;

	//hapcodes 00, 10, 11  represent geno_codes 0,1,2
	int hapcode1;
	int hapcode2;
	int max_geno_code;
	float max_geno_prob;

	for(int m = 0; m < num_markers; m++) {

		max_geno_prob = 0.0;
		for(int i=0; i<3;i++) {
			if(stats[m][41+i] > max_geno_prob) {
				max_geno_prob = stats[m][41+i];
				max_geno_code = i;
			}
		}

		if(max_geno_code == 0) {
			hapcode1 = 0;
			hapcode2 = 0;
		}
		if(max_geno_code == 1) {
			hapcode1 = 0;
			hapcode2 = 1;
		}
		if(max_geno_code == 2) {
			hapcode1 = 1;
			hapcode2 = 1;
		}

		h1.push_back(Pedigree::GetMarkerInfo(m)->GetAlleleLabel(hapcode1+1));
		h2.push_back(Pedigree::GetMarkerInfo(m)->GetAlleleLabel(hapcode2+1));

	}

	HaplotypePair hp(h1,h2);
	hp.printToFile(out_file);
	return hp;
	//	Print to file
}



/**
 * Translate maximum likelihood states to haplotypes.
 *
 * Haplotypes printed to out_file and returned.
 *
 */
HaplotypePair HaplotypePhaserSym::PrintHaplotypesToFile(int * ml_states, const char * out_file){
	std::vector<String> h1;
	std::vector<String> h2;

	int ref_hap1;
	int ref_hap2;
	int prev_ref_hap1;
	int prev_ref_hap2;




	// First marker. order does not matter
	ref_hap1 = states[ml_states[0]].first;
	ref_hap2 = states[ml_states[0]].second;

	h1.push_back(Pedigree::GetMarkerInfo(0)->GetAlleleLabel(haplotypes[ref_hap1][0]+1));
	h2.push_back(Pedigree::GetMarkerInfo(0)->GetAlleleLabel(haplotypes[ref_hap2][0]+1));

	prev_ref_hap1 = ref_hap1;
	prev_ref_hap2 = ref_hap2;

	for(int m = 1; m < num_markers; m++) {


		int first = states[ml_states[m]].first;
		int second = states[ml_states[m]].second;

		// No switch
		if(states[ml_states[m]].NumEquals(states[ml_states[m-1]]) == 2) {
			ref_hap1 = prev_ref_hap1;
			ref_hap2 = prev_ref_hap2;
		}
		else {
			// One switch
			if(states[ml_states[m]].NumEquals(states[ml_states[m-1]]) == 1) {

				if(first == prev_ref_hap1) {
					ref_hap1 = first;
					ref_hap2 = second;

				}
				else {
					if(first == prev_ref_hap2) {
						ref_hap1 = second;
						ref_hap2 = first;

					}
					else {
						if(second == prev_ref_hap1) {
							ref_hap1 = second;
							ref_hap2 = first;

						}
						// here we know s == prev_ref_hap2
						else {
							ref_hap1 = first;
							ref_hap2 = second;

						}
					}
				}
			}
			// if we have two switches then we can use the original order - should not matter?
			else {
				ref_hap1 = first;
				ref_hap2 = second;
			}

		}

		prev_ref_hap1 = ref_hap1;
		prev_ref_hap2 = ref_hap2;

		h1.push_back(Pedigree::GetMarkerInfo(m)->GetAlleleLabel(haplotypes[ref_hap1][m]+1));
		h2.push_back(Pedigree::GetMarkerInfo(m)->GetAlleleLabel(haplotypes[ref_hap2][m]+1));
	}

	HaplotypePair hp(h1,h2);
	hp.printToFile(out_file);
	return hp;
	//	Print to file
}


/**
 * Translate maximum likelihood states to haplotypes.
 *
 * Haplotypes returned.
 * Reference haplotypes at each position printed to stdout.
 *
 */
HaplotypePair HaplotypePhaserSym::PrintReferenceHaplotypes(int * ml_states, const char * out_file){


	std::vector<String> h1;
	std::vector<String> h2;

	std::vector<int> ref1;
	std::vector<int> ref2;


	int num_h = 2*num_inds - 2;
	int ref_hap1;
	int ref_hap2;
	int prev_ref_hap1;
	int prev_ref_hap2;


	std::vector<char> codes;

	for(int i = 65; i < 65+num_h; i++) {
		codes.push_back(i);
	}

	ref_hap1 = states[ml_states[0]].first;
	ref_hap2 = states[ml_states[0]].second;

	h1.push_back(Pedigree::GetMarkerInfo(0)->GetAlleleLabel(haplotypes[ref_hap1][0]+1));
	h2.push_back(Pedigree::GetMarkerInfo(0)->GetAlleleLabel(haplotypes[ref_hap2][0]+1));

	prev_ref_hap1 = ref_hap1;
	prev_ref_hap2 = ref_hap2;
	ref1.push_back(ref_hap1);
	ref2.push_back(ref_hap2);
	for(int m = 1; m < num_markers; m++) {

		int first = states[ml_states[m]].first;
		int second = states[ml_states[m]].second;

		// No switch
		if(states[ml_states[m]].NumEquals(states[ml_states[m-1]]) == 2) {
			ref_hap1 = prev_ref_hap1;
			ref_hap2 = prev_ref_hap2;
		}
		else {
			// One switch
			if(states[ml_states[m]].NumEquals(states[ml_states[m-1]]) == 1) {

				if(first == prev_ref_hap1) {
					ref_hap1 = first;
					ref_hap2 = second;

				}
				else {
					if(first == prev_ref_hap2) {
						ref_hap1 = second;
						ref_hap2 = first;

					}
					else {
						if(second == prev_ref_hap1) {
							ref_hap1 = second;
							ref_hap2 = first;

						}
						// here we know s == prev_ref_hap2
						else {
							ref_hap1 = first;
							ref_hap2 = second;

						}
					}
				}
			}
			// if we have two switches then we can use the original order - should not matter?
			else {
				ref_hap1 = first;
				ref_hap2 = second;
			}

		}
		prev_ref_hap1 = ref_hap1;
		prev_ref_hap2 = ref_hap2;

		ref1.push_back(ref_hap1);
		ref2.push_back(ref_hap2);

		h1.push_back(Pedigree::GetMarkerInfo(m)->GetAlleleLabel(haplotypes[ref_hap1][m]+1));
		h2.push_back(Pedigree::GetMarkerInfo(m)->GetAlleleLabel(haplotypes[ref_hap2][m]+1));

	}


	FILE * hapout = fopen(out_file, "w");

	for(auto h : ref1) {
		fprintf(hapout,"%d\n",h);
	}
	putc('\n', hapout);
	for(auto h : ref2) {
		fprintf(hapout,"%d\n",h);
	}

	fclose(hapout);


	//	for(auto h : ref1) {
	//		putc(codes[h], hapout);
	//	}
	//	putc('\n', hapout);
	//	for(auto h : ref2) {
	//		putc(codes[h], hapout);
	//	}
	//
	//	fclose(hapout);


	//	for(auto h : ref1){
	//		printf("%c",codes[h]);
	//	}
	//	printf("\n");
	//
	//	for(auto h : ref2){
	//		printf("%c",codes[h]);
	//	}
	//	printf("\n");

	HaplotypePair hp(h1,h2);
	return hp;
	//	Print to file
}


