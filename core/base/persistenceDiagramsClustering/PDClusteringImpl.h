#ifndef _PDCLUSTERINGIMPL_H
#define _PDCLUSTERINGIMPL_H

#define BLocalMax ttk::CriticalType::Local_maximum
#define BLocalMin ttk::CriticalType::Local_minimum
#define BSaddle1  ttk::CriticalType::Saddle1
#define BSaddle2  ttk::CriticalType::Saddle2

#include <stdlib.h>     /* srand, rand */
#include <cmath>
#include <PDClustering.h>

#include <random>
#include <algorithm>
#include <iterator>
#include <iostream>

using namespace ttk;

template <typename dataType>
std::vector<int>  PDClustering<dataType>::execute(std::vector<std::vector<diagramTuple>>& final_centroids){
	Timer t;
	{
	std::vector<bool*> current_dos;
	current_dos.push_back(&do_min_);
	current_dos.push_back(&do_sad_);
	current_dos.push_back(&do_max_);
	bool converged = false;
    std::vector<bool> diagrams_complete(3);
    for (int c = 0; c < 3; c++) {
        diagrams_complete[c]= (!use_progressive_)||(!original_dos[c]);
    }
    bool all_diagrams_complete = diagrams_complete[0] && diagrams_complete[1] && diagrams_complete[2];
	n_iterations_ = 0;
	double total_time = 0;

	//dataType cost = std::numeric_limits<dataType>::max();
	setBidderDiagrams();
	cost_ = std::numeric_limits<dataType>::max();
	dataType min_cost = std::numeric_limits<dataType>::max();
	dataType min_cost_min = std::numeric_limits<dataType>::max();
	dataType min_cost_max = std::numeric_limits<dataType>::max();
	dataType min_cost_sad = std::numeric_limits<dataType>::max();
	dataType last_min_cost_obtained=-1;
    std::vector<dataType> epsilon0(3);
    std::vector<dataType> epsilon_candidate(3);
    std::vector<dataType> rho(3);

    std::cout<<"checkpoint"<<std::endl;
	// Getting current diagrams (with only at most min_points_to_add points)
    std::vector<dataType> max_persistence(3);
    std::vector<dataType> lowest_persistence(3);
    std::vector<dataType> min_persistence(3);

    std::cout<<"checkpoint"<<std::endl;
    for(int i_crit = 0 ; i_crit < 3 ; i_crit++){

        std::cout<<"checkpoint"<<i_crit<<std::endl;
        max_persistence[i_crit] = getMostPersistent(i_crit);
        lowest_persistence[i_crit] = getLessPersistent(i_crit);
		min_persistence[i_crit] = 0;
        std::cout<<"size eps "<<epsilon_.size()<<std::endl;
        epsilon_[i_crit] = pow(max_persistence[i_crit],2)/8.;
        epsilon0[i_crit] = epsilon_[i_crit];
    }
    std::cout<<"checkpoint"<<std::endl;
	int min_points_to_add = 50;
	
	if(use_progressive_){
		 // min_persistence = max_persistence/2.;
		// min_persistence = 0;
	}
	else{
		min_points_to_add = std::numeric_limits<int>::max();
	}
	std::vector<std::vector<dataType>> min_diag_price(3);
	std::vector<std::vector<dataType>> min_off_diag_price(3);
	for(int c=0; c<3; ++c){
		for(int i=0; i<numberOfInputs_; i++){
			min_diag_price[c].push_back(0);
			min_off_diag_price[c].push_back(0);
		}
	}
    std::cout<<"first enrich"<<std::endl;
	min_persistence = enrichCurrentBidderDiagrams(max_persistence, min_persistence, min_diag_price, min_off_diag_price, min_points_to_add, false);
    std::cout<<"first enrich done"<<std::endl;
	min_points_to_add = 10;
	for(int c=0;c<3;c++){
	    if(min_persistence[c] <= lowest_persistence[c]){
            diagrams_complete[c] = true;
        }
	}
	all_diagrams_complete =diagrams_complete[0] && diagrams_complete[1] && diagrams_complete[2] ;
    if(all_diagrams_complete) use_progressive_=false;

	// Initializing centroids and clusters
	if(use_kmeanspp_){
        std::cout<<"kmeans pp"<<std::endl;
		initializeCentroidsKMeanspp();
        std::cout<<"kmeans pp done "<<std::endl;
	}
	else{
		initializeCentroids();
	cout << "here" << endl;
	}
	initializeEmptyClusters();
	if( use_accelerated_){
        // std::cout<<"acceleratedMkneans, update clusters"<<std::endl;
        initializeAcceleratedKMeans();
        getCentroidDistanceMatrix();
        acceleratedUpdateClusters();
        // std::cout<<"acceleratedMkneans, update clusters done"<<std::endl;
	}
	else{
         std::cout<<"update Clusters"<<std::endl;
		updateClusters();
	}
    // std::cout<<"initialized"<<std::endl;
	if(debugLevel_>1){
        // std::cout<<"Initial Clustering : "<<std::endl;
	    printClustering();
        // std::cout<<std::endl;
    }
	initializeBarycenterComputers();
    // std::cout<<"entering the loop"<<std::endl;
	while(!converged || (!all_diagrams_complete && use_progressive_)){
		Timer t_inside;{
			n_iterations_++;
	    cout << "pdate centroids" << endl;
            std::vector<dataType> max_shift_vec = updateCentroidsPosition();
	    cout << "update centroids done" << endl;

            if(epsilon_[0]<1e-6) do_min_ = false;
            if(epsilon_[1]<1e-6) do_sad_ = false;
            if(epsilon_[2]<1e-6) do_max_ = false;


            for(int i_crit = 0 ; i_crit < 3 ; i_crit++){
                if( *(current_dos[i_crit]) ){
                    epsilon_candidate[i_crit] = std::max(std::min(max_shift_vec[i_crit]/8., epsilon0[i_crit]/pow(n_iterations_, 2)), epsilon_[i_crit]/5.);

                    if(epsilon_candidate[i_crit]<epsilon_[i_crit]){
                        epsilon_[i_crit]=epsilon_candidate[i_crit];
                    }
                    rho[i_crit] = epsilon_[i_crit] >0 ? std::sqrt(8.0*epsilon_[i_crit]) : -1;
                }
            }

			if(use_progressive_ && n_iterations_>1 && min_persistence>rho && !all_diagrams_complete){
				if(epsilon_[0]<1e-6 && epsilon_[1]<1e-6 &&  epsilon_[2]<1e-6 ){
					// Add all remaining points for final convergence.
					min_persistence[0] = 0;
					min_persistence[1] = 0;
					min_persistence[2] = 0;
					min_points_to_add = std::numeric_limits<int>::max();
					diagrams_complete[0] = true;
					diagrams_complete[1] = true;
					diagrams_complete[2] = true;
					use_progressive_=false;
				}
                for(int i_crit =0 ; i_crit<3 ; i_crit++){
                    epsilon_candidate[i_crit] = pow(min_persistence[i_crit], 2)/8.;
                    if(epsilon_candidate[i_crit]>epsilon_[i_crit]){
                        // Should always be the case except if min_persistence is equal to zero
                        epsilon_[i_crit] = epsilon_candidate[i_crit];
                    }
                }


				min_diag_price = getMinDiagonalPrices();
				min_off_diag_price = getMinPrices();
                // std::cout << "enrich :" << std::endl;
				min_persistence = enrichCurrentBidderDiagrams(min_persistence, rho,  min_diag_price,min_off_diag_price, min_points_to_add, true);
                // std::cout << "enrich done" << std::endl;

                for(int i_crit =0 ; i_crit<3 ; i_crit++){
                    if(min_persistence[i_crit]<=lowest_persistence[i_crit]){
                        diagrams_complete[i_crit] = true;
                    }
                }
				// TODO Enrich barycenter using median diagonal and off-diagonal prices
			}
            // std::cout<<"going to NOT update clusters"<<std::endl;

			if(use_accelerated_){
                // std::cout<<"update clusters"<<std::endl;
				acceleratedUpdateClusters(); 
                // std::cout<<"update clusters done"<<std::endl;
			}
			else{
				updateClusters();
			}
            // std::cout<<"clusters updated"<<std::endl;
            // if(cost_<min_cost && n_iterations_>2 && epsilon_<epsilon0/1000.){
            //     min_cost=cost_;
            // }
            // else if(n_iterations_>2 && epsilon_<epsilon0/1000. && cost_>min_cost){
            //     converged=true;
            // }

            if(diagrams_complete[0] && diagrams_complete[1] && diagrams_complete[2]){ 
                use_progressive_=false;
                all_diagrams_complete = true;
            }

            // bool precision_criterion_reached = ( !(original_dos[0]) || (epsilon_[0]<epsilon0[0]/500.) ) /* && ( !(original_dos[1]) || (epsilon_[1]<epsilon0[1]/500.) ) */ && ( !(original_dos[2]) && (epsilon_[2]<epsilon0[2]/500.) );
            // bool precision_criterion_reached = epsilon_[0]<epsilon0[0]/500. && epsilon_[2]<epsilon0[2]/500.;
            bool precision_criterion_reached = precision_criterion_;
            std::cout<<"global precison criterion : "<<precision_criterion_reached<<std::endl;

            if(debugLevel_>0){
                std::cout<< "Iteration "<< n_iterations_<<", Epsilon = "<< epsilon_[0]<< " "<< epsilon_[1]<<" "<< epsilon_[2]<< std::endl;
                std::cout<< " complete ? :  "<< diagrams_complete[0]<<" " << diagrams_complete[1]<<" " <<diagrams_complete[2]<<" " << std::endl;
                std::cout<< " precision ? :  "<< (epsilon_[0]<epsilon0[0]/500.) <<" " << (epsilon_[1]<epsilon0[1]/500.) <<" " << (epsilon_[2]<epsilon0[2]/500.) <<" " << std::endl;
                std::cout<< " epsilons ? :  "<< epsilon0[0]/500. <<" " << epsilon0[1]/500. <<" " << epsilon0[2]/500. <<" " << std::endl;
                std::cout<< " all complete ? :  "<< all_diagrams_complete<<"   useprog ? "<<use_progressive_<<"  and DOs ? "<<do_min_<<do_sad_<<do_max_<<std::endl;// (epsilon_[0]<epsilon0[0]/500.) <<" " << (epsilon_[1]<epsilon0[1]/500.) <<" " << (epsilon_[2]<epsilon0[2]/500.) <<" " << std::endl;
                std::cout<<"  original DOs ? "<<original_dos[0]<<original_dos[1]<<original_dos[2]<<std::endl;// (epsilon_[0]<epsilon0[0]/500.) <<" " << (epsilon_[1]<epsilon0[1]/500.) <<" " << (epsilon_[2]<epsilon0[2]/500.) <<" " << std::endl;
                std::cout<<"                 costmin : "<<cost_min_<<" , min_cost_min : "<<min_cost_min<<std::endl;
                std::cout<<"                 costsad : "<<cost_sad_<<" , min_cost_sad : "<<min_cost_sad<<std::endl;
                std::cout<<"                 costmax : "<<cost_max_<<" , min_cost_max : "<<min_cost_max<<std::endl;
            }

			if(cost_<min_cost && n_iterations_>2 && all_diagrams_complete && precision_criterion_reached){
				min_cost=cost_;
				last_min_cost_obtained =0;
			}
			else if(n_iterations_>2 && precision_criterion_reached && !use_progressive_){
			last_min_cost_obtained +=1;
			}

			if(cost_<min_cost && n_iterations_>2 && all_diagrams_complete && precision_criterion_reached){
				min_cost=cost_;
				last_min_cost_obtained =0;
			}
			else if(n_iterations_>2 && precision_criterion_reached && !use_progressive_){
			last_min_cost_obtained +=1;
			}

			if(cost_<min_cost && n_iterations_>2 && all_diagrams_complete && precision_criterion_reached){
				min_cost=cost_;
				last_min_cost_obtained =0;
			}
			else if(n_iterations_>2 && precision_criterion_reached && !use_progressive_){
			last_min_cost_obtained +=1;
			}

			if(debugLevel_>1){
			    std::cout<< "Cost = "<< cost_<< std::endl;
			    printClustering();
            }
		    converged = converged || (all_diagrams_complete && last_min_cost_obtained > 1 && (precision_criterion_reached) );
		}
		total_time +=t_inside.getElapsedTime();
        // std::cout<<"total_cost_ "<<cost_<<"times "<<total_time<<" "<<t_inside.getElapsedTime()<<" "<<time_limit_<<std::endl;
		if(total_time+t_inside.getElapsedTime()>0.9*time_limit_){
		    min_cost=cost_;
			converged = true;
			all_diagrams_complete = true;
			diagrams_complete[0] = true ;
			diagrams_complete[1] = true ;
			diagrams_complete[2] = true ;
		}
        if(total_time>0.1*time_limit_){
			all_diagrams_complete = true;
			diagrams_complete[0] = true ;
			diagrams_complete[1] = true ;
			diagrams_complete[2] = true ;
            use_progressive_=false;
        }
        if(debugLevel_>4){
            std::cout<<"== Iteration "<< n_iterations_ <<" == complete : "<<all_diagrams_complete<<" , progressive : "<<use_progressive_<<" , converged : "<<converged<<std::endl;
            // std::cout<<"                 min_persistence : "<<min_persistence<<" , epsilon0 : "<<epsilon0<<std::endl;
            // std::cout<<"                 lowest_persistence : "<<lowest_persistence<<std::endl;
            // std::cout<<"                 time limit passed ?  : "<< (bool)(total_time>time_limit_) <<" , eps min passed? : "<<(bool)(epsilon_<epsilon0/500.)<<std::endl;
        }
	}
    resetDosToOriginalValues();
    std::cout<<"[PersistenceDiagramsClustering] Final Cost : "<<min_cost<<std::endl;
    if(!use_progressive_){
        clustering_=old_clustering_; // reverting to last clustering
    }
    invertClusters(); // this is to pass the old inverse clustering to the VTK wrapper
    printClustering();
	}// End of timer


    // Filling the final centroids for output

    final_centroids.resize(k_);

    for(int c=0; c<k_; ++c){
	    if(do_min_){
	        for(int i=0; i<centroids_min_[c].size(); ++i){
                Good<dataType>& g = centroids_min_[c].get(i);
                std::tuple<float,float,float> critCoords = g.GetCriticalCoordinates();
                float x = std::get<0>(critCoords);
                float y = std::get<1>(critCoords);
                float z = std::get<2>(critCoords);
                diagramTuple t = std::make_tuple(   0, ttk::CriticalType::Local_minimum, 
                                                    0,ttk::CriticalType::Saddle1,
                                                    g.getPersistence(),
                                                    0,
                                                    g.x_,x,y,z,
                                                    g.y_,x,y,z   );
                final_centroids[c].push_back(t);
                if(g.getPersistence()>1000){
                    std::cout<<"huho grosse persistence min"<<std::endl;
                }
            }
	    }

	    if(do_sad_){
	        for(int i=0; i<centroids_saddle_[c].size(); ++i){
				Good<dataType>& g = centroids_saddle_[c].get(i);
                std::tuple<float,float,float> critCoords = g.GetCriticalCoordinates();
                float x = std::get<0>(critCoords);
                float y = std::get<1>(critCoords);
                float z = std::get<2>(critCoords);
                diagramTuple t = std::make_tuple(   0, ttk::CriticalType::Saddle1, 
                                                    0,ttk::CriticalType::Saddle2,
                                                    g.getPersistence(),
                                                    1,
                                                    g.x_,x,y,z,
                                                    g.y_,x,y,z   );
                final_centroids[c].push_back(t);
                if(g.getPersistence()>1000){
                    std::cout<<"huho grosse persistence sad"<<std::endl;
                }
            }
	    }

	    if(do_max_){
	        for(int i=0; i<centroids_max_[c].size(); ++i){
                Good<dataType>& g = centroids_max_[c].get(i);
                std::tuple<float,float,float> critCoords = g.GetCriticalCoordinates();
                float x = std::get<0>(critCoords);
                float y = std::get<1>(critCoords);
                float z = std::get<2>(critCoords);
                ttk::CriticalType saddle_type;

                if(do_sad_)
                    saddle_type = ttk::CriticalType::Saddle2;
                else
                    saddle_type = ttk:: CriticalType::Saddle1;

                diagramTuple t = std::make_tuple(   0, saddle_type, 
                                                    0,ttk::CriticalType::Local_maximum,
                                                    g.getPersistence(),
                                                    2,
                                                    g.x_,x,y,z,
                                                    g.y_,x,y,z   );
                final_centroids[c].push_back(t);
                if(g.getPersistence()>1000){
                    std::cout<<"huho grosse persistence max"<<std::endl;
                }
            }
	    }
    }
	return inv_clustering_;
}

