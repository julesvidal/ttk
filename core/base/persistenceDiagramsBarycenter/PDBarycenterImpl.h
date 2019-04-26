#ifndef _PDBARYCENTERIMPL_H
#define _PDBARYCENTERIMPL_H

#define BLocalMax ttk::CriticalType::Local_maximum
#define BLocalMin ttk::CriticalType::Local_minimum
#define BSaddle1  ttk::CriticalType::Saddle1
#define BSaddle2  ttk::CriticalType::Saddle2

#include <stdlib.h>     /* srand, rand */
#include <cmath>
#include          <BottleneckDistance.h>

using namespace ttk;

template <typename dataType>
std::vector<std::vector<matchingTuple>> PDBarycenter<dataType>::execute(std::vector<diagramTuple>& barycenter){
	if(method_ == "Munkres")
		return executeMunkresBarycenter(barycenter);
	else if(method_ == "Auction")
		return executeAuctionBarycenter(barycenter);
	else if(method_ =="Partial Bidding")
		return executePartialBiddingBarycenter(barycenter);
	else{
		std::cout << "[PersistenceDiagramsBarycenter] Error : you need to select a valid matching algorithm" << '\n';
		std::vector<std::vector<matchingTuple>> v(0);
		return v;
	}
}


template <typename dataType>
void PDBarycenter<dataType>::runMatching(dataType* total_cost,
										dataType epsilon,
										std::vector<int> sizes,
										KDTree<dataType>* kdt,
										std::vector<KDTree<dataType>*>* correspondance_kdt_map,
										std::vector<dataType>* min_diag_price,
										std::vector<dataType>* min_price,
										std::vector<std::vector<matchingTuple>>* all_matchings,
										bool use_kdt
										){
	#ifdef TTK_ENABLE_OPENMP
	omp_set_num_threads(threadNumber_);
    //std::cout<<"\n\n number of threads : "<<threadNumber_<<" \n\n"<<std::endl;
	#pragma omp parallel for schedule(dynamic, 1)
	#endif
	for(int i=0; i<numberOfInputs_; i++){
		Auction<dataType> auction = Auction<dataType>(&current_bidder_diagrams_[i], &barycenter_goods_[i], wasserstein_, geometrical_factor_, lambda_, 0.01, kdt, *correspondance_kdt_map, epsilon, (*min_diag_price)[i], use_kdt);
		int n_biddings = 0;
		auction.buildUnassignedBidders();
		auction.reinitializeGoods();
		auction.runAuctionRound(n_biddings, i);
		auction.updateDiagonalPrices();

		min_diag_price->at(i) = auction.getMinimalDiagonalPrice();
		min_price->at(i) = getMinimalPrice(i);
        std::vector<matchingTuple> matchings;
		dataType cost = auction.getMatchingsAndDistance(&matchings, true);
		all_matchings->at(i) = matchings;
		(*total_cost) += cost*cost;
		
		if(cost*cost <= ((1.01*1.01)*(cost*cost - epsilon*auction.getAugmentedNumberOfBidders()))){
		    precision_objective_[i]=true;
		}
        else{
            precision_objective_[i]=false;
        }

		//std::cout<< "Barycenter cost for diagram " << i <<" : "<< cost << std::endl;
		//std::cout<< "Number of biddings : " << n_biddings << std::endl;
		// Resizes the diagram which was enrich with diagonal bidders during the auction
		// TODO do this inside the auction !
		current_bidder_diagrams_[i].bidders_.resize(sizes[i]);
	}
	/* print matchings
	 	for(long unsigned int i=0; i<(*all_matchings).size(); i++){
	 	    for (long unsigned int j = 0; j < (*all_matchings)[i].size(); j++) {
                std::cout << get<0>((*all_matchings)[i][j]) << " " ;
                std::cout << get<1>((*all_matchings)[i][j]) << " " ;
                std::cout << get<2>((*all_matchings)[i][j]) << "  |   " ;
	 	    }
            std::cout << "\n" ;
	 	}*/
}

template <typename dataType>
void PDBarycenter<dataType>::runMatching(dataType* total_cost,
										dataType epsilon,
										std::vector<int> sizes,
										KDTree<dataType>* kdt,
										std::vector<KDTree<dataType>*>* correspondance_kdt_map,
										std::vector<dataType>* min_diag_price,
										std::vector<dataType>* min_price,
										std::vector<std::vector<matchingTuple>>* all_matchings,
										bool use_kdt,
										vector<float> *timers,
										vector<int> *NB_BIDDERS,
										vector<int> *NB_BIDDINGS
										){
	#ifdef TTK_ENABLE_OPENMP
	omp_set_num_threads(threadNumber_);
    //std::cout<<"\n\n number of threads : "<<threadNumber_<<" \n\n"<<std::endl;
	#pragma omp parallel for schedule(dynamic, 1)
	#endif
	for(int i=0; i<numberOfInputs_; i++){
	    Timer t;
		Auction<dataType> auction = Auction<dataType>(&current_bidder_diagrams_[i], &barycenter_goods_[i], wasserstein_, geometrical_factor_, lambda_, 0.01, kdt, *correspondance_kdt_map, epsilon, (*min_diag_price)[i], use_kdt);
		int n_biddings = 0;
		auction.buildUnassignedBidders();
		auction.reinitializeGoods();
		auction.runAuctionRound(n_biddings, i);
		auction.updateDiagonalPrices();

		min_diag_price->at(i) = auction.getMinimalDiagonalPrice();
		min_price->at(i) = getMinimalPrice(i);
		std::vector<matchingTuple> matchings;
		dataType cost = auction.getMatchingsAndDistance(&matchings, true);
		all_matchings->at(i) = matchings;
		(*total_cost) += cost*cost;
		//std::cout<< "Barycenter cost for diagram " << i <<" : "<< cost << std::endl;
		//std::cout<< "Number of biddings : " << n_biddings << std::endl;
		// Resizes the diagram which was enrich with diagonal bidders during the auction
		// TODO do this inside the auction !
		(*NB_BIDDINGS)[i] = n_biddings;
		current_bidder_diagrams_[i].bidders_.resize(sizes[i]);
		(*NB_BIDDERS)[i] = current_bidder_diagrams_[i].bidders_.size();
		(*timers)[i]=t.getElapsedTime();
	}
	/* print matchings
	 	for(long unsigned int i=0; i<(*all_matchings).size(); i++){
	 	    for (long unsigned int j = 0; j < (*all_matchings)[i].size(); j++) {
                std::cout << get<0>((*all_matchings)[i][j]) << " " ;
                std::cout << get<1>((*all_matchings)[i][j]) << " " ;
                std::cout << get<2>((*all_matchings)[i][j]) << "  |   " ;
	 	    }
            std::cout << "\n" ;
	 	}*/
}

