// TODO:
// more tests and tests and tests .. (YO-YO).           (done with simple tests).
// verify_correctness().
// simple yo-yo experiments (comapres against a major algorithm at least).
// max_node() .. how it should be defined. change weight.
// additional round (out_degree_limitation)
// additional round (no M/S bridges).

#include "scat_wavvy_bsf.h"



// constructor:
ScatFormWavvy::ScatFormWavvy(BTNode *n):
    ScatFormator(n),
    my_status(INIT),
    current_iteration(0),
    my_role(NONE),
    finishing_time (0),
    parent_id(-1),
    down_neighbors_yes_no_table(this),
    up_neighbors_yes_no_table(this) {


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
    // return (true);            // trace every node.
    return (id_ == 13 || id_ == 30);
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

}

// main function ..
void ScatFormWavvy::_main() {

    init_scat_formation_algorithm();    // here: all nodes are already inserted into


    change_status(UP_TO_DOWN_WAIT);
}

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



    if (trace_node()) {
        fprintf(stderr, "node %d is building the yes_no value of %d \n", this->id_, rmt);
    }

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
    bool kill_cond = (max_forwarder != rmt);         // ORIGINAL RULE.



    if (yes_no_cond) {
        this->up_neighbors_yes_no_table.set_value(rmt, YES);
    } else {
        this->up_neighbors_yes_no_table.set_value(rmt, NO);
    }

    // note that KILL overrides YES and NO.
    if (kill_cond) {
        this->kill_me_neighbors.push_back(rmt);
        this->up_neighbors_yes_no_table.set_value(rmt, KILL);
    }

    if (trace_node()) {
        fprintf(stderr, "after exec. %s .. up_neighbor_yes_no_table[%d] = %s \n",
                __FUNCTION__, rmt, response2str(this->up_neighbors_yes_no_table.get_value(rmt)));
    }
}

