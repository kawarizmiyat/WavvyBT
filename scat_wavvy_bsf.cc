//TODO include other necessary header files.
#include "scat_wavvy_bsf.h"



// constructor:
ScatFormWavvy::ScatFormWavvy(BTNode *n):
    ScatFormator(n),
    id_(-1),
    my_status(INIT),
    current_iteration(0),
    my_role(NONE),
    finishing_time (0) {


    // Bluetooth layers configuration:
    node_->lmp_->single_page = 1;
    destroy_temp_pico = 1;
    node_->lmp_->HCI_Write_Page_Scan_Activity(8196, 8196);
    node_->lmp_->HCI_Write_Scan_Enable(1);

    // handling busy and waiting ..
    busyCond_  = false;
    waitingDiscCounter_ = 0 ;


}

// adding neighbors to a given node.
void ScatFormWavvy::_addNeighbor(node_id id) {
    if (id > -1) {

        all_neighbors.insert_node(id);



        // adding neighbor clock in the Bd_info;
        // Test: Getting the clk and offset of neighbor & adding the data
        Bd_info *bd = new Bd_info(id, 0, 0);
        bd->offset_changed_ = 1;
        bd->dump();

        int isnew;
        bd = node_->lmp_->_add_bd_info(bd, &isnew);	// old bd may got deleted.
        if(!isnew) {
            fprintf(stderr, "*** WARNING: THERE MUST BE an ERROR in %s \n", __FUNCTION__);
            abort();
        }
        // Changing the clock and offset values of node id, to the actual ones
        node_->bb_->recalculateNInfo(id,1);

    }
}

// tracer:
inline bool ScatFormWavvy::trace_node(){
    return true;            // trace every node.
}

void ScatFormWavvy::init_scat_formation_algorithm() {
    this->all_neighbors.sort();
    this->seperate_all_neighbors();     // up and down neighbors are sorted as a result.
}

// main function ..
void ScatFormWavvy::_main() {

    init_scat_formation_algorithm();    // here: all nodes are already inserted into


    change_status(UP_TO_DOWN_WAIT);
}

// handle the change of the status - initialize the node for that.
// TODO - some states may require shuffling the neighbor list ..
// TODO - a data structure that get randomly - efficiently
//          (that is, insert, fetch, fetch random element) efficiently.
void ScatFormWavvy::change_status(status new_stat) {

    if (trace_node()) {
        fprintf(stderr, "%d chgd Status %s : %s at %f \n",
                id_,
                state_str(this->my_status),
                state_str(new_stat),
                Scheduler::instance().clock());
    }
    // change the status ..
    this->my_status = new_stat;


    // handle the new status:
    // if status require a large code to initiated .. then write a new function ..
    // called .. initiated_status_$nameofstate().
    switch(this->my_status) {

    case UP_TO_DOWN_WAIT:
        ex_round_messages();
        break;

    case UP_TO_DOWN_ACTION:
        ex_round_messages();
        break;

    case DOWN_TO_UP_WAIT:
        // clear_contact_info(&down_neighbors);

        down_neighbors.mark_contacted_all();
        ex_round_messages();
        break;

    case DOWN_TO_UP_ACTION:
        // clear_contact_info(&up_neighbors);

        up_neighbors.mark_contacted_all();
        ex_round_messages();

    case TERMINATE:
        this->finishing_time = Scheduler::instance().clock() ;
        if (trace_node()) {
            fprintf(stderr, "node %d terminated the algorithm at %2.3%f",
                    this->id_, this->finishing_time);
        }

    default:
        fprintf(stderr, "error: state  is unsupportable in %s  .. ", __FUNCTION__);
        abort();
    }


}



// the procedure of contacting all neighbors in a given state.
void ScatFormWavvy::ex_round_messages() {

    Scheduler & s = Scheduler::instance();
    s.cancel(&watchDogEv_);

    if (trace_node()) {
        fprintf(stderr, "%d is in %s at status %s \n",
                this->id_, __FUNCTION__,
                state_str(my_status));
    }

    switch (this->my_status){


    case UP_TO_DOWN_WAIT:

        // if (! is_all_contacted()) {
        if (! up_neighbors.is_all_contacted()) {
            wait_in_page_scan();


        } else {
            cancel_page_scan();
            change_status(UP_TO_DOWN_ACTION);
        }

        break;

    case UP_TO_DOWN_ACTION:
        if (! down_neighbors.is_all_contacted()) {

            // connect_page(find_promising_receiver());
            connect_page(down_neighbors.next_not_contacted().get_id());


        } else {
            change_status(TERMINATE);
            // change_status(DOWN_TO_UP_WAIT);
        }


        break ;

    default:
        fprintf(stderr, "**** Error: %d unsupported case %d \n", this->id_, this->my_status);
        abort();
        break;

    }
}


