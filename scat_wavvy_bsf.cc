// how to terminate the algorithm?
// well, basically, you need to know if you have
// 0 down_neighbors, and 1 up_neighbors.
// This can be known on-the-fly!
// How?




#include "scat_wavvy_bsf.h"



// constructor:
ScatFormWavvy::ScatFormWavvy(BTNode *n):
    ScatFormator(n),
    my_status(INIT),
    current_iteration(0),
    my_role(NONE),
    finishing_time (0),
    yes_no_table(this){


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
        if (trace_node()) {
            fprintf(stderr, "node %d is adding neighbor %d to its neighborhood list ..\n",
                    this->id_, id);
        }


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

    this->up_neighbors_p2 = this->up_neighbors_p1;
    this->down_neighbors_p2 = this->down_neighbors_p1;


    this->up_neighbors_p1.mark_contacted_all(false);
    this->up_neighbors_p2.mark_contacted_all(false);
    this->down_neighbors_p1.mark_contacted_all(false);
    this->down_neighbors_p2.mark_contacted_all(false);



    fprintf(stderr, "node %d in %s: down_neighbors_p2: ", this->id_, state_str(this->my_status));
    down_neighbors_p2.print();

    fprintf(stderr, "node %d in %s: down_neighbors_p1: ", this->id_, state_str(this->my_status));
    down_neighbors_p1.print();

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

        ex_round_messages();
        break;

    case DOWN_TO_UP_ACTION:

        // initiate the state.
        init_state_down_to_up_action();
        ex_round_messages();
        break;

    case TERMINATE:
        this->finishing_time = Scheduler::instance().clock() ;
        if (trace_node()) {
            fprintf(stderr, "node %d terminated the algorithm at %2.3f\n",
                    this->id_, this->finishing_time);
        }
        break;

    default:
        fprintf(stderr, "error: state  is unsupportable in %s  ..\n", __FUNCTION__);
        abort();
    }


}

void ScatFormWavvy::build_up_neighbors_yes_no_table(node_id rmt, node_id max_candidate) {
    // the candidate that rmt forwarded.
    node_id the_candidate = candidate_table[rmt];

    // Q1: is rmt the maximum forwarder of candidate the_candidate?
    node_id max_forwarder = rmt;
    for (map<node_id,node_id>::iterator it = candidate_table.begin();
         it != candidate_table.end();
         it++) {

        if (it->second == the_candidate && it->first > rmt) {
            max_forwarder = it->first;
        }
    }

    // if answer of Q1 is yes!
    bool yes_no_cond = (this->down_neighbors_yes_no_table.is_none_NO()) &&
            (the_candidate == max_candidate);
    bool kill_cond = (max_foreward != rmt);

    if (yes_no_cond) {
        this->yes_no_table.set_value(rmt, YES);
    } else {
        this->yes_no_table.set_value(rmt, NO);
    }

    // note that KILL overrides YES and NO.
    if (kill_cond) {
        this->kill_me_neighbors.push_back(rmt);
        this->yes_no_table.set_value(rmt, KILL);
    }
}

void ScatFormWavvy::build_up_neighbors_yes_no_table() {


    node_id max_candidate = (max_map_value(candidate_table.begin(), candidate_table.end()))->second;
    for (unsigned int i = 0; i < up_neighbors_p1.size(); i++) {
        build_up_neighbors_yes_no_table(up_neighbors_p1.find_node_by_index(i), max_candidate);
    }

}