template <typename dataType>
dataType PDClustering<dataType>::getMostPersistent(int type){
	dataType max_persistence = 0;
    std::cout<<"type = "<<type<<std::endl;
	if( do_min_&& (type==-1 || type==0 )){
		for(unsigned int i=0; i< bidder_diagrams_min_.size(); ++i){
			for(int j=0; j< bidder_diagrams_min_[i].size(); ++j){
				Bidder<dataType> b = bidder_diagrams_min_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence>max_persistence){
					max_persistence = persistence;
				}
			}
		}
	}

	if( do_sad_&& (type==-1 || type==1 )){
		for(unsigned int i=0; i< bidder_diagrams_saddle_.size(); ++i){
			for(int j=0; j< bidder_diagrams_saddle_[i].size(); ++j){
				Bidder<dataType> b = bidder_diagrams_saddle_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence>max_persistence){
					max_persistence = persistence;
				}
			}
		}
	}

	if( do_max_&& (type==-1 || type==2 )){
		for(unsigned int i=0; i< bidder_diagrams_max_.size(); ++i){
			for(int j=0; j< bidder_diagrams_max_[i].size(); ++j){
				Bidder<dataType> b = bidder_diagrams_max_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence>max_persistence){
					max_persistence = persistence;
				}
			}
		}
	}
	return max_persistence;
}


template <typename dataType>
dataType PDClustering<dataType>::getLessPersistent(int type){
    // type == -1 : query the min of all the types of diagrams.
    // type = 0 : min,  1 : sad,   2 : max
    std::cout<<"type = "<<type<<std::endl;
	dataType min_persistence = std::numeric_limits<dataType>::max();
	if( do_min_ && (type==-1 || type==0)){
		for(unsigned int i=0; i< bidder_diagrams_min_.size(); ++i){
			for(int j=0; j< bidder_diagrams_min_[i].size(); ++j){
				Bidder<dataType> b = bidder_diagrams_min_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence<min_persistence){
					min_persistence = persistence;
				}
			}
		}
	}

	if( do_sad_ && (type == -1 || type==1) ){
		for(unsigned int i=0; i< bidder_diagrams_saddle_.size(); ++i){
			for(int j=0; j< bidder_diagrams_saddle_[i].size(); ++j){
				Bidder<dataType> b = bidder_diagrams_saddle_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence<min_persistence){
					min_persistence = persistence;
				}
			}
		}
	}

	if( do_max_ && (type == -1 || type==2) ){
		for(unsigned int i=0; i< bidder_diagrams_max_.size(); ++i){
			for(int j=0; j< bidder_diagrams_max_[i].size(); ++j){
				Bidder<dataType> b = bidder_diagrams_max_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence<min_persistence){
					min_persistence = persistence;
				}
			}
		}
	}
	return min_persistence;
}