void ScatFormWavvy::build_up_neighbors_yes_no_table() {


    node_id max_candidate = (max_map_value(candidate_table.begin(), candidate_table.end()))->second;
    for (unsigned int i = 0; i < up_neighbors_p1.size(); i++) {

        build_up_neighbors_yes_no_table(up_neighbors_p1.find_node_by_index(i).get_id(), max_candidate);
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

    if (trace_node()) {
        fprintf(stderr, "up_neighbors_yes_no_table[%d]: ", this->id_);
        up_neighbors_yes_no_table.print();
    }

    node_id the_yes_neighbor = -1;
    if (up_neighbors_yes_no_table.only_one_yes_neighbor(the_yes_neighbor)) {
        if (down_neighbors_p1.size() == 0 || rcvd_kill_or_your_child_f_all_d_neighbors()) {
            parent_id = the_yes_neighbor ;
            this->up_neighbors_yes_no_table.set_value(the_yes_neighbor, YOUR_CHILD);

            if (trace_node()) {
                fprintf(stderr, "node %d set the flag of %d as YOUR_CHILD since it has no down_neighbors in this "
                        "iteration and it received kill or your_child from all down_neighbors\n",
                        this->id_, the_yes_neighbor);
            }
        }
    }
}

// check all down_neighbors_p1 .. if received KILL or YOUR_CHILD
bool ScatFormWavvy::rcvd_kill_or_your_child_f_all_d_neighbors() {
    return down_neighbors_yes_no_table.is_all_kill_or_your_child();
}

// check if there is only one neighbor that sent YES. return its ide in the_yes_neighbor.
// bool ScatFormWavvy::only_one_yes_neighbor(node_id& the_yes_neighbor) {

//    return up_neighbors_yes_no_table.only_one_yes_neighbor(the_yes_neighbor);

// }

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

            if (trace_node()) {
                fprintf(stderr, "node %d is waiting for:", this->id_);
                up_neighbors_p1.print();
            }

            wait_in_page_scan();


        } else {
            cancel_page_scan();
            change_status(UP_TO_DOWN_ACTION);
        }

        break;

    case UP_TO_DOWN_ACTION:
        if (! down_neighbors_p1.is_all_contacted()) {

            if (trace_node()) {
                fprintf(stderr, "node %d needs to contact:", this->id_);
                down_neighbors_p1.print();
            }

            // connect_page(find_promising_receiver());
            connect_page(down_neighbors_p1.next_not_contacted().get_id());
        } else {
            // change_status(TERMINATE);
            change_status(DOWN_TO_UP_WAIT);
        }


        break ;

    case DOWN_TO_UP_WAIT:


        if (! down_neighbors_p2.is_all_contacted()) {
            if (trace_node()) {
                fprintf(stderr, "node %d is waiting for:", this->id_);
                down_neighbors_p2.print();
            }

            wait_in_page_scan();

        } else {
            cancel_page_scan();
            change_status(DOWN_TO_UP_ACTION);
        }
        break;

    case DOWN_TO_UP_ACTION:

        if (! up_neighbors_p2.is_all_contacted()) {

            if (trace_node()) {
                fprintf(stderr, "node %d needs to contact:", this->id_);
                up_neighbors_p2.print();
            }

            connect_page(up_neighbors_p2.next_not_contacted().get_id());


        } else {
            handle_iteration_end();                 // the sophisticated version.
        }
        break;

    default:
        fprintf(stderr, "**** Error: %d unsupported case %s \n", this->id_, state_str(this->my_status));
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
        if (down_neighbors_yes_no_table.is_none_NO()) {
            fprintf(stderr, "node %d is surviving iteration %d .. \n",
                    this->id_, this->current_iteration);
        } else {
            fprintf(stderr, "node %d is not surviving iteration %d ..\n",
                    this->id_, this->current_iteration);
            down_neighbors_yes_no_table.print();
        }
    }

    // ********************************************************************************
    // ********************************************************************************
    // TODO: this is not done yet ..
    //
    // read the yes_no_table ..  (flipping edges!)
    // if received a no from a neighbor .. then flip it!
    response_type tt;
    node_id tid;
    nvect temp_up, temp_down;


    fprintf(stderr, "*******************************\n");

    for (unsigned int i = 0; i < down_neighbors_p1.size(); i++) {

        tid = this->down_neighbors_p1.find_node_by_index(i).get_id();
        tt = down_neighbors_yes_no_table.get_value(tid);

        if (trace_node()) {
            fprintf(stderr, "node %d is checking the yes/no state of %d = %s \n", this->id_, tid, response2str(tt));
        }


        // we do nothing when kill or your_child.
        if (tt == NO) {
            temp_up.insert_node(tid);
            if (trace_node()) {
                fprintf(stderr, "*** NO: node %d will consider %d as in-to-out neighbors ... \n",
                    this->id_, tid);
            }

        } else if (tt == YES) {
            temp_down.insert_node(tid);

            if (trace_node()) {
                fprintf(stderr, "*** YES: node %d will consider %d without change  ... \n",
                    this->id_, tid);
            }

        } else if (tt == NOT_KNOWN) {
            fprintf(stderr, "error in %d yes_no_map[%d] = NOT_KNOWN at %s \n",
                    this->id_, tid, __FUNCTION__);
            abort();
        }
    }

    for (unsigned int i = 0; i < up_neighbors_p1.size(); i++) {

        tid = this->up_neighbors_p1.find_node_by_index(i).get_id();
        tt = up_neighbors_yes_no_table.get_value(tid);


        if (trace_node()) {
            fprintf(stderr, "node %d is checking the yes/no state of %d = %s \n", this->id_, tid, response2str(tt));
        }

        // we do nothing when kill or your_child
        if (tt == NO) {
            temp_down.insert_node(tid);
            if (trace_node()) {
                fprintf(stderr, "*** NO: node %d will consider %d as in to in neighbors ..\n",
                    this->id_, tid);
            }

        } else if (tt == YES) {
            temp_up.insert_node(tid);

            if (trace_node()) {
                fprintf(stderr, "*** YES: node %d will consider %d without change.  ..\n",
                    this->id_, tid);
            }

        } else if (tt == NOT_KNOWN) {
            fprintf(stderr, "error in %d yes_no_map[%d] = NOT_KNOWN at %s \n",
                    this->id_, tid, __FUNCTION__);
            abort();
        }
    }


    // check for dublicates - here?


    this->up_neighbors_p1 = temp_up;
    this->down_neighbors_p1 = temp_down;


    this->up_neighbors_p2 = this->up_neighbors_p1;
    this->down_neighbors_p2 = this->down_neighbors_p1;

    if (trace_node()) {

        fprintf(stderr, "the new up_neighbors (p1) of (%d) : ", this->id_);
        this->up_neighbors_p1.print();
        fprintf(stderr, "the new down_neighbors  (p1) of (%d) : ", this->id_);
        this->down_neighbors_p1.print();


        fprintf(stderr, "the new up_neighbors (p2) of (%d) : ", this->id_);
        this->up_neighbors_p2.print();
        fprintf(stderr, "the new down_neighbors  (p2) of (%d) : ", this->id_);
        this->down_neighbors_p2.print();
    }



    // .. when to terminate ..
    // now .. we just should switch to terminate.
    if (down_neighbors_p1.size() == 0 && up_neighbors_p1.size() == 0) {
        if (trace_node()) { fprintf(stderr, "*** node %d decides to terminate the algorithm at iteration %d \n",
                                    this->id_, current_iteration); }
        change_status(TERMINATE);
    } else {

        current_iteration ++;
        up_neighbors_yes_no_table.clear();
        down_neighbors_yes_no_table.clear();
        candidate_table.erase(candidate_table.begin(), candidate_table.end());

        if (trace_node()) { fprintf(stderr, "*** node %d starting a new iteration %d \n", this->id_, current_iteration); }
        change_status(UP_TO_DOWN_WAIT);
    }

    // for future: ..
    // check whether function terminated or not ..
    // TODO: test with as many scenarios as possible ..
    // This is the most important part of the algorithm ..

}