template <typename dataType>
void PDBarycenter<dataType>::runMatchingMunkres(dataType* total_cost,
										 std::vector<std::vector<matchingTuple>>* all_matchings,
										 std::vector<diagramTuple>& barycenter
									 	 ){
		#ifdef TTK_ENABLE_OPENMP
	 	omp_set_num_threads(threadNumber_);
	 	#pragma omp parallel for schedule(dynamic, 1)
	 	#endif
	 	for(int i=0; i<numberOfInputs_; i++){
			BottleneckDistance bottleneckDistance = BottleneckDistance();
			//bottleneckDistance.setMethod(1); //Munkres algorithm
			bottleneckDistance.setCTDiagram2(&barycenter);
			bottleneckDistance.setCTDiagram1(&((*inputDiagrams_)[i]));
			bottleneckDistance.setAlgorithm("ttk");
			bottleneckDistance.setPersistencePercentThreshold(0.0);
			bottleneckDistance.setPX(0.);
			bottleneckDistance.setPY(0.);
			bottleneckDistance.setPZ(0.);
			bottleneckDistance.setPE(1.);
			bottleneckDistance.setPS(1.);
			bottleneckDistance.setWasserstein(std::to_string(wasserstein_));
			bottleneckDistance.setOutputMatchings(&((*all_matchings)[i]));

			bottleneckDistance.execute<dataType>(1.);

			dataType cost = bottleneckDistance.getDistance();
            //std::cout << "cost of matching " << i <<" : "<<cost<<std::endl;
	 		(*total_cost) += cost;
            //std::cout << "now total : " << *total_cost<<std::endl;
	 	}
	 	/* to print matchings
	 	for(long unsigned int i=0; i<(*all_matchings).size(); i++){
	 	    for (long unsigned int j = 0; j < (*all_matchings)[i].size(); j++) {
                std::cout << get<0>((*all_matchings)[i][j]) << " " ;
                std::cout << get<1>((*all_matchings)[i][j]) << " " ;
                std::cout << get<2>((*all_matchings)[i][j]) << "  |   " ;
	 	    }
            std::cout << "\n" ;
	 	}
        std::cout<< std::endl;
        */
}

template <typename dataType>
void PDBarycenter<dataType>::runMatchingAuction(dataType* total_cost,
										std::vector<int> sizes,
										KDTree<dataType>* kdt,
										std::vector<KDTree<dataType>*>* correspondance_kdt_map,
										std::vector<dataType>* min_diag_price,
										std::vector<std::vector<matchingTuple>>* all_matchings,
										bool use_kdt
										){
	#ifdef TTK_ENABLE_OPENMP
	omp_set_num_threads(threadNumber_);
	#pragma omp parallel for schedule(dynamic, 1)
	#endif
	for(int i=0; i<numberOfInputs_; i++){
		Auction<dataType> auction = Auction<dataType>(&current_bidder_diagrams_[i], &barycenter_goods_[i],
			 wasserstein_, geometrical_factor_, lambda_, 0.01, kdt, *correspondance_kdt_map, (*min_diag_price)[i], use_kdt);
		std::vector<matchingTuple> matchings;
		dataType cost = auction.run(&matchings);
		all_matchings->at(i) = matchings;
        //std::cout << "cost of matching " << i <<" : "<<cost<<std::endl;
        (*total_cost) += cost*cost;
        //std::cout << "now total : " << *total_cost<<std::endl;
		
		//std::cout<< "Barycenter cost for diagram " << i <<" : "<< cost << std::endl;
		//std::cout<< "Number of biddings : " << n_biddings << std::endl;
		// Resizes the diagram which was enrich with diagonal bidders during the auction
		// TODO do this inside the auction !
		current_bidder_diagrams_[i].bidders_.resize(sizes[i]);
	}
	/* to print matchings 	
	for(long unsigned int i=0; i<(*all_matchings).size(); i++){
	 	    for (long unsigned int j = 0; j < (*all_matchings)[i].size(); j++) {
                std::cout << get<0>((*all_matchings)[i][j]) << " " ;
                std::cout << get<1>((*all_matchings)[i][j]) << " " ;
                std::cout << get<2>((*all_matchings)[i][j]) << "  |   " ;
	 	    }
            std::cout << "\n" ;
	 	}
	*/
}


template <typename dataType>
bool PDBarycenter<dataType>::hasBarycenterConverged(std::vector<std::vector<matchingTuple>>& matchings, std::vector<std::vector<matchingTuple>>& previous_matchings){
	if(points_added_>0 || points_deleted_>0 || previous_matchings.size()==0){
		return false;
	}

	for(unsigned int j=0; j<matchings.size(); j++){
		for(unsigned int i=0; i<matchings[j].size(); i++){
			matchingTuple t = matchings[j][i];
			matchingTuple previous_t = previous_matchings[j][i];

			if(std::get<1>(t) != std::get<1>(previous_t) && (std::get<0>(t)>=0 && std::get<0>(previous_t)>=0)){
				return false;
			}
		}
	}
	return true;
}


template <typename dataType>
std::vector<std::vector<matchingTuple>> PDBarycenter<dataType>::correctMatchings(std::vector<std::vector<matchingTuple>> previous_matchings){
	std::vector<std::vector<matchingTuple>> corrected_matchings(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; i++){
		// 1. Invert the current_bidder_ids_ vector
		std::vector<int> new_to_old_id(current_bidder_diagrams_[i].size());
		for(unsigned int j=0; j<current_bidder_ids_[i].size(); j++){
			int new_id = current_bidder_ids_[i][j];
			if(new_id>=0){
				new_to_old_id[new_id] = j;
			}
		}
		// 2. Reconstruct the matchings
		std::vector<matchingTuple> matchings_diagram_i;
		for(unsigned int j=0; j<previous_matchings[i].size(); j++){
			matchingTuple m = previous_matchings[i][j];
			int new_id = std::get<0>(m);
			if(new_id >=0 && std::get<1>(m)>=0){
				std::get<0>(m) = new_to_old_id[new_id];
				matchings_diagram_i.push_back(m);
			}
		}
		corrected_matchings[i] = matchings_diagram_i;
	}
	return corrected_matchings;
}