template <typename dataType>
std::vector<std::vector<dataType>> PDClustering<dataType>::getMinPrices(){
    std::vector<std::vector<dataType>> min_prices(3);
    
    if(do_min_){
        for(unsigned int i=0; i<centroids_with_price_min_.size(); ++i){
            min_prices[0].push_back(std::numeric_limits<dataType>::max());
            for(int j=0; j<centroids_with_price_min_[i].size(); ++j){
                Good<dataType> g = centroids_with_price_min_[i].get(j);
                dataType price = g.getPrice();
                if(price<min_prices[0][i]){
                    min_prices[0][i] = price;
                }
            }
        }
    }

    if(do_sad_){
        for(unsigned int i=0; i<centroids_with_price_saddle_.size(); ++i){
            min_prices[1].push_back(std::numeric_limits<dataType>::max());
            for(int j=0; j<centroids_with_price_saddle_[i].size(); ++j){
                Good<dataType> g = centroids_with_price_saddle_[i].get(j);
                dataType price = g.getPrice();
                if(price<min_prices[1][i]){
                    min_prices[1][i] = price;
                }
            }
        }
    }

    if(do_max_){
        for(unsigned int i=0; i<centroids_with_price_max_.size(); ++i){
            min_prices[2].push_back(std::numeric_limits<dataType>::max());
            for(int j=0; j<centroids_with_price_max_[i].size(); ++j){
                Good<dataType> g = centroids_with_price_max_[i].get(j);
                dataType price = g.getPrice();
                if(price<min_prices[2][i]){
                    min_prices[2][i] = price;
                }
            }
        }
    }

     return min_prices;
 }
 
 template <typename dataType>
 std::vector<std::vector<dataType>> PDClustering<dataType>::getMinDiagonalPrices(){
 	std::vector<std::vector<dataType>> min_prices(3);
 	if(do_min_){
 		for(unsigned int i=0; i< current_bidder_diagrams_min_.size(); ++i){
 			min_prices[0].push_back(std::numeric_limits<dataType>::max());
 			for(int j=0; j< current_bidder_diagrams_min_[i].size(); ++j){
				Bidder<dataType> b = current_bidder_diagrams_min_[i].get(j);
				dataType price = b.diagonal_price_;
				if(price<min_prices[0][i]){
					min_prices[0][i] = price;
				}
			}
			if(min_prices[0][i]>=std::numeric_limits<dataType>::max()/2.){
				min_prices[0][i] = 0;
			}
		}
	}

	if(do_sad_){
		for(unsigned int i=0; i< bidder_diagrams_saddle_.size(); ++i){
			min_prices[1].push_back(std::numeric_limits<dataType>::max());
			for(int j=0; j< current_bidder_diagrams_saddle_[i].size(); ++j){
				Bidder<dataType> b = current_bidder_diagrams_saddle_[i].get(j);
				dataType price = b.diagonal_price_;
				if(price<min_prices[1][i]){
					min_prices[1][i] = price;
				}
			}
			if(min_prices[1][i] >= std::numeric_limits<dataType>::max()/2.){
				min_prices[1][i] = 0;
			}
		}
	}

	if(do_max_){
		for(unsigned int i=0; i< current_bidder_diagrams_max_.size(); ++i){
			min_prices[2].push_back(std::numeric_limits<dataType>::max());
			for(int j=0; j< current_bidder_diagrams_max_[i].size(); ++j){
				Bidder<dataType> b = current_bidder_diagrams_max_[i].get(j);
				dataType price = b.diagonal_price_;
				if(price<min_prices[2][i]){
					min_prices[2][i] = price;
				}
			}
			if(min_prices[2][i] >= std::numeric_limits<dataType>::max()/2.){
				min_prices[2][i] = 0;
			}
		}
	}
	return min_prices;
}


template <typename dataType>
dataType PDClustering<dataType>::computeDistance(BidderDiagram<dataType>& D1, BidderDiagram<dataType>& D2, dataType delta_lim){
	GoodDiagram<dataType> D2_bis = diagramToCentroid(D2);
	return computeDistance(D1, D2_bis, delta_lim);
}

template <typename dataType>
dataType PDClustering<dataType>::computeDistance(BidderDiagram<dataType> D1, GoodDiagram<dataType> D2, dataType delta_lim){
	std::vector<matchingTuple> matchings;
	D2 = centroidWithZeroPrices(D2);
	Auction<dataType> auction(wasserstein_, geometrical_factor_, lambda_, delta_lim, use_kdtree_);
	auction.BuildAuctionDiagrams(&D1, &D2);
	dataType cost = auction.run(&matchings);
	return cost;
}


template <typename dataType>
dataType PDClustering<dataType>::computeDistance(BidderDiagram<dataType>* D1, GoodDiagram<dataType>* D2, dataType delta_lim){
	std::vector<matchingTuple> matchings;
	Auction<dataType> auction(wasserstein_, geometrical_factor_, lambda_, delta_lim, use_kdtree_);
	int size1 = D1->size();
	auction.BuildAuctionDiagrams(D1, D2);
	dataType cost = auction.run(&matchings);
        // Diagonal Points were added in the original diagram. The following line removes them.
	D1->bidders_.resize(size1);
	return cost;
}


template <typename dataType>
dataType PDClustering<dataType>::computeDistance(GoodDiagram<dataType>& D1, GoodDiagram<dataType>& D2, dataType delta_lim){
	BidderDiagram<dataType> D1_bis = centroidToDiagram(D1);
	return computeDistance(D1_bis, D2, delta_lim);
}


template <typename dataType>
GoodDiagram<dataType> PDClustering<dataType>::centroidWithZeroPrices(GoodDiagram<dataType> centroid){
	GoodDiagram<dataType> GD = GoodDiagram<dataType>();
	for(int i=0; i<centroid.size(); i++){
		Good<dataType> g = centroid.get(i);
		g.setPrice(0);
		GD.addGood(g);
	}
	return GD;
}


template <typename dataType>
BidderDiagram<dataType> PDClustering<dataType>::diagramWithZeroPrices(BidderDiagram<dataType> diagram){
	BidderDiagram<dataType> BD = BidderDiagram<dataType>();
	for(int i=0; i<diagram.size(); i++){
		Bidder<dataType> b = diagram.get(i);
		b.setDiagonalPrice(0);
		BD.addBidder(b);
	}
	return BD;
}


template <typename dataType>
BidderDiagram<dataType> PDClustering<dataType>::centroidToDiagram(GoodDiagram<dataType> centroid){
	BidderDiagram<dataType> BD = BidderDiagram<dataType>();
	for(int i=0; i<centroid.size(); i++){
		Good<dataType> g = centroid.get(i);

		Bidder<dataType> b = Bidder<dataType>(g.x_, g.y_, g.isDiagonal(), BD.size());
		b.SetCriticalCoordinates(g.coords_x_, g.coords_y_, g.coords_z_);
		b.setPositionInAuction(BD.size());
		BD.addBidder(b);
	}
	return BD;
}

template <typename dataType>
GoodDiagram<dataType> PDClustering<dataType>::diagramToCentroid(BidderDiagram<dataType> diagram){
	GoodDiagram<dataType> GD = GoodDiagram<dataType>();
	for(int i=0; i<diagram.size(); i++){
		Bidder<dataType> b = diagram.get(i);

		Good<dataType> g = Good<dataType>(b.x_, b.y_, b.isDiagonal(), GD.size());
		g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
		GD.addGood(g);
	}
	return GD;
}


template <typename dataType>
void PDClustering<dataType>::initializeEmptyClusters(){
	clustering_ = std::vector<std::vector<int>>(k_);
}

template <typename dataType>
void PDClustering<dataType>::initializeCentroids(){
	std::vector<int> idx(numberOfInputs_);
	// To perform a random draw with replacement, the vector {1, 2, ..., numberOfInputs_} is
	// shuffled, and we consider its k_ first elements to be the initial centroids.
	for(int i=0; i<numberOfInputs_; i++){
		idx[i] = i;
	}
	if(!deterministic_)
		std::random_shuffle(idx.begin(), idx.end());

	for(int c=0; c<k_; c++){
		if(do_min_){
			GoodDiagram<dataType> centroid_min = diagramToCentroid(current_bidder_diagrams_min_[idx[c]]);
			centroids_min_.push_back(centroid_min);
		}
		if(do_sad_){
			GoodDiagram<dataType> centroid_sad = diagramToCentroid(current_bidder_diagrams_saddle_[idx[c]]);
			centroids_saddle_.push_back(centroid_sad);
		}
		if(do_max_){
			GoodDiagram<dataType> centroid_max = diagramToCentroid(current_bidder_diagrams_max_[idx[c]]);
			centroids_max_.push_back(centroid_max);
		}
	}
}

template <typename dataType>
void PDClustering<dataType>::initializeCentroidsKMeanspp(){
	std::vector<int> indexes_clusters;
	int random_idx = deterministic_ ? 0 : rand() % numberOfInputs_;
	indexes_clusters.push_back(random_idx);

	if(do_min_){
		GoodDiagram<dataType> centroid_min = diagramToCentroid(current_bidder_diagrams_min_[random_idx]);
		centroids_min_.push_back(centroid_min);
	}
	if(do_sad_){
		GoodDiagram<dataType> centroid_sad = diagramToCentroid(current_bidder_diagrams_saddle_[random_idx]);
		centroids_saddle_.push_back(centroid_sad);
	}
	if(do_max_){
		GoodDiagram<dataType> centroid_max = diagramToCentroid(current_bidder_diagrams_max_[random_idx]);
		centroids_max_.push_back(centroid_max);
	}

	while((int) indexes_clusters.size()<k_){
		std::vector<dataType> min_distance_to_centroid(numberOfInputs_);
		std::vector<dataType> probabilities(numberOfInputs_);
		
		// Uncomment for a deterministic algorithm
		dataType maximal_distance = 0;
		int candidate_centroid = -1;

		for(int i=0; i<numberOfInputs_; i++){
			min_distance_to_centroid[i] = std::numeric_limits<dataType>::max();
			if(std::find(indexes_clusters.begin(), indexes_clusters.end(), i) != indexes_clusters.end()){
				min_distance_to_centroid[i] = 0;
			}
			else{
				for(unsigned int j=0; j<indexes_clusters.size(); ++j){
					dataType distance = 0;
					if(do_min_){
						GoodDiagram<dataType> centroid_min = centroidWithZeroPrices(centroids_min_[j]);
						distance += computeDistance(current_bidder_diagrams_min_[i], centroid_min, 0.01);
					}
					if(do_sad_){
						GoodDiagram<dataType> centroid_saddle = centroidWithZeroPrices(centroids_saddle_[j]);
						distance += computeDistance(current_bidder_diagrams_saddle_[i], centroid_saddle, 0.01);
					}
					if(do_max_){
						GoodDiagram<dataType> centroid_max = centroidWithZeroPrices(centroids_max_[j]);
						distance += computeDistance(current_bidder_diagrams_max_[i], centroid_max, 0.01);
					}
					if(distance<min_distance_to_centroid[i]){
						min_distance_to_centroid[i] = distance;
					}
				}
			}
			probabilities[i] = pow(min_distance_to_centroid[i], 2);

			// The following block is useful in case of need for a deterministic algoritm
			if( deterministic_ && min_distance_to_centroid[i]>maximal_distance ){
				maximal_distance = min_distance_to_centroid[i];
				candidate_centroid = i;
			}

		}
		// Comment the following four lines to make it deterministic
		std::random_device rd;
		std::mt19937 gen(rd());
		std::discrete_distribution<int> distribution (probabilities.begin(),probabilities.end());
		
		if( !deterministic_ ) {
            candidate_centroid = distribution(gen);
        }

		indexes_clusters.push_back(candidate_centroid);
		if(do_min_){
			GoodDiagram<dataType> centroid_min = diagramToCentroid(current_bidder_diagrams_min_[candidate_centroid]);
			centroids_min_.push_back(centroid_min);
		}
		if(do_sad_){
			GoodDiagram<dataType> centroid_sad = diagramToCentroid(current_bidder_diagrams_saddle_[candidate_centroid]);
			centroids_saddle_.push_back(centroid_sad);
		}
		if(do_max_){
			GoodDiagram<dataType> centroid_max = diagramToCentroid(current_bidder_diagrams_max_[candidate_centroid]);
			centroids_max_.push_back(centroid_max);
		}
	}
}