void ScatFormWavvy::connected(bd_addr_t rmt) {
    Scheduler & s = Scheduler::instance();

    s.cancel(&watchDogEv_);
    s.schedule(&timer_ , &watchDogEv_ , WATCH_DOG_TIMER );

    if (trace_node()) {

        fprintf(stderr, "**the action neighbors are: \n");
        fprintf(stderr, "**down_neighbors_p1 (%d): ", this->id_);
        down_neighbors_p1.print();

        fprintf(stderr, "**up_neighbors_p2 (%d): ", this->id_);
        up_neighbors_p2.print();


    }

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
        MsgCandidate candid_msg(this->id_, false, this->current_iteration);


        if (trace_node()) {
            fprintf(stderr, "source node %d sent msg_candidate to %d (reply:%s, candid_id:%d, iter:%d)\n",
                    this->id_, rmt, bool2str(candid_msg.reply),
                    candid_msg.candidate_id,
                    candid_msg.iteration);
        }
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else if (n_type == INTERMEDIATE) {
        // find the maximum of all the nodes in candidate_table ..
        node_id max_candidate = (max_map_value(candidate_table.begin(), candidate_table.end()))->second;

        // Q should we select a different name instead of CmdCandidate?
        // A: No, we don't need that .. the reaction to this message is similar to
        // the one above ..
        MsgCandidate candid_msg(max_candidate, false, this->current_iteration);

        if (trace_node()) {
            fprintf(stderr, "intermediate node %d sent msg_candidate to %d (reply:%s, max: candid_id:%d, iter:%d)\n",
                    this->id_, rmt, bool2str(candid_msg.reply),
                    candid_msg.candidate_id,
                    candid_msg.iteration);
        }
        sendMsg(CmdCandidate, (uchar *) &candid_msg, sizeof(MsgCandidate), rmt, rmt);

    } else {
        fprintf(stderr, "error (%d): in function %s \n", this->id_, __FUNCTION__);
        abort();
    }
}