template <typename dataType>
dataType PDBarycenter<dataType>::updateBarycenter(std::vector<std::vector<matchingTuple>>& matchings){
	// 1. Initialize variables used in the sequel
	Timer t_update;
	unsigned int n_goods = barycenter_goods_[0].size();

	unsigned int n_diagrams = current_bidder_diagrams_.size();
	points_added_ = 0;
	points_deleted_ = 0;
	dataType max_shift = 0;

	std::vector<unsigned int> count_diag_matchings(n_goods);     // Number of diagonal matchings for each point of the barycenter
	std::vector<dataType> x(n_goods);
	std::vector<dataType> y(n_goods);
	std::vector<dataType> crit_coords_x(n_goods);
	std::vector<dataType> crit_coords_y(n_goods);
	std::vector<dataType> crit_coords_z(n_goods);
	for(unsigned int i=0; i<n_goods; i++){
		count_diag_matchings[i] = 0;
		x[i] = 0;
		y[i] = 0;
		crit_coords_x[i] = 0;
		crit_coords_y[i] = 0;
		crit_coords_z[i] = 0;
	}
	std::vector<dataType> min_prices(n_diagrams);
	for(unsigned int j=0; j<n_diagrams; j++){
		min_prices[j] = std::numeric_limits<dataType>::max();
	}

	std::vector<Bidder<dataType>*> points_to_append;  //Will collect bidders linked to diagonal
	// 2. Preprocess the matchings
	for(unsigned int j=0; j<matchings.size(); j++){
		for(unsigned int i=0; i<matchings[j].size(); i++){
			int bidder_id = std::get<0>(matchings[j][i]);
			int good_id = std::get<1>(matchings[j][i]);
			if(good_id<0 && bidder_id>=0){
				// Future new barycenter point
				points_to_append.push_back(&current_bidder_diagrams_[j].get(bidder_id));
			}

			else if(good_id>=0 && bidder_id>=0){
				// Update coordinates (to be divided by the number of diagrams later on)
				x[good_id] += current_bidder_diagrams_[j].get(bidder_id).x_;
				y[good_id] += current_bidder_diagrams_[j].get(bidder_id).y_;
				if(geometrical_factor_<1){
					std::tuple<float, float, float> critical_coordinates = current_bidder_diagrams_[j].get(bidder_id).GetCriticalCoordinates();
					crit_coords_x[good_id] += std::get<0>(critical_coordinates);
					crit_coords_y[good_id] += std::get<1>(critical_coordinates);
					crit_coords_z[good_id] += std::get<2>(critical_coordinates);
				}
			}
			else if(good_id>=0 && bidder_id<0){
				// Counting the number of times this barycenter point is linked to the diagonal
				count_diag_matchings[good_id] = count_diag_matchings[good_id] + 1;
			}
		}
	}

	// 3. Update the previous points of the barycenter
	for(unsigned int i=0; i<n_goods; i++){
		if(count_diag_matchings[i]<n_diagrams){
			// Barycenter point i is matched at least to one off-diagonal bidder
			// 3.1 Compute the arithmetic mean of off-diagonal bidders linked to it
			dataType x_bar = x[i]/(double)(n_diagrams - count_diag_matchings[i]);
			dataType y_bar = y[i]/(double)(n_diagrams - count_diag_matchings[i]);
			// 3.2 Compute the new coordinates of the point (the more linked to the diagonal it was, the closer to the diagonal it'll be)
			dataType new_x = ( (double)(n_diagrams - count_diag_matchings[i])*x_bar  + (double)count_diag_matchings[i]*(x_bar+y_bar)/2. )/(double)n_diagrams;
			dataType new_y = ( (double)(n_diagrams - count_diag_matchings[i])*y_bar  + (double)count_diag_matchings[i]*(x_bar+y_bar)/2. )/(double)n_diagrams;
			//TODO Weight by persistence
			dataType new_crit_coord_x = crit_coords_x[i]/(double)(n_diagrams - count_diag_matchings[i]);
			dataType new_crit_coord_y = crit_coords_y[i]/(double)(n_diagrams - count_diag_matchings[i]);
			dataType new_crit_coord_z = crit_coords_z[i]/(double)(n_diagrams - count_diag_matchings[i]);

			// 3.3 Compute and store how much the point has shifted
			// TODO adjust shift with geometrical_factor_
			dataType dx = barycenter_goods_[0].get(i).x_ - new_x;
			dataType dy = barycenter_goods_[0].get(i).y_ - new_y;
			dataType shift = pow(abs(dx), wasserstein_) + pow(abs(dy), wasserstein_);
			if(shift>max_shift){
				max_shift = shift;
			}
			// 3.4 Update the position of the point
			for(unsigned int j=0; j<n_diagrams; j++){
				barycenter_goods_[j].get(i).SetCoordinates(new_x, new_y);
				if(geometrical_factor_<1){
					barycenter_goods_[j].get(i).SetCriticalCoordinates(new_crit_coord_x, new_crit_coord_y, new_crit_coord_z);
				}
				if(barycenter_goods_[j].get(i).getPrice()<min_prices[j]){
					min_prices[j] = barycenter_goods_[j].get(i).getPrice();
				}
			}
			// TODO Reinitialize/play with prices here if you wish
		}
	}
	for(unsigned int j=0; j<n_diagrams; j++){
		if(min_prices[j]>=std::numeric_limits<dataType>::max()/2.){
			min_prices[j] = 0;
		}
	}

	// 4. Delete off-diagonal barycenter points not linked to any
	// off-diagonal bidder
	for(unsigned int i=0; i<n_goods; i++){
		if(count_diag_matchings[i] == n_diagrams){
			points_deleted_ += 1;
			dataType shift = 2*pow(barycenter_goods_[0].get(i).getPersistence() / 2., wasserstein_);
			if(shift>max_shift){
				max_shift = shift;
			}
			for(unsigned int j=0; j<n_diagrams; j++){
				barycenter_goods_[j].get(i).id_ = -1;
			}
		}
	}

	// 5. Append the new points to the barycenter
	for(unsigned int k=0; k<points_to_append.size(); k++){
		points_added_ += 1;
		Bidder<dataType>* b = points_to_append[k];
		dataType x = (b->x_ + (n_diagrams -1)*(b->x_+b->y_)/2.)/(n_diagrams);
		dataType y = (b->y_ + (n_diagrams -1)*(b->x_+b->y_)/2.)/(n_diagrams);
		std::tuple<dataType, dataType, dataType> critical_coordinates = b->GetCriticalCoordinates();
		for(unsigned int j=0; j<n_diagrams; j++){
			Good<dataType> g = Good<dataType>(x, y, false, barycenter_goods_[j].size());
			g.setPrice(min_prices[j]);
			if(geometrical_factor_<1){
				g.SetCriticalCoordinates( std::get<0>(critical_coordinates), std::get<1>(critical_coordinates), std::get<2>(critical_coordinates) );
			}
			barycenter_goods_[j].addGood(g);
			dataType shift = 2*pow(barycenter_goods_[j].get(g.id_).getPersistence() / 2., wasserstein_);
			if(shift>max_shift){
				max_shift = shift;
			}
		}
	}

	// 6. Finally, recreate barycenter_goods
	for(unsigned int j=0; j<n_diagrams; j++){
		int count = 0;
		GoodDiagram<dataType> new_barycenter;
		for(int i=0; i<barycenter_goods_[j].size(); i++){
			Good<dataType> g = barycenter_goods_[j].get(i).copy();
			if(g.id_!=-1){
				g.id_ = count;
				new_barycenter.addGood(g);
				count ++;
			}
		}
		barycenter_goods_[j] = new_barycenter;
	}
	if(debugLevel_>2)
		std::cout << "Points deleted : " << points_deleted_ << " Points added : " << points_added_ << std::endl;

    // std::cout<<"TIME OF UPDATE : "<< t_update.getElapsedTime()<<std::endl;
	return max_shift;
}