template <typename dataType>
void PDClustering<dataType>::initializeAcceleratedKMeans(){
	// r_ is a vector stating for each diagram if its distance to its centroid is
	// up to date (false) or needs to be recomputed (true)
	r_ = std::vector<bool>(numberOfInputs_);
	// u_ is a vector of upper bounds of the distance of each diagram to its closest centroid
	u_ = std::vector<dataType>(numberOfInputs_);
	inv_clustering_ = std::vector<int>(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; i++){
		r_[i] = true;
		u_[i] = std::numeric_limits<dataType>::max();
		inv_clustering_[i] = -1;
	}
	// l_ is the matrix of lower bounds for the distance from each diagram
	// to each centroid
	l_ = std::vector<std::vector<dataType>>(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; ++i){
		l_[i] = std::vector<dataType>(k_);
		for(int c=0; c<k_; ++c){
			l_[i][c]=0;
		}
	}

	// And d_ is a (K x K) matrix storing the distances between each pair of centroids
	d_ = std::vector<std::vector<dataType>>(k_);
	for(int i=0; i<k_; ++i){
		for(int c=0; c<k_; ++c){
			d_[i].push_back(0);
		}
	}
	return ;
}


template <typename dataType>
std::vector<std::vector<dataType>> PDClustering<dataType>::getDistanceMatrix(){
	std::vector<std::vector<dataType>> D(numberOfInputs_);

	for(int i=0; i<numberOfInputs_; ++i){
		BidderDiagram<dataType> D1_min, D1_sad, D1_max;
		if(do_min_){
			D1_min = diagramWithZeroPrices(current_bidder_diagrams_min_[i]);
		}
		if(do_sad_){
			D1_sad = diagramWithZeroPrices(current_bidder_diagrams_saddle_[i]);
		}
		if(do_max_){
			D1_max = diagramWithZeroPrices(current_bidder_diagrams_max_[i]);
		}
		for(int c=0; c<k_; ++c){
			GoodDiagram<dataType> D2_min, D2_sad, D2_max;
			dataType distance = 0;
			if(do_min_){
				D2_min = centroids_min_[c];
				distance += computeDistance(D1_min, D2_min, 0.01);
			}
			if(do_sad_){
				D2_sad = centroids_saddle_[c];
				distance += computeDistance(D1_sad, D2_sad, 0.01);
			}
			if(do_max_){
				D2_max = centroids_max_[c];
				distance += computeDistance(D1_max, D2_max, 0.01);
			}
			D[i].push_back(distance);
		}
	}
	return D;
}

template <typename dataType>
void PDClustering<dataType>::getCentroidDistanceMatrix(){
	for(int i=0; i<k_; ++i){
		GoodDiagram<dataType> D1_min, D1_sad, D1_max;
		if(do_min_){
			D1_min = centroidWithZeroPrices(centroids_min_[i]);
		}
		if(do_sad_){
			D1_sad = centroidWithZeroPrices(centroids_saddle_[i]);
		}
		if(do_max_){
			D1_max = centroidWithZeroPrices(centroids_max_[i]);
		}
		for(int j=i+1; j<k_; ++j){
			dataType distance = 0;
			GoodDiagram<dataType> D2_min, D2_sad, D2_max;
			if(do_min_){
				D2_min = centroidWithZeroPrices(centroids_min_[j]);
				distance += computeDistance(D1_min, D2_min, 0.01);
			}
			if(do_sad_){
				D2_sad = centroidWithZeroPrices(centroids_saddle_[j]);
				distance += computeDistance(D1_sad, D2_sad, 0.01);
			}
			if(do_max_){
				D2_max = centroidWithZeroPrices(centroids_max_[j]);
				distance += computeDistance(D1_max, D2_max, 0.01);
			}

			d_[i][j] = distance;
			d_[j][i] = distance;
		}
	}
	return;
}


template <typename dataType>
void PDClustering<dataType>::updateClusters(){
    if(k_>1){
        std::vector<std::vector<dataType>> distance_matrix = getDistanceMatrix();
        old_clustering_ = clustering_;
        invertClusters();
        initializeEmptyClusters();

        for(int i=0; i<numberOfInputs_; ++i){
            dataType min_distance_to_centroid = std::numeric_limits<dataType>::max();
            int cluster = -1;
            for(int c=0; c<k_; ++c){
                if(distance_matrix[i][c]<min_distance_to_centroid){
                    min_distance_to_centroid = distance_matrix[i][c];
                    cluster = c;
                }
            }

            clustering_[cluster].push_back(i);
            if(cluster!=inv_clustering_[i]){
                // New centroid attributed to this diagram
                resetDosToOriginalValues();
                barycenter_inputs_reset_flag=true;
                if(do_min_){
                    centroids_with_price_min_[i] = centroidWithZeroPrices(centroids_min_[cluster]);
                }
                if(do_sad_){
                    centroids_with_price_saddle_[i] = centroidWithZeroPrices(centroids_saddle_[cluster]);
                }
                if(do_max_){
                    centroids_with_price_max_[i] = centroidWithZeroPrices(centroids_max_[cluster]);
                }
                inv_clustering_[i] = cluster;
            }
        }
        // for(int c=0; c<k_; ++c){
        // 	if(clustering_[c].size()==0){
        // 	    // int candidate_tmp = deterministic_ ? 0 : rand() % numberOfInputs_;
        // 	    int candidate_tmp = std::distance(u_.begin(), std::max_element(u_.begin(), u_.end()));
        // 		clustering_[c].push_back( candidate_tmp );
        // 	}
        // }
    }
    else{
        old_clustering_ = clustering_;
        invertClusters();
        initializeEmptyClusters();

        for(int i=0; i<numberOfInputs_; i++){
        clustering_[0].push_back(i);
        if(do_min_){
            centroids_with_price_min_[i] = centroidWithZeroPrices(centroids_min_[0]);
        }
        if(do_sad_){
            centroids_with_price_saddle_[i] = centroidWithZeroPrices(centroids_saddle_[0]);
        }
        if(do_max_){
            centroids_with_price_max_[i] = centroidWithZeroPrices(centroids_max_[0]);
        }
        inv_clustering_[i]=0;
        }
    }
	return ;
}

template <typename dataType>
void PDClustering<dataType>::invertClusters(){
	/// Converts the clustering (vector of vector of diagram's id) into
	/// a vector of size numberOfInputs_ containg the cluster of each input diagram.

	// Initializes clusters with -1
	inv_clustering_ = std::vector<int>(numberOfInputs_);
	for(int i=0; i<numberOfInputs_; ++i){
		inv_clustering_[i] = -1;
	}

	// Fill in the clusters
	for(int c=0; c<k_; ++c){
		for(unsigned int j=0; j<clustering_[c].size(); ++j){
			int idx = clustering_[c][j];
			inv_clustering_[idx] = c;
		}
	}
}

template <typename dataType>
void PDClustering<dataType>::invertInverseClusters(){
	clustering_ = std::vector<std::vector<int>>(k_);
	for(int i=0; i<numberOfInputs_; ++i){
		clustering_[inv_clustering_[i]].push_back(i);
	}

	// Check if a cluster was left without diagram
	for(int c=0; c<k_; ++c){
		if(clustering_[c].size() == 0){
			std::cout<< "Problem in invertInverseClusters()... \nCluster " << c << " was left with no diagram attached to it... " << std::endl;
		}
	}
}