void ScatFormWavvy::init_state_down_to_up_action() {




    // YES is sent to the max neighbor that sent the maximum candidate.
    // NO is sent to the max neighbor that sent any other candidate.
    // every other neighbor is given KILL.


    // additional structures --
    // down_neighbors_yes_no_table, up_neigbhors_yes_no_table.
    // down_neighbors_yes_no_table -- built as a result of rcvd messages from down
    //                              -- neighbors.
    // up_neighhnors_yes_no_table .. built in this stage.
    build_up_neighbors_yes_no_table();


    // what shall be done in this function then?
    // That is, how do we know whether we should terminat or not?

    // rules:
    // if a node u has only one up_neighbor v as yes, while everything else is kill (or just don't exist)
    // then: (u,v) becomes yes+terminated.
    //        v: should take the new


    // Ok: so, each node except the root, shall sent a "your_child" command to only one
    // up_neighbor. This has the same action of "kill" - except that it stores the parent-child
    // relationship in a special vector. .
    //
    // rules:
    pair<node_id, node_id> child_parent(-1,-1);
    node_id y;
    if (onle_one_yes_neighbor(y)) {
        if (down_neighbors_p1.size() == 0 || rcvd_kill_f_all_d_neighbors()) {
            child_parent.first = this->id_;
            child_parent.second = y;
        }
    }
}

// check all down_neighbors_p1 .. if received KILL or YOUR_CHILD
bool ScatFormWavvy::rcvd_kill_f_all_d_neighbors() {
    return true;
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
        if (! up_neighbors_p1.is_all_contacted()) {
            wait_in_page_scan();


        } else {
            cancel_page_scan();
            change_status(UP_TO_DOWN_ACTION);
        }

        break;

    case UP_TO_DOWN_ACTION:
        if (! down_neighbors_p1.is_all_contacted()) {

            // connect_page(find_promising_receiver());
            connect_page(down_neighbors_p1.next_not_contacted().get_id());
        } else {
            // change_status(TERMINATE);
            change_status(DOWN_TO_UP_WAIT);
        }


        break ;

    case DOWN_TO_UP_WAIT:


        if (! down_neighbors_p2.is_all_contacted()) {

            wait_in_page_scan();

        } else {
            cancel_page_scan();
            change_status(DOWN_TO_UP_ACTION);
        }
        break;

    case DOWN_TO_UP_ACTION:

        if (! up_neighbors_p2.is_all_contacted()) {
            connect_page(up_neighbors_p2.next_not_contacted().get_id());
        } else {
            handle_iteration_end();                 // the sophisticated version.
        }
        break;

    default:
        fprintf(stderr, "**** Error: %d unsupported case %d \n", this->id_, state_str(this->my_status));
        abort();
        break;

    }
}