template <typename dataType>
dataType PDBarycenter<dataType>::getEpsilon(dataType rho){
	return pow(rho, 2)/8.0;
}

template <typename dataType>
dataType PDBarycenter<dataType>::getRho(dataType epsilon){
	return std::sqrt(8.0*epsilon);
}



template <typename dataType>
void PDBarycenter<dataType>::setBidderDiagrams(){

	for(int i=0; i<numberOfInputs_; i++){
		std::vector<diagramTuple> *CTDiagram = &((*inputDiagrams_)[i]);

    BidderDiagram<dataType> bidders;
		for(unsigned int j=0; j<CTDiagram->size(); j++){
			//Add bidder to bidders
			Bidder<dataType> b((*CTDiagram)[j], j, lambda_);

			b.setPositionInAuction(bidders.size());
			bidders.addBidder(b);
			if(b.isDiagonal() || b.x_==b.y_){
				std::cout<<"Diagonal point in diagram !!!"<<std::endl;
			}
		}
		bidder_diagrams_.push_back(bidders);
		current_bidder_diagrams_.push_back(BidderDiagram<dataType>());
		std::vector<int> ids(bidders.size());
		for(unsigned int j=0; j<ids.size(); j++){
			ids[j] = -1;
		}
		current_bidder_ids_.push_back(ids);
	}
	return;
}


template <typename dataType>
dataType PDBarycenter<dataType>::enrichCurrentBidderDiagrams(dataType previous_min_persistence,
                                                             dataType min_persistence, 
                                                             std::vector<dataType> initial_diagonal_prices, 
                                                             std::vector<dataType> initial_off_diagonal_prices, 
                                                             int min_points_to_add, 
                                                             bool add_points_to_barycenter){

  dataType new_min_persistence = min_persistence;

  // 1. Get size of the largest current diagram, deduce the maximal number of
  // points to append
	int max_diagram_size=0;
	for(int i=0; i<numberOfInputs_; i++){
		if(current_bidder_diagrams_[i].size()>max_diagram_size){
			max_diagram_size = current_bidder_diagrams_[i].size();
		}
	}
	int max_points_to_add = std::max(min_points_to_add, min_points_to_add + (int) (max_diagram_size/10));

	// 2. Get which points can be added, deduce the new minimal persistence
	std::vector<std::vector<int>> candidates_to_be_added(numberOfInputs_);
	std::vector<std::vector<int>> idx(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; i++){

		std::vector<dataType> persistences;
		for(int j=0; j<bidder_diagrams_[i].size(); j++){
			Bidder<dataType> b = bidder_diagrams_[i].get(j);
			dataType persistence = b.getPersistence();
			if(persistence>=min_persistence && persistence<previous_min_persistence){
				candidates_to_be_added[i].push_back(j);
				idx[i].push_back(idx[i].size());
				persistences.push_back(persistence);
			}
		}
		sort(idx[i].begin(), idx[i].end(), [&persistences](int& a, int& b){
      return ((persistences[a] > persistences[b])
      ||((persistences[a] == persistences[b])&&(a > b)));
    });
		int size =  candidates_to_be_added[i].size();
		if(size>=max_points_to_add){
			dataType last_persistence_added = persistences[idx[i][max_points_to_add-1]];
			if(last_persistence_added>new_min_persistence){
				new_min_persistence = last_persistence_added;
			}
		}
	}

	// 3. Add the points to the current diagrams
	
	// only to give determinism
	int compteur_for_adding_points = 0;

	for(int i=0; i<numberOfInputs_; i++){
		int size =  candidates_to_be_added[i].size();
		for(int j=0; j<std::min(max_points_to_add, size); j++){
			Bidder<dataType> b = bidder_diagrams_[i].get(candidates_to_be_added[i][idx[i][j]]);
			if(b.getPersistence()>=new_min_persistence){
				b.id_ = current_bidder_diagrams_[i].size();
				b.setPositionInAuction(current_bidder_diagrams_[i].size());
				b.setDiagonalPrice(initial_diagonal_prices[i]);
				current_bidder_diagrams_[i].addBidder(b);
				// b.id_ --> position of b in current_bidder_diagrams_[i]
				current_bidder_ids_[i][candidates_to_be_added[i][idx[i][j]]] = current_bidder_diagrams_[i].size()-1;

				int to_be_added_to_barycenter = deterministic_ ?  compteur_for_adding_points % numberOfInputs_ : rand() % numberOfInputs_;
				// We add the bidder as a good with probability 1/n_diagrams
				if(to_be_added_to_barycenter==0 && add_points_to_barycenter){
					for(int k=0; k<numberOfInputs_; k++){
						Good<dataType> g = Good<dataType>(b.x_, b.y_, false, barycenter_goods_[k].size());
						g.setPrice(initial_off_diagonal_prices[k]);
						g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
						barycenter_goods_[k].addGood(g);

					}
				}
			}
		    compteur_for_adding_points++;
		}
		if(debugLevel_>6)
		std::cout<< " Diagram " << i << " size : " << current_bidder_diagrams_[i].size() << std::endl;
	}
	return new_min_persistence;
}