// we don't need any of these functions anymore ..
/*
node_id ScatFormWavvy::find_promising_receiver() {

    node_id return_addr;
    while (!is_promising(return_addr)) {
        increase_current_page_counter();
    }
    return return_addr;
}
bool ScatFormWavvy::is_promising(int& return_addr) {
    wavvy_neighbor* n = null;
    bool return_val;

    switch (this->my_status) {

        case UP_TO_DOWN_ACTION:
        n = &(this->down_neighbors.at(current_page_counter));
        return_val = !(n->contacted);
        break;

        case DOWN_TO_UP_ACTION:
        n = &(this->up_neighbors.at(current_page_counter));
        return_val = !(n->contacted);
        break;

        default:
        fprintf(stderr, "error in %d - status cannot be handled in %s", this->id_, __FUNCTION__);
        abort();
    }

    return_addr = n->id;
    return return_val;
}
void ScatFormWavvy::increase_current_page_counter() {
    switch(this->my_status) {
        case UP_TO_DOWN_ACTION:
        current_page_counter = (current_page_counter + 1) % this->down_neighbors.size();
        break;

        case DOWN_TO_UP_WAIT:
        current_page_counter = (current_page_counter + 1) % this->up_neighbors.size();
        break;

    default:
        fprintf(stderr, "error in %d - status cannot be handled in %s", this->id_, __FUNCTION__);
        abort();
    }
}
node_id ScatFormWavvy::find_promising_receiver() {

    switch (this->my_status) {

    case (UP_TO_DOWN_ACTION):
        for (nmap_iter it = down_neighbors.begin(); it != down_neighbors.end(); it++) {
            if (!it->second.contacted) return it->first;
        }
        break;

    case (DOWN_TO_UP_ACTION):
        for (nmap_iter it = up_neighbors.begin(); it != up_neighbors.end(); it++) {
            if (!it->second.contacted) return it->first;
        }
        break;

    default:
        fprintf(stderr, "error(%d): status is not supported in %s\n", this->id_, __FUNCTION__);
        abort();
        break;
    }

    fprintf(stderr, "warninig: most-likely we found that there are no promising neighbors ");
    fprintf(stderr, "in state %s of node %d - (%s)", state_str(this->my_status), this->id_,
            __FUNCTION__);
    abort();


}
*/


void ScatFormWavvy::connected(bd_addr_t rmt) {
    Scheduler & s = Scheduler::instance();

    s.cancel(&watchDogEv_);
    s.schedule(&timer_ , &watchDogEv_ , WATCH_DOG_TIMER );

    if (node_->lmp_->curPico && node_->lmp_->curPico->isMaster()){

        if (trace_node()) {
            fprintf(stderr, "CONNECTED: %d:%d at %f \n", id_, rmt, s.clock());
        }

        switch(this->my_status) {
        case UP_TO_DOWN_ACTION:
            initiate_connected_up_to_down_action(rmt);
            break;

        case DOWN_TO_UP_ACTION:
            initiate_connected_down_to_up_action(rmt);
            break;

        default:
            fprintf(stderr, "error(%d): satatus is not handled by %s \n", this->id_, __FUNCTION__);

        }

    }
}