template <typename dataType>
void PDClustering<dataType>::acceleratedUpdateClusters(){
	// Step 1
	getCentroidDistanceMatrix();
	old_clustering_ = clustering_;
	//self.old_clusters = copy.copy(self.clusters)
	invertClusters();
	initializeEmptyClusters();
	bool do_min = original_dos[0];
	bool do_sad = original_dos[1];
	bool do_max = original_dos[2];

	for(int i=0; i<numberOfInputs_; ++i){
		// Step 3 find potential changes of clusters
		BidderDiagram<dataType> D1_min, D1_sad, D1_max;
		if(do_min){
			D1_min = diagramWithZeroPrices(current_bidder_diagrams_min_[i]);
		}
		if(do_sad){
			D1_sad = diagramWithZeroPrices(current_bidder_diagrams_saddle_[i]);
		}
		if(do_max){
			D1_max = diagramWithZeroPrices(current_bidder_diagrams_max_[i]);
		}

		for(int c=0; c<k_; ++c){
			if(inv_clustering_[i]==-1){
				// If not yet assigned, assign it first to a random cluster

				if(deterministic_){
				    inv_clustering_[i] = i % k_;
                }
                else{
                    std::cout << " - ASSIGNED TO A RANDOM CLUSTER " << '\n';
                    inv_clustering_[i] = rand() % (k_);
                }

				r_[i] = true;
				if(do_min){
					centroids_with_price_min_[i] = centroidWithZeroPrices(centroids_min_[inv_clustering_[i]]);
				}
				if(do_sad){
					centroids_with_price_saddle_[i] = centroidWithZeroPrices(centroids_saddle_[inv_clustering_[i]]);
				}
				if(do_max){
					centroids_with_price_max_[i] = centroidWithZeroPrices(centroids_max_[inv_clustering_[i]]);
				}
			}

			if(c!=inv_clustering_[i] && u_[i]>l_[i][c] && u_[i]>0.5*d_[inv_clustering_[i]][c]){
				// Step 3a, If necessary, recompute the distance to centroid
				if(r_[i]){
					dataType distance = 0;
					GoodDiagram<dataType> centroid_min, centroid_sad, centroid_max;
					if(do_min){
						centroid_min = centroidWithZeroPrices(centroids_min_[inv_clustering_[i]]);
						distance += computeDistance(D1_min, centroid_min, 0.01);
					}
					if(do_sad){
						centroid_sad = centroidWithZeroPrices(centroids_saddle_[inv_clustering_[i]]);
						distance += computeDistance(D1_sad, centroid_sad, 0.01);
					}
					if(do_max){
						centroid_max = centroidWithZeroPrices(centroids_max_[inv_clustering_[i]]);
						distance += computeDistance(D1_max, centroid_max, 0.01);
					}
					r_[i] = false;
					u_[i] = distance;
					l_[i][inv_clustering_[i]] = distance;
				}
				// Step 3b, check if still potential change of clusters
				if(u_[i]>l_[i][c] || u_[i]>0.5*d_[inv_clustering_[i]][c]){
					BidderDiagram<dataType> diagram_min, diagram_sad, diagram_max;
					GoodDiagram<dataType> centroid_min, centroid_sad, centroid_max;
					dataType distance = 0;

					if(do_min){
						centroid_min = centroidWithZeroPrices(centroids_min_[c]);
						diagram_min = diagramWithZeroPrices(current_bidder_diagrams_min_[i]);
						distance += computeDistance(diagram_min, centroid_min, 0.01);
					}
					if(do_sad){
						centroid_sad = centroidWithZeroPrices(centroids_saddle_[c]);
						diagram_sad = diagramWithZeroPrices(current_bidder_diagrams_saddle_[i]);
						distance += computeDistance(diagram_sad, centroid_sad, 0.01);
					}
					if(do_max){
						centroid_max = centroidWithZeroPrices(centroids_max_[c]);
						diagram_max = diagramWithZeroPrices(current_bidder_diagrams_max_[i]);
						distance += computeDistance(diagram_max, centroid_max, 0.01);
					}
					l_[i][c] = distance;
					// TODO Prices are lost here... If distance<self.u[i], we should keep the prices
					if(distance<u_[i]){
						// Changing cluster
						resetDosToOriginalValues();
						u_[i] = distance;
						inv_clustering_[i] = c;

						if(do_min){
							centroids_with_price_min_[i] = centroidWithZeroPrices(centroids_min_[c]);
						}
						if(do_sad){
							centroids_with_price_saddle_[i] = centroidWithZeroPrices(centroids_saddle_[c]);
						}
						if(do_max){
							centroids_with_price_max_[i] = centroidWithZeroPrices(centroids_max_[c]);
						}
					}
				}
			}
		}
	}
	invertInverseClusters();
	for(int c=0; c<k_; ++c){
		if(clustering_[c].size()==0){
			std::cout<< "Adding artificial centroid because a cluster was empty" <<std::endl;
			bool idx_acceptable = false;
			int idx=0;
			int increment=0;
            
            // std::cout<< " u_ : [ ";
            // for(int i=0; i<u_.size(); i++){
            //         std::cout<<" "<<u_[i];
            //         }
            //         std::cout<<" ] "<<std::endl;
            std::vector<dataType>copy_of_u(u_.size());
            copy_of_u = u_;

			while(!idx_acceptable){
			    auto argMax = std::max_element(copy_of_u.begin(), copy_of_u.end());
			    idx = std::distance(copy_of_u.begin(),argMax);
				// idx = deterministic_ ? idx+1 : rand() % k_;
				if(inv_clustering_[idx]<k_ && inv_clustering_[idx]>=0  && clustering_[inv_clustering_[idx]].size()>1){
					idx_acceptable = true;
					int cluster_removal = inv_clustering_[idx];
					// Removing the index to remove
                    // std::cout<<"\n c : "<<c<< " , idx : "<<idx<<" , cluster removal = "<<cluster_removal<<std::endl;
                    // std::cout<<" chosen idx : "<<idx<<" and its distance : "<<u_[idx]<<std::endl;
                    // std::cout<<" [ ";
                    // for(int iter=0;iter<clustering_[cluster_removal].size(); iter++){
                    //     std::cout<<" "<<clustering_[cluster_removal][iter]<<" ";
                    // }
                    // std::cout<<" ]"<<std::endl;
					clustering_[cluster_removal].erase(std::remove(clustering_[cluster_removal].begin(), clustering_[cluster_removal].end(), idx), clustering_[cluster_removal].end());
                    // std::cout<<"[ ";
                    // for(int iter=0;iter<clustering_[cluster_removal].size(); iter++){
                    //     std::cout<<" "<<clustering_[cluster_removal][iter]<<" ";
                    // }
                    // std::cout<<" ]"<<std::endl;
				}
                else{
                copy_of_u.erase(argMax);
            // std::cout<< " copy_of_u : [ ";
            // for(int i=0; i<copy_of_u.size(); i++){
            //         std::cout<<" "<<copy_of_u[i];
            //         }
            //         std::cout<<" ] "<<std::endl;
                }
				increment+=1;

			}

			clustering_[c].push_back(idx);
			inv_clustering_[idx] = c;

			if(do_min){
				centroids_min_[c] = diagramToCentroid(current_bidder_diagrams_min_[idx]);
				centroids_with_price_min_[idx] = centroidWithZeroPrices(centroids_min_[c]);
			}
			if(do_sad){
				centroids_saddle_[c] = diagramToCentroid(current_bidder_diagrams_saddle_[idx]);
				centroids_with_price_saddle_[idx] = centroidWithZeroPrices(centroids_saddle_[c]);
			}
			if(do_max){
				centroids_max_[c] = diagramToCentroid(current_bidder_diagrams_max_[idx]);
				centroids_with_price_max_[idx] = centroidWithZeroPrices(centroids_max_[c]);
			}
		//resetDosToOriginalValues();
		}
	}
	return ;
}