void ScatFormWavvy::handle_iteration_end() {


    // handle the end of an iteration ..
    // which nodes survived ..
    // -- wait for up_neighbors_p1, up_neighbors_p2, down_neighbors_p1, down_neighbors_p2
    // -- we can't reach this level if those are not already marked.
    // to be all contacted ..
    // flip edges as necessary ..
    // increase iteration ..

    if ( !(up_neighbors_p1.is_all_contacted() &&
           up_neighbors_p2.is_all_contacted() &&
           down_neighbors_p1.is_all_contacted() &&
           down_neighbors_p2.is_all_contacted())
         ) {

        fprintf(stderr, "error in %d .. some neighbors has not been contacted twice .. how come?",
                this->id_);
    }


    if (get_node_type() == SOURCE) {
        if (yes_no_table.is_all_yes()) {
            fprintf(stderr, "node %d is a surviving iteration %d .. \n",
                    this->id_, this->current_iteration);
        } else {
            fprintf(stderr, "node %d is not surviving iteration %d ..\n",
                    this->id_, this->current_iteration);
            yes_no_table.print();
        }
    }


    // read the yes_no_table ..  (flipping edges!)
    // if received a no from a neighbor .. then flip it!
    response_type tt;
    node_id tid;
    vector<node_id> in_to_out_nodes;
    nvect temp_up, temp_down;

    for (unsigned int i = 0; i < down_neighbors_p1.size(); i++) {

        tid = this->down_neighbors_p1.find_node_by_index(i).get_id();
        tt = yes_no_table.get_value(tid);


        // we do nothing when kill ..
        if (tt == NO) {
            temp_up.insert_node(tid);
            fprintf(stderr, "*** NO: node %d will consider %d as in to out neighbors ... ",
                    this->id_, tid);

        } else if (tt == YES) {
            temp_down.insert_node(tid);

        } else if (tt == NOT_KNOWN) {
            fprintf(stderr, "error in %d yes_no_map[%d] = NOT_KNOWN at %s \n",
                    this->id_, tid, __FUNCTION__);
            abort();
        }
    }

    for (unsigned int i = 0; i < up_neighbors_p1.size(); i++) {

        tid = this->up_neighbors_p1.find_node_by_index(i).get_id();
        tt = yes_no_table.get_value(tid);


        // we do nothing when kill ..
        if (tt == NO) {
            temp_down.insert_node(tid);
            fprintf(stderr, "*** NO: node %d will consider %d as in to in neighbors ... ",
                    this->id_, tid);

        } else if (tt == YES) {
            temp_up.insert_node(tid);

        } else if (tt == NOT_KNOWN) {
            fprintf(stderr, "error in %d yes_no_map[%d] = NOT_KNOWN at %s \n",
                    this->id_, tid, __FUNCTION__);
            abort();
        }
    }

    this->up_neighbors_p1 = temp_up;
    this->down_neighbors_p1 = temp_down;


    this->up_neighbors_p2 = this->up_neighbors_p1;
    this->down_neighbors_p2 = this->down_neighbors_p1;

    current_iteration ++;

    // .. when to terminate ..
    // now .. we just should switch to terminate.
    change_status(TERMINATE);

    // for future: ..
    // check whether function terminated or not ..
    // TODO: test with as many scenarios as possible ..
    // This is the most important part of the algorithm ..

}

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


        if (trace_node()) {
            fprintf(stderr, "source node %d sent msg_candidate to %d (reply:%s, candid_id:%d)\n",
                    this->id_, rmt, bool2str(candid_msg.reply),
                    candid_msg.candidate_id);
        }
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else if (n_type == INTERMEDIATE) {
        // find the maximum of all the nodes in candidate_table ..
        node_id max_candidate = (max_map_value(candidate_table.begin(), candidate_table.end()))->second;

        // Q should we select a different name instead of CmdCandidate?
        // A: No, we don't need that .. the reaction to this message is similar to
        // the one above ..
        MsgCandidate candid_msg(max_candidate, false);

        if (trace_node()) {
            fprintf(stderr, "intermediate node %d sent msg_candidate to %d (reply:%s, max: candid_id:%d)\n",
                    this->id_, rmt, bool2str(candid_msg.reply),
                    candid_msg.candidate_id);
        }
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else {
        fprintf(stderr, "error (%d): in function %s \n", this->id_, __FUNCTION__);
        abort();
    }
}


void ScatFormWavvy::initiate_connected_down_to_up_action(bd_addr_t rmt) {

    response_type tt = up_neighbors_yes_no_table.get_value(rmt);
    bool yes_no_cond = (tt == YES);
    bool kill_cond = (tt == KILL);

    MsgResult result_msg(max_candidate,
                         yes_no_cond,
                         kill_cond,
                         false);

    sendMsg(CmdResult, (uchar *) &result_msg, sizeof(MsgResult), rmt, rmt);

}


void ScatFormWavvy::recv_handler(SFmsg* msg, int rmt) {


    // keep track of rcvd statistics ..
    msg_stats.rcvd ++;

    if (this->trace_node()) {
        fprintf(stderr, "node %d received msg from %d (at %s) \n",
                this->id_, rmt, __FUNCTION__);
    }

    switch (msg->code) {
    case CmdCandidate:
        recv_handler_cmd_candidate(msg, rmt);
        break;

    case CmdResult:
        recv_handler_cmd_result(msg, rmt);
        break;

    default:
        fprintf(stderr, "*** Error at %d: unsupported type of messages \n", id_);
        abort();
        break;
    }
}