void ScatFormWavvy::initiate_connected_down_to_up_action(bd_addr_t rmt) {




    node_id max_candidate = (max_map_value(candidate_table.begin(), candidate_table.end()))->second;
    response_type tt = up_neighbors_yes_no_table.get_value(rmt);
    bool yes_no_cond = (tt == YES || tt == YOUR_CHILD);
    bool kill_cond = (tt == KILL);
    // bool child_parent_cond = (parent_id != -1);
    bool child_parent_cond = (tt == YOUR_CHILD);

    if (trace_node()) {
        fprintf(stderr, "node %d preparing a cmd_result to %d with yes_no status = %s \n",
                this->id_, rmt, response2str(tt));
    }

    MsgResult result_msg(max_candidate,
                         yes_no_cond,
                         kill_cond,
                         child_parent_cond,
                         false,
                         this->current_iteration);


    if (trace_node()) {

        fprintf(stderr, "node %d send cmd_result message to %d with (mc: %d, y_n:%s, k:%s, cp:%s, reply:%s, iter:%d)\n",
                this->id_, rmt, result_msg.max_candidate_id,
                bool2str(result_msg.yes_no),
                bool2str(result_msg.kill),
                bool2str(result_msg.is_child),
                bool2str(result_msg.reply),
                this->current_iteration);
    }

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

    case CmdBusy:
        recv_handler_cmd_busy(msg, rmt);
        break;

    default:
        fprintf(stderr, "*** Error at %d: unsupported type of messages \n", id_);
        abort();
        break;
    }
}

void ScatFormWavvy::recv_handler_cmd_busy(SFmsg* msg, int rmt) {
    if (trace_node()) {

        fprintf(stderr, "**the action neighbors are: \n");
        fprintf(stderr, "**down_neighbors_p1 (%d): ", this->id_);
        down_neighbors_p1.print();

        fprintf(stderr, "**up_neighbors_p2 (%d): ", this->id_);
        up_neighbors_p2.print();


    }

    if (trace_node()) {
        fprintf(stderr, "node %d received a cmd_busy from %d\n", this->id_, rmt);
    }
    // simply disconnet the link - id: must be a master in this case, since reply is sent only by slaves.

    // debug .. now.
    // this->busyCond_ = true;
    // well this works ..
    // Let's make it better ..

    // should we set the busyCond_ flag or not? well that depends whether the next not contacted
    // receiver is the same as rmt.
    set_busy_cond_if_required(rmt);

    disconnect_page(rmt);

}

void ScatFormWavvy::set_busy_cond_if_required(const node_id& rmt) {




    nvect* nlist_ptr = NULL;
    switch (this->my_status) {
    case UP_TO_DOWN_ACTION:
        nlist_ptr = &(this->down_neighbors_p1);
        break;

    case DOWN_TO_UP_ACTION:
        nlist_ptr = &(this->up_neighbors_p2);
        break;

    default:
        fprintf(stderr, "error in %d .. case %s is not considered in %s \n",
                this->id_,
                state_str(this->my_status),
                __FUNCTION__);
    };


    if (nlist_ptr->next_not_contacted_id() == rmt) {
        this->busyCond_ = true;
    }




}

void ScatFormWavvy::send_busy_msg(int rmt) {
    // prepare a busy message .. and return.
    MsgBusy busy_msg(true);
    if (trace_node()) {
        fprintf(stderr, "busy: node %d is sending busy to %d since iteration does not match \n",
                this->id_, rmt);
    }

    sendMsg(CmdBusy, (uchar *) &busy_msg, sizeof(MsgBusy), rmt, rmt);
}