template <typename dataType>
std::vector<dataType> PDClustering<dataType>::updateCentroidsPosition(){
	dataType max_shift = 0;
	dataType max_shift_c_min = 0;
	dataType max_shift_c_sad = 0;
	dataType max_shift_c_max = 0;
	dataType max_wasserstein_shift = 0;
	bool precision_min = true;
	bool precision_sad = true;
	bool precision_max = true;
	cost_ = 0;
    // std::cout<<"here 1"<<std::endl;
	for(int c=0; c<k_; ++c){
	    if(clustering_[c].size()>0){
		std::vector<GoodDiagram<dataType> > centroids_with_price_min, centroids_with_price_sad, centroids_with_price_max;
		int count = 0;
		for(int idx : clustering_[c]){
			int number_of_points = 0;
			// Find the position of diagrams[idx] in old cluster c
			vector<int>::iterator i = std::find(old_clustering_[c].begin(), old_clustering_[c].end (), idx);
			int pos = (i==old_clustering_[c].end()) ? -1 : std::distance(old_clustering_[c].begin(), i);
			if(pos>=0){
				// Diagram was already linked to this centroid before
				if(do_min_){
					centroids_with_price_min.push_back(centroids_with_price_min_[idx]);
					number_of_points += centroids_with_price_min_[idx].size() + current_bidder_diagrams_min_[idx].size();
				}
				if(do_sad_){
					centroids_with_price_sad.push_back(centroids_with_price_saddle_[idx]);
					number_of_points += centroids_with_price_saddle_[idx].size() + current_bidder_diagrams_saddle_[idx].size();
				}
				if(do_max_){
					centroids_with_price_max.push_back(centroids_with_price_max_[idx]);
					number_of_points += centroids_with_price_max_[idx].size() + current_bidder_diagrams_max_[idx].size();
				}
			}
			else{
				// Otherwise, centroid is given 0 prices and the diagram is given 0 diagonal-prices
				if(do_min_){
					centroids_with_price_min.push_back(centroidWithZeroPrices( centroids_min_[c] ));
					current_bidder_diagrams_min_[idx] = diagramWithZeroPrices(current_bidder_diagrams_min_[idx]);
					number_of_points += centroids_with_price_min_[idx].size() + current_bidder_diagrams_min_[idx].size();
				}
				if(do_sad_){
					centroids_with_price_sad.push_back(centroidWithZeroPrices( centroids_saddle_[c] ));
					current_bidder_diagrams_saddle_[idx] = diagramWithZeroPrices(current_bidder_diagrams_saddle_[idx]);
					number_of_points += centroids_with_price_saddle_[idx].size() + current_bidder_diagrams_saddle_[idx].size();
				}
				if(do_max_){
					centroids_with_price_max.push_back(centroidWithZeroPrices( centroids_max_[c] ));
					current_bidder_diagrams_max_[idx] = diagramWithZeroPrices(current_bidder_diagrams_max_[idx]);
					number_of_points += centroids_with_price_max_[idx].size() + current_bidder_diagrams_max_[idx].size();
				}

				if(n_iterations_>1){
					// If diagram new to cluster and we're not at first iteration,
					// precompute prices for the objects via compute_distance()
					number_of_points /= (int) do_min_ + (int) do_sad_ +  (int) do_max_;
					dataType d_estimated = pow(cost_/numberOfInputs_, 1./wasserstein_)+ 1e-7;
					// We use pointer in the auction in order to keep the prices at the end

					if(do_min_){
                        dataType estimated_delta_lim = 2 * number_of_points * epsilon_[0] / d_estimated;
                        if(estimated_delta_lim>1){
                            estimated_delta_lim=1;
                        }
						computeDistance(&(current_bidder_diagrams_min_[idx]), &(centroids_with_price_min[count]), estimated_delta_lim);
					}
					if(do_sad_){
                        dataType estimated_delta_lim = 2 * number_of_points * epsilon_[1] / d_estimated;
                        if(estimated_delta_lim>1){
                            estimated_delta_lim=1;
                        }
						computeDistance(&(current_bidder_diagrams_saddle_[idx]), &(centroids_with_price_sad[count]), estimated_delta_lim);
					}
					if(do_max_){
                        dataType estimated_delta_lim = 2 * number_of_points * epsilon_[2] / d_estimated;
                        if(estimated_delta_lim>1){
                            estimated_delta_lim=1;
                        }
						computeDistance(&(current_bidder_diagrams_max_[idx]), &(centroids_with_price_max[count]), estimated_delta_lim);
					}

				}
			}
			count++;
		}
    // std::cout<<"here 2"<<std::endl;
		std::vector<BidderDiagram<dataType>> diagrams_c_min, diagrams_c_sad, diagrams_c_max;
		dataType total_cost = 0;
		dataType wasserstein_shift = 0;

                if (do_min_) {
                  cout << "starting bullshit before matchings computations"
                       << endl;
                  std::vector<int> sizes;

                  if (barycenter_inputs_reset_flag) {
                  	cout<<"resetting inputs bec of flag"<<endl;
                    std::vector<BidderDiagram<dataType>> diagrams_c_min;
                    for (int idx : clustering_[c]) {
                      diagrams_c_min.push_back(
                          current_bidder_diagrams_min_[idx]);
                    }
                    sizes.resize(diagrams_c_min.size());
                    for (unsigned int i = 0; i < diagrams_c_min.size(); i++) {
                      sizes[i] = diagrams_c_min[i].size();
                    }
                    barycenter_computer_min_[c].setNumberOfInputs(diagrams_c_min.size());
                    barycenter_computer_min_[c].setCurrentBidders(diagrams_c_min);
                    cout << "all done" << endl;
                  } else {
                    cout << "keeping same inputs" << endl;
                    sizes.resize(
                        barycenter_computer_min_[c].getCurrentBidders().size());
                    for (unsigned int i = 0;
                         i <
                         barycenter_computer_min_[c].getCurrentBidders().size();
                         i++) {
                      sizes[i] = barycenter_computer_min_[c]
                                     .getCurrentBidders()
                                     .at(i)
                                     .size();
                    }
                  }
                  cout << "more bs" << endl;
                  std::vector<dataType> min_diag_price(barycenter_computer_min_[c].getCurrentBidders().size());
                  std::vector<dataType> min_price(barycenter_computer_min_[c].getCurrentBidders().size());
                  for (unsigned int i = 0; i < barycenter_computer_min_[c].getCurrentBidders().size(); i++) {
                    min_diag_price[i] = 0;
                    min_price[i] = 0;
                  }
cout << "min diag prices and all done" << endl;
                  std::pair<KDTree<dataType> *, std::vector<KDTree<dataType> *>>
                      pair;
                  bool use_kdt = false;
                  if (centroids_with_price_min[0].size() > 0) {
                    pair = barycenter_computer_min_[c].getKDTree();
                    use_kdt = true;
                  }
                  KDTree<dataType> *kdt = pair.first;
                  std::vector<KDTree<dataType> *> &correspondance_kdt_map = pair.second;

                  std::vector<std::vector<matchingTuple>> all_matchings(diagrams_c_min.size());
                  std::cout << "min : run matchings" << std::endl;
                  barycenter_computer_min_[c].runMatching(
                      &total_cost, epsilon_[0], sizes, kdt,
                      &correspondance_kdt_map, &min_diag_price, &min_price,
                      &all_matchings, use_kdt);
                  std::cout<<"min : runned, now updating barycenter"<<std::endl;
                  precision_min =
                      barycenter_computer_min_[c].isPrecisionObjectiveMet();
                  cost_min_ = total_cost;
                  max_shift_c_min =
                      barycenter_computer_min_[c].updateBarycenter(
                          all_matchings);
                  // std::cout<<"min : barycenter updated"<<std::endl;
                  if (max_shift_c_min > max_shift) {
                    max_shift = max_shift_c_min;
                  }

                  // Now that barycenters and diagrams are updated in
                  // PDBarycenter class, we import the results here.
                  diagrams_c_min =
                      barycenter_computer_min_[c].getCurrentBidders();
                  centroids_with_price_min =
                      barycenter_computer_min_[c].getCurrentBarycenter();
                  int i = 0;
                  for (int idx : clustering_[c]) {
                    current_bidder_diagrams_min_[idx] = diagrams_c_min[i];
                    centroids_with_price_min_[idx] =
                        centroids_with_price_min[i];
                    i++;
                  }

                  GoodDiagram<dataType> old_centroid = centroids_min_[c];
                  centroids_min_[c] = centroidWithZeroPrices(
                      centroids_with_price_min_[clustering_[c][0]]);
                  // std::cout<<"yo"<<std::endl;
                  if (use_accelerated_)
                    wasserstein_shift +=
                        computeDistance(old_centroid, centroids_min_[c], 0.01);

                  delete kdt;
                }

    // std::cout<<"here 3"<<std::endl;
		if(do_sad_){
			total_cost=0;
            std::vector<int> sizes;


			if(barycenter_inputs_reset_flag){
			    std::vector<BidderDiagram<dataType>> diagrams_c_min;
                for(int idx : clustering_[c]){
                    diagrams_c_min.push_back(current_bidder_diagrams_saddle_[idx]);
                }
                sizes.resize(diagrams_c_min.size());
                for(unsigned int i=0; i<diagrams_c_min.size(); i++){
                    sizes[i] = diagrams_c_min[i].size();
                }
				barycenter_computer_sad_[c].setNumberOfInputs(diagrams_c_min.size());
				barycenter_computer_sad_[c].setCurrentBidders(diagrams_c_min);
			}
			else{
                sizes.resize(barycenter_computer_sad_[c].getCurrentBidders().size());
                for(unsigned int i=0; i<barycenter_computer_sad_[c].getCurrentBidders().size(); i++){
                    sizes[i] = barycenter_computer_sad_[c].getCurrentBidders().at(i).size();
                }
			}

			std::vector<dataType> min_diag_price(barycenter_computer_sad_[c].getCurrentBidders().size());
			std::vector<dataType> min_price(barycenter_computer_sad_[c].getCurrentBidders().size());
			for(unsigned int i=0; i<barycenter_computer_sad_[c].getCurrentBidders().size(); i++){
				min_diag_price[i] = 0;
				min_price[i] = 0;
			}
			
			std::pair<KDTree<dataType>*, std::vector<KDTree<dataType>*>> pair;
			bool use_kdt = false;
			if(centroids_with_price_sad[0].size()>0){
				pair = barycenter_computer_sad_[c].getKDTree();
				use_kdt=true;
			}
			KDTree<dataType>* kdt = pair.first;
			std::vector<KDTree<dataType>*>& correspondance_kdt_map = pair.second;


			std::vector<std::vector<matchingTuple>> all_matchings(diagrams_c_sad.size());
            // std::cout<<"sad : run matchings"<<std::endl;
			barycenter_computer_sad_[c].runMatching(&total_cost,
										epsilon_[1],
										sizes,
										kdt,
										&correspondance_kdt_map,
										&min_diag_price,
										&min_price,
										&all_matchings,
										use_kdt);
	    precision_sad = barycenter_computer_sad_[c].isPrecisionObjectiveMet();
            // std::cout<<"sad : runned, now updating barycenter"<<std::endl; 
			 max_shift_c_sad = barycenter_computer_sad_[c].updateBarycenter(all_matchings);
		    cost_sad_ += total_cost;
            // std::cout<<"sad : barycenter updated"<<std::endl; 
			if(max_shift_c_sad>max_shift){
				max_shift = max_shift_c_sad;
			}

			// Now that barycenters and diagrams are updated in PDBarycenter class,
			// we import the results here.
			diagrams_c_sad = barycenter_computer_sad_[c].getCurrentBidders();
			centroids_with_price_sad = barycenter_computer_sad_[c].getCurrentBarycenter();
			int i = 0;
			for(int idx : clustering_[c]){
				current_bidder_diagrams_saddle_[idx] = diagrams_c_sad[i];
				centroids_with_price_saddle_[idx] = centroids_with_price_sad[i];
				i++;
			}
			GoodDiagram<dataType> old_centroid = centroids_saddle_[c];
			centroids_saddle_[c] = centroidWithZeroPrices(centroids_with_price_saddle_[clustering_[c][0]]);
			if(use_accelerated_) wasserstein_shift += computeDistance(old_centroid, centroids_saddle_[c], 0.01);
			
			delete kdt;
		}

    // std::cout<<"here 4"<<std::endl;
		if(do_max_){
            total_cost =0;
            std::vector<int> sizes;
            if (barycenter_inputs_reset_flag) {
              std::vector<BidderDiagram<dataType>> diagrams_c_min;
              for (int idx : clustering_[c]) {
                diagrams_c_min.push_back(current_bidder_diagrams_max_[idx]);
              }
              sizes.resize(diagrams_c_min.size());
              for (unsigned int i = 0; i < diagrams_c_min.size(); i++) {
                sizes[i] = diagrams_c_min[i].size();
              }
              barycenter_computer_max_[c].setNumberOfInputs(
                  diagrams_c_min.size());
              barycenter_computer_max_[c].setCurrentBidders(diagrams_c_min);
            } else {
              sizes.resize(
                  barycenter_computer_max_[c].getCurrentBidders().size());
              for (unsigned int i = 0;
                   i < barycenter_computer_max_[c].getCurrentBidders().size();
                   i++) {
                sizes[i] = barycenter_computer_max_[c]
                               .getCurrentBidders()
                               .at(i)
                               .size();
              }
            }

                        std::vector<dataType> min_diag_price(barycenter_computer_max_[c].getCurrentBidders().size());
			std::vector<dataType> min_price(barycenter_computer_max_[c].getCurrentBidders().size());
			for(unsigned int i=0; i<barycenter_computer_max_[c].getCurrentBidders().size(); i++){
				min_diag_price[i] = 0;
				min_price[i] = 0;
			}
			std::pair<KDTree<dataType>*, std::vector<KDTree<dataType>*>> pair;
			bool use_kdt = false;
			if(centroids_with_price_max[0].size()>0){
				pair = barycenter_computer_max_[c].getKDTree();
				use_kdt=true;
			}
			KDTree<dataType>* kdt = pair.first;
			std::vector<KDTree<dataType>*>& correspondance_kdt_map = pair.second;

			std::vector<std::vector<matchingTuple>> all_matchings(diagrams_c_max.size());

            // std::cout<<"max : run matchings"<<std::endl;
			barycenter_computer_max_[c].runMatching(&total_cost,
										epsilon_[2],
										sizes,
										kdt,
										&correspondance_kdt_map,
										&min_diag_price,
										&min_price,
										&all_matchings,
										use_kdt);
	    precision_max = barycenter_computer_max_[c].isPrecisionObjectiveMet();
            // std::cout<<"max : runned, now updating barycenter"<<std::endl; 
			cost_max_ += total_cost;
			 max_shift_c_max = barycenter_computer_max_[c].updateBarycenter(all_matchings);
            // std::cout<<"max: barycenter updated"<<std::endl; 
			if(max_shift_c_max>max_shift){
				max_shift = max_shift_c_max;
			}

			// Now that barycenters and diagrams are updated in PDBarycenter class,
			// we import the results here.
			diagrams_c_max = barycenter_computer_max_[c].getCurrentBidders();
			centroids_with_price_max = barycenter_computer_max_[c].getCurrentBarycenter();
			int i = 0;
			for(int idx : clustering_[c]){
				current_bidder_diagrams_max_[idx] = diagrams_c_max[i];
				centroids_with_price_max_[idx] = centroids_with_price_max[i];
				i++;
			}
			GoodDiagram<dataType> old_centroid = centroids_max_[c];
			centroids_max_[c] = centroidWithZeroPrices(centroids_with_price_max_[clustering_[c][0]]);
			if(use_accelerated_) wasserstein_shift += computeDistance(old_centroid, centroids_max_[c], 0.01);

			delete kdt;
			// for(unsigned int i_kdt=0; i_kdt<correspondance_kdt_map.size(); i_kdt++){
			//     delete correspondance_kdt_map[i_kdt];
			// }
		}

    cost_ = cost_min_ + cost_sad_ + cost_max_;
        // std::cout<<"here"<<std::endl;
    if(wasserstein_shift>max_wasserstein_shift){
			max_wasserstein_shift = wasserstein_shift;
		}
        // std::cout<<"there"<<std::endl;
    if(use_accelerated_){
        for(int i=0; i< numberOfInputs_; ++i){
        // Step 5 of Accelerated KMeans: Update the lower bound on distance thanks to the triangular inequality
            l_[i][c] = pow( pow(l_[i][c], 1./wasserstein_) - pow(wasserstein_shift, 1./wasserstein_), wasserstein_);
            if(l_[i][c]<0){
                l_[i][c] = 0;
            }
        }
        for(int idx : clustering_[c]){
            // Step 6, update the upper bound on the distance to the centroid thanks to the triangle inequality
            u_[idx] = pow( pow(u_[idx], 1./wasserstein_) + pow(wasserstein_shift, 1./wasserstein_), wasserstein_);
            r_[idx] = true;
        }
    }
    }
	}
	// Normally return max_shift, but it seems there is a bug
	// yielding max_shift > 100 * max_wasserstein_shift
	// which should logically not really happen...
	// This is supposed to be only a temporary patch...
    // std::cout<<"shifts : "<<max_shift<<" "<<max_wasserstein_shift<<std::endl;
    std::vector<dataType> max_shift_vector(3);
    max_shift_vector[0] = max_shift_c_min;
    max_shift_vector[1] = max_shift_c_sad;
    max_shift_vector[2] = max_shift_c_max;

    precision_criterion_ = precision_min && precision_sad && precision_max;
    barycenter_inputs_reset_flag = false;
    return max_shift_vector; //std::min(max_shift, max_wasserstein_shift);
}