void ScatFormWavvy::recv_handler_cmd_result(SFmsg* msg, int rmt) {

    MsgResult* rcvd_msg = (MsgResult *) msg->data;


    if (trace_node()) {
        fprintf(stderr,
                "node %d received cmd_result msg from %d:(reply:%s, yes_no:%s, kill:%s"
                "max_cand_id:%d) \n",
                this->id_, rmt, bool2str(rcvd_msg->reply),
                bool2str(rcvd_msg->yes_no),
                bool2str(rcvd_msg->kill),
                rcvd_msg->max_candidate_id);
    }

    if (! rcvd_msg->reply) {

        yes_no_table.set_value(rmt, rcvd_msg->yes_no ? YES : NO);
        down_neighbors_p2.mark_contacted_by_node_id(rmt, true);


        if (rcvd_msg->kill) {
            this->kill_me_neighbors.push_back(rmt);
            yes_no_table.set_value(rmt, KILL);
        }

        if (trace_node()) {
            fprintf(stderr, "node %d marked node %d as contacted and set its yes_no value to %s\n",
                    this->id_, rmt, response2str(yes_no_table.get_value(rmt)));
        }


        MsgResult result_msg(this->id_, false, true);
        if (trace_node()) {
            fprintf(stderr, "node %d sent msg_result to %d (reply:%s, max: candid_id:%d)\n",
                    this->id_, rmt,
                    bool2str(result_msg.reply),
                    bool2str(result_msg.yes_no),
                    result_msg.max_candidate_id);
        }
        sendMsg(CmdResult, (uchar *) &result_msg, sizeof(MsgResult), rmt, rmt);


    } else {

        up_neighbors_p2.mark_contacted_by_node_id(rmt, true);
        disconnect_page(rmt);

        if (trace_node()) {
            fprintf(stderr, "node %d marked %d as contacted and destroyed link .. \n",
                    this->id_, rmt);
        }

    }

}

void ScatFormWavvy::recv_handler_cmd_candidate(SFmsg* msg, int rmt) {

    MsgCandidate* rcvd_msg = (MsgCandidate*) msg->data;

    if (trace_node()) {
        fprintf(stderr, "node %d received cmd_candidate msg from %d: (reply:%s, cand_id:%d) \n",
                this->id_, rmt, bool2str(rcvd_msg->reply), rcvd_msg->candidate_id);
    }

    if (! rcvd_msg->reply) {

        // TODO here - we assume that everything will be correct .. perhaps
        // we should check the existence of rmt before ..
        candidate_table[rmt] = rcvd_msg->candidate_id;
        up_neighbors_p1.mark_contacted_by_node_id(rmt, true);

        if (trace_node()) {
            fprintf(stderr, "node %d added %d to its candidates and marked %d contacted .. \n",
                    this->id_, rcvd_msg->candidate_id, rmt);
        }

        // mark_neighbor_contacted(up_neighbors, rmt);

        MsgCandidate candid_msg(this->id_, true);
        if (trace_node()) {
            fprintf(stderr, "node %d sent msg_candidate to %d (reply:%s, max: candid_id:%d)\n",
                    this->id_, rmt, bool2str(candid_msg.reply),
                    candid_msg.candidate_id);
        }
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else {

        // mark_neighbor_contacted(down_neighbors, rmt);
        down_neighbors_p1.mark_contacted_by_node_id(rmt, true);
        disconnect_page(rmt);

        if (trace_node()) {
            fprintf(stderr, "node %d marked %d as contacted and destroyed link .. \n",
                    this->id_, rmt);
        }
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
            up_neighbors_p1.insert_node(temp_node);
        } else if (temp_node.get_id() < this->id_) {
            down_neighbors_p1.insert_node(temp_node);
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

    if (strcmp(code, OUT_TO_IN )) {
        from = &down_neighbors_p1;
        to = &up_neighbors_p1;
    } else if (strcmp(code, IN_TO_OUT)) {
        from = &up_neighbors_p1;
        to = &down_neighbors_p1;
    } else {
        fprintf(stdout, "error: code %s in %s is wrong \n", code, __FUNCTION__);
    }

    move_node(u, from, to);
}