template <typename dataType>
dataType PDBarycenter<dataType>::getMaxPersistence(){
	dataType max_persistence = 0;
	for(int i=0; i<numberOfInputs_; i++){
		BidderDiagram<dataType>& D = bidder_diagrams_[i];
		for(int j=0; j<D.size(); j++){
			//Add bidder to bidders
			Bidder<dataType>& b = D.get(j);
			dataType persistence = b.getPersistence();
			if(persistence>max_persistence){
				max_persistence = persistence;
			}
		}
	}
	return max_persistence;
}


template <typename dataType>
dataType PDBarycenter<dataType>::getMinimalPrice(int i){
	dataType min_price = std::numeric_limits<dataType>::max();

	GoodDiagram<dataType>& D = barycenter_goods_[i];
	if(D.size() ==0){
		return 0;
	}
	for(int j=0; j<D.size(); j++){
		Good<dataType>& b = D.get(j);
		dataType price = b.getPrice();
		if(price<min_price){
			min_price = price;
		}
	}
	if(min_price>=std::numeric_limits<dataType>::max()/2.){
		return 0;
	}
	return min_price;
}


template <typename dataType>
dataType PDBarycenter<dataType>::getLowestPersistence(){
	dataType lowest_persistence = std::numeric_limits<dataType>::max();
	for(int i=0; i<numberOfInputs_; i++){
		BidderDiagram<dataType>& D = bidder_diagrams_[i];
		for(int j=0; j<D.size(); j++){
			//Add bidder to bidders
			Bidder<dataType>& b = D.get(j);
			dataType persistence = b.getPersistence();
			if(persistence<lowest_persistence && persistence>0){
				lowest_persistence = persistence;
			}
		}
	}
	if(lowest_persistence>=std::numeric_limits<dataType>::max()/2.){
		return 0;
	}
	return lowest_persistence;
}


template <typename dataType>
void PDBarycenter<dataType>::setInitialBarycenter(dataType min_persistence){
	int size =0;
	int random_idx;
	std::vector<diagramTuple>* CTDiagram;
	int iter=0;
	while(size==0){
		random_idx = deterministic_ ? iter%numberOfInputs_ : rand() % numberOfInputs_ ;
		CTDiagram = &((*inputDiagrams_)[random_idx]);
		size = CTDiagram->size();
		for(int i=0; i<numberOfInputs_; i++){
			GoodDiagram<dataType> goods;
			int count=0;
			for(unsigned int j=0; j<CTDiagram->size(); j++){
				//Add bidder to bidders
				Good<dataType> g = Good<dataType>((*CTDiagram)[j], count, lambda_);
				if(g.getPersistence()>=min_persistence){
					goods.addGood(g);
					count ++;
				}
			}
			if(barycenter_goods_.size()<(unsigned int)(i+1)){
				barycenter_goods_.push_back(goods);
			}
			else{
				barycenter_goods_[i] = goods;
			}
		}
		size = barycenter_goods_[0].size();
		iter++;
	}
}


template <typename dataType>
std::pair<KDTree<dataType>*, std::vector<KDTree<dataType>*>> PDBarycenter<dataType>::getKDTree(){
	Timer t;
	KDTree<dataType>* kdt = new KDTree<dataType>(true, wasserstein_);

	const int dimension = geometrical_factor_ >= 1 ? 2 : 5;

	std::vector<dataType> coordinates;
	std::vector<std::vector<dataType>> weights;

	for(int i=0; i<barycenter_goods_[0].size(); i++){
		Good<dataType>& g = barycenter_goods_[0].get(i);
		coordinates.push_back(geometrical_factor_*g.x_);
		coordinates.push_back(geometrical_factor_*g.y_);
		if(geometrical_factor_<1){
			coordinates.push_back((1-geometrical_factor_)*g.coords_x_);
			coordinates.push_back((1-geometrical_factor_)*g.coords_y_);
			coordinates.push_back((1-geometrical_factor_)*g.coords_z_);
		}
	}

	for(unsigned int idx=0; idx<barycenter_goods_.size(); idx++){
		std::vector<dataType> empty_weights;
		weights.push_back(empty_weights);
		for(int i=0; i<barycenter_goods_[idx].size(); i++){
			Good<dataType>& g = barycenter_goods_[idx].get(i);
			weights[idx].push_back(g.getPrice());
		}
	}
	// Correspondance map : position in barycenter_goods_ --> KDT node

	std::vector<KDTree<dataType>*> correspondance_kdt_map = kdt->build(coordinates.data(), barycenter_goods_[0].size(), dimension, weights, barycenter_goods_.size());
	if(debugLevel_>3)
		std::cout<<"[Building KD-Tree] Time elapsed : " << t.getElapsedTime() << " s."<<std::endl;
	return std::make_pair(kdt, correspondance_kdt_map);
}