void ScatFormWavvy::recv_handler_cmd_result(SFmsg* msg, int rmt) {

    MsgResult* rcvd_msg = (MsgResult *) msg->data;


    if (trace_node()) {
        fprintf(stderr,
                "node %d received cmd_result msg from %d:(reply:%s, yes_no:%s, kill:%s, your_child:%s, "
                "max_cand_id:%d, iter: %d) \n",
                this->id_, rmt, bool2str(rcvd_msg->reply),
                bool2str(rcvd_msg->yes_no),
                bool2str(rcvd_msg->kill),
                bool2str(rcvd_msg->is_child),
                rcvd_msg->max_candidate_id,
                rcvd_msg->iteration);
    }


    // busy cases:
    if (rcvd_msg->iteration != this->current_iteration) {
        send_busy_msg(rmt);
        return;
    }


    if (! rcvd_msg->reply) {

        down_neighbors_yes_no_table.set_value(rmt, rcvd_msg->yes_no ? YES : NO);
        down_neighbors_p2.mark_contacted_by_node_id(rmt, true);


        if (rcvd_msg->kill) {
            this->kill_me_neighbors.push_back(rmt);
            down_neighbors_yes_no_table.set_value(rmt, KILL);
        }


        if (rcvd_msg->is_child) {
            children_id.push_back(rmt);
            down_neighbors_yes_no_table.set_value(rmt, YOUR_CHILD);
        }

        if (trace_node()) {
            fprintf(stderr, "node %d marked node %d as contacted and set its yes_no value to %s\n",
                    this->id_, rmt,
                    response2str(down_neighbors_yes_no_table.get_value(rmt)));

            fprintf(stderr, "** now the list of contacted down_neighbors_2(%d) is: ", this->id_);
            down_neighbors_p2.print();
        }


        // we only care here about the reply=tre flag .. every other flag is of no importance.
        // see the else part of this if-statement.
        MsgResult result_msg(this->id_, false,  false, false, true, this->current_iteration);
        if (trace_node()) {
            fprintf(stderr, "node %d sent msg_result to %d (reply:%s, yes_no:%s, kill: %s, your_child: %s, max_candid_id:%d)\n",
                    this->id_, rmt,
                    bool2str(result_msg.reply),
                    bool2str(result_msg.yes_no),
                    bool2str(result_msg.kill),
                    bool2str(result_msg.is_child),
                    result_msg.max_candidate_id);
        }
        sendMsg(CmdResult, (uchar *) &result_msg, sizeof(MsgResult), rmt, rmt);


    } else {

        up_neighbors_p2.mark_contacted_by_node_id(rmt, true);
        disconnect_page(rmt);

        if (trace_node()) {
            fprintf(stderr, "node %d marked %d as contacted and destroyed link .. \n",
                    this->id_, rmt);
            fprintf(stderr, "** now the list of contacted up_neighbors_2(%d) is: ", this->id_);
            up_neighbors_p2.print();
        }

    }

}

void ScatFormWavvy::recv_handler_cmd_candidate(SFmsg* msg, int rmt) {

    MsgCandidate* rcvd_msg = (MsgCandidate*) msg->data;

    if (trace_node()) {
        fprintf(stderr, "node %d received cmd_candidate msg from %d: (reply:%s, cand_id:%d) \n",
                this->id_, rmt, bool2str(rcvd_msg->reply), rcvd_msg->candidate_id);
    }


    // busy cases:
    if (rcvd_msg->iteration != this->current_iteration) {
        send_busy_msg(rmt);
        return;
    }

    if (! rcvd_msg->reply) {

        candidate_table[rmt] = rcvd_msg->candidate_id;
        up_neighbors_p1.mark_contacted_by_node_id(rmt, true);

        if (trace_node()) {
            fprintf(stderr, "node %d added %d to its candidates and marked %d contacted .. \n",
                    this->id_, rcvd_msg->candidate_id, rmt);

            fprintf(stderr, "** now the list of contacted up_neighbors_1(%d) is: ", this->id_);
            up_neighbors_p1.print();

        }

        // mark_neighbor_contacted(up_neighbors, rmt);

        MsgCandidate candid_msg(this->id_, true, this->current_iteration);
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

            fprintf(stderr, "** now the list of contacted down_neighbors_1(%d) is: ", this->id_);
            down_neighbors_p1.print();
        }
    }
}