template <typename dataType>
void PDClustering<dataType>::setBidderDiagrams(){

	for(int i=0; i<numberOfInputs_; i++){
		if(do_min_){
			std::vector<diagramTuple> *CTDiagram = &((*inputDiagramsMin_)[i]);
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
			bidder_diagrams_min_.push_back(bidders);
			current_bidder_diagrams_min_.push_back(BidderDiagram<dataType>());
			centroids_with_price_min_.push_back(GoodDiagram<dataType>());
		}

		if(do_sad_){
			std::vector<diagramTuple> *CTDiagram = &((*inputDiagramsSaddle_)[i]);

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
			bidder_diagrams_saddle_.push_back(bidders);
			current_bidder_diagrams_saddle_.push_back(BidderDiagram<dataType>());
			centroids_with_price_saddle_.push_back(GoodDiagram<dataType>());
		}

		if(do_max_){
			std::vector<diagramTuple> *CTDiagram = &((*inputDiagramsMax_)[i]);

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
			bidder_diagrams_max_.push_back(bidders);
			current_bidder_diagrams_max_.push_back(BidderDiagram<dataType>());
			centroids_with_price_max_.push_back(GoodDiagram<dataType>());
		}

	}
	return;
}


template <typename dataType>
std::vector<dataType> PDClustering<dataType>::enrichCurrentBidderDiagrams(std::vector<dataType>previous_min_persistence, 
                                                            std::vector<dataType> min_persistence,
                                                            std::vector<std::vector<dataType>> initial_diagonal_prices, 
                                                            std::vector<std::vector<dataType>> initial_off_diagonal_prices,
                                                            int min_points_to_add,
                                                            bool add_points_to_barycenter){

    std::vector<dataType> new_min_persistence = min_persistence;

  // 1. Get size of the largest current diagram, deduce the maximal number of
  // points to append
    int max_diagram_size_min =0;
    int max_diagram_size_sad =0;
    int max_diagram_size_max =0;
	if(do_min_){
		for(int i=0; i<numberOfInputs_; i++){
			if(current_bidder_diagrams_min_[i].size()>max_diagram_size_min){
				max_diagram_size_min = current_bidder_diagrams_min_[i].size();
			}
		}
	}
	if(do_sad_){
		for(int i=0; i<numberOfInputs_; i++){
			if(current_bidder_diagrams_saddle_[i].size()>max_diagram_size_sad){
				max_diagram_size_sad = current_bidder_diagrams_saddle_[i].size();
			}
		}
	}
	if(do_max_){
		for(int i=0; i<numberOfInputs_; i++){
			if(current_bidder_diagrams_max_[i].size()>max_diagram_size_max){
				max_diagram_size_max = current_bidder_diagrams_max_[i].size();
			}
		}
	}
	int max_points_to_add_min = std::max(min_points_to_add, min_points_to_add + (int) (max_diagram_size_min/10));
	int max_points_to_add_sad = std::max(min_points_to_add, min_points_to_add + (int) (max_diagram_size_sad/10));
	int max_points_to_add_max = std::max(min_points_to_add, min_points_to_add + (int) (max_diagram_size_max/10));

	// 2. Get which points can be added, deduce the new minimal persistence
	std::vector<std::vector<int>> candidates_to_be_added_min(numberOfInputs_);
	std::vector<std::vector<int>> candidates_to_be_added_sad(numberOfInputs_);
	std::vector<std::vector<int>> candidates_to_be_added_max(numberOfInputs_);
	std::vector<std::vector<int>> idx_min(numberOfInputs_);
	std::vector<std::vector<int>> idx_sad(numberOfInputs_);
	std::vector<std::vector<int>> idx_max(numberOfInputs_);

	if(do_min_){
		for(int i=0; i<numberOfInputs_; i++){
			std::vector<dataType> persistences;
			for(int j=0; j<bidder_diagrams_min_[i].size(); j++){
				Bidder<dataType> b = bidder_diagrams_min_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence>=min_persistence[0] && persistence<=previous_min_persistence[0]){
					candidates_to_be_added_min[i].push_back(j);
					idx_min[i].push_back(idx_min[i].size());
					persistences.push_back(persistence);
				}
			}
			sort(idx_min[i].begin(), idx_min[i].end(), [&persistences](int& a, int& b){
				return ((persistences[a] > persistences[b])
				||((persistences[a] == persistences[b])&&(a > b)));
				});
			int size =  candidates_to_be_added_min[i].size();
			if(size>=max_points_to_add_min){
				dataType last_persistence_added_min = persistences[idx_min[i][max_points_to_add_min-1]];
				if(last_persistence_added_min>new_min_persistence[0]){
					new_min_persistence[0] = last_persistence_added_min;
				}
			}
		}
	}

	if(do_sad_){
		for(int i=0; i<numberOfInputs_; i++){
			std::vector<dataType> persistences;
			for(int j=0; j<bidder_diagrams_saddle_[i].size(); j++){
				Bidder<dataType> b = bidder_diagrams_saddle_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence>=min_persistence[1] && persistence<=previous_min_persistence[1]){
					candidates_to_be_added_sad[i].push_back(j);
					idx_sad[i].push_back(idx_sad[i].size());
					persistences.push_back(persistence);
				}
			}
			sort(idx_sad[i].begin(), idx_sad[i].end(), [&persistences](int& a, int& b){
				return ((persistences[a] > persistences[b])
				||((persistences[a] == persistences[b])&&(a > b)));
				});
			int size =  candidates_to_be_added_sad[i].size();
			if(size>=max_points_to_add_sad){
				dataType last_persistence_added_sad = persistences[idx_sad[i][max_points_to_add_sad-1]];
				if(last_persistence_added_sad>new_min_persistence[1]){
					new_min_persistence[1] = last_persistence_added_sad;
				}
			}
		}
	}
	if(do_max_){
		for(int i=0; i<numberOfInputs_; i++){
			std::vector<dataType> persistences;
			for(int j=0; j<bidder_diagrams_max_[i].size(); j++){
				Bidder<dataType> b = bidder_diagrams_max_[i].get(j);
				dataType persistence = b.getPersistence();
				if(persistence>=min_persistence[2] && persistence<=previous_min_persistence[2]){
					candidates_to_be_added_max[i].push_back(j);
					idx_max[i].push_back(idx_max[i].size());
					persistences.push_back(persistence);
				}
			}
			sort(idx_max[i].begin(), idx_max[i].end(), [&persistences](int& a, int& b){
				return ((persistences[a] > persistences[b])
				||((persistences[a] == persistences[b])&&(a > b)));
				});
			int size =  candidates_to_be_added_max[i].size();
			if(size>=max_points_to_add_max){
				dataType last_persistence_added_max = persistences[idx_max[i][max_points_to_add_max-1]];
				if(last_persistence_added_max>new_min_persistence[2]){
					new_min_persistence[2] = last_persistence_added_max;
				}
			}
		}
	}

	// 3. Add the points to the current diagrams
	if(do_min_){
        int compteur_for_adding_points = 0;
		for(int i=0; i<numberOfInputs_; i++){
			int size =  candidates_to_be_added_min[i].size();
			for(int j=0; j<std::min(max_points_to_add_min, size); j++){
				Bidder<dataType> b = bidder_diagrams_min_[i].get(candidates_to_be_added_min[i][idx_min[i][j]]);
				dataType persistence = b.getPersistence();
				if(persistence>=new_min_persistence[0]){
					b.id_ = current_bidder_diagrams_min_[i].size();
					b.setPositionInAuction(current_bidder_diagrams_min_[i].size());
					b.setDiagonalPrice(initial_diagonal_prices[0][i]);
					current_bidder_diagrams_min_[i].addBidder(b);

					if(use_accelerated_ && n_iterations_>0){
						for(int c=0; c<k_; ++c){
							// Step 5 of Accelerated KMeans: Update the lower bound on distance thanks to the triangular inequality
							l_[i][c] = pow( pow(l_[i][c], 1./wasserstein_) - persistence/pow(2, 0.5), wasserstein_);
							if(l_[i][c]<0){
								l_[i][c] = 0;
							}
						}
						// Step 6, update the upper bound on the distance to the centroid thanks to the triangle inequality
						u_[i] = pow( pow(u_[i], 1./wasserstein_) + persistence/pow(2, 0.5), wasserstein_);
						r_[i] = true;
					}
                    int to_be_added_to_barycenter = deterministic_ ?  compteur_for_adding_points % numberOfInputs_ : rand() % numberOfInputs_;
                    if(to_be_added_to_barycenter==0 && add_points_to_barycenter){
                         // std::cout << "here we are adding points to the centroid_min" << std::endl;
                        for(int k=0; k<numberOfInputs_; k++){
                            if(inv_clustering_[i]==inv_clustering_[k]){
                                // std::cout<< "index "<<centroids_with_price_min_[k].size()<<std::endl;
                                Good<dataType> g = Good<dataType>(b.x_, b.y_, false, centroids_with_price_min_[k].size());
                                g.setPrice(initial_off_diagonal_prices[0][k]);
                                g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
                                centroids_with_price_min_[k].addGood(g);
                                // std::cout<<"added for "<<k<<std::endl;
                            }

                        }
                             // std::cout<<"size of centroid "<<centroids_min_[inv_clustering_[i]].size()<<std::endl;
                            Good<dataType> g = Good<dataType>(b.x_, b.y_, false, centroids_min_[inv_clustering_[i]].size());
                            g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
                            centroids_min_[inv_clustering_[i]].addGood(g);
                         // std::cout << "all added" << std::endl;
                    }
				}
				compteur_for_adding_points++;

			}
			if(debugLevel_>5)
			    std::cout<< " Diagram " << i << " size : " << current_bidder_diagrams_min_[i].size() << std::endl;
		}
	}
	if(do_sad_){
        int compteur_for_adding_points = 0;
		for(int i=0; i<numberOfInputs_; i++){
			int size =  candidates_to_be_added_sad[i].size();
			for(int j=0; j<std::min(max_points_to_add_sad, size); j++){
				Bidder<dataType> b = bidder_diagrams_saddle_[i].get(candidates_to_be_added_sad[i][idx_sad[i][j]]);
				dataType persistence = b.getPersistence();
				if(persistence>=new_min_persistence[1]){
					b.id_ = current_bidder_diagrams_saddle_[i].size();
					b.setPositionInAuction(current_bidder_diagrams_saddle_[i].size());
					b.setDiagonalPrice(initial_diagonal_prices[1][i]);
					current_bidder_diagrams_saddle_[i].addBidder(b);

					if(use_accelerated_ && n_iterations_>0){
						for(int c=0; c<k_; ++c){
							// Step 5 of Accelerated KMeans: Update the lower bound on distance thanks to the triangular inequality
							l_[i][c] = pow( pow(l_[i][c], 1./wasserstein_) - persistence/pow(2, 0.5), wasserstein_);
							if(l_[i][c]<0){
								l_[i][c] = 0;
							}
						}
						// Step 6, update the upper bound on the distance to the centroid thanks to the triangle inequality
						u_[i] = pow( pow(u_[i], 1./wasserstein_) + persistence/pow(2, 0.5), wasserstein_);
						r_[i] = true;
					}
					int to_be_added_to_barycenter = deterministic_ ?  compteur_for_adding_points % numberOfInputs_ : rand() % numberOfInputs_;
                    if(to_be_added_to_barycenter==0 && add_points_to_barycenter){
                        // std::cout << "here we are adding points to the centroid_min" << std::endl;
                        for(int k=0; k<numberOfInputs_; k++){
                            if(inv_clustering_[i]==inv_clustering_[k]){
                                Good<dataType> g = Good<dataType>(b.x_, b.y_, false, centroids_with_price_saddle_[k].size());
                                g.setPrice(initial_off_diagonal_prices[1][k]);
                                g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
                                centroids_with_price_saddle_[k].addGood(g);
                                // std::cout<<"added for "<<k<<std::endl;
                            }
                        }
                        // std::cout << "all added" << std::endl;
                    }
				}
				compteur_for_adding_points++;
			}
			if(debugLevel_>5)
                std::cout<< " Diagram " << i << " size : " << current_bidder_diagrams_saddle_[i].size() << std::endl;
		}
	}
	if(do_max_){
        int compteur_for_adding_points = 0;
		for(int i=0; i<numberOfInputs_; i++){
			int size =  candidates_to_be_added_max[i].size();
			for(int j=0; j<std::min(max_points_to_add_max, size); j++){
				Bidder<dataType> b = bidder_diagrams_max_[i].get(candidates_to_be_added_max[i][idx_max[i][j]]);
				dataType persistence = b.getPersistence();
				if(persistence>=new_min_persistence[2]){
					b.id_ = current_bidder_diagrams_max_[i].size();
					b.setPositionInAuction(current_bidder_diagrams_max_[i].size());
					b.setDiagonalPrice(initial_diagonal_prices[2][i]);
					current_bidder_diagrams_max_[i].addBidder(b);

					if(use_accelerated_ && n_iterations_>0){
						for(int c=0; c<k_; ++c){
							// Step 5 of Accelerated KMeans: Update the lower bound on distance thanks to the triangular inequality
							l_[i][c] = pow( pow(l_[i][c], 1./wasserstein_) - persistence/pow(2, 0.5), wasserstein_);
							if(l_[i][c]<0){
								l_[i][c] = 0;
							}
						}
						// Step 6, update the upper bound on the distance to the centroid thanks to the triangle inequality
						u_[i] = pow( pow(u_[i], 1./wasserstein_) + persistence/pow(2, 0.5), wasserstein_);
						r_[i] = true;
					}	
					int to_be_added_to_barycenter = deterministic_ ?  compteur_for_adding_points % numberOfInputs_ : rand() % numberOfInputs_;
                    if(to_be_added_to_barycenter==0 && add_points_to_barycenter){
                        // std::cout << "here we are adding points to the centroid_max" << std::endl;
                        for(int k=0; k<numberOfInputs_; k++){
                            if(inv_clustering_[i]==inv_clustering_[k]){
                                Good<dataType> g = Good<dataType>(b.x_, b.y_, false, centroids_with_price_max_[k].size());
                                g.setPrice(initial_off_diagonal_prices[2][k]);
                                g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
                                centroids_with_price_max_[k].addGood(g);
                                // std::cout<<"added for "<<k<<std::endl;
                            }
                        }
                        Good<dataType> g = Good<dataType>(b.x_, b.y_, false, centroids_max_[inv_clustering_[i]].size());
                        g.SetCriticalCoordinates(b.coords_x_, b.coords_y_, b.coords_z_);
                        centroids_max_[inv_clustering_[i]].addGood(g);
                        // std::cout << "all added" << std::endl;
                    }
				}
				compteur_for_adding_points++;
			}
			if(debugLevel_>5)
                std::cout<< " Diagram " << i << " size : " << current_bidder_diagrams_max_[i].size() << std::endl;
		}
	}

	return new_min_persistence;
}