void ScatFormWavvy::initiate_connected_up_to_down_action(bd_addr_t rmt) {
    // action depends .. on whether you are a source or intermediate ..
    // you should not reach this state if you are a sink ..

    node_type n_type = get_node_type();
    if (n_type == SOURCE) {
        MsgCandidate candid_msg(this->id_, false);
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else if (n_type == INTERMEDIATE) {
        // find the maximum of all the nodes in candidate_table ..
        node_id max_candidate = (max_map_value(candidate_table.begin(), candidate_table.end()))->second;

        // Q should we select a different name instead of CmdCandidate?
        // A: No, we don't need that .. the reaction to this message is similar to
        // the one above ..
        MsgCandidate candid_msg(max_candidate, false);
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else {
        fprintf(stderr, "error (%d): in function %s \n", this->id_, __FUNCTION__);
        abort();
    }
}

void ScatFormWavvy::initiate_connected_down_to_up_action(bd_addr_t rmt) {
    // action depends on whether you are a sink or intermediate ..
    // you should not reach this state if you are a source ..
}




void ScatFormWavvy::recv_handler(SFmsg* msg, int rmt) {


    // keep track of rcvd statistics ..
    msg_stats.rcvd ++;

    if (this->trace_node() && false) {
        fprintf(stderr, "node %d received msg from %d \n", this->id_, rmt);
    }

    switch (msg->code) {
    case CmdCandidate:
        recv_handler_cmd_candidate(msg, rmt);
        break;

    default:
        fprintf(stderr, "*** Error at %d: unsupported type of messages \n", id_);
        abort();
        break;
    }
}


void ScatFormWavvy::recv_handler_cmd_candidate(SFmsg* msg, int rmt) {

    MsgCandidate* rcvd_msg = (MsgCandidate*) msg->data;

    if (trace_node()) {
        fprintf(stderr, "node %d received cmd_candidate msg from %d: [reply:%s, cand_id:%d] \n",
                this->id_, rmt, bool2str(rcvd_msg->reply), rcvd_msg->candidate_id);
    }

    if (! rcvd_msg->reply) {

        // TODO here - we assume that everything will be correct .. perhaps
        // we should check the existence of rmt before ..
        candidate_table[rmt] = rcvd_msg->candidate_id;
        up_neighbors.mark_contacted_by_node_id(rmt);

        // mark_neighbor_contacted(up_neighbors, rmt);

        MsgCandidate candid_msg(this->id_, true);
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else {

        // mark_neighbor_contacted(down_neighbors, rmt);
        down_neighbors.mark_contacted_by_node_id(rmt);
        disconnect_page(rmt);

    }
}


// printign the final result ..
void ScatFormWavvy::_printResultsInFile(const char* filename) {
  FILE *cfPtr;

  if ((cfPtr = fopen(filename, "w")) == NULL) {
    fprintf(stderr, "*** Error: ScatFormTesting: File could not be opened");

  } else {

   print_all(cfPtr);

  } // end of if-statement

  fclose(cfPtr);

}


void ScatFormWavvy::print_all(FILE* cfPtr) {

    BTNode *wk = node_;
    do {
        ((ScatFormWavvy *) wk->scatFormator_)->print_result_in_row(cfPtr);
    } while ((wk = wk->getNext()) != node_);

}

void ScatFormWavvy::print_result_in_row(FILE* cfPtr) {



    fprintf(cfPtr, "<id> %d ", id_);

    //TODO: how to handle the masters and slaves of a given node. ..
    // create a function in ScatFormWavvy .. make_master(node_id), make_slave(node_id) ..
    //

    fprintf(cfPtr, "<iterations> %d ", this->current_iteration + 1);

    // Note: We don't need sent msgs .. we should only have rcvd .. since sent is
    // hard to track + it is equivalanet to rcvd.
    // fprintf(cfPtr, "<sentmsgs> %d ", this->msg_stats.sent);
    fprintf(cfPtr, "<rcvdmsgs> %d ", this->msg_stats.rcvd);
    fprintf(cfPtr, "<numconns> %d ", this->msg_stats.conns);


    fprintf(cfPtr, "<finish> %2.3f ", finishing_time);


    fprintf(cfPtr, "\n");


}



// TODO test ..
void ScatFormWavvy::seperate_all_neighbors() {
    for (unsigned int i = 0; i < all_neighbors.size(); i++) {
        const wavvy_neighbor& temp_node = all_neighbors[i];

        if (temp_node.get_id() > this->id_) {
            up_neighbors.insert_node(temp_node);
        } else if (temp_node.get_id() < this->id_) {
            down_neighbors.insert_node(temp_node);
        } else {
            fprintf(stderr, "error(%d): neighbor %d has my id!\n",
                    this->id_, temp_node.get_id());
            abort();
        }
    }

}

void ScatFormWavvy::make_unnecessary_neighbor(node_id u, nvect& neighbors_list) {
    move_node(u, &neighbors_list, &this->unnecessary_neighbors);
}


void ScatFormWavvy::flip_neighbor_direction(node_id u, const char* code) {

    nvect *from, *to;

    if (strcmp(code, "out-to-in")) {
        from = &down_neighbors;
        to = &up_neighbors;
    } else if (strcmp(code, "in-to-out")) {
        from = &up_neighbors;
        to = &down_neighbors;
    } else {
        fprintf(stdout, "error: code %s in %s is wrong \n", code, __FUNCTION__);
    }

    move_node(u, from, to);
}