// printign the final result ..
void ScatFormWavvy::_printResultsInFile(const char* filename) {

    // before printing the final result, we need to verify that the execution is correct
    // using simple rules.
    // We know that _printResultsInFile is called only when the algorithm terminates, and hence
    // it is logical to call verify_correctness() from this point of the algorithm.
    // of course, we can write a commend in the tcl file. However, this would require a modification
    // on other files .. I don't think it would be necessary to do that.
    verify_correctness();

    // there is no need to write an if-statement to check whether the correctness is correct.
    // correctness will print the results on the stderr output.


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


    cfPtr = stderr;

    fprintf(cfPtr, "<id> %d ", id_);

    //TODO: how to handle the masters and slaves of a given node. ..
    // create a function in ScatFormWavvy .. make_master(node_id), make_slave(node_id) ..
    //

    fprintf(cfPtr, "<parent> %d ", this->parent_id);

    fprintf(cfPtr, "<children> ");
    for (auto i = 0; i < this->children_id.size(); i++)
        fprintf(cfPtr, "%d ", this->children_id[i]);

    fprintf(cfPtr, "<iterations> %d ", this->current_iteration + 1);

    // Note: We don't need sent msgs .. we should only have rcvd .. since sent is
    // hard to track + it is equivalanet to rcvd.
    // fprintf(cfPtr, "<sentmsgs> %d ", this->msg_stats.sent);
    fprintf(cfPtr, "<rcvdmsgs> %d ", this->msg_stats.rcvd);
    fprintf(cfPtr, "<numconns> %d ", this->msg_stats.conns);


    fprintf(cfPtr, "<finish> %2.3f ", finishing_time);


    fprintf(cfPtr, "\n");


}




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

node_id ScatFormWavvy::find_max_node_in_scatternet() {
    node_id max_id = -1;
    BTNode *wk = node_;
    ScatFormWavvy *c_node;
    do {
        c_node = ((ScatFormWavvy *) wk->scatFormator_);
        if (c_node->id_ > max_id) max_id = c_node->id_;
    } while ((wk = wk->getNext()) != node_);
    return max_id;
}

void ScatFormWavvy::verify_correctness() {

    BTNode *wk = node_;
    ScatFormWavvy *cnode;
    char buffer_string[12800];
    char error_message[128];
    strcpy (buffer_string,"");

    node_id max_id = find_max_node_in_scatternet();
    unsigned int root_count = 0;
    bool error = false;

    do {
        cnode = ((ScatFormWavvy *) wk->scatFormator_);

        if (cnode->parent_id == -1) {
            root_count ++;

            if (cnode->id_ != max_id) {
                error = true;
                sprintf(error_message, "node %d is a root but not the max\n", cnode->id_);
                strcat(buffer_string, error_message);

            }

            if (root_count > 1) {
                error = true;

                sprintf(error_message, "node %d is found to be root .. now we have %d root nodes \n",
                        cnode->id_, root_count);
                strcat(buffer_string, error_message);
            }

        }

        if (cnode->my_status != TERMINATE) {
            error = true;
            sprintf(error_message, "node %d did not terminate!\n", cnode->id_);
            strcat(buffer_string, error_message);
        }



    } while ((wk = wk->getNext()) != node_);


    // TODO: starting from the maximum node, find if all nodes were visited (i.e. check connectivity).
    // We usually do this in analyze_result script.

    if (error) {
        fputs(buffer_string, stderr);
        abort();
    }
}