template<typename dataType>
void PDClustering<dataType>::initializeBarycenterComputers(){
    cout << "initializing barys" << endl;
    if(do_min_){
    	cout<<"here0"<<endl;
	barycenter_computer_min_.resize(k_);
	for (int c = 0; c < k_; c++) {
	    std::vector<BidderDiagram<dataType>> diagrams_c;
	    cout<<"here1"<<endl;
	    for (int idx : clustering_[c]) {
		diagrams_c.push_back(current_bidder_diagrams_min_[idx]);
	    }
	    cout<<"here2"<<endl;
	    barycenter_computer_min_[c]= PDBarycenter<dataType>();
	    barycenter_computer_min_[c].setThreadNumber(threadNumber_);
	    barycenter_computer_min_[c].setWasserstein(wasserstein_);
	    barycenter_computer_min_[c].setDiagramType(0);
	    barycenter_computer_min_[c].setUseProgressive(false);
	    barycenter_computer_min_[c].setDeterministic(true);
	    barycenter_computer_min_[c].setGeometricalFactor(geometrical_factor_);
	    barycenter_computer_min_[c].setDebugLevel(debugLevel_);
	    cout<<"here3"<<endl;
	    barycenter_computer_min_[c].setNumberOfInputs(diagrams_c.size());
	    cout<<"here4"<<endl;
	    barycenter_computer_min_[c].setCurrentBidders(diagrams_c);
	    cout<<"here5"<<endl;
	    barycenter_computer_min_[c].setInitialBarycenter(0);
	    cout<<"here6"<<endl;
	    // barycenter_computer_min_[c].setNumberOfInputs(diagrams_c_min.size());
	    // barycenter_computer_min_[c].setCurrentBidders(diagrams_c_min);
	    // barycenter_computer_min_[c].setCurrentBarycenter(centroids_with_price_min);
	}
    }
    if(do_sad_){
	barycenter_computer_sad_.resize(k_);
	for (int c = 0; c < k_; c++) {
	    std::vector<BidderDiagram<dataType>> diagrams_c;
	    for (int idx : clustering_[c]) {
		diagrams_c.push_back(current_bidder_diagrams_saddle_[idx]);
	    }
	    barycenter_computer_sad_[c]= PDBarycenter<dataType>();
	    barycenter_computer_sad_[c].setThreadNumber(threadNumber_);
	    barycenter_computer_sad_[c].setWasserstein(wasserstein_);
	    barycenter_computer_sad_[c].setDiagramType(0);
	    barycenter_computer_sad_[c].setUseProgressive(false);
	    barycenter_computer_sad_[c].setDeterministic(true);
	    barycenter_computer_sad_[c].setGeometricalFactor(geometrical_factor_);
	    barycenter_computer_sad_[c].setDebugLevel(debugLevel_);
	    barycenter_computer_sad_[c].setNumberOfInputs(diagrams_c.size());
	    barycenter_computer_sad_[c].setCurrentBidders(diagrams_c);
	    barycenter_computer_sad_[c].setInitialBarycenter(0);
	    // barycenter_computer_sad_[c].setNumberOfInputs(diagrams_c_min.size());
	    // barycenter_computer_sad_[c].setCurrentBidders(diagrams_c_min);
	    // barycenter_computer_sad_[c].setCurrentBarycenter(centroids_with_price_min);
	}
    }
    if(do_max_){
	barycenter_computer_max_.resize(k_);
	for (int c = 0; c < k_; c++) {
	    std::vector<BidderDiagram<dataType>> diagrams_c;
	    for (int idx : clustering_[c]) {
		diagrams_c.push_back(current_bidder_diagrams_max_[idx]);
	    }
	    barycenter_computer_max_[c]= PDBarycenter<dataType>();
	    barycenter_computer_max_[c].setThreadNumber(threadNumber_);
	    barycenter_computer_max_[c].setWasserstein(wasserstein_);
	    barycenter_computer_max_[c].setDiagramType(0);
	    barycenter_computer_max_[c].setUseProgressive(false);
	    barycenter_computer_max_[c].setDeterministic(true);
	    barycenter_computer_max_[c].setGeometricalFactor(geometrical_factor_);
	    barycenter_computer_max_[c].setDebugLevel(debugLevel_);
	    barycenter_computer_max_[c].setNumberOfInputs(diagrams_c.size());
	    barycenter_computer_max_[c].setCurrentBidders(diagrams_c);
	    barycenter_computer_max_[c].setInitialBarycenter(0);
	    // barycenter_computer_max_[c].setNumberOfInputs(diagrams_c_min.size());
	    // barycenter_computer_max_[c].setCurrentBidders(diagrams_c_min);
	    // barycenter_computer_max_[c].setCurrentBarycenter(centroids_with_price_min);
	}
    }
    barycenter_inputs_reset_flag=true;
}

#endif