template<typename dataType>
std::vector<std::vector<matchingTuple>> PDBarycenter<dataType>::executeMunkresBarycenter(std::vector<diagramTuple>& barycenter){
	double total_time = 0;

	std::vector<std::vector<matchingTuple>> previous_matchings;
	dataType min_persistence = 0;
	dataType min_cost = std::numeric_limits<dataType>::max();
	int last_min_cost_obtained = 0;

	this->setBidderDiagrams();
	this->setInitialBarycenter(min_persistence); // false for a determinist initialization

		dataType max_persistence = getMaxPersistence();

	std::vector<dataType> min_diag_price(numberOfInputs_);
	std::vector<dataType> min_price(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; i++){
		min_diag_price[i] = 0;
		min_price[i] = 0;
	}

	int min_points_to_add = std::numeric_limits<int>::max();
	min_persistence = this->enrichCurrentBidderDiagrams(2*max_persistence, min_persistence, min_diag_price, min_price, min_points_to_add, false);

	if(debugLevel_>1)
		std::cout<< "Barycenter size : "<< barycenter_goods_[0].size() << std::endl;

	int n_iterations = 0;

	bool converged = false;
	bool finished = false;
	dataType total_cost;

	while(!finished){
        Timer t;

		n_iterations += 1;

		std::vector<std::vector<matchingTuple>> all_matchings(numberOfInputs_);
		std::vector<int> sizes(numberOfInputs_);
		for(int i=0; i<numberOfInputs_; i++){
			sizes[i] = current_bidder_diagrams_[i].size();
		}
		total_cost = 0;

		barycenter.resize(0);
		for(int j=0; j<barycenter_goods_[0].size(); j++){
			Good<dataType>& g = barycenter_goods_[0].get(j);
			diagramTuple t = std::make_tuple(0, nt1_, 0, nt2_, g.getPersistence(), diagramType_, g.x_, 0,0,0, g.y_, 0,0,0);
			barycenter.push_back(t);
		}

		runMatchingMunkres(&total_cost, &all_matchings, barycenter);

		std::cout<< "[PersistenceDiagramsBarycenter] Barycenter cost : "<< total_cost << std::endl;

		if(converged){
			finished = true;
		}

		if(!finished){
			updateBarycenter(all_matchings);
			if(debugLevel_>1)
				std::cout<< "Barycenter size : "<< barycenter_goods_[0].size() << std::endl;



			if(min_cost>total_cost){
				min_cost = total_cost;
				last_min_cost_obtained = 0;
			}
			else{
				last_min_cost_obtained += 1;
			}

			converged = converged || last_min_cost_obtained>1;
		}


		previous_matchings = std::move(all_matchings);
		// END OF TIMER
		total_time += t.getElapsedTime();
	}
	barycenter.resize(0);
	for(int j=0; j<barycenter_goods_[0].size(); j++){
		Good<dataType>& g = barycenter_goods_[0].get(j);
		diagramTuple t = std::make_tuple(0, nt1_, 0, nt2_, g.getPersistence(), diagramType_, g.x_, 0,0,0, g.y_, 0,0,0);
		barycenter.push_back(t);
	}

    cost_ = total_cost;
	std::vector<std::vector<matchingTuple>> corrected_matchings = correctMatchings(previous_matchings);
	for(unsigned int d=0; d<current_bidder_diagrams_.size(); ++d){
		if(debugLevel_>1)
			std::cout << "Size of diagram " << d << " : " << current_bidder_diagrams_[d].size() << std::endl;
	}
	return corrected_matchings;
}

template<typename dataType>
std::vector<std::vector<matchingTuple>> PDBarycenter<dataType>::executeAuctionBarycenter(std::vector<diagramTuple>& barycenter){
	double total_time = 0;

	std::vector<std::vector<matchingTuple>> previous_matchings;
	dataType min_persistence = 0;
	dataType min_cost = std::numeric_limits<dataType>::max();
	int last_min_cost_obtained = 0;

	this->setBidderDiagrams();
	this->setInitialBarycenter(min_persistence); // false for a determinist initialization

		dataType max_persistence = getMaxPersistence();

	std::vector<dataType> min_diag_price(numberOfInputs_);
	std::vector<dataType> min_price(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; i++){
		min_diag_price[i] = 0;
		min_price[i] = 0;
	}

	int min_points_to_add = std::numeric_limits<int>::max();
	min_persistence = this->enrichCurrentBidderDiagrams(2*max_persistence, min_persistence, min_diag_price, min_price, min_points_to_add, false);

	if(debugLevel_>1)
		std::cout<< "Barycenter size : "<< barycenter_goods_[0].size() << std::endl;

	int n_iterations = 0;

	bool converged = false;
	bool finished = false;
	dataType total_cost;

	while(!finished){
		Timer t;

		n_iterations += 1;

		std::pair<KDTree<dataType>*, std::vector<KDTree<dataType>*>> pair;
		bool use_kdt = false;
		// If the barycenter is empty, do not compute the kdt (or it will crash :/)
		// TODO Fix KDTree to handle empty inputs...
		if(barycenter_goods_[0].size()>0){
			pair = this->getKDTree();
			use_kdt = true;
		}
		KDTree<dataType>* kdt = pair.first;
		std::vector<KDTree<dataType>*>& correspondance_kdt_map = pair.second;

		std::vector<std::vector<matchingTuple>> all_matchings(numberOfInputs_);
		std::vector<int> sizes(numberOfInputs_);
		for(int i=0; i<numberOfInputs_; i++){
			sizes[i] = current_bidder_diagrams_[i].size();
		}
			
		total_cost = 0;

		barycenter.resize(0);
		for(int j=0; j<barycenter_goods_[0].size(); j++){
			Good<dataType>& g = barycenter_goods_[0].get(j);
			diagramTuple t = std::make_tuple(0, nt1_, 0, nt2_, g.getPersistence(), diagramType_, g.x_, 0,0,0, g.y_, 0,0,0);
			barycenter.push_back(t);
		}

		runMatchingAuction(&total_cost, sizes, kdt, &correspondance_kdt_map,
					&min_diag_price, &all_matchings, use_kdt);

		std::cout<< "[PersistenceDiagramsBarycenter] Barycenter cost : "<< total_cost << std::endl;

		if(converged){
			finished = true;
		}

		if(!finished){
			updateBarycenter(all_matchings);
			if(debugLevel_>1)
				std::cout<< "Barycenter size : "<< barycenter_goods_[0].size() << std::endl;



			if(min_cost>total_cost){
				min_cost = total_cost;
				last_min_cost_obtained = 0;
			}
			else{
				last_min_cost_obtained += 1;
			}

			converged = converged || last_min_cost_obtained>1;
		}


		previous_matchings = std::move(all_matchings);
		// END OF TIMER
		total_time += t.getElapsedTime();
        std::cout<<"Time elapsed so far : "<<total_time<<std::endl;

		for(unsigned int i=0; i<barycenter_goods_.size(); ++i){
			for(int j=0; j<barycenter_goods_[i].size(); ++j){
				barycenter_goods_[i].get(j).setPrice(0);
			}
		}
		for(unsigned int i=0; i<current_bidder_diagrams_.size(); ++i){
			for(int j=0; j<current_bidder_diagrams_[i].size(); ++j){
				current_bidder_diagrams_[i].get(j).setDiagonalPrice(0);
			}
		}
		for(int i=0; i<numberOfInputs_; i++){
			min_diag_price[i] = 0;
			min_price[i] = 0;
		}

	}
	barycenter.resize(0);
	for(int j=0; j<barycenter_goods_[0].size(); j++){
		Good<dataType>& g = barycenter_goods_[0].get(j);
		diagramTuple t = std::make_tuple(0, nt1_, 0, nt2_, g.getPersistence(), diagramType_, g.x_, 0,0,0, g.y_, 0,0,0);
		barycenter.push_back(t);
	}

	cost_ = total_cost;
	std::vector<std::vector<matchingTuple>> corrected_matchings = correctMatchings(previous_matchings);
	for(unsigned int d=0; d<current_bidder_diagrams_.size(); ++d){
		if(debugLevel_>1)
			std::cout << "Size of diagram " << d << " : " << current_bidder_diagrams_[d].size() << std::endl;
	}
	return corrected_matchings;
}

template <typename dataType>
std::vector<std::vector<matchingTuple>> PDBarycenter<dataType>::executePartialBiddingBarycenter(std::vector<diagramTuple>& barycenter){

    for(unsigned int i_input=0; i_input<numberOfInputs_; i_input++){
        precision_objective_[i_input]=false;
    }
	double total_time = 0;
    int nb_of_auctions = 0;
	std::vector<std::vector<matchingTuple>> previous_matchings;

	this->setBidderDiagrams();

	dataType max_persistence = getMaxPersistence();
	dataType lowest_persistence = getLowestPersistence();
	dataType epsilon_0 = getEpsilon(max_persistence);

	if(!epsilon_decreases_){
		epsilon_0 = 2e-5;
	}
	dataType epsilon = epsilon_0;
	dataType min_persistence;
	dataType min_cost = std::numeric_limits<dataType>::max();
	int last_min_cost_obtained = 0;
	int min_points_to_add = 10;
	if(use_progressive_){
		min_persistence = 0;
	}
	else{
		min_persistence = 0;
		min_points_to_add = std::numeric_limits<int>::max();
	}
	dataType previous_min_persistence=min_persistence;
	std::vector<dataType> min_diag_price(numberOfInputs_);
	std::vector<dataType> min_price(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; i++){
		min_diag_price[i] = 0;
		min_price[i] = 0;
	}

	min_persistence = this->enrichCurrentBidderDiagrams(2*max_persistence, min_persistence, min_diag_price, min_price, min_points_to_add, false);
	this->setInitialBarycenter(min_persistence); // false for a determinist initialization
	if(debugLevel_>1)
		std::cout<< "Barycenter size : "<< barycenter_goods_[0].size() << std::endl;

	int n_iterations = 0;

	bool converged = false;
	bool finished = false;
	dataType total_cost;

	while(!finished){
		Timer t;
		{
		n_iterations += 1;
		dataType rho = getRho(epsilon);
		if(use_progressive_ && n_iterations>1 && min_persistence>rho && epsilon_decreases_){
			dataType epsilon_candidate = getEpsilon(min_persistence);
			if(epsilon_candidate>epsilon){
				// Should always be the case
				epsilon = epsilon_candidate;
			}

			if(epsilon<5e-5){
				// Add all remaining points for final convergence.
				min_persistence = 0;
				min_points_to_add = std::numeric_limits<int>::max();
				use_progressive_=false;
			}

			min_persistence = this->enrichCurrentBidderDiagrams(min_persistence, rho, min_diag_price, min_price, min_points_to_add);
			if(debugLevel_>2)
				std::cout<< "Min persistence : " << min_persistence <<std::endl;
			if(min_persistence<=lowest_persistence){
				use_progressive_=false;
			}
			// TODO Enrich barycenter using median diagonal and off-diagonal prices
		}
		if(epsilon<epsilon_min_){
			epsilon=epsilon_min_;
			converged = true;
		}
		if(debugLevel_>2)
			std::cout<< "epsilon : "<< epsilon << std::endl;
		std::pair<KDTree<dataType>*, std::vector<KDTree<dataType>*>> pair;
		bool use_kdt = false;
		// If the barycenter is empty, do not compute the kdt (or it will crash :/)
		// TODO Fix KDTree to handle empty inputs...
		if(barycenter_goods_[0].size()>0){
			pair = this->getKDTree();
			use_kdt = true;
		}
		KDTree<dataType>* kdt = pair.first;
		std::vector<KDTree<dataType>*>& correspondance_kdt_map = pair.second;

		std::vector<std::vector<matchingTuple>> all_matchings(numberOfInputs_);
		std::vector<int> sizes(numberOfInputs_);
		for(int i=0; i<numberOfInputs_; i++){
			sizes[i] = current_bidder_diagrams_[i].size();
		}

		total_cost = 0;
		if(!epsilon_decreases_){
			epsilon = epsilon_0;
			// if(debugLevel_>9){
			//     runMatching(&total_cost, epsilon, sizes, kdt, &correspondance_kdt_map,
			// 				&min_diag_price,  &min_price, &all_matchings, use_kdt, &(timers[nb_of_auctions]), &(NB_BIDDERS[nb_of_auctions]), &(NB_BIDDINGS[nb_of_auctions]));
            // }
            // else{
			    runMatching(&total_cost, epsilon, sizes, kdt, &correspondance_kdt_map,
							&min_diag_price,  &min_price, &all_matchings, use_kdt);
            // }
			nb_of_auctions ++;

		}
		else if(!early_stoppage_ && epsilon_decreases_){
			epsilon = epsilon_0;
			while(epsilon>1e-5){
				if(debugLevel_>2)
					std::cout<< "epsilon : "<< epsilon << std::endl;
				total_cost = 0;
				runMatching(&total_cost, epsilon, sizes, kdt, &correspondance_kdt_map,
							&min_diag_price,  &min_price, &all_matchings, use_kdt);
				epsilon = epsilon/5.;
			}
			epsilon = epsilon_0;
		}
		else{
			// if(debugLevel_>9){
			//     runMatching(&total_cost, epsilon, sizes, kdt, &correspondance_kdt_map,
			// 				&min_diag_price,  &min_price, &all_matchings, use_kdt, &(timers[nb_of_auctions]), &(NB_BIDDERS[nb_of_auctions]), &(NB_BIDDINGS[nb_of_auctions]));
            // }
            // else{
                Timer t_matchings;
			    runMatching(&total_cost, epsilon, sizes, kdt, &correspondance_kdt_map,
							&min_diag_price,  &min_price, &all_matchings, use_kdt);
                // std::cout<<"Time of MATCHINGS : "<<t_matchings.getElapsedTime()<<std::endl;
            // }
            nb_of_auctions ++;
		}
		if(debugLevel_>1)
			std::cout<< "[PersistenceDiagramsBarycenter] Barycenter cost : "<< total_cost << std::endl;

        // if(converged){
            // Timer t2;
            // {
            //     dataType real_total_cost;
            //     real_total_cost = computeRealCost();
            //     // std::cout<< "Real cost : "<<real_total_cost<<std::endl;
            //         std::cout<< "Real cost : "<<real_total_cost<<std::endl;
            // }
            // double time_real_cost_computation = t2.getElapsedTime();
            // time_passed_doing_test_computations += time_real_cost_computation;
        // // }


		delete kdt;

		if(converged){
			finished = true;
		}

		if(!finished){
			dataType max_shift = updateBarycenter(all_matchings);

			if(debugLevel_>2)
				std::cout<< "Barycenter size : "<< barycenter_goods_[0].size() << std::endl;

			if(epsilon_decreases_){
				dataType epsilon_candidate = std::min(std::max(max_shift/8., epsilon/5.), epsilon_0/pow(n_iterations, 2));
				if(epsilon_candidate<epsilon && use_progressive_){
					epsilon=epsilon_candidate;
				}
				else if (!use_progressive_){
					epsilon=epsilon_candidate;
				}
				else if (use_progressive_){
					epsilon *= 0.95;
				}
			}
            // bool identicalMatchings = hasBarycenterConverged(all_matchings, previous_matchings);
            bool precisionObjectiveMet = isPrecisionObjectiveMet();
            std::cout<<"precision objective met ? "<<precisionObjectiveMet<<"  and epsilon "<<epsilon<<std::endl;
            // std::cout<<"identical matchings ? "<<identicalMatchings<<" epsilon : "<<epsilon<<" cost "<<total_cost<<std::endl;
			if(!use_progressive_ && precisionObjectiveMet /*epsilon<epsilon_0/500.*/ && min_cost>total_cost){
				min_cost = total_cost;
				last_min_cost_obtained = 0;
			}
			else if(!use_progressive_ && precisionObjectiveMet/*epsilon<epsilon_0/500.*/ ){
				last_min_cost_obtained += 1;
			}
            
			converged = converged ||\
						((previous_min_persistence<=lowest_persistence || !use_progressive_) && \
						(last_min_cost_obtained>1 && (!epsilon_decreases_ || !early_stoppage_ || precisionObjectiveMet/*epsilon<epsilon_0/500.*/ || epsilon<epsilon_min_)));

		}

		previous_matchings = std::move(all_matchings);
		previous_min_persistence = min_persistence;


		total_time += t.getElapsedTime(); //-time_real_cost_computation;
        }  // End of timer
        if(debugLevel_>2){
            std::cout<<"Time elapsed so far : "<<total_time<<std::endl;
        }
		if(total_time>0.1*time_limit_){
			setUseProgressive(false);
		}
		if(total_time > 0.9*time_limit_){
			converged=true;
		}

		if(!reinit_prices_){
			for(unsigned int i=0; i<barycenter_goods_.size(); ++i){
				for(int j=0; j<barycenter_goods_[i].size(); ++j){
					barycenter_goods_[i].get(j).setPrice(0);
				}
			}
			for(unsigned int i=0; i<current_bidder_diagrams_.size(); ++i){
				for(int j=0; j<current_bidder_diagrams_[i].size(); ++j){
					current_bidder_diagrams_[i].get(j).setDiagonalPrice(0);
				}
			}
			for(int i=0; i<numberOfInputs_; i++){
				min_diag_price[i] = 0;
				min_price[i] = 0;
			}
		}
	}

	for(int j=0; j<barycenter_goods_[0].size(); j++){
		Good<dataType>& g = barycenter_goods_[0].get(j);
		diagramTuple t = std::make_tuple(0, nt1_, 0, nt2_, g.getPersistence(), diagramType_, g.x_, 0,0,0, g.y_, 0,0,0);
		barycenter.push_back(t);
	}

    cost_ = total_cost;
	std::vector<std::vector<matchingTuple>> corrected_matchings = correctMatchings(previous_matchings);
	for(unsigned int d=0; d<current_bidder_diagrams_.size(); ++d){
		if(debugLevel_>3)
			std::cout << "Size of diagram " << d << " : " << current_bidder_diagrams_[d].size() << std::endl;
	}
    
    /*
    ofstream times_file;
    stringstream filename1;
    stringstream filename2;
    stringstream filename3;
    filename1<<"/home/jules/Desktop/bidders1_"<< diagramType_ <<".txt";
    filename2<<"/home/jules/Desktop/times_"<< diagramType_ <<".txt";
    filename3<<"/home/jules/Desktop/biddings_"<< diagramType_ <<".txt";

    if(debugLevel_>10){
        times_file.open(filename1.str(), ios::out);
        for(unsigned int  i=0; i<timers.size(); i++){
            for (unsigned int j=0; j<timers[i].size(); j++) {
                times_file << NB_BIDDERS[i][j] << "\t";
            }
            times_file << "\n";
        }
        times_file.close();

        times_file.open(filename2.str(), ios::out);
        for(unsigned int i=0; i<timers.size(); i++){
            for (unsigned int j=0; j<timers[i].size(); j++) {
                times_file << timers[i][j] << "\t";
            }
            times_file << "\n";
        }
        times_file.close();

        times_file.open(filename3.str(), ios::out);
        for(unsigned int i=0; i<timers.size(); i++){
            for (unsigned int j=0; j<timers[i].size(); j++) {
                times_file << NB_BIDDINGS[i][j] << "\t";
            }
            times_file << "\n";
        }
        times_file.close();
    }
    if(debugLevel_>-3){
        std::cout << "Total number of Auction rounds : " << nb_of_auctions<< std::endl;
    }
    */

 //   std::cout<<"Time passed doing test computations : "<<time_passed_doing_test_computations<<std::endl;
	return corrected_matchings;
}
template <typename dataType>
dataType PDBarycenter<dataType>::computeRealCost(){
    dataType total_real_cost = 0;
    std::vector<matchingTuple> fake_matchings;
    // for(int j=0; j<barycenter_goods_[0].size(); j++){
    //     Good<dataType> good_tmp = barycenter_goods_[0].get(j).copy();
    //     good_tmp.setPrice(0);
    //     current_barycenter.addGood(good_tmp);
    // }
    for(int i=0; i<numberOfInputs_; i++){
        Auction<dataType> auction(wasserstein_, geometrical_factor_, lambda_, 0.01, true);
        GoodDiagram<dataType> current_barycenter = barycenter_goods_[0];
        BidderDiagram<dataType> current_bidder_diagram = bidder_diagrams_[i];
        auction.BuildAuctionDiagrams(&(current_bidder_diagram) , &(current_barycenter));
        dataType cost = auction.run(&fake_matchings);
        total_real_cost += cost*cost;
    }
    return pow(total_real_cost,1./2);
}

template <typename dataType>
bool PDBarycenter<dataType>::isPrecisionObjectiveMet(){
    for(int i_input=0; i_input<numberOfInputs_; i_input++){
        if(!precision_objective_[i_input]){
            return false;
        }
    }
    return true;
}


#endif